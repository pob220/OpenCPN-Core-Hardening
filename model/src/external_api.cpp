/***************************************************************************
 * Copyright (C) 2026 OpenCPN contributors
 ***************************************************************************/

#include "model/external_api.h"

#include <algorithm>
#include <cmath>
#include <cctype>
#include <iomanip>
#include <sstream>
#include <utility>

#include "ocpn-nlohmann/json.hpp"
#include "picosha2.h"

namespace ocpn::control {
namespace {

using json = nlohmann::json;

HttpResponse JsonResponse(int status, const json& body) {
  return {status,
          {{"Content-Type", "application/json"},
           {"Cache-Control", "no-store"}},
          body.dump() + "\n"};
}

HttpResponse Error(int status, const std::string& code,
                   const std::string& message) {
  return JsonResponse(status,
                      {{"error", {{"code", code}, {"message", message}}}});
}

std::string Lower(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
    return static_cast<char>(std::tolower(c));
  });
  return value;
}

std::optional<std::string> Header(const HttpRequest& request,
                                  const std::string& wanted) {
  const auto lower_wanted = Lower(wanted);
  for (const auto& [name, value] : request.headers) {
    if (Lower(name) == lower_wanted) return value;
  }
  return std::nullopt;
}

bool HasScope(const TokenAuthorizer::Principal& principal,
              const std::string& scope) {
  return principal.scopes.count(scope) != 0;
}

std::string TokenDigest(const std::string& token) {
  return picosha2::hash256_hex_string(token);
}

bool IsLoopback(const std::string& address) {
  return address == "127.0.0.1" || address == "::1" ||
         address == "[::1]" || address.rfind("127.", 0) == 0;
}

HttpResponse RequireScope(const TokenAuthorizer::Principal& principal,
                          const std::string& scope) {
  if (HasScope(principal, scope)) return {};
  return Error(403, "permission_denied", "Required scope: " + scope);
}

std::string Iso8601(Clock::time_point time) {
  const auto raw = Clock::to_time_t(time);
  std::tm utc{};
#ifdef _WIN32
  gmtime_s(&utc, &raw);
#else
  gmtime_r(&raw, &utc);
#endif
  std::ostringstream stream;
  stream << std::put_time(&utc, "%Y-%m-%dT%H:%M:%SZ");
  return stream.str();
}

json CoordinateJson(const Coordinate& coordinate) {
  return {{"latitudeDegrees", coordinate.latitude_degrees},
          {"longitudeDegrees", coordinate.longitude_degrees}};
}

json RouteSummaryJson(const RouteSummary& route) {
  return {{"guid", route.guid},
          {"name", route.name},
          {"revision", route.revision},
          {"waypointCount", route.waypoint_count},
          {"isLayer", route.is_layer},
          {"isDraft", route.is_draft}};
}

json RouteJson(const RouteSnapshot& route) {
  json result = RouteSummaryJson(route);
  result["waypoints"] = json::array();
  for (const auto& waypoint : route.waypoints) {
    result["waypoints"].push_back(
        {{"guid", waypoint.guid},
         {"name", waypoint.name},
         {"position", CoordinateJson(waypoint.position)}});
  }
  return result;
}

const char* DecisionText(ChartSafetyDecision decision) {
  switch (decision) {
    case ChartSafetyDecision::Pass:
      return "pass";
    case ChartSafetyDecision::Fail:
      return "fail";
    case ChartSafetyDecision::Unknown:
      return "unknown";
  }
  return "unknown";
}

const char* AuthorityText(ChartSafetyAuthority authority) {
  switch (authority) {
    case ChartSafetyAuthority::Authoritative:
      return "authoritative";
    case ChartSafetyAuthority::Fallback:
      return "fallback";
    case ChartSafetyAuthority::Unknown:
      return "unknown";
  }
  return "unknown";
}

HttpResponse ServiceFailure(const ServiceError& error) {
  const bool client_error =
      error.code.rfind("invalid_", 0) == 0 || error.code == "not_ready" ||
      error.code == "active_route" || error.code == "layer_owned";
  const int status = error.code == "not_found" ? 404
                     : error.code == "conflict" ? 409
                     : client_error ? 400
                                    : 503;
  return Error(status, error.code, error.message);
}

bool ValidCoordinate(const Coordinate& point) {
  return std::isfinite(point.latitude_degrees) &&
         std::isfinite(point.longitude_degrees) &&
         point.latitude_degrees >= -90.0 && point.latitude_degrees <= 90.0 &&
         point.longitude_degrees >= -180.0 &&
         point.longitude_degrees <= 180.0;
}

