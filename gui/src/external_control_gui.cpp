/***************************************************************************
 * Copyright (C) 2026 OpenCPN contributors
 ***************************************************************************/

#include "external_control_gui.h"

#include <cmath>
#include <condition_variable>
#include <cstdint>
#include <iomanip>
#include <memory>
#include <mutex>
#include <sstream>
#include <unordered_set>

#include <wx/log.h>
#include <wx/thread.h>
#include <wx/app.h>

#include "observable/observable.h"

#include "model/comm_appmsg.h"
#include "model/config_vars.h"
#include "model/gui_events.h"
#include "model/nav_object_database.h"
#include "model/navobj_db.h"
#include "model/own_ship.h"
#include "model/route.h"
#include "model/route_point.h"
#include "model/routeman.h"
#include "model/select.h"
#include "ocpn_plugin.h"

namespace {
using namespace ocpn::control;
constexpr const char* kExternalDraftMarker = "opencpn:external-control:draft:v1";

class LiveApplicationEvents final : public BoundedApplicationEventStream {
public:
  LiveApplicationEvents() : BoundedApplicationEventStream(256) {
    routes_listener_.Init(GuiEvents::GetInstance().on_routes_update,
                          [this](ObservedEvt&) {
      Publish({0, {}, ApplicationEventType::RouteCatalogue, {}});
    });
    BasicNavDataMsg navigation_key;
    navigation_listener_.Init(navigation_key, [this](ObservedEvt&) {
      Publish({0, {}, ApplicationEventType::Navigation, {}});
    });
    if (g_pRouteMan)
      active_route_listener_.Init(g_pRouteMan->json_msg_evt,
                                  [this](ObservedEvt&) {
        Publish({0, {}, ApplicationEventType::ActiveRoute,
                 g_active_route.ToStdString()});
      });
    chart_listener_.Init(GuiEvents::GetInstance().on_finalize_chartdbs,
                         [this](ObservedEvt&) {
      Publish({0, {}, ApplicationEventType::ChartDatabase, {}});
    });
  }

  ~LiveApplicationEvents() override { Close(); }

private:
  obs::Listener routes_listener_;
  obs::Listener navigation_listener_;
  obs::Listener active_route_listener_;
  obs::Listener chart_listener_;
};

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
          route.m_bIsInLayer,
          route.m_RouteDescription == kExternalDraftMarker};
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

void DestroyUnregisteredRoute(Route* route) {
  if (!route) return;
  for (auto* point : *route->pRoutePointList) delete point;
  route->pRoutePointList->clear();
  delete route;
}

Result<Route*> BuildDraftRoute(const RouteMutation& mutation,
                               const std::optional<std::string>& route_guid) {
  if (mutation.waypoints.size() < 2)
    return Result<Route*>::FromError("invalid_route",
                                    "A route requires at least two waypoints");
  auto* route = new Route();
  if (route_guid) route->m_GUID = *route_guid;
  route->m_RouteNameString = mutation.name;
  route->m_RouteDescription = kExternalDraftMarker;
  route->m_btemp = false;
  std::unordered_set<std::string> guids;
  for (const auto& waypoint : mutation.waypoints) {
    auto* point = new RoutePoint(
        waypoint.position.latitude_degrees,
        waypoint.position.longitude_degrees, g_default_wp_icon, waypoint.name,
        waypoint.guid, false);
    const auto guid = point->m_GUID.ToStdString();
    if (!guids.insert(guid).second ||
        (pWayPointMan && pWayPointMan->FindRoutePointByGUID(guid))) {
      delete point;
      DestroyUnregisteredRoute(route);
      return Result<Route*>::FromError(
          "conflict", "Waypoint GUID is duplicated or already exists");
    }
    route->AddPoint(point, false, true);
  }
  route->FinalizeForRendering();
  return Result<Route*>::FromValue(route);
}

void RegisterDraftRoute(Route* route) {
  for (auto* point : *route->pRoutePointList)
    if (pWayPointMan) pWayPointMan->AddRoutePoint(point);
  InsertRouteA(route, nullptr);
  GuiEvents::GetInstance().on_routes_update.Notify();
}

class LiveRouteCommands final : public RouteCommandService {
public:
  Result<RouteCommandResult> CreateDraft(
      const RouteMutation& mutation, const std::string& command_id) override {
    if (!Ready()) return NotReady<RouteCommandResult>();
    auto built = BuildDraftRoute(mutation, std::nullopt);
    if (built.error) return Result<RouteCommandResult>{std::nullopt, built.error};
    auto* route = *built.value;
    if (!NavObj_dB::GetInstance().InsertRoute(route)) {
      NavObj_dB::GetInstance().DeleteRoute(route);
      DestroyUnregisteredRoute(route);
      return Result<RouteCommandResult>::FromError(
          "persistence_failed", "Draft route could not be persisted");
    }
    RegisterDraftRoute(route);
    wxLogMessage("External control audit: command=%s action=create-draft route=%s",
                 command_id, route->m_GUID);
    return Result<RouteCommandResult>::FromValue(
        {command_id, Snapshot(*route), true, {}});
  }

