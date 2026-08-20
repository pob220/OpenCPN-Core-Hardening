#include <limits>

#include <gtest/gtest.h>

#include "model/plugin_comm.h"

TEST(PluginComm, ParsesNumericWmmVariation) {
  const auto value = ParseWmmVariation(R"({"Decl":-6.17265445})");
  ASSERT_TRUE(value.has_value());
  EXPECT_DOUBLE_EQ(*value, -6.17265445);
}

TEST(PluginComm, RetainsLegacyStringWmmVariationCompatibility) {
  const auto value = ParseWmmVariation(R"({"Decl":"12.5"})");
  ASSERT_TRUE(value.has_value());
  EXPECT_DOUBLE_EQ(*value, 12.5);
}

TEST(PluginComm, RejectsInvalidWmmVariationWithoutThrowing) {
  EXPECT_FALSE(ParseWmmVariation(R"({"Decl":null})").has_value());
  EXPECT_FALSE(ParseWmmVariation(R"({"Decl":"12.5 degrees"})").has_value());
  EXPECT_FALSE(ParseWmmVariation(R"({"Decl":"NaN"})").has_value());
  EXPECT_FALSE(ParseWmmVariation(R"({"Decl":)").has_value());
}