Result<Coordinate> ParseCoordinate(const json& value) {
  try {
    Coordinate coordinate{value.at("latitudeDegrees").get<double>(),
                          value.at("longitudeDegrees").get<double>()};
    if (!ValidCoordinate(coordinate)) {
      return Result<Coordinate>::FromError("invalid_coordinate",
                                         "Coordinate is outside WGS84 bounds");
    }
    return Result<Coordinate>::FromValue(coordinate);
  } catch (const json::exception&) {
    return Result<Coordinate>::FromError(
        "invalid_coordinate",
        "Coordinate requires numeric latitudeDegrees and longitudeDegrees");
  }
}

Result<ChartSafetyConstraints> ParseConstraints(const json& body) {
  try {
    ChartSafetyConstraints constraints{
        body.at("minimumDepthMeters").get<double>(),
        body.value("landMarginNauticalMiles", 0.0)};
    if (!std::isfinite(constraints.minimum_depth_meters) ||
        constraints.minimum_depth_meters < 0.0 ||
        !std::isfinite(constraints.land_margin_nautical_miles) ||
        constraints.land_margin_nautical_miles < 0.0) {
      return Result<ChartSafetyConstraints>::FromError(
          "invalid_constraints", "Safety constraints must be finite and non-negative");
    }
    return Result<ChartSafetyConstraints>::FromValue(constraints);
  } catch (const json::exception&) {
    return Result<ChartSafetyConstraints>::FromError(
        "invalid_constraints", "minimumDepthMeters is required and must be numeric");
  }
}

HttpResponse ChartResultResponse(const ChartSafetyResult& result) {
  json body = {{"decision", DecisionText(result.decision)},
               {"authority", AuthorityText(result.authority)},
               {"causeCode", result.cause_code},
               {"chartDatabaseIdentity", result.chart_database_identity},
               {"constraints",
                {{"minimumDepthMeters", result.constraints.minimum_depth_meters},
                 {"landMarginNauticalMiles",
                  result.constraints.land_margin_nautical_miles}}},
               {"warnings", result.warnings}};
  if (result.failed_segment_index)
    body["failedSegmentIndex"] = *result.failed_segment_index;
  return JsonResponse(200, body);
}

Result<RouteMutation> ParseRouteMutation(const json& body) {
  try {
    RouteMutation mutation;
    mutation.name = body.at("name").get<std::string>();
    if (mutation.name.empty() || mutation.name.size() > 256)
      return Result<RouteMutation>::FromError(
          "invalid_route", "Route name must contain 1 to 256 characters");
    for (const auto& value : body.at("waypoints")) {
      const auto position = ParseCoordinate(value.at("position"));
      if (position.error)
        return Result<RouteMutation>::FromError(position.error->code,
                                                position.error->message);
      WaypointSnapshot waypoint;
      waypoint.guid = value.value("guid", std::string());
      waypoint.name = value.value("name", std::string());
      waypoint.position = *position.value;
      if (waypoint.guid.size() > 128 || waypoint.name.size() > 256)
        return Result<RouteMutation>::FromError(
            "invalid_route", "Waypoint name or GUID exceeds its size limit");
      mutation.waypoints.push_back(std::move(waypoint));
    }
    if (mutation.waypoints.size() < 2 || mutation.waypoints.size() > 10000)
      return Result<RouteMutation>::FromError(
          "invalid_route", "A route requires between 2 and 10000 waypoints");
    return Result<RouteMutation>::FromValue(std::move(mutation));
  } catch (const json::exception&) {
    return Result<RouteMutation>::FromError(
        "invalid_route", "Route requires a name and waypoint array");
  }
}

HttpResponse CommandResponse(int status, const RouteCommandResult& result) {
  json body = {{"commandId", result.command_id},
               {"changed", result.changed},
               {"warnings", result.warnings}};
  body["route"] = result.route ? RouteJson(*result.route) : json(nullptr);
  return JsonResponse(status, body);
}

}  // namespace

void TokenAuthorizer::Put(std::string token, Principal principal) {
  PutDigest(TokenDigest(token), std::move(principal));
}

void TokenAuthorizer::PutDigest(std::string token_sha256,
                                Principal principal) {
  std::lock_guard<std::mutex> lock(mutex_);
  tokens_[Lower(std::move(token_sha256))] = std::move(principal);
}

