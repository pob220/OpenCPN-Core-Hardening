#include <gtest/gtest.h>

#include "gui/cm93_dictionary_parser.h"

TEST(Cm93DictionaryParser, AcceptsValidOneByteIdentifiers) {
  int index = 0;

  EXPECT_TRUE(ParseCm93DictionaryRecordIndex("DEPARE|42|A", 3, &index));
  EXPECT_EQ(index, 42);
  EXPECT_TRUE(ParseCm93DictionaryRecordIndex("_m_sor|255|A|0", 3, &index));
  EXPECT_EQ(index, 255);
}

TEST(Cm93DictionaryParser, RejectsBlankCommentAndTruncatedRecords) {
  int index = 73;

  EXPECT_FALSE(ParseCm93DictionaryRecordIndex("", 3, &index));
  EXPECT_FALSE(ParseCm93DictionaryRecordIndex("\r\n", 3, &index));
  EXPECT_FALSE(ParseCm93DictionaryRecordIndex("; comment", 3, &index));
  EXPECT_FALSE(ParseCm93DictionaryRecordIndex("DEPARE", 3, &index));
  EXPECT_FALSE(ParseCm93DictionaryRecordIndex("DEPARE|42", 3, &index));
  EXPECT_EQ(index, 73);
}

TEST(Cm93DictionaryParser, RejectsInvalidOrUnrepresentableIdentifiers) {
  int index = 19;

  EXPECT_FALSE(ParseCm93DictionaryRecordIndex("DEPARE|not-a-number|A", 3,
                                               &index));
  EXPECT_FALSE(ParseCm93DictionaryRecordIndex("DEPARE|-1|A", 3, &index));
  EXPECT_FALSE(ParseCm93DictionaryRecordIndex("DEPARE|0|A", 3, &index));
  EXPECT_FALSE(ParseCm93DictionaryRecordIndex("DEPARE|256|A", 3, &index));
  EXPECT_FALSE(ParseCm93DictionaryRecordIndex(
      "DEPARE|101606342656|A", 3, &index));
  EXPECT_EQ(index, 19);
}
