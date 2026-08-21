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
          {{"Content-Type", "application/json"}, {"Cache-Control", "no-store"}},
          body.dump() + "\n"};
}

HttpResponse Error(int status, const std::string& code,
                   const std::string& message) {
  return JsonResponse(status,
                      {{"error", {{"code", code}, {"message", message}}}});
}

std::string Lower(std::string value) {
  std::transform(
      value.begin(), value.end(), value.begin(),
      [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
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
  return address == "127.0.0.1" || address == "::1" || address == "[::1]" ||
         address.rfind("127.", 0) == 0;
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

std::optional<Clock::time_point> ParseUtcTime(const std::string& value) {
  std::tm utc{};
  std::istringstream stream(value);
  stream >> std::get_time(&utc, "%Y-%m-%dT%H:%M:%SZ");
  if (stream.fail() || stream.peek() != std::char_traits<char>::eof())
    return std::nullopt;
#ifdef _WIN32
  const auto seconds = _mkgmtime(&utc);
#else
  const auto seconds = timegm(&utc);
#endif
  if (seconds == static_cast<std::time_t>(-1)) return std::nullopt;
  return Clock::from_time_t(seconds);
}

json CoordinateJson(const Coordinate& coordinate) {
  return {{"latitudeDegrees", coordinate.latitude_degrees},
          {"longitudeDegrees", coordinate.longitude_degrees}};
}

const char* ProviderKindText(ProviderKind kind) {
  return kind == ProviderKind::EnvironmentalData ? "environmental-data"
                                                  : "route-planning";
}

const char* ProviderFieldTypeText(ProviderFieldType type) {
  switch (type) {
    case ProviderFieldType::String:
      return "string";
    case ProviderFieldType::Integer:
      return "integer";
    case ProviderFieldType::Number:
      return "number";
    case ProviderFieldType::Boolean:
      return "boolean";
    case ProviderFieldType::Coordinate:
      return "coordinate";
    case ProviderFieldType::Resource:
      return "resource";
  }
  return "string";
}

json ProviderDescriptorJson(const ProviderDescriptor& descriptor) {
  json fields = json::array();
  for (const auto& field : descriptor.fields) {
    json encoded = {{"name", field.name},
                    {"label", field.label},
                    {"type", ProviderFieldTypeText(field.type)},
                    {"required", field.required}};
    if (!field.unit.empty()) encoded["unit"] = field.unit;
    if (!field.default_value.empty())
      encoded["defaultValue"] = field.default_value;
    if (field.minimum) encoded["minimum"] = *field.minimum;
    if (field.maximum) encoded["maximum"] = *field.maximum;
    if (!field.resource_kind.empty())
      encoded["resourceKind"] = field.resource_kind;
    if (!field.choices.empty()) {
      encoded["choices"] = json::array();
      for (const auto& choice : field.choices)
        encoded["choices"].push_back(
            {{"value", choice.value}, {"label", choice.label}});
    }
    fields.push_back(std::move(encoded));
  }
  json resources = json::array();
  for (const auto& resource : descriptor.resources) {
    json metadata = json::object();
    for (const auto& [name, value] : resource.metadata)
      metadata[name] = value;
    resources.push_back({{"kind", resource.kind},
                         {"identity", resource.identity},
                         {"label", resource.label},
                         {"available", resource.available},
                         {"metadata", std::move(metadata)}});
  }
  return {{"capability", descriptor.capability},
          {"displayName", descriptor.display_name},
          {"kind", ProviderKindText(descriptor.kind)},
          {"schemaVersion", descriptor.schema_version},
          {"cancellable", descriptor.cancellable},
          {"maximumConcurrentJobs", descriptor.maximum_concurrent_jobs},
          {"requiredScope", descriptor.required_scope},
          {"fields", std::move(fields)},
          {"resources", std::move(resources)}};
}

json RouteSummaryJson(const RouteSummary& route) {
  return {{"guid", route.guid},         {"name", route.name},
          {"revision", route.revision}, {"waypointCount", route.waypoint_count},
          {"isLayer", route.is_layer},  {"isDraft", route.is_draft}};
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

const char* EventTypeText(ApplicationEventType type) {
  switch (type) {
    case ApplicationEventType::Navigation:
      return "navigation";
    case ApplicationEventType::NavigationValidity:
      return "navigation-validity";
    case ApplicationEventType::RouteCatalogue:
      return "route-catalogue";
    case ApplicationEventType::ActiveRoute:
      return "active-route";
    case ApplicationEventType::ChartDatabase:
      return "chart-database";
    case ApplicationEventType::Readiness:
      return "readiness";
    case ApplicationEventType::PlanningJob:
      return "planning-job";
  }
  return "unknown";
}

std::uint32_t EventBit(ApplicationEventType type) {
  return 1U << static_cast<unsigned>(type);
}

std::optional<ApplicationEventType> ParseEventType(const std::string& type) {
  if (type == "navigation") return ApplicationEventType::Navigation;
  if (type == "navigation-validity")
    return ApplicationEventType::NavigationValidity;
  if (type == "route-catalogue") return ApplicationEventType::RouteCatalogue;
  if (type == "active-route") return ApplicationEventType::ActiveRoute;
  if (type == "chart-database") return ApplicationEventType::ChartDatabase;
  if (type == "readiness") return ApplicationEventType::Readiness;
  if (type == "planning-job") return ApplicationEventType::PlanningJob;
  return std::nullopt;
}

json EventJson(const ApplicationEvent& event) {
  json result = {{"type", EventTypeText(event.type)},
                 {"sequence", event.sequence},
                 {"timestampUtc", Iso8601(event.timestamp)},
                 {"schemaVersion", 1}};
  if (!event.subject_id.empty()) result["subjectId"] = event.subject_id;
  return result;
}

HttpResponse ServiceFailure(const ServiceError& error) {
  const bool client_error =
      error.code.rfind("invalid_", 0) == 0 || error.code == "not_ready" ||
      error.code == "active_route" || error.code == "layer_owned";
  const int status = error.code == "not_found" ? 404
                     : error.code == "conflict" ? 409
                     : error.code == "result_not_ready" ? 409
                     : client_error ? 400
                                    : 503;
  return Error(status, error.code, error.message);
}

bool ValidCoordinate(const Coordinate& point) {
  return std::isfinite(point.latitude_degrees) &&
         std::isfinite(point.longitude_degrees) &&
         point.latitude_degrees >= -90.0 && point.latitude_degrees <= 90.0 &&
         point.longitude_degrees >= -180.0 && point.longitude_degrees <= 180.0;
}

Result<Coordinate> ParseCoordinate(const json& value) {
  try {
    Coordinate coordinate{value.at("latitudeDegrees").get<double>(),
                          value.at("longitudeDegrees").get<double>()};
    if (!ValidCoordinate(coordinate)) {
      return Result<Coordinate>::FromError(
          "invalid_coordinate", "Coordinate is outside WGS84 bounds");
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
          "invalid_constraints",
          "Safety constraints must be finite and non-negative");
    }
    return Result<ChartSafetyConstraints>::FromValue(constraints);
  } catch (const json::exception&) {
    return Result<ChartSafetyConstraints>::FromError(
        "invalid_constraints",
        "minimumDepthMeters is required and must be numeric");
  }
}

HttpResponse ChartResultResponse(const ChartSafetyResult& result) {
  json body = {
      {"decision", DecisionText(result.decision)},
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

const char* PlanningStateText(PlanningJobState state) {
  switch (state) {
    case PlanningJobState::Queued:
      return "queued";
    case PlanningJobState::Running:
      return "running";
    case PlanningJobState::Completed:
      return "completed";
    case PlanningJobState::Failed:
      return "failed";
    case PlanningJobState::Cancelled:
      return "cancelled";
  }
  return "failed";
}

json PlanningJobJson(const PlanningJobSnapshot& job) {
  json result = {{"id", job.id},
                 {"providerCapability", job.provider_capability},
                 {"state", PlanningStateText(job.state)},
                 {"progress", job.progress},
                 {"cancellationRequested", job.cancellation_requested},
                 {"submittedTimeUtc", Iso8601(job.submitted_time)},
                 {"updatedTimeUtc", Iso8601(job.updated_time)}};
  result["error"] = job.error ? json{{"code", job.error->code},
                                     {"message", job.error->message}}
                              : json(nullptr);
  return result;
}

Result<PlanningRequest> ParsePlanningRequest(const json& body) {
  try {
    PlanningRequest request;
    request.provider_capability =
        body.at("providerCapability").get<std::string>();
    const auto start = ParseCoordinate(body.at("start"));
    const auto destination = ParseCoordinate(body.at("destination"));
    if (start.error)
      return Result<PlanningRequest>::FromError(start.error->code,
                                                start.error->message);
    if (destination.error)
      return Result<PlanningRequest>::FromError(destination.error->code,
                                                destination.error->message);
    request.start = *start.value;
    request.destination = *destination.value;
    if (body.contains("departureTimeUtc")) {
      const auto parsed_time =
          ParseUtcTime(body.at("departureTimeUtc").get<std::string>());
      if (!parsed_time)
        return Result<PlanningRequest>::FromError(
            "invalid_planning_request",
            "departureTimeUtc must be an ISO 8601 UTC timestamp ending in Z");
      request.departure_time = *parsed_time;
    }
    request.safety.minimum_depth_meters =
        body.at("minimumDepthMeters").get<double>();
    request.safety.land_margin_nautical_miles =
        body.value("landMarginNauticalMiles", 0.0);
    request.vessel_identity = body.value("vesselIdentity", std::string());
    request.polar_identity = body.value("polarIdentity", std::string());
    request.weather_dataset_identity =
        body.value("weatherDatasetIdentity", std::string());
    request.current_dataset_identity =
        body.value("currentDatasetIdentity", std::string());
    request.horizon = std::chrono::hours(body.value("horizonHours", 240));
    request.allow_climatology_fallback =
        body.value("allowClimatologyFallback", false);
    request.effort_limit = body.value("effortLimit", std::uint64_t{1000000});
    request.departure_window_before_minutes =
        body.value("departureWindowBeforeMinutes", 0);
    request.departure_window_after_minutes =
        body.value("departureWindowAfterMinutes", 0);
    request.departure_step_minutes = body.value("departureStepMinutes", 60);
    request.concurrent_routes = body.value("concurrentRoutes", 1);
    request.routing_effort_percent = body.value("routingEffortPercent", 100);
    if (request.provider_capability.empty() ||
        !std::isfinite(request.safety.minimum_depth_meters) ||
        request.safety.minimum_depth_meters < 0.0 ||
        !std::isfinite(request.safety.land_margin_nautical_miles) ||
        request.safety.land_margin_nautical_miles < 0.0 ||
        request.horizon.count() <= 0 || request.horizon.count() > 24 * 365 ||
        request.effort_limit == 0 || request.effort_limit > 1000000000ULL ||
        request.departure_window_before_minutes < 0 ||
        request.departure_window_before_minutes > 7 * 24 * 60 ||
        request.departure_window_after_minutes < 0 ||
        request.departure_window_after_minutes > 7 * 24 * 60 ||
        request.departure_step_minutes <= 0 ||
        request.departure_step_minutes > 24 * 60 ||
        request.concurrent_routes <= 0 || request.concurrent_routes > 16 ||
        request.routing_effort_percent < 10 ||
        request.routing_effort_percent > 400)
      return Result<PlanningRequest>::FromError(
          "invalid_planning_request",
          "Planning constraints are outside limits");
    return Result<PlanningRequest>::FromValue(std::move(request));
  } catch (const json::exception&) {
    return Result<PlanningRequest>::FromError(
        "invalid_planning_request",
        "Planning requires providerCapability, start, destination and "
        "minimumDepthMeters");
  }
}

json PlanningResultJson(const PlanningResult& result) {
  const auto route = [](const RouteMutation& value) {
    json output = {{"name", value.name}, {"waypoints", json::array()}};
    for (const auto& waypoint : value.waypoints)
      output["waypoints"].push_back(
          {{"guid", waypoint.guid},
           {"name", waypoint.name},
           {"position", CoordinateJson(waypoint.position)}});
    return output;
  };
  json alternatives = json::array();
  for (const auto& alternative : result.alternatives)
    alternatives.push_back(route(alternative));
  const auto& safety = result.final_safety;
  return {{"draftRoute", route(result.draft_route)},
          {"alternatives", std::move(alternatives)},
          {"inputProvenance", result.input_provenance},
          {"chartDatabaseIdentity", result.chart_database_identity},
          {"warnings", result.warnings},
          {"finalSafety",
           {{"decision", DecisionText(safety.decision)},
            {"authority", AuthorityText(safety.authority)},
           {"causeCode", safety.cause_code},
            {"chartDatabaseIdentity", safety.chart_database_identity},
            {"warnings", safety.warnings},
            {"constraints",
             {{"minimumDepthMeters", safety.constraints.minimum_depth_meters},
              {"landMarginNauticalMiles",
               safety.constraints.land_margin_nautical_miles}}}}}};
}

}  // namespace

void TokenAuthorizer::Put(std::string token, Principal principal) {
  PutDigest(TokenDigest(token), std::move(principal));
}

void TokenAuthorizer::PutDigest(std::string token_sha256, Principal principal) {
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

bool ExternalApiRouter::CanHandleOnTransportThread(
    const HttpRequest& request) {
  if (request.method != "GET" && request.method != "DELETE") return false;
  const auto path = request.path.substr(0, request.path.find('?'));
  constexpr const char* prefix = "/api/v2/planning/jobs/";
  if (path.rfind(prefix, 0) != 0) return false;
  auto suffix = path.substr(std::char_traits<char>::length(prefix));
  if (suffix.size() > 7 &&
      suffix.compare(suffix.size() - 7, 7, "/result") == 0) {
    if (request.method != "GET") return false;
    suffix.resize(suffix.size() - 7);
  }
  return !suffix.empty() && suffix.find('/') == std::string::npos;
}

HttpResponse ExternalApiRouter::Handle(const HttpRequest& request) const {
  if (!options_.enabled)
    return Error(404, "api_disabled", "External control API is disabled");
  if (!options_.allow_lan && !IsLoopback(request.remote_address))
    return Error(403, "loopback_required",
                 "LAN access is disabled for the external control API");
  if (request.body.size() > options_.maximum_body_bytes)
    return Error(413, "request_too_large",
                 "Request body exceeds configured limit");

  const auto authorization = Header(request, "Authorization");
  constexpr const char* prefix = "Bearer ";
  if (!authorization || authorization->rfind(prefix, 0) != 0)
    return Error(401, "authentication_required", "A Bearer token is required");
  if (!authorizer_)
    return Error(503, "authentication_unavailable",
                 "Token service is unavailable");
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
    if (services_.events) capabilities.push_back("events.semantic.v1");
    if (services_.planning && HasScope(principal, "planning:run")) {
      for (const auto& provider : services_.planning->ProviderCapabilities())
        capabilities.push_back(provider);
    }
    return JsonResponse(200, {{"apiVersion", options_.api_version},
                              {"capabilities", capabilities}});
  }
  if (request.method == "GET" && path == "/api/v2/providers") {
    json providers = json::array();
    if (services_.planning && HasScope(principal, "planning:run")) {
      for (const auto& descriptor : services_.planning->ProviderDescriptors())
        providers.push_back(ProviderDescriptorJson(descriptor));
    }
    return JsonResponse(200, {{"providers", std::move(providers)}});
  }
  if (request.method == "GET" && path == "/api/v2/events") {
    if (!services_.events)
      return Error(503, "capability_unavailable",
                   "Event service is unavailable");
    std::uint32_t allowed_mask = EventBit(ApplicationEventType::Readiness);
    if (HasScope(principal, "navigation:read"))
      allowed_mask |= EventBit(ApplicationEventType::Navigation) |
                      EventBit(ApplicationEventType::NavigationValidity);
    if (HasScope(principal, "routes:read"))
      allowed_mask |= EventBit(ApplicationEventType::RouteCatalogue) |
                      EventBit(ApplicationEventType::ActiveRoute);
    if (HasScope(principal, "charts:query"))
      allowed_mask |= EventBit(ApplicationEventType::ChartDatabase);
    if (HasScope(principal, "planning:run"))
      allowed_mask |= EventBit(ApplicationEventType::PlanningJob);

    const auto sequence = services_.events->LatestSequence();
    json payload = {{"type", "snapshot"},
                    {"sequence", sequence},
                    {"timestampUtc", Iso8601(Clock::now())},
                    {"schemaVersion", 1}};
    if (services_.readiness) {
      const auto state = services_.readiness->GetReadiness();
      payload["readiness"] = {
          {"ready", state.ready},
          {"closing", state.closing},
          {"unavailableCapabilities", state.unavailable_capabilities}};
    }
    if (services_.navigation && HasScope(principal, "navigation:read")) {
      const auto state = services_.navigation->GetSnapshot();
      if (state.value) {
        payload["navigation"] = {{"positionValid", state.value->position_valid},
                                 {"stale", state.value->stale},
                                 {"source", state.value->source}};
        payload["navigation"]["position"] =
            state.value->position ? CoordinateJson(*state.value->position)
                                  : json(nullptr);
      }
    }
    if (services_.routes && HasScope(principal, "routes:read")) {
      const auto catalogue = services_.routes->ListRoutes();
      if (catalogue.value) {
        payload["routes"] = json::array();
        for (const auto& route : *catalogue.value)
          payload["routes"].push_back(RouteSummaryJson(route));
      }
      const auto active = services_.routes->GetActiveRoute();
      if (active.value)
        payload["activeRoute"] = {
            {"active", active.value->active},
            {"routeGuid", active.value->route_guid},
            {"activeWaypointGuid", active.value->active_waypoint_guid},
            {"activeLegIndex", active.value->active_leg_index}};
    }
    return {101,
            {{"X-OpenCPN-Event-Cursor", std::to_string(sequence)},
             {"X-OpenCPN-Event-Mask", std::to_string(allowed_mask)},
             {"Cache-Control", "no-store"}},
            payload.dump()};
  }
  constexpr const char* planning_prefix = "/api/v2/planning/jobs/";
  if (path == "/api/v2/planning/jobs" && request.method == "POST") {
    if (const auto denied = RequireScope(principal, "planning:run");
        denied.status != 500)
      return denied;
    if (!services_.planning)
      return Error(503, "capability_unavailable",
                   "Planning service is unavailable");
    const auto key = Header(request, "Idempotency-Key");
    if (!key || key->empty() || key->size() > 128)
      return Error(400, "idempotency_key_required",
                   "Idempotency-Key must contain 1 to 128 characters");
    json body;
    try {
      body = json::parse(request.body);
    } catch (const json::exception&) {
      return Error(400, "malformed_json", "Request body is not valid JSON");
    }
    const auto parsed = ParsePlanningRequest(body);
    if (parsed.error) return ServiceFailure(*parsed.error);
    const std::string cache_key = principal.id + ":" + *key;
    const std::string fingerprint =
        request.method + "\n" + path + "\n" + request.body;
    std::lock_guard<std::mutex> lock(idempotency_mutex_);
    if (const auto found = idempotency_results_.find(cache_key);
        found != idempotency_results_.end()) {
      if (found->second.first != fingerprint)
        return Error(
            409, "idempotency_conflict",
            "Idempotency-Key was already used for a different command");
      return found->second.second;
    }
    const auto submitted =
        services_.planning->Submit(*parsed.value, principal.id);
    if (submitted.error) return ServiceFailure(*submitted.error);
    auto response = JsonResponse(202, PlanningJobJson(*submitted.value));
    idempotency_results_[cache_key] = {fingerprint, response};
    return response;
  }
  if (path.rfind(planning_prefix, 0) == 0) {
    if (const auto denied = RequireScope(principal, "planning:run");
        denied.status != 500)
      return denied;
    if (!services_.planning)
      return Error(503, "capability_unavailable",
                   "Planning service is unavailable");
    auto suffix = path.substr(std::char_traits<char>::length(planning_prefix));
    const bool result_request =
        suffix.size() > 7 &&
        suffix.compare(suffix.size() - 7, 7, "/result") == 0;
    if (result_request) suffix.resize(suffix.size() - 7);
    if (suffix.empty() || suffix.find('/') != std::string::npos)
      return Error(404, "not_found", "No matching planning endpoint");
    if (request.method == "GET" && result_request) {
      const auto result = services_.planning->GetResult(suffix, principal.id);
      if (result.error) return ServiceFailure(*result.error);
      return JsonResponse(200, PlanningResultJson(*result.value));
    }
    if (request.method == "GET" && !result_request) {
      const auto result = services_.planning->Get(suffix, principal.id);
      if (result.error) return ServiceFailure(*result.error);
      return JsonResponse(200, PlanningJobJson(*result.value));
    }
    if (request.method == "DELETE" && !result_request) {
      const auto result = services_.planning->Cancel(suffix, principal.id);
      if (result.error) return ServiceFailure(*result.error);
      return JsonResponse(200, PlanningJobJson(*result.value));
    }
  }
  if (request.method == "GET" && path == "/api/v2/readiness") {
    if (!services_.readiness)
      return Error(503, "capability_unavailable",
                   "Readiness service is unavailable");
    const auto state = services_.readiness->GetReadiness();
    return JsonResponse(
        200, {{"ready", state.ready},
              {"closing", state.closing},
              {"unavailableCapabilities", state.unavailable_capabilities}});
  }
  if (request.method == "GET" && path == "/api/v2/navigation") {
    if (const auto denied = RequireScope(principal, "navigation:read");
        denied.status != 500)
      return denied;
    if (!services_.navigation)
      return Error(503, "capability_unavailable",
                   "Navigation service is unavailable");
    const auto result = services_.navigation->GetSnapshot();
    if (result.error) return ServiceFailure(*result.error);
    const auto& snapshot = *result.value;
    json body = {{"positionValid", snapshot.position_valid},
                 {"stale", snapshot.stale},
                 {"source", snapshot.source},
                 {"receiptTimeUtc", Iso8601(snapshot.receipt_time)}};
    body["position"] =
        snapshot.position ? CoordinateJson(*snapshot.position) : json(nullptr);
    body["courseOverGroundDegreesTrue"] =
        snapshot.course_over_ground_degrees_true;
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
      return Error(503, "capability_unavailable",
                   "Route service is unavailable");
    const auto result = services_.routes->ListRoutes();
    if (result.error) return ServiceFailure(*result.error);
    json routes = json::array();
    for (const auto& route : *result.value)
      routes.push_back(RouteSummaryJson(route));
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
      return Error(503, "capability_unavailable",
                   "Route service is unavailable");
    const auto result = services_.routes->GetActiveRoute();
    if (result.error) return ServiceFailure(*result.error);
    const auto& active = *result.value;
    return JsonResponse(200,
                        {{"active", active.active},
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
      return Error(503, "capability_unavailable",
                   "Route service is unavailable");
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
      return Error(503, "capability_unavailable",
                   "Chart safety service is unavailable");
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
      if (point.error)
        return Error(400, point.error->code, point.error->message);
      const auto result = services_.chart_safety->ValidatePoint(
          *point.value, *constraints.value);
      if (result.error) return ServiceFailure(*result.error);
      return ChartResultResponse(*result.value);
    }
    if (path == "/api/v2/chart-safety/validate-segment") {
      const auto start = ParseCoordinate(body.value("start", json::object()));
      const auto end = ParseCoordinate(body.value("end", json::object()));
      if (start.error)
        return Error(400, start.error->code, start.error->message);
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
        if (point.error)
          return Error(400, point.error->code, point.error->message);
        route.push_back(*point.value);
      }
    } catch (const json::exception&) {
      return Error(400, "invalid_route",
                   "route must be an array of coordinates");
    }
    if (route.size() < 2)
      return Error(400, "invalid_route",
                   "A route requires at least two coordinates");
    const auto result =
        services_.chart_safety->ValidateRoute(route, *constraints.value);
    if (result.error) return ServiceFailure(*result.error);
    return ChartResultResponse(*result.value);
  }

  return Error(404, "not_found", "No matching API v2 endpoint");
}

HttpResponse ExternalApiRouter::ReadEvents(std::uint64_t after_sequence,
                                           std::size_t maximum,
                                           std::uint32_t type_mask) const {
  if (!services_.events)
    return Error(503, "capability_unavailable", "Event service is unavailable");
  const auto batch = services_.events->ReadAfter(after_sequence, maximum);
  json events = json::array();
  for (const auto& event : batch.events) {
    if ((type_mask & EventBit(event.type)) != 0)
      events.push_back(EventJson(event));
  }
  const auto count = events.size();
  auto response = JsonResponse(
      200, {{"type", "events"},
            {"gap", batch.gap},
            {"oldestAvailableSequence", batch.oldest_available_sequence},
            {"latestSequence", batch.latest_sequence},
            {"events", std::move(events)}});
  response.headers["X-OpenCPN-Event-Count"] = std::to_string(count);
  response.headers["X-OpenCPN-Event-Cursor"] =
      std::to_string(batch.latest_sequence);
  response.headers["X-OpenCPN-Event-Gap"] = batch.gap ? "1" : "0";
  return response;
}

Result<std::uint32_t> ExternalApiRouter::ParseEventSubscription(
    const std::string& message) const {
  try {
    const auto body = json::parse(message);
    std::uint32_t mask = 0;
    for (const auto& value : body.at("subscribe")) {
      const auto type = ParseEventType(value.get<std::string>());
      if (!type)
        return Result<std::uint32_t>::FromError(
            "invalid_subscription",
            "Subscription contains an unknown event type");
      mask |= EventBit(*type);
    }
    return Result<std::uint32_t>::FromValue(mask);
  } catch (const json::exception&) {
    return Result<std::uint32_t>::FromError(
        "invalid_subscription", "Expected {\"subscribe\":[event types]}");
  }
}

void ExternalApiRouter::CloseEvents() {
  if (services_.events) services_.events->Close();
}

void ExternalApiRouter::Shutdown() {
  if (services_.planning) services_.planning->Shutdown();
  CloseEvents();
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
  const std::string fingerprint =
      request.method + "\n" + path + "\n" + request.body;
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
  } else if (request.method == "POST" && path == "/api/v2/routes/deactivate") {
    result = services_.route_commands->Deactivate(*key);
  } else {
    std::string suffix =
        path.substr(std::char_traits<char>::length(route_prefix));
    const auto activate_offset = suffix.rfind("/activate");
    if (request.method == "POST" && activate_offset != std::string::npos &&
        activate_offset + 9 == suffix.size()) {
      const auto guid = suffix.substr(0, activate_offset);
      std::optional<std::string> waypoint;
      if (body.contains("waypointGuid"))
        waypoint = body.at("waypointGuid").get<std::string>();
      result = services_.route_commands->Activate(guid, waypoint, *key);
    } else if (request.method == "PUT") {
      const auto route = ParseRouteMutation(body);
      if (route.error)
        return Error(400, route.error->code, route.error->message);
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