void TokenAuthorizer::Revoke(const std::string& token) {
  std::lock_guard<std::mutex> lock(mutex_);
  tokens_.erase(TokenDigest(token));
}

std::optional<TokenAuthorizer::Principal> TokenAuthorizer::Authenticate(
    const std::string& token) const {
  std::lock_guard<std::mutex> lock(mutex_);
  const auto found = tokens_.find(TokenDigest(token));
  if (found == tokens_.end()) return std::nullopt;
  return found->second;
}

ExternalApiRouter::ExternalApiRouter(
    ServiceBundle services, std::shared_ptr<TokenAuthorizer> authorizer,
    Options options)
    : services_(std::move(services)),
      authorizer_(std::move(authorizer)),
      options_(std::move(options)) {}

HttpResponse ExternalApiRouter::Handle(const HttpRequest& request) const {
  if (!options_.enabled)
    return Error(404, "api_disabled", "External control API is disabled");
  if (!options_.allow_lan && !IsLoopback(request.remote_address))
    return Error(403, "loopback_required",
                 "LAN access is disabled for the external control API");
  if (request.body.size() > options_.maximum_body_bytes)
    return Error(413, "request_too_large", "Request body exceeds configured limit");

  const auto authorization = Header(request, "Authorization");
  constexpr const char* prefix = "Bearer ";
  if (!authorization || authorization->rfind(prefix, 0) != 0)
    return Error(401, "authentication_required", "A Bearer token is required");
  if (!authorizer_)
    return Error(503, "authentication_unavailable", "Token service is unavailable");
  const auto principal = authorizer_->Authenticate(authorization->substr(7));
  if (!principal)
    return Error(401, "invalid_token", "Bearer token is invalid or revoked");
  return HandleAuthenticated(request, *principal);
}