  Result<RouteCommandResult> Update(
      const std::string& guid, std::uint64_t expected_revision,
      const RouteMutation& mutation, const std::string& command_id) override {
    if (!Ready()) return NotReady<RouteCommandResult>();
    auto* current = g_pRouteMan->FindRouteByGUID(guid);
    if (!current)
      return Result<RouteCommandResult>::FromError("not_found", "Route not found");
    if (current->m_bIsInLayer)
      return Result<RouteCommandResult>::FromError("layer_owned",
                                                   "Layer routes cannot be edited");
    if (current->m_RouteDescription != kExternalDraftMarker)
      return Result<RouteCommandResult>::FromError(
          "invalid_route", "Only external-control draft routes can be replaced");
    if (g_pRouteMan->GetpActiveRoute() == current)
      return Result<RouteCommandResult>::FromError(
          "active_route", "Deactivate the route before updating it");
    if (Revision(*current) != expected_revision)
      return Result<RouteCommandResult>::FromError(
          "conflict", "Route revision does not match expectedRevision");

    auto built = BuildDraftRoute(mutation, guid);
    if (built.error) return Result<RouteCommandResult>{std::nullopt, built.error};
    auto* replacement = *built.value;
    if (!NavObj_dB::GetInstance().UpdateRoute(replacement)) {
      DestroyUnregisteredRoute(replacement);
      return Result<RouteCommandResult>::FromError(
          "persistence_failed", "Draft route update could not be persisted");
    }

    pSelect->DeleteAllSelectableRouteSegments(current);
    pSelect->DeleteAllSelectableRoutePoints(current);
    const auto found = std::find(pRouteList->begin(), pRouteList->end(), current);
    if (found != pRouteList->end()) *found = replacement;
    for (auto* point : *replacement->pRoutePointList)
      pWayPointMan->AddRoutePoint(point);
    pSelect->AddAllSelectableRouteSegments(replacement);
    pSelect->AddAllSelectableRoutePoints(replacement);
    for (auto* point : *current->pRoutePointList) {
      NavObj_dB::GetInstance().DeleteRoutePoint(point);
      delete point;
    }
    current->pRoutePointList->clear();
    delete current;
    GuiEvents::GetInstance().on_routes_update.Notify();
    wxLogMessage("External control audit: command=%s action=update-draft route=%s",
                 command_id, replacement->m_GUID);
    return Result<RouteCommandResult>::FromValue(
        {command_id, Snapshot(*replacement), true, {}});
  }

  Result<RouteCommandResult> Delete(
      const std::string& guid, std::uint64_t expected_revision,
      const std::string& command_id) override {
    if (!Ready()) return NotReady<RouteCommandResult>();
    auto* route = g_pRouteMan->FindRouteByGUID(guid);
    if (!route)
      return Result<RouteCommandResult>::FromError("not_found", "Route not found");
    if (route->m_bIsInLayer)
      return Result<RouteCommandResult>::FromError("layer_owned",
                                                   "Layer routes cannot be deleted");
    if (Revision(*route) != expected_revision)
      return Result<RouteCommandResult>::FromError(
          "conflict", "Route revision does not match expectedRevision");
    if (!NavObj_dB::GetInstance().DeleteRoute(route))
      return Result<RouteCommandResult>::FromError(
          "persistence_failed", "Route deletion could not be persisted");
    if (!g_pRouteMan->DeleteRoute(route))
      return Result<RouteCommandResult>::FromError(
          "command_failed", "Route manager rejected deletion");
    GuiEvents::GetInstance().on_routes_update.Notify();
    wxLogMessage("External control audit: command=%s action=delete-route route=%s",
                 command_id, guid);
    return Result<RouteCommandResult>::FromValue(
        {command_id, std::nullopt, true, {}});
  }

  Result<RouteCommandResult> Activate(
      const std::string& guid, const std::optional<std::string>& waypoint_guid,
      const std::string& command_id) override {
    if (!Ready()) return NotReady<RouteCommandResult>();
    auto* route = g_pRouteMan->FindRouteByGUID(guid);
    if (!route)
      return Result<RouteCommandResult>::FromError("not_found", "Route not found");
    if (route->GetnPoints() < 2)
      return Result<RouteCommandResult>::FromError("invalid_route",
                                                   "Route has fewer than two points");
    RoutePoint* start = nullptr;
    if (waypoint_guid) {
      start = route->GetPoint(*waypoint_guid);
      if (!start)
        return Result<RouteCommandResult>::FromError(
            "not_found", "Activation waypoint is not part of the route");
    }
    if (g_pRouteMan->GetpActiveRoute() == route &&
        (!start || g_pRouteMan->GetpActivePoint() == start)) {
      return Result<RouteCommandResult>::FromValue(
          {command_id, Snapshot(*route), false,
           {"Route was already active at the requested waypoint"}});
    }
    if (g_pRouteMan->GetpActiveRoute()) g_pRouteMan->DeactivateRoute();
    if (!g_pRouteMan->ActivateRoute(route, start))
      return Result<RouteCommandResult>::FromError(
          "command_failed", "Route manager rejected activation");
    wxLogMessage("External control audit: command=%s action=activate-route route=%s",
                 command_id, guid);
    return Result<RouteCommandResult>::FromValue(
        {command_id, Snapshot(*route), true,
         {"Route activation may affect configured navigation outputs"}});
  }

