/**************************************************************************
 *   Copyright (C) 2026 OpenCPN contributors                              *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 **************************************************************************/

#ifndef AIS_SAFETY_MESSAGE_H_
#define AIS_SAFETY_MESSAGE_H_

#include <ctime>
#include <string>

/** A decoded AIS message 14 safety-related broadcast. */
struct AisSafetyMessage {
  unsigned source_mmsi = 0;
  std::string text;
  std::time_t received_at = 0;
  bool source_was_known = false;
};

#endif  // AIS_SAFETY_MESSAGE_H_
