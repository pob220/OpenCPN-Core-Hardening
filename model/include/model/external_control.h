/***************************************************************************
 * Copyright (C) 2026 OpenCPN contributors
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 or later.
 ***************************************************************************/

#ifndef MODEL_EXTERNAL_CONTROL_H_
#define MODEL_EXTERNAL_CONTROL_H_

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace ocpn::control {

using Clock = std::chrono::system_clock;

struct Coordinate {
  double latitude_degrees = 0.0;
  double longitude_degrees = 0.0;
};

struct NavigationSnapshot {
  std::optional<Coordinate> position;
  std::optional<double> course_over_ground_degrees_true;
  std::optional<double> speed_over_ground_knots;
  std::optional<double> heading_degrees_true;
  std::optional<double> magnetic_variation_degrees;
  bool position_valid = false;
  bool stale = true;
  std::string source;
  std::optional<Clock::time_point> measurement_time;
  Clock::time_point receipt_time{};
};

struct WaypointSnapshot {
  std::string guid;
  std::string name;
  Coordinate position;
};

struct RouteSummary {
  std::string guid;
  std::string name;
  std::uint64_t revision = 0;
  std::size_t waypoint_count = 0;
  bool is_layer = false;
  bool is_draft = false;
};

struct RouteSnapshot : RouteSummary {
  std::vector<WaypointSnapshot> waypoints;
};

struct RouteMutation {
  std::string name;
  std::vector<WaypointSnapshot> waypoints;
};

struct RouteCommandResult {
  std::string command_id;
  std::optional<RouteSnapshot> route;
  bool changed = false;
  std::vector<std::string> warnings;
};

struct ActiveRouteSnapshot {
  bool active = false;
  std::optional<std::string> route_guid;
  std::optional<std::string> active_waypoint_guid;
  std::optional<std::size_t> active_leg_index;
  std::optional<std::uint64_t> route_revision;
};

struct ReadinessSnapshot {
  bool ready = false;
  bool closing = false;
  std::vector<std::string> unavailable_capabilities;
};

enum class ChartSafetyDecision { Pass, Fail, Unknown };
enum class ChartSafetyAuthority { Authoritative, Fallback, Unknown };

struct ChartSafetyConstraints {
  double minimum_depth_meters = 0.0;
  double land_margin_nautical_miles = 0.0;
};

struct ChartSafetyResult {
  ChartSafetyDecision decision = ChartSafetyDecision::Unknown;
  ChartSafetyAuthority authority = ChartSafetyAuthority::Unknown;
  std::string cause_code;
  std::string chart_database_identity;
  ChartSafetyConstraints constraints;
  std::vector<std::string> warnings;
  std::optional<std::size_t> failed_segment_index;
};

struct ServiceError {
  std::string code;
  std::string message;
};

template <typename T>
struct Result {
  std::optional<T> value;
  std::optional<ServiceError> error;

  static Result FromValue(T result) {
    return {std::move(result), std::nullopt};
  }
  static Result FromError(std::string code, std::string message) {
    return {std::nullopt, ServiceError{std::move(code), std::move(message)}};
  }
};

class ReadinessService {
public:
  virtual ~ReadinessService() = default;
  virtual ReadinessSnapshot GetReadiness() const = 0;
};

class NavigationSnapshotService {
public:
  virtual ~NavigationSnapshotService() = default;
  virtual Result<NavigationSnapshot> GetSnapshot() const = 0;
};

class RouteQueryService {
public:
  virtual ~RouteQueryService() = default;
  virtual Result<std::vector<RouteSummary>> ListRoutes() const = 0;
  virtual Result<RouteSnapshot> GetRoute(const std::string& guid) const = 0;
  virtual Result<ActiveRouteSnapshot> GetActiveRoute() const = 0;
};

class RouteCommandService {
public:
  virtual ~RouteCommandService() = default;
  virtual Result<RouteCommandResult> CreateDraft(
      const RouteMutation& route, const std::string& command_id) = 0;
  virtual Result<RouteCommandResult> Update(
      const std::string& guid, std::uint64_t expected_revision,
      const RouteMutation& route, const std::string& command_id) = 0;
  virtual Result<RouteCommandResult> Delete(
      const std::string& guid, std::uint64_t expected_revision,
      const std::string& command_id) = 0;
  virtual Result<RouteCommandResult> Activate(
      const std::string& guid,
      const std::optional<std::string>& waypoint_guid,
      const std::string& command_id) = 0;
  virtual Result<RouteCommandResult> Deactivate(
      const std::string& command_id) = 0;
};

class ChartSafetyQuery {
public:
  virtual ~ChartSafetyQuery() = default;
  virtual Result<ChartSafetyResult> ValidatePoint(
      const Coordinate& point, const ChartSafetyConstraints& constraints) = 0;
  virtual Result<ChartSafetyResult> ValidateSegment(
      const Coordinate& start, const Coordinate& end,
      const ChartSafetyConstraints& constraints) = 0;
  virtual Result<ChartSafetyResult> ValidateRoute(
      const std::vector<Coordinate>& route,
      const ChartSafetyConstraints& constraints) = 0;
};

struct ServiceBundle {
  std::shared_ptr<ReadinessService> readiness;
  std::shared_ptr<NavigationSnapshotService> navigation;
  std::shared_ptr<RouteQueryService> routes;
  std::shared_ptr<RouteCommandService> route_commands;
  std::shared_ptr<ChartSafetyQuery> chart_safety;
};

}  // namespace ocpn::control

#endif  // MODEL_EXTERNAL_CONTROL_H_
