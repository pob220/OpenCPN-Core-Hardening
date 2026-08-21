#include <atomic>
#include <thread>

#include <gtest/gtest.h>

#include "model/external_api.h"
#include "ocpn-nlohmann/json.hpp"

namespace {
using namespace ocpn::control;

class Readiness final : public ReadinessService {
public:
  ReadinessSnapshot GetReadiness() const override { return {true, false, {}}; }
};

class Navigation final : public NavigationSnapshotService {
public:
  Result<NavigationSnapshot> GetSnapshot() const override {
    NavigationSnapshot snapshot;
    snapshot.position = Coordinate{53.307, -4.632};
    snapshot.position_valid = true;
    snapshot.stale = false;
    snapshot.source = "unit-test";
    snapshot.speed_over_ground_knots = 6.25;
    snapshot.receipt_time = Clock::now();
    return Result<NavigationSnapshot>::FromValue(snapshot);
  }
};

class Routes final : public RouteQueryService {
public:
  Result<std::vector<RouteSummary>> ListRoutes() const override {
    return Result<std::vector<RouteSummary>>::FromValue(
        {{"route-1", "Holyhead \"to\" Foyle", 7, 2, false, true}});
  }
  Result<RouteSnapshot> GetRoute(const std::string& guid) const override {
    if (guid != "route-1")
      return Result<RouteSnapshot>::FromError("not_found", "Route not found");
    RouteSnapshot route;
    route.guid = guid;
    route.name = "Holyhead to Foyle";
    route.revision = 7;
    route.waypoint_count = 2;
    route.waypoints = {{"wpt-1", "Start", {53.307, -4.632}},
                       {"wpt-2", "Finish", {55.0, -7.3}}};
    return Result<RouteSnapshot>::FromValue(route);
  }
  Result<ActiveRouteSnapshot> GetActiveRoute() const override {
    return Result<ActiveRouteSnapshot>::FromValue({});
  }
};

class Safety final : public ChartSafetyQuery {
public:
  Result<ChartSafetyResult> ValidatePoint(
      const Coordinate&, const ChartSafetyConstraints& constraints) override {
    return Unknown(constraints);
  }
  Result<ChartSafetyResult> ValidateSegment(
      const Coordinate&, const Coordinate&,
      const ChartSafetyConstraints& constraints) override {
    return Unknown(constraints);
  }
  Result<ChartSafetyResult> ValidateRoute(
      const std::vector<Coordinate>&,
      const ChartSafetyConstraints& constraints) override {
    return Unknown(constraints);
  }

private:
  static Result<ChartSafetyResult> Unknown(
      const ChartSafetyConstraints& constraints) {
    ChartSafetyResult result;
    result.decision = ChartSafetyDecision::Unknown;
    result.authority = ChartSafetyAuthority::Unknown;
    result.cause_code = "chart_data_unavailable";
    result.constraints = constraints;
    result.warnings = {"No suitable chart is loaded"};
    return Result<ChartSafetyResult>::FromValue(result);
  }
};

class Commands final : public RouteCommandService {
public:
  Result<RouteCommandResult> CreateDraft(
      const RouteMutation& mutation, const std::string& command_id) override {
    ++calls;
    RouteSnapshot route;
    route.guid = "created-route";
    route.name = mutation.name;
    route.revision = 11;
    route.waypoint_count = mutation.waypoints.size();
    route.is_draft = true;
    route.waypoints = mutation.waypoints;
    return Result<RouteCommandResult>::FromValue({command_id, route, true, {}});
  }
  Result<RouteCommandResult> Update(const std::string&,
                                    std::uint64_t expected_revision,
                                    const RouteMutation&,
                                    const std::string& command_id) override {
    ++calls;
    if (expected_revision != 11)
      return Result<RouteCommandResult>::FromError("conflict",
                                                   "stale revision");
    return Result<RouteCommandResult>::FromValue(
        {command_id, std::nullopt, true, {}});
  }
  Result<RouteCommandResult> Delete(const std::string&, std::uint64_t,
                                    const std::string& command_id) override {
    ++calls;
    return Result<RouteCommandResult>::FromValue(
        {command_id, std::nullopt, true, {}});
  }
  Result<RouteCommandResult> Activate(const std::string&,
                                      const std::optional<std::string>&,
                                      const std::string& command_id) override {
    ++calls;
    return Result<RouteCommandResult>::FromValue(
        {command_id, std::nullopt, true, {"output-affecting"}});
  }
  Result<RouteCommandResult> Deactivate(
      const std::string& command_id) override {
    ++calls;
    return Result<RouteCommandResult>::FromValue(
        {command_id, std::nullopt, true, {}});
  }
  std::atomic<int> calls{0};
};

class DeterministicPlanningProvider final : public PlanningProvider {
public:
  std::string Capability() const override { return "route-planning.test.v1"; }
  ProviderDescriptor Describe() const override {
    ProviderDescriptor descriptor;
    descriptor.capability = Capability();
    descriptor.display_name = "Deterministic test planner";
    descriptor.fields = {
        {"polarIdentity", "Polar", ProviderFieldType::Resource, false, {},
         {}, std::nullopt, std::nullopt, "polar"}};
    descriptor.resources = {
        {"polar", "test.pol", "Test polar", true, {{"format", "pol"}}}};
    return descriptor;
  }
  Result<PlanningResult> Run(
      const PlanningRequest& request, const PlanningCancellation& cancellation,
      const std::function<void(double)>& report_progress) override {
    last_request = request;
    report_progress(0.5);
    if (cancellation.IsCancellationRequested())
      return Result<PlanningResult>::FromError("cancelled", "cancelled");
    PlanningResult result;
    result.draft_route.name = "Planned draft";
    result.draft_route.waypoints = {{"", "Start", request.start},
                                    {"", "Finish", request.destination}};
    result.input_provenance = {"deterministic-test-provider"};
    result.final_safety.decision = ChartSafetyDecision::Pass;
    result.final_safety.authority = ChartSafetyAuthority::Authoritative;
    result.final_safety.cause_code = "clear";
    result.final_safety.constraints = request.safety;
    return Result<PlanningResult>::FromValue(std::move(result));
  }
  std::optional<PlanningRequest> last_request;
};

class DeterministicEnvironmentProvider final : public EnvironmentalProvider {
public:
  ProviderDescriptor Describe() const override {
    ProviderDescriptor descriptor;
    descriptor.capability = "environmental-data.test.v1";
    descriptor.display_name = "Deterministic environment provider";
    descriptor.kind = ProviderKind::EnvironmentalData;
    descriptor.fields = {
        {"hours", "Duration", ProviderFieldType::Integer, true, "h", "72",
         1.0, 8760.0}};
    descriptor.resources = {
        {"weather-provider", "fixture", "Fixture model", true, {}}};
    return descriptor;
  }

