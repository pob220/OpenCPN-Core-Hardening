/***************************************************************************
 *   Copyright (C) 2026 OpenCPN contributors                              *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#include "model/route_leg_state.h"

#include <cmath>

#include "model/georef.h"
#include "vector2D.h"

namespace {
constexpr double kPi = 3.1415926535897931160;
}

RouteLegState CalculateRouteLegState(RoutePosition vessel,
                                     RoutePosition segment_begin,
                                     RoutePosition waypoint) {
  RouteLegState state;

  // Bearing from vessel to active waypoint, using the existing Mercator
  // sailing convention and antimeridian handling.
  double north = 0.0;
  double east = 0.0;
  toSM(waypoint.latitude, waypoint.longitude, vessel.latitude,
       vessel.longitude, &east, &north);
  const double angle = std::atan(north / east);
  if (std::fabs(waypoint.longitude - vessel.longitude) < 180.0) {
    state.bearing_to_waypoint = waypoint.longitude >= vessel.longitude
                                    ? 90.0 - angle * 180.0 / kPi
                                    : 270.0 - angle * 180.0 / kPi;
  } else {
    state.bearing_to_waypoint = waypoint.longitude >= vessel.longitude
                                    ? 270.0 - angle * 180.0 / kPi
                                    : 90.0 - angle * 180.0 / kPi;
  }

  state.range_to_waypoint = DistGreatCircle(
      vessel.latitude, vessel.longitude, waypoint.latitude, waypoint.longitude);

  // Vectors are based at the active waypoint: vb points to the segment
  // origin and va points to the vessel.
  vector2D vessel_vector;
  vector2D segment_vector;
  vector2D normal;
  double bearing = 0.0;
  double distance = 0.0;
  DistanceBearingMercator(waypoint.latitude, waypoint.longitude,
                          segment_begin.latitude, segment_begin.longitude,
                          &bearing, &distance);
  segment_vector.x = distance * std::sin(bearing * kPi / 180.0);
  segment_vector.y = distance * std::cos(bearing * kPi / 180.0);

  DistanceBearingMercator(waypoint.latitude, waypoint.longitude,
                          vessel.latitude, vessel.longitude, &bearing,
                          &distance);
  vessel_vector.x = distance * std::sin(bearing * kPi / 180.0);
  vessel_vector.y = distance * std::cos(bearing * kPi / 180.0);

  state.cross_track_error =
      vGetLengthOfNormal(&vessel_vector, &segment_vector, &normal);

  vector2D arrival_vector;
  vSubtractVectors(&vessel_vector, &normal, &arrival_vector);
  state.range_to_arrival_normal = vVectorMagnitude(&arrival_vector);
  if (std::isnan(state.range_to_arrival_normal))
    state.range_to_arrival_normal = state.range_to_waypoint;

  double x1 = 0.0;
  double y1 = 0.0;
  double x2 = 0.0;
  double y2 = 0.0;
  toSM(segment_begin.latitude, segment_begin.longitude,
       segment_begin.latitude, segment_begin.longitude, &x1, &y1);
  toSM(waypoint.latitude, waypoint.longitude, segment_begin.latitude,
       segment_begin.longitude, &x2, &y2);
  state.segment_course = std::atan2(x2 - x1, y2 - y1) * 180.0 / kPi;
  if (state.segment_course < 0.0) state.segment_course += 360.0;

  const double normal_angle = std::atan(normal.y / normal.x);
  state.course_to_segment = normal.x > 0.0
                                ? 90.0 - normal_angle * 180.0 / kPi
                                : 270.0 - normal_angle * 180.0 / kPi;

  double direction_angle =
      state.bearing_to_waypoint - state.course_to_segment;
  if (direction_angle < 0.0) direction_angle += 360.0;
  state.cross_track_direction = direction_angle > 180.0 ? 1 : -1;

  return state;
}
