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

struct ExternalApiTest : public testing::Test {
  ExternalApiTest() {
    authorizer->Put("read-token",
                    {"test-client", {"navigation:read", "routes:read",
                                     "charts:query"}});
    authorizer->Put("navigation-only", {"limited", {"navigation:read"}});
  }

  HttpRequest Request(std::string method, std::string path,
                      std::string body = {}) const {
    return {std::move(method), std::move(path),
            {{"Authorization", "Bearer read-token"}}, std::move(body),
            "127.0.0.1"};
  }

  std::shared_ptr<TokenAuthorizer> authorizer =
      std::make_shared<TokenAuthorizer>();
  ServiceBundle services{std::make_shared<Readiness>(),
                         std::make_shared<Navigation>(),
                         std::make_shared<Routes>(),
                         std::make_shared<Safety>()};
  ExternalApiRouter router{services, authorizer, {true, 1024, "5.16-test"}};
};

TEST_F(ExternalApiTest, DisabledApiIsIndistinguishableFromMissingEndpoint) {
  ExternalApiRouter disabled(services, authorizer, {false, 1024, "5.16-test"});
  const auto response = disabled.Handle(Request("GET", "/api/v2/version"));
  EXPECT_EQ(response.status, 404);
  EXPECT_EQ(nlohmann::json::parse(response.body)["error"]["code"], "api_disabled");
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
            nlohmann::json::array({"readiness.v1", "navigation.snapshot.v1"}));
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
  EXPECT_EQ(router.Handle(Request("POST", "/api/v2/chart-safety/validate-point",
                                  "not-json"))
                .status,
            400);
  EXPECT_EQ(router.Handle(Request("POST", "/api/v2/chart-safety/validate-point",
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

}  // namespace