  Result<EnvironmentalDataset> Run(
      const EnvironmentalRequest& request,
      const PlanningCancellation& cancellation,
      const std::function<void(double)>& report_progress) override {
    last_request = request;
    report_progress(0.5);
    if (cancellation.IsCancellationRequested())
      return Result<EnvironmentalDataset>::FromError("cancelled", "cancelled");
    EnvironmentalDataset dataset;
    dataset.identity = "fixture-20260821T0000Z";
    dataset.provider_capability = Describe().capability;
    dataset.provider_handle = "/private/not-serialized.grb2";
    dataset.model = "fixture";
    dataset.cycle = "2026-08-21T00:00:00Z";
    dataset.south_west = {50.0, -9.0};
    dataset.north_east = {56.0, -4.0};
    dataset.valid_from = Clock::from_time_t(1787270400);
    dataset.valid_to = Clock::from_time_t(1787533200);
    dataset.fields = {"wind-u-10m", "wind-v-10m"};
    dataset.checksum_sha256 = std::string(64, 'a');
    dataset.byte_size = 1024;
    dataset.provenance = {"deterministic fixture"};
    return Result<EnvironmentalDataset>::FromValue(std::move(dataset));
  }

  Result<EnvironmentalDataset> Activate(
      const EnvironmentalDataset& dataset) override {
    ++activation_calls;
    auto activated = dataset;
    activated.active = true;
    return Result<EnvironmentalDataset>::FromValue(std::move(activated));
  }

