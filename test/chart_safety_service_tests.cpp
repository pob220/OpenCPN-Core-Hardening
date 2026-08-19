#include <gtest/gtest.h>

#include "gui/chart_safety_service.h"

namespace {

using ocpn::chart_safety::MayPrefetchNeighbour;

TEST(ChartSafetyService, PermitsPrefetchBeforePositiveBudgetExpires) {
  EXPECT_TRUE(MayPrefetchNeighbour(49, 50));
}

TEST(ChartSafetyService, StopsPrefetchAtAndAfterBudget) {
  EXPECT_FALSE(MayPrefetchNeighbour(50, 50));
  EXPECT_FALSE(MayPrefetchNeighbour(5000, 50));
}

TEST(ChartSafetyService, NonPositiveBudgetIsUnlimited) {
  EXPECT_TRUE(MayPrefetchNeighbour(5000, 0));
  EXPECT_TRUE(MayPrefetchNeighbour(5000, -1));
}

}  // namespace
