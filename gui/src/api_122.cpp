/**************************************************************************
 *   Copyright (C) 2024 by David S. Register                               *
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
 *   along with this program; if not, write to the                         *
 *   along with this program; if not, see <https://www.gnu.org/licenses/>. *
 **************************************************************************/

/**
 * \file
 *
 * ocpn_plugin.h HostApi122 implementation
 */

#include <wx/app.h>

#include <algorithm>
#include <climits>
#include <cmath>

#include "ocpn_plugin.h"

#include "ocpn-nlohmann/json.hpp"
#include "observable/observable.h"

#include "model/comm_navmsg.h"

// FIXME (leamas) find new home.
std::unique_ptr<HostApi> GetHostApi() {
  auto impl = dynamic_cast<Api122Impl*>(wxTheApp);
  assert(impl && "wxTheApp does not implement Api122Impl");
  return std::make_unique<HostApi122>(HostApi122(impl));
}

void HostApi122::RegisterApiEventCallback(
    const std::string& plugin_name, std::function<void(EventType)> callback) {
  auto impl = dynamic_cast<Api122Impl*>(wxTheApp);
  assert(impl && "wxTheApp does not implement Api122Impl");
  impl->RegisterApiEventCallback(plugin_name, callback);
}

std::string HostApi122::GetSignalkPayload(ObservedEvt ev) {
  auto msg = obs::UnpackEvtPointer<SignalkMsg>(ev);
  nlohmann::json root;
  root["Data"] = msg->raw_message;
  root["Context"] = msg->context;
  root["ContextSelf"] = msg->context_self;
  return root.dump();
}

std::vector<HostApi122::TideStationInfo> HostApi122::GetNearestTideStations(
    double latitude, double longitude, double maximum_distance_nm,
    std::size_t maximum_results) {
  std::vector<TideStationInfo> result;
  if (maximum_results == 0) return result;
  const auto maximum_count =
      std::min<std::size_t>(maximum_results, static_cast<std::size_t>(INT_MAX));
  std::vector<PlugIn_TideStationInfoV1> raw(maximum_count);
  for (auto& item : raw) item.struct_size = sizeof(item);
  const int count = PlugIn_GetNearestTideStationsV1(
      latitude, longitude, maximum_distance_nm, raw.data(),
      static_cast<int>(maximum_count));
  result.reserve(std::max(0, count));
  for (int index = 0; index < count; ++index) {
    const auto& item = raw[index];
    TideStationInfo station;
    station.index = item.index;
    station.stable_id = item.stable_id;
    station.display_name = item.name;
    station.latitude = item.lat;
    station.longitude = item.lon;
    station.distance_nm = item.distance_nm;
    station.subordinate = item.subordinate;
    station.reference_name = item.reference_name;
    station.station_id_context = item.station_id_context;
    station.station_id = item.station_id;
    station.source_dataset_id = item.source_dataset_id;
    station.source_dataset_name = item.source_dataset_name;
    station.source_dataset_version = item.source_dataset_version;
    station.source_description = item.source_description;
    station.level_units = item.level_units;
    station.vertical_datum.name = item.datum_name;
    station.vertical_datum.equivalence_key = item.datum_equivalence_key;
    station.vertical_datum.status =
        static_cast<TideDatumStatus>(item.datum_status);
    station.vertical_datum.approximate = item.datum_approximate;
    if (std::isfinite(item.datum_offset_m))
      station.vertical_datum.datum_offset_m = item.datum_offset_m;
    result.push_back(std::move(station));
  }
  return result;
}

std::optional<double> HostApi122::GetTideHeightMeters(int station_index,
                                                      time_t time) {
  double height_m = 0.0;
  if (!PlugIn_GetTideHeightMetersV1(station_index, time, &height_m))
    return std::nullopt;
  return height_m;
}