  Result<RouteCommandResult> Deactivate(
      const std::string& command_id) override {
    if (!Ready()) return NotReady<RouteCommandResult>();
    if (!g_pRouteMan->GetpActiveRoute())
      return Result<RouteCommandResult>::FromValue(
          {command_id, std::nullopt, false, {}});
    if (!g_pRouteMan->DeactivateRoute())
      return Result<RouteCommandResult>::FromError(
          "command_failed", "Route manager rejected deactivation");
    wxLogMessage("External control audit: command=%s action=deactivate-route",
                 command_id);
    return Result<RouteCommandResult>::FromValue(
        {command_id, std::nullopt, true, {}});
  }

private:
  bool Ready() const {
    return wxThread::IsMain() && pRouteList && pWayPointMan && pSelect && g_pRouteMan;
  }

  template <typename T>
  Result<T> NotReady() const {
    return Result<T>::FromError("not_ready", "Route services are not ready");
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

/**
 * Minimal real provider proving the asynchronous planning boundary without
 * moving a routing engine into core.  It only accepts a direct segment and
 * completes when the chart service authoritatively validates that segment.
 */
class ChartDirectPlanningProvider final : public PlanningProvider {
public:
  explicit ChartDirectPlanningProvider(std::shared_ptr<ChartSafetyQuery> safety)
      : safety_(std::move(safety)) {}

  std::string Capability() const override {
    return "route-planning.chart-direct.v1";
  }

  Result<PlanningResult> Run(
      const PlanningRequest& request, const PlanningCancellation& cancellation,
      const std::function<void(double)>& report_progress) override {
    if (!wxTheApp)
      return Result<PlanningResult>::FromError(
          "not_ready", "OpenCPN application executor is unavailable");
    struct QueryState {
      std::mutex mutex;
      std::condition_variable changed;
      bool complete = false;
      Result<ChartSafetyResult> result;
    };
    auto state = std::make_shared<QueryState>();
    auto safety = safety_;
    wxTheApp->CallAfter([state, safety, request] {
      auto answer =
          safety->ValidateSegment(request.start, request.destination, request.safety);
      {
        std::lock_guard<std::mutex> lock(state->mutex);
        state->result = std::move(answer);
        state->complete = true;
      }
      state->changed.notify_all();
    });
    report_progress(0.25);
    std::unique_lock<std::mutex> lock(state->mutex);
    while (!state->complete) {
      if (cancellation.IsCancellationRequested())
        return Result<PlanningResult>::FromError(
            "cancelled", "Planning was cancelled while chart data was queried");
      state->changed.wait_for(lock, std::chrono::milliseconds(50));
    }
    auto answer = std::move(state->result);
    lock.unlock();
    if (answer.error)
      return Result<PlanningResult>{std::nullopt, std::move(answer.error)};
    if (answer.value->decision != ChartSafetyDecision::Pass ||
        answer.value->authority != ChartSafetyAuthority::Authoritative)
      return Result<PlanningResult>::FromError(
          answer.value->cause_code.empty() ? "unsafe_or_unknown"
                                           : answer.value->cause_code,
          "Direct route did not pass authoritative chart validation");
    report_progress(0.9);
    PlanningResult result;
    result.draft_route.name = "Chart-validated direct route";
    result.draft_route.waypoints = {
        {"", "Start", request.start}, {"", "Destination", request.destination}};
    result.input_provenance = {
        "OpenCPN chart-safety service",
        "direct segment; no weather or current optimization"};
    result.chart_database_identity = answer.value->chart_database_identity;
    result.final_safety = *answer.value;
    result.warnings = {
        "Direct chart validation is not a weather-routing optimization"};
    report_progress(1.0);
    return Result<PlanningResult>::FromValue(std::move(result));
  }

private:
  std::shared_ptr<ChartSafetyQuery> safety_;
};

}  // namespace

ocpn::control::ServiceBundle MakeExternalControlServices() {
  auto events = std::make_shared<LiveApplicationEvents>();
  auto planning = std::make_shared<InProcessPlanningJobService>(events, 2, 64);
  auto chart_safety = std::make_shared<LiveChartSafety>();
  planning->RegisterProvider(
      std::make_shared<ChartDirectPlanningProvider>(chart_safety));
  return {std::make_shared<LiveReadiness>(), std::make_shared<LiveNavigation>(),
          std::make_shared<LiveRoutes>(), std::make_shared<LiveRouteCommands>(),
          std::move(chart_safety), std::move(events),
          std::move(planning)};
}
