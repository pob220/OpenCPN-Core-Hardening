/***************************************************************************
 *   Copyright (C) 2013 by David S. Register                               *
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
 * Implement station_data.h -- amplitude measurement container
 */

#include <stdlib.h>
#include <string.h>

#include "station_data.h"

#include <wx/arrimpl.cpp>

WX_DEFINE_OBJARRAY(ArrayOfStationData);

Station_Data::Station_Data() {
  station_name = NULL;
  amplitude = NULL;
  epoch = NULL;
  DATUM = 0.0;
  meridian = 0;
  zone_offset = 0.0;
  memset(tzfile, 0, sizeof(tzfile));
  memset(unit, 0, sizeof(unit));
  memset(units_conv, 0, sizeof(units_conv));
  memset(units_abbrv, 0, sizeof(units_abbrv));
  have_BOGUS = 0;
  memset(datum_name, 0, sizeof(datum_name));
  memset(datum_equivalence_key, 0, sizeof(datum_equivalence_key));
  memset(source_name, 0, sizeof(source_name));
  datum_status = 0;
  datum_approximate = false;
}

Station_Data::~Station_Data() {
  free(station_name);
  free(amplitude);
  free(epoch);
}
