/***************************************************************************
 *   Copyright (C) 2026 OpenCPN contributors                              *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#include <chrono>
#include <future>

#include <gtest/gtest.h>

#include "gui/async_worker_lifecycle.h"

using namespace std::chrono_literals;

TEST(AsyncWorkerLifecycle, ShutdownWaitsUntilBorrowedStateIsReleased) {
  AsyncWorkerLifecycle lifecycle;
  ASSERT_TRUE(lifecycle.TryStart());
  ASSERT_EQ(lifecycle.ActiveWorkers(), 1U);

  lifecycle.BeginShutdown();
  EXPECT_TRUE(lifecycle.IsShuttingDown());
  EXPECT_FALSE(lifecycle.TryStart());

  auto shutdown = std::async(std::launch::async,
                             [&] { lifecycle.WaitForWorkers(); });
  EXPECT_EQ(shutdown.wait_for(20ms), std::future_status::timeout);

  lifecycle.Finish();
  EXPECT_EQ(shutdown.wait_for(1s), std::future_status::ready);
  EXPECT_EQ(lifecycle.ActiveWorkers(), 0U);
}

TEST(AsyncWorkerLifecycle, ShutdownWithoutWorkersReturnsImmediately) {
  AsyncWorkerLifecycle lifecycle;
  lifecycle.BeginShutdown();

  auto shutdown = std::async(std::launch::async,
                             [&] { lifecycle.WaitForWorkers(); });
  EXPECT_EQ(shutdown.wait_for(1s), std::future_status::ready);
}
