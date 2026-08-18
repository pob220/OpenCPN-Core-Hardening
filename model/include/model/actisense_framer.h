/***************************************************************************
 *   Copyright (C) 2026 OpenCPN contributors                              *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#ifndef ACTISENSE_FRAMER_H_
#define ACTISENSE_FRAMER_H_

#include <cstddef>
#include <cstdint>
#include <vector>

namespace actisense {

constexpr std::uint8_t kEscape = 0x10;
constexpr std::uint8_t kStartOfText = 0x02;
constexpr std::uint8_t kEndOfText = 0x03;

enum class FeedStatus { kIncomplete, kFrame, kRejected };

struct FeedResult {
  FeedStatus status = FeedStatus::kIncomplete;
  std::vector<std::uint8_t> frame;
};

enum class FrameError { kNone, kTooShort, kLengthMismatch, kBadChecksum };

/** Validate the length byte and additive checksum of an unescaped frame. */
FrameError ValidateFrame(const std::vector<std::uint8_t> &frame);

/**
 * Incremental DLE/STX ... DLE/ETX stream decoder.
 *
 * The returned frame excludes framing bytes and contains unescaped data.
 * Malformed and oversized frames are rejected and the decoder resynchronizes
 * at the next DLE/STX sequence.
 */
class Framer {
public:
  explicit Framer(std::size_t max_frame_size = 4096)
      : m_max_frame_size(max_frame_size) {}

  FeedResult Feed(std::uint8_t byte);
  void Reset();

private:
  enum class State { kWaiting, kWaitingEscape, kFrame, kFrameEscape };

  FeedResult Append(std::uint8_t byte);

  const std::size_t m_max_frame_size;
  State m_state = State::kWaiting;
  std::vector<std::uint8_t> m_frame;
};

}  // namespace actisense

#endif  // ACTISENSE_FRAMER_H_