HttpResponse ExternalApiRouter::HandleAuthenticated(
    const HttpRequest& request,
    const TokenAuthorizer::Principal& principal) const {
  const auto path = request.path.substr(0, request.path.find('?'));
  if (request.method == "GET" && path == "/api/v2/version") {
    return JsonResponse(200, {{"apiVersion", options_.api_version},
                              {"openCpnVersion", options_.server_version}});
  }
  if (request.method == "GET" && path == "/api/v2/capabilities") {
    json capabilities = json::array();
    if (services_.readiness) capabilities.push_back("readiness.v1");
    if (services_.navigation && HasScope(principal, "navigation:read"))
      capabilities.push_back("navigation.snapshot.v1");
    if (services_.routes && HasScope(principal, "routes:read"))
      capabilities.push_back("routes.query.v1");
    if (services_.route_commands && HasScope(principal, "routes:write"))
      capabilities.push_back("routes.draft-mutation.v1");
    if (services_.route_commands && HasScope(principal, "routes:activate"))
      capabilities.push_back("routes.guarded-activation.v1");
    if (services_.chart_safety && HasScope(principal, "charts:query"))
      capabilities.push_back("chart-safety.v1");
    return JsonResponse(200, {{"apiVersion", options_.api_version},
                              {"capabilities", capabilities}});
  }
  if (request.method == "GET" && path == "/api/v2/readiness") {
    if (!services_.readiness)
      return Error(503, "capability_unavailable", "Readiness service is unavailable");
    const auto state = services_.readiness->GetReadiness();
    return JsonResponse(200, {{"ready", state.ready},
                              {"closing", state.closing},
                              {"unavailableCapabilities",
                               state.unavailable_capabilities}});
  }
  if (request.method == "GET" && path == "/api/v2/navigation") {
    if (const auto denied = RequireScope(principal, "navigation:read");
        denied.status != 500)
      return denied;
    if (!services_.navigation)
      return Error(503, "capability_unavailable", "Navigation service is unavailable");
    const auto result = services_.navigation->GetSnapshot();
    if (result.error) return ServiceFailure(*result.error);
    const auto& snapshot = *result.value;
    json body = {{"positionValid", snapshot.position_valid},
                 {"stale", snapshot.stale},
                 {"source", snapshot.source},
                 {"receiptTimeUtc", Iso8601(snapshot.receipt_time)}};
    body["position"] = snapshot.position ? CoordinateJson(*snapshot.position) : json(nullptr);
    body["courseOverGroundDegreesTrue"] = snapshot.course_over_ground_degrees_true;
    body["speedOverGroundKnots"] = snapshot.speed_over_ground_knots;
    body["headingDegreesTrue"] = snapshot.heading_degrees_true;
    body["magneticVariationDegrees"] = snapshot.magnetic_variation_degrees;
    body["measurementTimeUtc"] = snapshot.measurement_time
                                         ? json(Iso8601(*snapshot.measurement_time))
                                         : json(nullptr);
    const auto age = std::chrono::duration_cast<std::chrono::milliseconds>(
                         Clock::now() - snapshot.receipt_time)
                         .count();
    body["ageMilliseconds"] = std::max<std::int64_t>(0, age);
    return JsonResponse(200, body);
  }
  if (request.method == "GET" && path == "/api/v2/routes") {
    if (const auto denied = RequireScope(principal, "routes:read");
        denied.status != 500)
      return denied;
    if (!services_.routes)
      return Error(503, "capability_unavailable", "Route service is unavailable");
    const auto result = services_.routes->ListRoutes();
    if (result.error) return ServiceFailure(*result.error);
    json routes = json::array();
    for (const auto& route : *result.value) routes.push_back(RouteSummaryJson(route));
    return JsonResponse(200, {{"routes", routes}});
  }
  if (services_.route_commands &&
      ((request.method == "POST" && path == "/api/v2/routes") ||
       (request.method == "PUT" && path.rfind("/api/v2/routes/", 0) == 0) ||
       (request.method == "DELETE" && path.rfind("/api/v2/routes/", 0) == 0) ||
       (request.method == "POST" &&
        (path == "/api/v2/routes/deactivate" ||
         path.find("/activate") != std::string::npos)))) {
    return HandleRouteCommand(request, principal, path);
  }
  if (request.method == "GET" && path == "/api/v2/active-route") {
    if (const auto denied = RequireScope(principal, "routes:read");
        denied.status != 500)
      return denied;
    if (!services_.routes)
      return Error(503, "capability_unavailable", "Route service is unavailable");
    const auto result = services_.routes->GetActiveRoute();
    if (result.error) return ServiceFailure(*result.error);
    const auto& active = *result.value;
    return JsonResponse(200, {{"active", active.active},
                              {"routeGuid", active.route_guid},
                              {"activeWaypointGuid", active.active_waypoint_guid},
                              {"activeLegIndex", active.active_leg_index},
                              {"routeRevision", active.route_revision}});
  }
  constexpr const char* route_prefix = "/api/v2/routes/";
  if (request.method == "GET" && path.rfind(route_prefix, 0) == 0 &&
      path.size() > std::char_traits<char>::length(route_prefix)) {
    if (const auto denied = RequireScope(principal, "routes:read");
        denied.status != 500)
      return denied;
    if (!services_.routes)
      return Error(503, "capability_unavailable", "Route service is unavailable");
    const auto result = services_.routes->GetRoute(
        path.substr(std::char_traits<char>::length(route_prefix)));
    if (result.error) return ServiceFailure(*result.error);
    return JsonResponse(200, RouteJson(*result.value));
  }

  const bool chart_endpoint =
      request.method == "POST" &&
      (path == "/api/v2/chart-safety/validate-point" ||
       path == "/api/v2/chart-safety/validate-segment" ||
       path == "/api/v2/chart-safety/validate-route");
  if (chart_endpoint) {
    if (const auto denied = RequireScope(principal, "charts:query");
        denied.status != 500)
      return denied;
    if (!services_.chart_safety)
      return Error(503, "capability_unavailable", "Chart safety service is unavailable");
    json body;
    try {
      body = json::parse(request.body);
    } catch (const json::exception&) {
      return Error(400, "malformed_json", "Request body is not valid JSON");
    }
    const auto constraints = ParseConstraints(body);
    if (constraints.error) return ServiceFailure(*constraints.error);

    if (path == "/api/v2/chart-safety/validate-point") {
      const auto point = ParseCoordinate(body.value("point", json::object()));
      if (point.error) return Error(400, point.error->code, point.error->message);
      const auto result = services_.chart_safety->ValidatePoint(*point.value,
                                                                 *constraints.value);
      if (result.error) return ServiceFailure(*result.error);
      return ChartResultResponse(*result.value);
    }
    if (path == "/api/v2/chart-safety/validate-segment") {
      const auto start = ParseCoordinate(body.value("start", json::object()));
      const auto end = ParseCoordinate(body.value("end", json::object()));
      if (start.error) return Error(400, start.error->code, start.error->message);
      if (end.error) return Error(400, end.error->code, end.error->message);
      const auto result = services_.chart_safety->ValidateSegment(
          *start.value, *end.value, *constraints.value);
      if (result.error) return ServiceFailure(*result.error);
      return ChartResultResponse(*result.value);
    }
    std::vector<Coordinate> route;
    try {
      for (const auto& value : body.at("route")) {
        const auto point = ParseCoordinate(value);
        if (point.error) return Error(400, point.error->code, point.error->message);
        route.push_back(*point.value);
      }
    } catch (const json::exception&) {
      return Error(400, "invalid_route", "route must be an array of coordinates");
    }
    if (route.size() < 2)
      return Error(400, "invalid_route", "A route requires at least two coordinates");
    const auto result = services_.chart_safety->ValidateRoute(route, *constraints.value);
    if (result.error) return ServiceFailure(*result.error);
    return ChartResultResponse(*result.value);
  }

  return Error(404, "not_found", "No matching API v2 endpoint");
}

