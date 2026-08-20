/***************************************************************************
 * Copyright (C) 2026 OpenCPN contributors
 ***************************************************************************/

#include "external_control_gui.h"

#include <cmath>
#include <cstdint>
#include <iomanip>
#include <sstream>

#include <wx/thread.h>

#include "model/own_ship.h"
#include "model/route.h"
#include "model/route_point.h"
#include "model/routeman.h"
#include "ocpn_plugin.h"

namespace {
using namespace ocpn::control;

std::uint64_t Revision(const Route& route) {
  std::ostringstream value;
  value << std::setprecision(17) << route.GetGUID().ToStdString() << '\n'
        << route.GetName().ToStdString() << '\n';
  for (const auto* point : *route.pRoutePointList)
    value << point->m_GUID.ToStdString() << ':' << point->m_lat << ':'
          << point->m_lon << '\n';
  std::uint64_t hash = 1469598103934665603ULL;
  for (const unsigned char byte : value.str()) {
    hash ^= byte;
    hash *= 1099511628211ULL;
  }
  return hash;
}

RouteSummary Summary(const Route& route) {
  return {route.GetGUID().ToStdString(), route.GetName().ToStdString(),
          Revision(route), route.pRoutePointList->size(),
          route.m_bIsInLayer, false};
}

RouteSnapshot Snapshot(const Route& route) {
  RouteSnapshot result;
  static_cast<RouteSummary&>(result) = Summary(route);
  for (auto* point : *route.pRoutePointList) {
    result.waypoints.push_back(
        {point->m_GUID.ToStdString(), point->GetName().ToStdString(),
         {point->m_lat, point->m_lon}});
  }
  return result;
}

class LiveReadiness final : public ReadinessService {
public:
  ReadinessSnapshot GetReadiness() const override {
    ReadinessSnapshot result;
    result.ready = wxThread::IsMain() && pRouteList && g_pRouteMan;
    result.closing = false;
    if (!result.ready) result.unavailable_capabilities.push_back("routes.query.v1");
    char identity[256] = {};
    if (!PlugIn_GetSegmentSafetyChartIdentity(identity, sizeof(identity)))
      result.unavailable_capabilities.push_back("chart-safety.v1");
    return result;
  }
};

class LiveNavigation final : public NavigationSnapshotService {
public:
  Result<NavigationSnapshot> GetSnapshot() const override {
    if (!wxThread::IsMain())
      return Result<NavigationSnapshot>::FromError(
          "thread_affinity", "Navigation snapshots require the application thread");
    NavigationSnapshot result;
    result.position_valid = bGPSValid && std::isfinite(gLat) && std::isfinite(gLon);
    result.stale = !bGPSValid;
    result.source = "OpenCPN consolidated navigation state";
    result.receipt_time = Clock::now();
    if (result.position_valid) result.position = Coordinate{gLat, gLon};
    if (std::isfinite(gCog)) result.course_over_ground_degrees_true = gCog;
    if (std::isfinite(gSog)) result.speed_over_ground_knots = gSog;
    if (std::isfinite(gHdt)) result.heading_degrees_true = gHdt;
    if (std::isfinite(gVar)) result.magnetic_variation_degrees = gVar;
    return Result<NavigationSnapshot>::FromValue(result);
  }
};

class LiveRoutes final : public RouteQueryService {
public:
  Result<std::vector<RouteSummary>> ListRoutes() const override {
    if (!wxThread::IsMain() || !pRouteList)
      return Result<std::vector<RouteSummary>>::FromError(
          "not_ready", "Route catalogue is not ready");
    std::vector<RouteSummary> result;
    result.reserve(pRouteList->size());
    for (const auto* route : *pRouteList) result.push_back(Summary(*route));
    return Result<std::vector<RouteSummary>>::FromValue(std::move(result));
  }

  Result<RouteSnapshot> GetRoute(const std::string& guid) const override {
    if (!wxThread::IsMain() || !g_pRouteMan)
      return Result<RouteSnapshot>::FromError("not_ready", "Route manager is not ready");
    const auto* route = g_pRouteMan->FindRouteByGUID(guid);
    if (!route)
      return Result<RouteSnapshot>::FromError("not_found", "Route not found");
    return Result<RouteSnapshot>::FromValue(Snapshot(*route));
  }

