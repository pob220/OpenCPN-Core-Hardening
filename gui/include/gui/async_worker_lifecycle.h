/***************************************************************************
 *   Copyright (C) 2026 OpenCPN contributors                              *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#ifndef ASYNC_WORKER_LIFECYCLE_H_
#define ASYNC_WORKER_LIFECYCLE_H_

#include <cassert>
#include <condition_variable>
#include <cstddef>
#include <mutex>

/**
 * Tracks detached workers which borrow state owned by another object.
 *
 * The owner calls TryStart() immediately before starting each worker. Each
 * accepted worker must call Finish() as its final access to the owner. During
 * destruction the owner first calls BeginShutdown(), preventing new workers,
 * and then WaitForWorkers() before releasing any borrowed state.
 */
class AsyncWorkerLifecycle {
public:
  AsyncWorkerLifecycle() = default;
  AsyncWorkerLifecycle(const AsyncWorkerLifecycle &) = delete;
  AsyncWorkerLifecycle &operator=(const AsyncWorkerLifecycle &) = delete;

  bool TryStart() {
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_shutting_down) return false;
    ++m_active_workers;
    return true;
  }

  void Finish() {
    std::lock_guard<std::mutex> lock(m_mutex);
    assert(m_active_workers > 0);
    --m_active_workers;
    if (m_active_workers == 0) m_workers_done.notify_all();
  }

  void BeginShutdown() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_shutting_down = true;
  }

  void WaitForWorkers() {
    std::unique_lock<std::mutex> lock(m_mutex);
    m_workers_done.wait(lock, [this] { return m_active_workers == 0; });
  }

  bool IsShuttingDown() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_shutting_down;
  }

  std::size_t ActiveWorkers() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_active_workers;
  }

private:
  mutable std::mutex m_mutex;
  std::condition_variable m_workers_done;
  std::size_t m_active_workers = 0;
  bool m_shutting_down = false;
};

#endif  // ASYNC_WORKER_LIFECYCLE_H_
