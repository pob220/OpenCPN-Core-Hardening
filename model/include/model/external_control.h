/***************************************************************************
 * Copyright (C) 2026 OpenCPN contributors
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 or later.
 ***************************************************************************/

#ifndef MODEL_EXTERNAL_CONTROL_H_
#define MODEL_EXTERNAL_CONTROL_H_

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
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

enum class ApplicationEventType {
  Navigation,
  NavigationValidity,
  RouteCatalogue,
  ActiveRoute,
  ChartDatabase,
  Readiness,
  PlanningJob
};

struct ApplicationEvent {
  std::uint64_t sequence = 0;
  Clock::time_point timestamp{};
  ApplicationEventType type = ApplicationEventType::Readiness;
  std::string subject_id;
};

struct ApplicationEventBatch {
  bool gap = false;
  std::uint64_t oldest_available_sequence = 0;
  std::uint64_t latest_sequence = 0;
  std::vector<ApplicationEvent> events;
};

class ApplicationEventStream {
public:
  virtual ~ApplicationEventStream() = default;
  virtual std::uint64_t Publish(ApplicationEvent event) = 0;
  virtual ApplicationEventBatch ReadAfter(std::uint64_t sequence,
                                          std::size_t maximum) const = 0;
  virtual std::uint64_t LatestSequence() const = 0;
  virtual void Close() = 0;
};

