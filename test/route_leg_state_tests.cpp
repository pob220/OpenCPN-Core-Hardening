/***************************************************************************
 *   Copyright (C) 2026 OpenCPN contributors                              *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#include <gtest/gtest.h>

#include <cmath>

#include "model/route_leg_state.h"

TEST(RouteLegState, EastboundLegUsesCrossTrackCorrectionDirection) {
  const auto state = CalculateRouteLegState(
      RoutePosition{0.1, 0.9}, RoutePosition{0.0, 0.0},
      RoutePosition{0.0, 1.0});

  EXPECT_NEAR(state.segment_course, 90.0, 0.01);
  EXPECT_NEAR(state.cross_track_error, 6.0, 0.1);
  EXPECT_EQ(state.cross_track_direction, 1);  // South/right toward track.
  EXPECT_TRUE(std::isfinite(state.range_to_arrival_normal));
}

TEST(RouteLegState, RecomputesAllFieldsForNewLegAtTransition) {
  const RoutePosition vessel{-0.02, 0.01};
  const auto arrived_leg = CalculateRouteLegState(
      vessel, RoutePosition{0.0, -1.0}, RoutePosition{0.0, 0.0});
  const auto next_leg = CalculateRouteLegState(
      vessel, RoutePosition{0.0, 0.0}, RoutePosition{1.0, 0.0});

  EXPECT_NEAR(arrived_leg.segment_course, 90.0, 0.01);
  EXPECT_NEAR(next_leg.segment_course, 0.0, 0.01);
  EXPECT_GT(std::fabs(arrived_leg.bearing_to_waypoint -
                      next_leg.bearing_to_waypoint),
            20.0);
  EXPECT_NE(arrived_leg.cross_track_direction,
            next_leg.cross_track_direction);
}

TEST(RouteLegState, HandlesAntimeridianBearing) {
  const auto state = CalculateRouteLegState(
      RoutePosition{0.0, 179.9}, RoutePosition{0.0, 179.0},
      RoutePosition{0.0, -179.9});

  EXPECT_NEAR(state.bearing_to_waypoint, 90.0, 0.01);
  EXPECT_TRUE(std::isfinite(state.range_to_waypoint));
  EXPECT_LT(state.range_to_waypoint, 20.0);
}