HttpResponse ExternalApiRouter::HandleRouteCommand(
    const HttpRequest& request, const TokenAuthorizer::Principal& principal,
    const std::string& path) const {
  const bool activation =
      path == "/api/v2/routes/deactivate" ||
      (path.size() > 9 && path.compare(path.size() - 9, 9, "/activate") == 0);
  if (const auto denied = RequireScope(
          principal, activation ? "routes:activate" : "routes:write");
      denied.status != 500)
    return denied;

  const auto key = Header(request, "Idempotency-Key");
  if (!key || key->empty() || key->size() > 128)
    return Error(400, "idempotency_key_required",
                 "Idempotency-Key must contain 1 to 128 characters");
  const std::string cache_key = principal.id + ":" + *key;
  const std::string fingerprint = request.method + "\n" + path + "\n" + request.body;
  std::lock_guard<std::mutex> idempotency_lock(idempotency_mutex_);
  if (const auto found = idempotency_results_.find(cache_key);
      found != idempotency_results_.end()) {
    if (found->second.first != fingerprint)
      return Error(409, "idempotency_conflict",
                   "Idempotency-Key was already used for a different command");
    return found->second.second;
  }

  json body = json::object();
  if (!request.body.empty()) {
    try {
      body = json::parse(request.body);
    } catch (const json::exception&) {
      return Error(400, "malformed_json", "Request body is not valid JSON");
    }
  }

  Result<RouteCommandResult> result;
  int success_status = 200;
  constexpr const char* route_prefix = "/api/v2/routes/";
  if (request.method == "POST" && path == "/api/v2/routes") {
    const auto route = ParseRouteMutation(body);
    if (route.error) return Error(400, route.error->code, route.error->message);
    result = services_.route_commands->CreateDraft(*route.value, *key);
    success_status = 201;
  } else if (request.method == "POST" &&
             path == "/api/v2/routes/deactivate") {
    result = services_.route_commands->Deactivate(*key);
  } else {
    std::string suffix = path.substr(std::char_traits<char>::length(route_prefix));
    const auto activate_offset = suffix.rfind("/activate");
    if (request.method == "POST" && activate_offset != std::string::npos &&
        activate_offset + 9 == suffix.size()) {
      const auto guid = suffix.substr(0, activate_offset);
      std::optional<std::string> waypoint;
      if (body.contains("waypointGuid")) waypoint = body.at("waypointGuid").get<std::string>();
      result = services_.route_commands->Activate(guid, waypoint, *key);
    } else if (request.method == "PUT") {
      const auto route = ParseRouteMutation(body);
      if (route.error) return Error(400, route.error->code, route.error->message);
      try {
        result = services_.route_commands->Update(
            suffix, body.at("expectedRevision").get<std::uint64_t>(),
            *route.value, *key);
      } catch (const json::exception&) {
        return Error(400, "expected_revision_required",
                     "expectedRevision must be an unsigned integer");
      }
    } else if (request.method == "DELETE") {
      try {
        result = services_.route_commands->Delete(
            suffix, body.at("expectedRevision").get<std::uint64_t>(), *key);
      } catch (const json::exception&) {
        return Error(400, "expected_revision_required",
                     "expectedRevision must be an unsigned integer");
      }
    } else {
      return Error(404, "not_found", "No matching route command endpoint");
    }
  }
  const auto response = result.error
                            ? ServiceFailure(*result.error)
                            : CommandResponse(success_status, *result.value);
  if (idempotency_results_.size() >= 1024) idempotency_results_.clear();
  idempotency_results_.emplace(cache_key,
                               std::make_pair(fingerprint, response));
  return response;
}

}  // namespace ocpn::control