/** Thread-safe bounded semantic event history for transport adapters. */
class BoundedApplicationEventStream : public ApplicationEventStream {
public:
  explicit BoundedApplicationEventStream(std::size_t capacity = 256);
  std::uint64_t Publish(ApplicationEvent event) override;
  ApplicationEventBatch ReadAfter(std::uint64_t sequence,
                                  std::size_t maximum) const override;
  std::uint64_t LatestSequence() const override;
  void Close() override;

private:
  const std::size_t capacity_;
  mutable std::mutex mutex_;
  std::deque<ApplicationEvent> events_;
  std::uint64_t next_sequence_ = 1;
  bool closed_ = false;
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

enum class ProviderKind { RoutePlanning, EnvironmentalData };
enum class ProviderFieldType {
  String,
  Integer,
  Number,
  Boolean,
  Coordinate,
  Resource
};

struct ProviderChoice {
  std::string value;
  std::string label;
};

struct ProviderFieldDescriptor {
  std::string name;
  std::string label;
  ProviderFieldType type = ProviderFieldType::String;
  bool required = false;
  std::string unit;
  std::string default_value;
  std::optional<double> minimum;
  std::optional<double> maximum;
  std::string resource_kind;
  std::vector<ProviderChoice> choices;
};

struct ProviderResource {
  std::string kind;
  std::string identity;
  std::string label;
  bool available = true;
  std::vector<std::pair<std::string, std::string>> metadata;
};

/** Typed provider description; no transport, JSON or wxWidgets types. */
struct ProviderDescriptor {
  std::string capability;
  std::string display_name;
  ProviderKind kind = ProviderKind::RoutePlanning;
  std::uint32_t schema_version = 1;
  bool cancellable = true;
  std::size_t maximum_concurrent_jobs = 1;
  std::string required_scope;
  std::vector<ProviderFieldDescriptor> fields;
  std::vector<ProviderResource> resources;
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
  virtual Result<RouteCommandResult> Update(const std::string& guid,
                                            std::uint64_t expected_revision,
                                            const RouteMutation& route,
                                            const std::string& command_id) = 0;
  virtual Result<RouteCommandResult> Delete(const std::string& guid,
                                            std::uint64_t expected_revision,
                                            const std::string& command_id) = 0;
  virtual Result<RouteCommandResult> Activate(
      const std::string& guid, const std::optional<std::string>& waypoint_guid,
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

enum class PlanningJobState { Queued, Running, Completed, Failed, Cancelled };

struct PlanningRequest {
  std::string provider_capability;
  Coordinate start;
  Coordinate destination;
  std::optional<Clock::time_point> departure_time;
  ChartSafetyConstraints safety;
  std::string vessel_identity;
  std::string polar_identity;
  std::string weather_dataset_identity;
  std::string current_dataset_identity;
  std::chrono::hours horizon{240};
  bool allow_climatology_fallback = false;
  std::uint64_t effort_limit = 1000000;
  int departure_window_before_minutes = 0;
  int departure_window_after_minutes = 0;
  int departure_step_minutes = 60;
  int concurrent_routes = 1;
  int routing_effort_percent = 100;
};

struct PlanningResult {
  RouteMutation draft_route;
  std::vector<RouteMutation> alternatives;
  std::vector<std::string> input_provenance;
  std::string chart_database_identity;
  ChartSafetyResult final_safety;
  std::vector<std::string> warnings;
};

struct PlanningJobSnapshot {
  std::string id;
  std::string owner_id;
  std::string provider_capability;
  PlanningJobState state = PlanningJobState::Queued;
  double progress = 0.0;
  bool cancellation_requested = false;
  Clock::time_point submitted_time{};
  Clock::time_point updated_time{};
  std::optional<ServiceError> error;
};

class PlanningCancellation {
public:
  virtual ~PlanningCancellation() = default;
  virtual bool IsCancellationRequested() const = 0;
};

class PlanningProvider {
public:
  virtual ~PlanningProvider() = default;
  virtual std::string Capability() const = 0;
  virtual ProviderDescriptor Describe() const {
    ProviderDescriptor descriptor;
    descriptor.capability = Capability();
    descriptor.display_name = Capability();
    descriptor.kind = ProviderKind::RoutePlanning;
    descriptor.required_scope = "planning:run";
    return descriptor;
  }
  virtual Result<PlanningResult> Run(
      const PlanningRequest& request, const PlanningCancellation& cancellation,
      const std::function<void(double)>& report_progress) = 0;
};

class PlanningJobService {
public:
  virtual ~PlanningJobService() = default;
  virtual bool RegisterProvider(std::shared_ptr<PlanningProvider> provider) = 0;
  virtual bool UnregisterProvider(const std::string& capability) = 0;
  virtual std::vector<std::string> ProviderCapabilities() const = 0;
  virtual std::vector<ProviderDescriptor> ProviderDescriptors() const = 0;
  virtual Result<PlanningJobSnapshot> Submit(const PlanningRequest& request,
                                             const std::string& owner_id) = 0;
  virtual Result<PlanningJobSnapshot> Get(
      const std::string& id, const std::string& owner_id) const = 0;
  virtual Result<PlanningJobSnapshot> Cancel(const std::string& id,
                                             const std::string& owner_id) = 0;
  virtual Result<PlanningResult> GetResult(
      const std::string& id, const std::string& owner_id) const = 0;
  virtual void Shutdown() = 0;
};

/** Bounded worker-pool job host. Providers are pinned while jobs run. */
class InProcessPlanningJobService final : public PlanningJobService {
public:
  explicit InProcessPlanningJobService(
      std::shared_ptr<ApplicationEventStream> events = nullptr,
      std::size_t worker_count = 2, std::size_t maximum_jobs = 64);
  ~InProcessPlanningJobService() override;
  bool RegisterProvider(std::shared_ptr<PlanningProvider> provider) override;
  bool UnregisterProvider(const std::string& capability) override;
  std::vector<std::string> ProviderCapabilities() const override;
  std::vector<ProviderDescriptor> ProviderDescriptors() const override;
  Result<PlanningJobSnapshot> Submit(const PlanningRequest& request,
                                     const std::string& owner_id) override;
  Result<PlanningJobSnapshot> Get(const std::string& id,
                                  const std::string& owner_id) const override;
  Result<PlanningJobSnapshot> Cancel(const std::string& id,
                                     const std::string& owner_id) override;
  Result<PlanningResult> GetResult(const std::string& id,
                                   const std::string& owner_id) const override;
  void Shutdown() override;

private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

struct ServiceBundle {
  std::shared_ptr<ReadinessService> readiness;
  std::shared_ptr<NavigationSnapshotService> navigation;
  std::shared_ptr<RouteQueryService> routes;
  std::shared_ptr<RouteCommandService> route_commands;
  std::shared_ptr<ChartSafetyQuery> chart_safety;
  std::shared_ptr<ApplicationEventStream> events;
  std::shared_ptr<PlanningJobService> planning;
};

}  // namespace ocpn::control

#endif  // MODEL_EXTERNAL_CONTROL_H_
