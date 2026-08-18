/***************************************************************************
 *   Copyright (C) 2026 OpenCPN contributors                              *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#include "model/actisense_framer.h"

#include <utility>

namespace actisense {

FrameError ValidateFrame(const std::vector<std::uint8_t> &frame) {
  // Message type, payload length and checksum.
  if (frame.size() < 3) return FrameError::kTooShort;
  if (frame.size() != static_cast<std::size_t>(frame[1]) + 3)
    return FrameError::kLengthMismatch;

  std::uint8_t sum = 0;
  for (std::uint8_t byte : frame) sum = static_cast<std::uint8_t>(sum + byte);
  return sum == 0 ? FrameError::kNone : FrameError::kBadChecksum;
}

FeedResult Framer::Append(std::uint8_t byte) {
  if (m_frame.size() >= m_max_frame_size) {
    Reset();
    return {FeedStatus::kRejected, {}};
  }
  m_frame.push_back(byte);
  return {};
}

FeedResult Framer::Feed(std::uint8_t byte) {
  switch (m_state) {
    case State::kWaiting:
      if (byte == kEscape) m_state = State::kWaitingEscape;
      return {};

    case State::kWaitingEscape:
      if (byte == kStartOfText) {
        m_frame.clear();
        m_state = State::kFrame;
      } else if (byte != kEscape) {
        m_state = State::kWaiting;
      }
      return {};

    case State::kFrame:
      if (byte == kEscape) {
        m_state = State::kFrameEscape;
        return {};
      }
      return Append(byte);

    case State::kFrameEscape:
      if (byte == kEscape) {
        m_state = State::kFrame;
        return Append(byte);
      }
      if (byte == kEndOfText) {
        FeedResult result{FeedStatus::kFrame, std::move(m_frame)};
        m_frame.clear();
        m_state = State::kWaiting;
        return result;
      }
      if (byte == kStartOfText) {
        // A new start marker provides an immediate resynchronization point.
        m_frame.clear();
        m_state = State::kFrame;
        return {FeedStatus::kRejected, {}};
      }
      Reset();
      return {FeedStatus::kRejected, {}};
  }
  return {};
}

void Framer::Reset() {
  m_frame.clear();
  m_state = State::kWaiting;
}

}  // namespace actisense
