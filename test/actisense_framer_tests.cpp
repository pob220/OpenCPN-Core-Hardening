/***************************************************************************
 *   Copyright (C) 2026 OpenCPN contributors                              *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#include <cstdint>
#include <vector>

#include <gtest/gtest.h>

#include "model/actisense_framer.h"

namespace {

std::vector<std::uint8_t> Wrap(const std::vector<std::uint8_t> &frame) {
  std::vector<std::uint8_t> bytes{actisense::kEscape,
                                  actisense::kStartOfText};
  for (std::uint8_t byte : frame) {
    bytes.push_back(byte);
    if (byte == actisense::kEscape) bytes.push_back(byte);
  }
  bytes.push_back(actisense::kEscape);
  bytes.push_back(actisense::kEndOfText);
  return bytes;
}

actisense::FeedResult FeedAll(actisense::Framer &framer,
                              const std::vector<std::uint8_t> &bytes) {
  actisense::FeedResult result;
  for (std::uint8_t byte : bytes) {
    auto next = framer.Feed(byte);
    if (next.status != actisense::FeedStatus::kIncomplete)
      result = std::move(next);
  }
  return result;
}

}  // namespace

TEST(ActisenseFramer, DecodesEscapedFrameAcrossPartialInput) {
  // A0 + one payload byte (DLE), with a checksum making the byte sum zero.
  const std::vector<std::uint8_t> frame{0xA0, 0x01, 0x10, 0x4F};
  const auto bytes = Wrap(frame);
  actisense::Framer framer;

  for (std::size_t i = 0; i + 1 < bytes.size(); ++i)
    EXPECT_EQ(framer.Feed(bytes[i]).status,
              actisense::FeedStatus::kIncomplete);
  auto result = framer.Feed(bytes.back());

  ASSERT_EQ(result.status, actisense::FeedStatus::kFrame);
  EXPECT_EQ(result.frame, frame);
  EXPECT_EQ(actisense::ValidateFrame(result.frame),
            actisense::FrameError::kNone);
}

TEST(ActisenseFramer, RejectsOversizedFrameAndResynchronizes) {
  actisense::Framer framer(4);
  auto oversized = Wrap({0x93, 0x03, 0x01, 0x02, 0x03, 0x64});
  auto rejected = FeedAll(framer, oversized);
  EXPECT_EQ(rejected.status, actisense::FeedStatus::kRejected);

  const std::vector<std::uint8_t> valid{0xA0, 0x00, 0x60};
  auto decoded = FeedAll(framer, Wrap(valid));
  ASSERT_EQ(decoded.status, actisense::FeedStatus::kFrame);
  EXPECT_EQ(decoded.frame, valid);
}

TEST(ActisenseFramer, RejectsMalformedEscapeAndResynchronizesAtNewStart) {
  actisense::Framer framer;
  const std::vector<std::uint8_t> malformed{
      actisense::kEscape, actisense::kStartOfText, 0x93,
      actisense::kEscape, 0x7F, actisense::kEscape,
      actisense::kStartOfText};
  auto rejected = FeedAll(framer, malformed);
  EXPECT_EQ(rejected.status, actisense::FeedStatus::kRejected);

  const std::vector<std::uint8_t> valid{0xA0, 0x00, 0x60};
  auto remainder = Wrap(valid);
  // The malformed input ended with a new DLE/STX, so feed only frame content
  // and its terminator.
  remainder.erase(remainder.begin(), remainder.begin() + 2);
  auto decoded = FeedAll(framer, remainder);
  ASSERT_EQ(decoded.status, actisense::FeedStatus::kFrame);
  EXPECT_EQ(decoded.frame, valid);
}

TEST(ActisenseFramer, ValidatesDeclaredLengthAndChecksum) {
  EXPECT_EQ(actisense::ValidateFrame({0x93, 0x00, 0x6D}),
            actisense::FrameError::kNone);
  EXPECT_EQ(actisense::ValidateFrame({0x93, 0x01, 0x00}),
            actisense::FrameError::kLengthMismatch);
  EXPECT_EQ(actisense::ValidateFrame({0x93, 0x00, 0x00}),
            actisense::FrameError::kBadChecksum);
  EXPECT_EQ(actisense::ValidateFrame({0x93, 0x00}),
            actisense::FrameError::kTooShort);
}