  std::optional<EnvironmentalRequest> last_request;
  std::atomic<int> activation_calls{0};
};

struct ExternalApiTest : public testing::Test {
  ExternalApiTest() {
    authorizer->Put(
        "read-token",
        {"test-client", {"navigation:read", "routes:read", "charts:query"}});
    authorizer->Put("navigation-only", {"limited", {"navigation:read"}});
    authorizer->Put("write-token",
                    {"writer",
                     {"navigation:read", "routes:read", "routes:write",
                      "routes:activate", "planning:run", "environment:read",
                      "environment:acquire", "environment:activate"}});
    planning->RegisterProvider(provider);
    environment->RegisterProvider(environment_provider);
  }

  HttpRequest Request(std::string method, std::string path,
                      std::string body = {}) const {
    return {std::move(method),
            std::move(path),
            {{"Authorization", "Bearer read-token"}},
            std::move(body),
            "127.0.0.1"};
  }

  std::shared_ptr<TokenAuthorizer> authorizer =
      std::make_shared<TokenAuthorizer>();
  std::shared_ptr<Commands> commands = std::make_shared<Commands>();
  std::shared_ptr<BoundedApplicationEventStream> events =
      std::make_shared<BoundedApplicationEventStream>(4);
  std::shared_ptr<InProcessPlanningJobService> planning =
      std::make_shared<InProcessPlanningJobService>(events, 1, 8);
  std::shared_ptr<DeterministicPlanningProvider> provider =
      std::make_shared<DeterministicPlanningProvider>();
  std::shared_ptr<InProcessEnvironmentalJobService> environment =
      std::make_shared<InProcessEnvironmentalJobService>(events, 1, 8, 4);
  std::shared_ptr<DeterministicEnvironmentProvider> environment_provider =
      std::make_shared<DeterministicEnvironmentProvider>();
  ServiceBundle services{std::make_shared<Readiness>(),
                         std::make_shared<Navigation>(),
                         std::make_shared<Routes>(),
                         commands,
                         std::make_shared<Safety>(),
                         events,
                         planning,
                         environment};
  ExternalApiRouter router{services, authorizer, {true, 1024, "5.16-test"}};
};

TEST_F(ExternalApiTest, DisabledApiIsIndistinguishableFromMissingEndpoint) {
  ExternalApiRouter disabled(services, authorizer, {false, 1024, "5.16-test"});
  const auto response = disabled.Handle(Request("GET", "/api/v2/version"));
  EXPECT_EQ(response.status, 404);
  EXPECT_EQ(nlohmann::json::parse(response.body)["error"]["code"],
            "api_disabled");
}

TEST_F(ExternalApiTest, RejectsMissingAndRevokedCredentials) {
  auto request = Request("GET", "/api/v2/version");
  request.headers.clear();
  EXPECT_EQ(router.Handle(request).status, 401);
  authorizer->Revoke("read-token");
  request.headers["Authorization"] = "Bearer read-token";
  EXPECT_EQ(router.Handle(request).status, 401);
}

TEST_F(ExternalApiTest, RejectsLanClientsByDefault) {
  auto request = Request("GET", "/api/v2/version");
  request.remote_address = "192.0.2.20";
  const auto response = router.Handle(request);
  EXPECT_EQ(response.status, 403);
  EXPECT_EQ(nlohmann::json::parse(response.body)["error"]["code"],
            "loopback_required");
}

TEST_F(ExternalApiTest, DiscoversOnlyGrantedAndAvailableCapabilities) {
  auto request = Request("GET", "/api/v2/capabilities");
  request.headers["Authorization"] = "Bearer navigation-only";
  const auto response = router.Handle(request);
  const auto body = nlohmann::json::parse(response.body);
  EXPECT_EQ(response.status, 200);
  EXPECT_EQ(body["capabilities"],
            nlohmann::json::array({"readiness.v1", "navigation.snapshot.v1",
                                   "events.semantic.v1"}));
}

TEST_F(ExternalApiTest, DiscoversTypedProviderDescriptorsByScope) {
  auto denied = Request("GET", "/api/v2/providers");
  EXPECT_TRUE(nlohmann::json::parse(router.Handle(denied).body)["providers"]
                  .empty());

  auto request = Request("GET", "/api/v2/providers");
  request.headers["Authorization"] = "Bearer write-token";
  const auto response = router.Handle(request);
  ASSERT_EQ(response.status, 200);
  const auto body = nlohmann::json::parse(response.body);
  ASSERT_EQ(body["providers"].size(), 2);
  EXPECT_EQ(body["providers"][0]["capability"],
            "route-planning.test.v1");
  EXPECT_EQ(body["providers"][0]["fields"][0]["resourceKind"], "polar");
  EXPECT_EQ(body["providers"][0]["resources"][0]["identity"], "test.pol");
  EXPECT_EQ(body["providers"][1]["capability"],
            "environmental-data.test.v1");
}

TEST_F(ExternalApiTest, EnvironmentalDatasetPublishesThenActivatesAtomically) {
  auto submit = Request(
      "POST", "/api/v2/environment/jobs",
      R"({"providerCapability":"environmental-data.test.v1","parameters":{"hours":72,"includeWaves":false}})");
  submit.headers["Authorization"] = "Bearer write-token";
  submit.headers["Idempotency-Key"] = "environment-fixture-1";
  const auto accepted = router.Handle(submit);
  ASSERT_EQ(accepted.status, 202);
  const auto id = nlohmann::json::parse(accepted.body)["id"].get<std::string>();

  nlohmann::json status;
  for (int attempt = 0; attempt < 100; ++attempt) {
    auto query = Request("GET", "/api/v2/environment/jobs/" + id);
    query.headers["Authorization"] = "Bearer write-token";
    const auto response = router.Handle(query);
    ASSERT_EQ(response.status, 200);
    status = nlohmann::json::parse(response.body);
    if (status["state"] == "completed") break;
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
  ASSERT_EQ(status["state"], "completed");

  auto result_request =
      Request("GET", "/api/v2/environment/jobs/" + id + "/result");
  result_request.headers["Authorization"] = "Bearer write-token";
  const auto result = router.Handle(result_request);
  ASSERT_EQ(result.status, 200);
  const auto dataset = nlohmann::json::parse(result.body);
  EXPECT_EQ(dataset["identity"], "fixture-20260821T0000Z");
  EXPECT_FALSE(dataset.contains("providerHandle"));
  EXPECT_FALSE(dataset["active"]);

  auto activate = Request(
      "POST",
      "/api/v2/environment/datasets/fixture-20260821T0000Z/activate");
  activate.headers["Authorization"] = "Bearer write-token";
  const auto activated = router.Handle(activate);
  ASSERT_EQ(activated.status, 200);
  EXPECT_TRUE(nlohmann::json::parse(activated.body)["active"]);
  EXPECT_EQ(environment_provider->activation_calls, 1);

  auto list = Request("GET", "/api/v2/environment/datasets");
  list.headers["Authorization"] = "Bearer write-token";
  const auto listed = router.Handle(list);
  ASSERT_EQ(listed.status, 200);
  EXPECT_TRUE(nlohmann::json::parse(listed.body)["datasets"][0]["active"]);
}

TEST_F(ExternalApiTest, EventUpgradeStartsWithScopedSnapshot) {
  const auto response = router.Handle(Request("GET", "/api/v2/events"));
  ASSERT_EQ(response.status, 101);
  const auto body = nlohmann::json::parse(response.body);
  EXPECT_EQ(body["type"], "snapshot");
  EXPECT_TRUE(body.contains("navigation"));
  EXPECT_TRUE(body.contains("routes"));
  EXPECT_TRUE(response.headers.count("X-OpenCPN-Event-Cursor"));
}

TEST_F(ExternalApiTest, EventFiltersAndBoundedGapAreExplicit) {
  auto subscription =
      router.ParseEventSubscription(R"({"subscribe":["navigation"]})");
  ASSERT_TRUE(subscription.value);
  events->Publish({0, {}, ApplicationEventType::RouteCatalogue, "route-1"});
  events->Publish({0, {}, ApplicationEventType::Navigation, {}});
  auto response = router.ReadEvents(0, 10, *subscription.value);
  auto body = nlohmann::json::parse(response.body);
  ASSERT_EQ(body["events"].size(), 1);
  EXPECT_EQ(body["events"][0]["type"], "navigation");

  events->Publish({0, {}, ApplicationEventType::RouteCatalogue, "route-2"});
  events->Publish({0, {}, ApplicationEventType::ActiveRoute, "route-2"});
  events->Publish({0, {}, ApplicationEventType::Readiness, {}});
  events->Publish({0, {}, ApplicationEventType::ChartDatabase, {}});
  response = router.ReadEvents(1, 10, 0xffffffffU);
  EXPECT_TRUE(nlohmann::json::parse(response.body)["gap"]);
}

TEST(BoundedApplicationEventStreamTest, CoalescesNavigationAndClosesCleanly) {
  BoundedApplicationEventStream stream(2);
  EXPECT_EQ(stream.Publish({0, {}, ApplicationEventType::Navigation, {}}), 1);
  EXPECT_EQ(stream.Publish({0, {}, ApplicationEventType::Navigation, {}}), 2);
  const auto batch = stream.ReadAfter(1, 10);
  ASSERT_EQ(batch.events.size(), 1);
  EXPECT_EQ(batch.events[0].sequence, 2);
  stream.Close();
  EXPECT_EQ(stream.Publish({0, {}, ApplicationEventType::Readiness, {}}), 0);
}

class CancellablePlanningProvider final : public PlanningProvider {
public:
  std::string Capability() const override {
    return "route-planning.blocking.v1";
  }
  Result<PlanningResult> Run(
      const PlanningRequest&, const PlanningCancellation& cancellation,
      const std::function<void(double)>& report_progress) override {
    started.store(true);
    while (!cancellation.IsCancellationRequested()) {
      report_progress(0.25);
      std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return Result<PlanningResult>::FromError("cancelled", "cancelled by test");
  }
  std::atomic_bool started{false};
};

TEST(InProcessPlanningJobServiceTest, CancellationCompletesAndPinsProvider) {
  auto provider = std::make_shared<CancellablePlanningProvider>();
  InProcessPlanningJobService service(nullptr, 1, 4);
  ASSERT_TRUE(service.RegisterProvider(provider));
  PlanningRequest request;
  request.provider_capability = provider->Capability();
  const auto submitted = service.Submit(request, "owner");
  ASSERT_TRUE(submitted.value);
  for (int attempt = 0; attempt < 100 && !provider->started.load(); ++attempt)
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  ASSERT_TRUE(provider->started.load());
  EXPECT_FALSE(service.UnregisterProvider(provider->Capability()));
  EXPECT_TRUE(service.ProviderCapabilities().empty());
  const auto rejected = service.Submit(request, "owner");
  ASSERT_TRUE(rejected.error);
  EXPECT_EQ(rejected.error->code, "provider_unavailable");
  ASSERT_TRUE(service.Cancel(submitted.value->id, "owner").value);
  PlanningJobSnapshot snapshot;
  for (int attempt = 0; attempt < 100; ++attempt) {
    auto state = service.Get(submitted.value->id, "owner");
    ASSERT_TRUE(state.value);
    snapshot = *state.value;
    if (snapshot.state == PlanningJobState::Cancelled) break;
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  EXPECT_EQ(snapshot.state, PlanningJobState::Cancelled);
  EXPECT_TRUE(service.UnregisterProvider(provider->Capability()));
  EXPECT_TRUE(service.Get(submitted.value->id, "different-owner").error);
  service.Shutdown();
}

TEST(InProcessPlanningJobServiceTest, ShutdownCancelsAndDrainsRunningProvider) {
  auto provider = std::make_shared<CancellablePlanningProvider>();
  InProcessPlanningJobService service(nullptr, 1, 4);
  ASSERT_TRUE(service.RegisterProvider(provider));
  PlanningRequest request;
  request.provider_capability = provider->Capability();
  const auto submitted = service.Submit(request, "owner");
  ASSERT_TRUE(submitted.value);
  for (int attempt = 0; attempt < 100 && !provider->started.load(); ++attempt)
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  ASSERT_TRUE(provider->started.load());

  service.Shutdown();

  const auto snapshot = service.Get(submitted.value->id, "owner");
  ASSERT_TRUE(snapshot.value);
  EXPECT_EQ(snapshot.value->state, PlanningJobState::Cancelled);
  EXPECT_TRUE(snapshot.value->cancellation_requested);
}

TEST(ExternalApiRouterTest, OnlyPlanningObservationAndCancellationBypassGui) {
  HttpRequest request;
  request.path = "/api/v2/planning/jobs/plan-7";
  request.method = "GET";
  EXPECT_TRUE(ExternalApiRouter::CanHandleOnTransportThread(request));
  request.method = "DELETE";
  EXPECT_TRUE(ExternalApiRouter::CanHandleOnTransportThread(request));
  request.path += "/result";
  EXPECT_FALSE(ExternalApiRouter::CanHandleOnTransportThread(request));
  request.method = "GET";
  EXPECT_TRUE(ExternalApiRouter::CanHandleOnTransportThread(request));

  request.method = "POST";
  EXPECT_FALSE(ExternalApiRouter::CanHandleOnTransportThread(request));
  request.method = "GET";
  request.path = "/api/v2/routes";
  EXPECT_FALSE(ExternalApiRouter::CanHandleOnTransportThread(request));
  request.path = "/api/v2/planning/jobs/plan-7/unexpected";
  EXPECT_FALSE(ExternalApiRouter::CanHandleOnTransportThread(request));
}

TEST_F(ExternalApiTest, NavigationUsesExplicitUnitsValidityAndNulls) {
  const auto response = router.Handle(Request("GET", "/api/v2/navigation"));
  const auto body = nlohmann::json::parse(response.body);
  EXPECT_EQ(response.status, 200);
  EXPECT_TRUE(body["positionValid"]);
  EXPECT_FALSE(body["stale"]);
  EXPECT_EQ(body["speedOverGroundKnots"], 6.25);
  EXPECT_TRUE(body["courseOverGroundDegreesTrue"].is_null());
  EXPECT_EQ(body["position"]["latitudeDegrees"], 53.307);
}

TEST_F(ExternalApiTest, RouteNamesAreEscapedByJsonSerializer) {
  const auto response = router.Handle(Request("GET", "/api/v2/routes"));
  EXPECT_EQ(response.status, 200);
  const auto body = nlohmann::json::parse(response.body);
  EXPECT_EQ(body["routes"][0]["name"], "Holyhead \"to\" Foyle");
  EXPECT_EQ(body["routes"][0]["revision"], 7);
}

TEST_F(ExternalApiTest, EnforcesScopes) {
  auto request = Request("GET", "/api/v2/routes");
  request.headers["Authorization"] = "Bearer navigation-only";
  const auto response = router.Handle(request);
  EXPECT_EQ(response.status, 403);
  EXPECT_EQ(nlohmann::json::parse(response.body)["error"]["code"],
            "permission_denied");
}

TEST_F(ExternalApiTest, ChartDataAbsenceRemainsUnknownNeverPass) {
  const auto response = router.Handle(Request(
      "POST", "/api/v2/chart-safety/validate-route",
      R"({"minimumDepthMeters":5.0,"landMarginNauticalMiles":0.4,"route":[{"latitudeDegrees":53.3,"longitudeDegrees":-4.6},{"latitudeDegrees":55.0,"longitudeDegrees":-7.3}]})"));
  const auto body = nlohmann::json::parse(response.body);
  EXPECT_EQ(response.status, 200);
  EXPECT_EQ(body["decision"], "unknown");
  EXPECT_EQ(body["authority"], "unknown");
  EXPECT_EQ(body["constraints"]["minimumDepthMeters"], 5.0);
}

TEST_F(ExternalApiTest, RejectsMalformedOversizedAndInvalidRequests) {
  EXPECT_EQ(router
                .Handle(Request("POST", "/api/v2/chart-safety/validate-point",
                                "not-json"))
                .status,
            400);
  EXPECT_EQ(router
                .Handle(Request("POST", "/api/v2/chart-safety/validate-point",
                                std::string(1025, 'x')))
                .status,
            413);
  const auto invalid = router.Handle(Request(
      "POST", "/api/v2/chart-safety/validate-point",
      R"({"minimumDepthMeters":5,"point":{"latitudeDegrees":91,"longitudeDegrees":0}})"));
  EXPECT_EQ(invalid.status, 400);
  const auto invalid_constraints = router.Handle(Request(
      "POST", "/api/v2/chart-safety/validate-point",
      R"({"minimumDepthMeters":-1,"point":{"latitudeDegrees":53,"longitudeDegrees":-4}})"));
  EXPECT_EQ(invalid_constraints.status, 400);
  EXPECT_EQ(nlohmann::json::parse(invalid_constraints.body)["error"]["code"],
            "invalid_constraints");
}

TEST_F(ExternalApiTest, ConcurrentRequestsCannotShareResponseState) {
  std::atomic<int> failures{0};
  std::vector<std::thread> threads;
  for (int i = 0; i < 32; ++i) {
    threads.emplace_back([&, i] {
      const auto path = i % 2 ? "/api/v2/navigation" : "/api/v2/routes";
      const auto response = router.Handle(Request("GET", path));
      const auto body = nlohmann::json::parse(response.body);
      if (response.status != 200 ||
          (i % 2 ? !body.contains("positionValid") : !body.contains("routes")))
        ++failures;
    });
  }
  for (auto& thread : threads) thread.join();
  EXPECT_EQ(failures, 0);
}

TEST_F(ExternalApiTest, CreatesDraftWithScopeAndIdempotency) {
  auto request = Request(
      "POST", "/api/v2/routes",
      R"({"name":"API draft","waypoints":[{"name":"A","position":{"latitudeDegrees":53,"longitudeDegrees":-4}},{"name":"B","position":{"latitudeDegrees":54,"longitudeDegrees":-5}}]})");
  request.headers["Authorization"] = "Bearer write-token";
  request.headers["Idempotency-Key"] = "create-1";
  auto response = router.Handle(request);
  ASSERT_EQ(response.status, 201);
  auto body = nlohmann::json::parse(response.body);
  EXPECT_TRUE(body["route"]["isDraft"]);
  EXPECT_EQ(body["route"]["revision"], 11);
  EXPECT_EQ(commands->calls, 1);
  EXPECT_EQ(router.Handle(request).status, 201);
  EXPECT_EQ(commands->calls, 1);
  request.body = R"({"name":"different","waypoints":[]})";
  EXPECT_EQ(router.Handle(request).status, 409);
}

TEST_F(ExternalApiTest, RequiresMutationScopeAndIdempotencyKey) {
  auto request = Request("POST", "/api/v2/routes", "{}");
  EXPECT_EQ(router.Handle(request).status, 403);
  request.headers["Authorization"] = "Bearer write-token";
  EXPECT_EQ(router.Handle(request).status, 400);
}

TEST_F(ExternalApiTest, RevisionConflictIsHttpConflict) {
  auto request = Request(
      "PUT", "/api/v2/routes/created-route",
      R"({"expectedRevision":10,"name":"API draft","waypoints":[{"position":{"latitudeDegrees":53,"longitudeDegrees":-4}},{"position":{"latitudeDegrees":54,"longitudeDegrees":-5}}]})");
  request.headers["Authorization"] = "Bearer write-token";
  request.headers["Idempotency-Key"] = "update-1";
  const auto response = router.Handle(request);
  EXPECT_EQ(response.status, 409);
  EXPECT_EQ(nlohmann::json::parse(response.body)["error"]["code"], "conflict");
}

TEST_F(ExternalApiTest, ActivationHasSeparateScopeAndExplicitWarning) {
  auto request = Request("POST", "/api/v2/routes/route-1/activate", "{}");
  request.headers["Idempotency-Key"] = "activate-1";
  EXPECT_EQ(router.Handle(request).status, 403);
  request.headers["Authorization"] = "Bearer write-token";
  const auto response = router.Handle(request);
  EXPECT_EQ(response.status, 200);
  EXPECT_EQ(nlohmann::json::parse(response.body)["warnings"][0],
            "output-affecting");
}

TEST_F(ExternalApiTest, PlanningJobCompletesAsDraftWithProvenance) {
  auto request = Request(
      "POST", "/api/v2/planning/jobs",
      R"({"providerCapability":"route-planning.test.v1","start":{"latitudeDegrees":53,"longitudeDegrees":-4},"destination":{"latitudeDegrees":55,"longitudeDegrees":-7},"minimumDepthMeters":5,"landMarginNauticalMiles":0.4,"allowClimatologyFallback":true,"departureWindowBeforeMinutes":180,"departureWindowAfterMinutes":240,"departureStepMinutes":60,"concurrentRoutes":7,"routingEffortPercent":200,"effortLimit":1000})");
  request.headers["Authorization"] = "Bearer write-token";
  request.headers["Idempotency-Key"] = "plan-1";
  const auto submitted = router.Handle(request);
  ASSERT_EQ(submitted.status, 202);
  const auto id =
      nlohmann::json::parse(submitted.body)["id"].get<std::string>();
  HttpResponse status;
  for (int attempt = 0; attempt < 100; ++attempt) {
    auto status_request = Request("GET", "/api/v2/planning/jobs/" + id);
    status_request.headers["Authorization"] = "Bearer write-token";
    status = router.Handle(status_request);
    auto status_body = nlohmann::json::parse(status.body);
    if (status_body["state"] == "completed") break;
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
  }
  EXPECT_EQ(nlohmann::json::parse(status.body)["state"], "completed");
  ASSERT_TRUE(provider->last_request);
  EXPECT_TRUE(provider->last_request->allow_climatology_fallback);
  EXPECT_EQ(provider->last_request->departure_window_before_minutes, 180);
  EXPECT_EQ(provider->last_request->departure_window_after_minutes, 240);
  EXPECT_EQ(provider->last_request->departure_step_minutes, 60);
  EXPECT_EQ(provider->last_request->concurrent_routes, 7);
  EXPECT_EQ(provider->last_request->routing_effort_percent, 200);
  auto result_request =
      Request("GET", "/api/v2/planning/jobs/" + id + "/result");
  result_request.headers["Authorization"] = "Bearer write-token";
  const auto result = router.Handle(result_request);
  ASSERT_EQ(result.status, 200);
  const auto body = nlohmann::json::parse(result.body);
  EXPECT_EQ(body["draftRoute"]["waypoints"].size(), 2);
  EXPECT_EQ(body["finalSafety"]["decision"], "pass");
  EXPECT_EQ(body["inputProvenance"][0], "deterministic-test-provider");
}

}  // namespace
