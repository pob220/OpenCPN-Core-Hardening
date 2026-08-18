#include <gtest/gtest.h>

#include "model/autopilot_output.h"
#include "N2kMessages.h"
#include "N2kMsg.h"

TEST(AutopilotOutput, Pgn129284EncodesClosingVelocityInMetersPerSecond) {
  autopilot_output::Pgn129284Data data;
  data.distance_to_waypoint_nm = 2.5;
  data.eta_time_seconds = 3600.0;
  data.eta_date_days = 20000;
  data.bearing_origin_to_destination_degrees = 90.0;
  data.bearing_position_to_destination_degrees = 45.0;
  data.destination_latitude_degrees = 50.1234567;
  data.destination_longitude_degrees = -1.2345678;
  data.waypoint_closing_velocity_knots = 10.0;

  const auto payload = autopilot_output::EncodePgn129284(data);
  tN2kMsg message;
  message.SetPGN(129284);
  message.AddBuf(payload.data(), payload.size());

  unsigned char sid = 0;
  double distance_to_waypoint = 0.0;
  tN2kHeadingReference bearing_reference = N2khr_magnetic;
  bool perpendicular_crossed = false;
  bool arrival_circle_entered = false;
  tN2kDistanceCalculationType calculation_type = N2kdct_GreatCircle;
  double eta_time = 0.0;
  int16_t eta_date = 0;
  double bearing_origin_to_destination = 0.0;
  double bearing_position_to_destination = 0.0;
  uint8_t origin_waypoint = 0;
  uint8_t destination_waypoint = 0;
  double destination_latitude = 0.0;
  double destination_longitude = 0.0;
  double waypoint_closing_velocity = 0.0;

  ASSERT_TRUE(ParseN2kPGN129284(
      message, sid, distance_to_waypoint, bearing_reference,
      perpendicular_crossed, arrival_circle_entered, calculation_type, eta_time,
      eta_date, bearing_origin_to_destination, bearing_position_to_destination,
      origin_waypoint, destination_waypoint, destination_latitude,
      destination_longitude, waypoint_closing_velocity));

  EXPECT_NEAR(distance_to_waypoint, 2.5 * 1852.0, 0.01);
  EXPECT_NEAR(waypoint_closing_velocity, 10.0 * 1852.0 / 3600.0, 0.005);
}
