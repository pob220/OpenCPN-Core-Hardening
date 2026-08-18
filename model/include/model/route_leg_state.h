/***************************************************************************
 *   Copyright (C) 2026 OpenCPN contributors                              *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#ifndef ROUTE_LEG_STATE_H_
#define ROUTE_LEG_STATE_H_

/** Geographic position in degrees used by route-leg calculations. */
struct RoutePosition {
  double latitude = 0.0;
  double longitude = 0.0;
};

/** Derived navigation values for one active route leg. */
struct RouteLegState {
  double bearing_to_waypoint = 0.0;
  double range_to_waypoint = 0.0;
  double cross_track_error = 0.0;
  double range_to_arrival_normal = 0.0;
  double segment_course = 0.0;
  double course_to_segment = 0.0;

  // -1 means steer left to regain the track, +1 means steer right.
  int cross_track_direction = 1;
};

/**
 * Calculate the values used for route progress and autopilot output.
 *
 * This preserves OpenCPN's Mercator-sailing cross-track semantics while
 * making the calculation independent of route, UI and output ownership.
 */
RouteLegState CalculateRouteLegState(RoutePosition vessel,
                                     RoutePosition segment_begin,
                                     RoutePosition waypoint);

#endif  // ROUTE_LEG_STATE_H_
