/***************************************************************************
 *   Copyright (C) 2026 by the OpenCPN contributors                        *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#include "tide_station_metadata.h"

#include <algorithm>
#include <cctype>
#include <sstream>
#include <string_view>

namespace {

std::string Trim(std::string value) {
  const auto whitespace = [](unsigned char c) { return std::isspace(c); };
  value.erase(value.begin(),
              std::find_if_not(value.begin(), value.end(), whitespace));
  value.erase(
      std::find_if_not(value.rbegin(), value.rend(), whitespace).base(),
      value.end());
  return value;
}

std::string Lower(std::string value) {
  std::transform(value.begin(), value.end(), value.begin(),
                 [](unsigned char c) { return std::tolower(c); });
  return value;
}

bool StartsWith(const std::string &value, std::string_view prefix) {
  return value.size() >= prefix.size() &&
         value.compare(0, prefix.size(), prefix) == 0;
}

}  // namespace

TideDatumDescription DescribeTideDatum(const std::string &name,
                                       TideDatumStatus status) {
  TideDatumDescription result;
  result.name = Trim(name);
  result.status = status;
  std::string normalized = Lower(result.name);
  if (normalized.empty() || normalized == "unknown") {
    result.status = TideDatumStatus::kUnknown;
    return result;
  }

  constexpr std::string_view approximate = "approximate level of ";
  if (StartsWith(normalized, approximate)) {
    result.approximate = true;
    normalized = Trim(normalized.substr(approximate.size()));
  }

  if (normalized == "lowest astronomical tide" || normalized == "lat")
    result.equivalence_key = "LAT";
  else if (normalized == "mean lower low water" || normalized == "mllw")
    result.equivalence_key = "MLLW";
  else if (normalized == "lower low water, large tide" ||
           normalized == "llwlt")
    result.equivalence_key = "LLWLT";
  else if (normalized == "mean low water springs" || normalized == "mlws")
    result.equivalence_key = "MLWS";
  else if (normalized == "mean sea level" || normalized == "msl")
    result.equivalence_key = "MSL";
  else if (normalized == "indian spring low water" || normalized == "islw")
    result.equivalence_key = "ISLW";
  else if (normalized == "theoretical lowest tide" || normalized == "tlt")
    result.equivalence_key = "TLT";
  else if (normalized == "admiralty chart datum" ||
           normalized == "admiralty chart datum (acd)" ||
           normalized == "chart datum" || normalized == "acd")
    result.equivalence_key = "CHART_DATUM";
  else if (normalized == "mean tide level" || normalized == "mtl")
    result.equivalence_key = "MTL";
  else if (normalized == "mean water level" || normalized == "mwl")
    result.equivalence_key = "MWL";

  return result;
}

void ParseLegacyHarmonicMetadataComment(const std::string &line,
                                        std::string *datum_name,
                                        std::string *source_name) {
  std::string value = Trim(line);
  if (!value.empty() && value.front() == '#') value = Trim(value.substr(1));
  const std::string lowered = Lower(value);
  if (StartsWith(lowered, "datum:")) {
    if (datum_name) *datum_name = Trim(value.substr(6));
  } else if (StartsWith(lowered, "source:")) {
    if (source_name) *source_name = Trim(value.substr(7));
  } else if (StartsWith(lowered, "datum information:") && datum_name &&
             datum_name->empty()) {
    const std::string detail = Trim(value.substr(18));
    const std::string detail_lower = Lower(detail);
    constexpr std::string_view refers_to = "the data refer to ";
    if (StartsWith(detail_lower, refers_to))
      *datum_name = Trim(detail.substr(refers_to.size()));
  }
}

std::string BuildStableTideStationId(const std::string &dataset_id,
                                     const std::string &station_id_context,
                                     const std::string &station_id,
                                     int source_record_number) {
  std::ostringstream result;
  result << dataset_id << ':';
  if (!station_id_context.empty() || !station_id.empty())
    result << station_id_context << ':' << station_id << ':';
  result << source_record_number;
  return result.str();
}
