#include <gtest/gtest.h>

#include "tide_station_metadata.h"

namespace {

TEST(TideStationMetadata, NormalizesExactAndApproximateDatums) {
  const auto lat = DescribeTideDatum("Lowest Astronomical Tide",
                                     TideDatumStatus::kDeclared);
  EXPECT_EQ(lat.equivalence_key, "LAT");
  EXPECT_FALSE(lat.approximate);
  EXPECT_EQ(lat.status, TideDatumStatus::kDeclared);

  const auto approximate = DescribeTideDatum(
      "Approximate Level of Mean Lower Low Water",
      TideDatumStatus::kDeclared);
  EXPECT_EQ(approximate.equivalence_key, "MLLW");
  EXPECT_TRUE(approximate.approximate);
}

TEST(TideStationMetadata, KeepsUnknownAndDistinctDatumsFailClosed) {
  EXPECT_EQ(DescribeTideDatum("Unknown", TideDatumStatus::kDeclared).status,
            TideDatumStatus::kUnknown);
  EXPECT_EQ(DescribeTideDatum("Admiralty Chart Datum",
                              TideDatumStatus::kDeclared)
                .equivalence_key,
            "CHART_DATUM");
  EXPECT_NE(DescribeTideDatum("Mean Tide Level", TideDatumStatus::kDeclared)
                .equivalence_key,
            "MSL");
}

TEST(TideStationMetadata, ParsesStructuredLegacyComments) {
  std::string datum;
  std::string source;
  ParseLegacyHarmonicMetadataComment(
      "# Datum information: The data refer to Admiralty Chart Datum (ACD)",
      &datum, &source);
  EXPECT_EQ(datum, "Admiralty Chart Datum (ACD)");
  ParseLegacyHarmonicMetadataComment("# Datum:Admiralty Chart Datum", &datum,
                                     &source);
  ParseLegacyHarmonicMetadataComment(
      "# Source:Derived from BODC data with Harmgen 2.2", &datum, &source);
  EXPECT_EQ(datum, "Admiralty Chart Datum");
  EXPECT_EQ(source, "Derived from BODC data with Harmgen 2.2");
}

TEST(TideStationMetadata, StableIdDisambiguatesReusedAuthorityIds) {
  const auto first =
      BuildStableTideStationId("catalogue.tcd", "NOS", "8410714", 42);
  const auto second =
      BuildStableTideStationId("catalogue.tcd", "NOS", "8410714", 43);
  EXPECT_EQ(first, "catalogue.tcd:NOS:8410714:42");
  EXPECT_EQ(second, "catalogue.tcd:NOS:8410714:43");
  EXPECT_NE(first, second);
  EXPECT_EQ(BuildStableTideStationId("legacy.IDX", "", "", 7),
            "legacy.IDX:7");
}

}  // namespace