  Result<ActiveRouteSnapshot> GetActiveRoute() const override {
    if (!wxThread::IsMain() || !g_pRouteMan)
      return Result<ActiveRouteSnapshot>::FromError("not_ready",
                                                   "Route manager is not ready");
    ActiveRouteSnapshot result;
    const auto* route = g_pRouteMan->GetpActiveRoute();
    if (!route) return Result<ActiveRouteSnapshot>::FromValue(result);
    result.active = true;
    result.route_guid = route->GetGUID().ToStdString();
    result.route_revision = Revision(*route);
    const auto* active = g_pRouteMan->GetpActivePoint();
    if (active) {
      result.active_waypoint_guid = active->m_GUID.ToStdString();
      std::size_t index = 0;
      for (const auto* point : *route->pRoutePointList) {
        if (point == active) {
          result.active_leg_index = index;
          break;
        }
        ++index;
      }
    }
    return Result<ActiveRouteSnapshot>::FromValue(result);
  }
};

std::string StatusCode(int status) {
  switch (status) {
    case PI_SEGMENT_SAFETY_SAFE:
      return "clear";
    case PI_SEGMENT_SAFETY_CROSSES_LAND:
      return "crosses_land";
    case PI_SEGMENT_SAFETY_WITHIN_LAND_MARGIN:
      return "within_land_margin";
    case PI_SEGMENT_SAFETY_UNSAFE_AREA:
      return "unsafe_area";
    case PI_SEGMENT_SAFETY_DRYING_AREA:
      return "drying_area";
    case PI_SEGMENT_SAFETY_TOO_SHALLOW:
      return "too_shallow";
    case PI_SEGMENT_SAFETY_UNKNOWN_DEPTH:
      return "unknown_depth";
    case PI_SEGMENT_SAFETY_PENDING_DATA:
      return "pending_chart_data";
    case PI_SEGMENT_SAFETY_NO_DATA:
      return "chart_data_unavailable";
    default:
      return "chart_query_error";
  }
}

ChartSafetyDecision Decision(int status) {
  if (status == PI_SEGMENT_SAFETY_SAFE) return ChartSafetyDecision::Pass;
  if (status == PI_SEGMENT_SAFETY_CROSSES_LAND ||
      status == PI_SEGMENT_SAFETY_WITHIN_LAND_MARGIN ||
      status == PI_SEGMENT_SAFETY_UNSAFE_AREA ||
      status == PI_SEGMENT_SAFETY_DRYING_AREA ||
      status == PI_SEGMENT_SAFETY_TOO_SHALLOW)
    return ChartSafetyDecision::Fail;
  return ChartSafetyDecision::Unknown;
}

class LiveChartSafety final : public ChartSafetyQuery {
public:
  Result<ChartSafetyResult> ValidatePoint(
      const Coordinate& point, const ChartSafetyConstraints& constraints) override {
    return ValidateSegment(point, point, constraints);
  }

  Result<ChartSafetyResult> ValidateSegment(
      const Coordinate& start, const Coordinate& end,
      const ChartSafetyConstraints& constraints) override {
    if (!wxThread::IsMain())
      return Result<ChartSafetyResult>::FromError(
          "thread_affinity", "Chart safety requires the application thread");
    PlugInSegmentSafetyOptions options{};
    options.struct_size = sizeof(options);
    options.safety_margin_nm = constraints.land_margin_nautical_miles;
    options.check_land = 1;
    options.allow_gshhs_fallback = 0;
    options.check_depth = constraints.minimum_depth_meters > 0.0 ? 1 : 0;
    options.minimum_depth_m = constraints.minimum_depth_meters;
    options.force_authoritative_fine_validation = 1;
    PlugInSegmentSafetyResult raw{};
    raw.struct_size = sizeof(raw);
    if (!PlugIn_CheckSegmentSafety(
            start.latitude_degrees, start.longitude_degrees,
            end.latitude_degrees, end.longitude_degrees, &options, &raw)) {
      return Result<ChartSafetyResult>::FromError(
          "chart_query_failed", "Chart engine rejected the safety query");
    }
    ChartSafetyResult result;
    result.decision = Decision(raw.status);
    result.authority =
        raw.source == PI_SEGMENT_SAFETY_SOURCE_GSHHS_FALLBACK
            ? ChartSafetyAuthority::Fallback
            : (raw.source == PI_SEGMENT_SAFETY_SOURCE_NONE
                   ? ChartSafetyAuthority::Unknown
                   : ChartSafetyAuthority::Authoritative);
    result.cause_code = StatusCode(raw.status);
    result.constraints = constraints;
    char identity[256] = {};
    if (PlugIn_GetSegmentSafetyChartIdentity(identity, sizeof(identity)))
      result.chart_database_identity = identity;
    if (raw.message[0]) result.warnings.push_back(raw.message);
    return Result<ChartSafetyResult>::FromValue(std::move(result));
  }

  Result<ChartSafetyResult> ValidateRoute(
      const std::vector<Coordinate>& route,
      const ChartSafetyConstraints& constraints) override {
    ChartSafetyResult aggregate;
    aggregate.decision = ChartSafetyDecision::Pass;
    aggregate.authority = ChartSafetyAuthority::Authoritative;
    aggregate.cause_code = "clear";
    aggregate.constraints = constraints;
    for (std::size_t i = 1; i < route.size(); ++i) {
      auto segment = ValidateSegment(route[i - 1], route[i], constraints);
      if (segment.error) return segment;
      if (segment.value->decision != ChartSafetyDecision::Pass) {
        segment.value->failed_segment_index = i - 1;
        return segment;
      }
      if (segment.value->authority != ChartSafetyAuthority::Authoritative)
        aggregate.authority = segment.value->authority;
      aggregate.chart_database_identity = segment.value->chart_database_identity;
    }
    return Result<ChartSafetyResult>::FromValue(std::move(aggregate));
  }
};

}  // namespace

ocpn::control::ServiceBundle MakeExternalControlServices() {
  return {std::make_shared<LiveReadiness>(), std::make_shared<LiveNavigation>(),
          std::make_shared<LiveRoutes>(), std::make_shared<LiveChartSafety>()};
}
