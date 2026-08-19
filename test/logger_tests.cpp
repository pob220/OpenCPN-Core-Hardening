/***************************************************************************
 *   Copyright (C) 2026 OpenCPN contributors                              *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#include <chrono>

#include <gtest/gtest.h>
#include <wx/log.h>
#include <wx/string.h>

#include "model/logger.h"

namespace {

class CapturingLog final : public wxLog {
public:
  const wxString& Text() const { return text_; }

protected:
  void DoLogTextAtLevel(wxLogLevel, const wxString& message) override {
    if (!text_.empty()) text_ += '\n';
    text_ += message;
  }

private:
  wxString text_;
};

class ScopedLogTarget final {
public:
  explicit ScopedLogTarget(wxLog* target)
      : previous_(wxLog::SetActiveTarget(target)) {}

  ~ScopedLogTarget() { wxLog::SetActiveTarget(previous_); }

  ScopedLogTarget(const ScopedLogTarget&) = delete;
  ScopedLogTarget& operator=(const ScopedLogTarget&) = delete;

private:
  wxLog* previous_;
};

TEST(LoggerFilters, CountedFilterFormatsUnsignedCount) {
  CapturingLog log;
  ScopedLogTarget target(&log);
  CountedLogFilter filter(2);

  filter.Log("repeated failure");
  EXPECT_TRUE(log.Text().empty());
  filter.Log("repeated failure");

  EXPECT_NE(log.Text().Find("repeated failure"), wxNOT_FOUND);
  EXPECT_NE(log.Text().Find("Previous message suppressed 2 times"),
            wxNOT_FOUND);
}

TEST(LoggerFilters, TimedFilterFormatsUnsignedCount) {
  CapturingLog log;
  ScopedLogTarget target(&log);
  TimedLogFilter filter(std::chrono::seconds(0));

  filter.Log("timed failure");

  EXPECT_NE(log.Text().Find("timed failure"), wxNOT_FOUND);
  EXPECT_NE(log.Text().Find("Previous message suppressed 1 times"),
            wxNOT_FOUND);
}

}  // namespace
