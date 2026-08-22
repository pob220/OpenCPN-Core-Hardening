/***************************************************************************
 *   Copyright (C) 2026 by the OpenCPN contributors                        *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#ifndef TIDE_STATION_METADATA_H_
#define TIDE_STATION_METADATA_H_

#include <cstddef>
#include <limits>
#include <optional>
#include <string>
#include <vector>

enum class TideDatumStatus { kUnknown = 0, kDeclared = 1, kInherited = 2 };

struct TideDatumDescription {
  std::string name;
  std::string equivalence_key;
  TideDatumStatus status{TideDatumStatus::kUnknown};
  bool approximate{};
};

struct TideStationMetadata {
  int index{};
  std::string stable_id;
  std::string name;
  double latitude{};
  double longitude{};
  bool subordinate{};
  std::string reference_name;
  std::string station_id_context;
  std::string station_id;
  std::string source_dataset_id;
  std::string source_dataset_name;
  std::string source_dataset_version;
  std::string source_description;
  TideDatumDescription vertical_datum;
  double datum_offset_m{std::numeric_limits<double>::quiet_NaN()};
  std::string level_units;
  bool height_in_metres_available{};
};

struct NearbyTideStation {
  TideStationMetadata station;
  double distance_nm{};
};

/** Normalize a libtcd or legacy HARMONIC datum label for exact matching. */
TideDatumDescription DescribeTideDatum(const std::string &name,
                                       TideDatumStatus status);

/** Parse the structured metadata comments used by legacy HARMONIC files. */
void ParseLegacyHarmonicMetadataComment(const std::string &line,
                                        std::string *datum_name,
                                        std::string *source_name);

/** Build a station key which is unique within a dataset and version-stable. */
std::string BuildStableTideStationId(const std::string &dataset_id,
                                     const std::string &station_id_context,
                                     const std::string &station_id,
                                     int source_record_number);

#endif  // TIDE_STATION_METADATA_H_
