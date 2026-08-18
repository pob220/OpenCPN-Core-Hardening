/***************************************************************************
 *   Copyright (C) 2025 by David S. Register                               *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, see <https://www.gnu.org/licenses/>. *
 **************************************************************************/

/**
 * \file
 *
 * Autopilot output support
 */

#ifndef _AUTOPILOTOUTPUT_H__
#define _AUTOPILOTOUTPUT_H__

#include <cstdint>
#include <vector>

#include "comm_driver.h"
#include "model/route.h"

namespace autopilot_output {

/** Values used to encode NMEA 2000 PGN 129284 Navigation Data. */
struct Pgn129284Data {
  double distance_to_waypoint_nm = 0.0;
  double eta_time_seconds = 0.0;
  int16_t eta_date_days = 0;
  double bearing_origin_to_destination_degrees = 0.0;
  double bearing_position_to_destination_degrees = 0.0;
  double destination_latitude_degrees = 0.0;
  double destination_longitude_degrees = 0.0;
  double waypoint_closing_velocity_knots = 0.0;
};

/** Encode PGN 129284 using the SI units required on the NMEA 2000 wire. */
std::vector<unsigned char> EncodePgn129284(const Pgn129284Data &data);

}  // namespace autopilot_output

bool UpdateAutopilotN0183(Routeman &routeman);
bool UpdateAutopilotN2K(Routeman &routeman);

/** Send RMC + a faked RMB when there is no active route. */
bool SendNoRouteRmbRmc(Routeman &routeman);

bool SendPGN129283(Routeman &routeman, AbstractCommDriver *driver);
bool SendPGN129284(Routeman &routeman, AbstractCommDriver *driver);
bool SendPGN129285(Routeman &routeman, AbstractCommDriver *driver);

#endif
