/***************************************************************************
 * Copyright (C) 2026 OpenCPN contributors
 ***************************************************************************/

#include "model/external_control.h"

#include <algorithm>
#include <condition_variable>
#include <exception>
#include <thread>
#include <unordered_map>

namespace ocpn::control {

BoundedApplicationEventStream::BoundedApplicationEventStream(
    std::size_t capacity)
    : capacity_(std::max<std::size_t>(1, capacity)) {}

std::uint64_t BoundedApplicationEventStream::Publish(ApplicationEvent event) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (closed_) return 0;
  event.sequence = next_sequence_++;
  event.timestamp =
      event.timestamp == Clock::time_point{} ? Clock::now() : event.timestamp;
  // Navigation updates are snapshots. Coalescing an unread tail update keeps
  // high-rate instrument traffic bounded without reordering semantic events.
  if (event.type == ApplicationEventType::Navigation && !events_.empty() &&
      events_.back().type == ApplicationEventType::Navigation) {
    events_.back() = event;
  } else {
    events_.push_back(event);
  }
  while (events_.size() > capacity_) events_.pop_front();
  return event.sequence;
}

ApplicationEventBatch BoundedApplicationEventStream::ReadAfter(
    std::uint64_t sequence, std::size_t maximum) const {
  std::lock_guard<std::mutex> lock(mutex_);
  ApplicationEventBatch result;
  result.latest_sequence = next_sequence_ - 1;
  result.oldest_available_sequence =
      events_.empty() ? next_sequence_ : events_.front().sequence;
  result.gap = !events_.empty() && sequence != 0 &&
               sequence + 1 < result.oldest_available_sequence;
  if (maximum == 0) return result;
  for (const auto& event : events_) {
    if (event.sequence <= sequence) continue;
    result.events.push_back(event);
    if (result.events.size() == maximum) break;
  }
  return result;
}

std::uint64_t BoundedApplicationEventStream::LatestSequence() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return next_sequence_ - 1;
}

void BoundedApplicationEventStream::Close() {
  std::lock_guard<std::mutex> lock(mutex_);
  closed_ = true;
  events_.clear();
}

namespace {

bool Terminal(PlanningJobState state) {
  return state == PlanningJobState::Completed ||
         state == PlanningJobState::Failed ||
         state == PlanningJobState::Cancelled;
}

class AtomicPlanningCancellation final : public PlanningCancellation {
public:
  bool IsCancellationRequested() const override {
    return requested.load(std::memory_order_relaxed);
  }
  std::atomic_bool requested{false};
};

}  // namespace

class InProcessPlanningJobService::Impl {
public:
  struct Job {
    PlanningRequest request;
    PlanningJobSnapshot snapshot;
    std::shared_ptr<PlanningProvider> provider;
    std::shared_ptr<AtomicPlanningCancellation> cancellation;
    std::optional<PlanningResult> result;
  };

  Impl(std::shared_ptr<ApplicationEventStream> event_stream,
       std::size_t worker_count, std::size_t maximum_jobs)
      : events(std::move(event_stream)),
        maximum_jobs(std::max<std::size_t>(1, maximum_jobs)) {
    for (std::size_t index = 0; index < std::max<std::size_t>(1, worker_count);
         ++index)
      workers.emplace_back([this] { WorkerLoop(); });
  }

  ~Impl() { Shutdown(); }

  void Publish(const std::string& id) const {
    if (events) events->Publish({0, {}, ApplicationEventType::PlanningJob, id});
  }

  void WorkerLoop() {
    while (true) {
      std::shared_ptr<Job> job;
      bool cancelled = false;
      {
        std::unique_lock<std::mutex> lock(mutex);
        changed.wait(lock, [&] { return stopping || !queue.empty(); });
        if (stopping && queue.empty()) return;
        job = queue.front();
        queue.pop_front();
        if (job->cancellation->IsCancellationRequested()) {
          job->snapshot.state = PlanningJobState::Cancelled;
          job->snapshot.updated_time = Clock::now();
          cancelled = true;
        } else {
          job->snapshot.state = PlanningJobState::Running;
          job->snapshot.updated_time = Clock::now();
        }
      }
      Publish(job->snapshot.id);
      if (cancelled) continue;

      Result<PlanningResult> result;
      try {
        result = job->provider->Run(
            job->request, *job->cancellation,
            [this, weak_job = std::weak_ptr<Job>(job)](double progress) {
              auto current = weak_job.lock();
              if (!current) return;
              bool publish = false;
              {
                std::lock_guard<std::mutex> lock(mutex);
                if (current->snapshot.state != PlanningJobState::Running)
                  return;
                const double bounded = std::max(0.0, std::min(1.0, progress));
                if (bounded >= current->snapshot.progress + 0.01 ||
                    bounded == 1.0) {
                  current->snapshot.progress =
                      std::max(current->snapshot.progress, bounded);
                  current->snapshot.updated_time = Clock::now();
                  publish = true;
                }
              }
              if (publish) Publish(current->snapshot.id);
            });
      } catch (const std::exception& error) {
        result = Result<PlanningResult>::FromError("provider_exception",
                                                   error.what());
      } catch (...) {
        result = Result<PlanningResult>::FromError(
            "provider_exception",
            "Planning provider threw an unknown exception");
      }

      {
        std::lock_guard<std::mutex> lock(mutex);
        job->snapshot.updated_time = Clock::now();
        if (job->cancellation->IsCancellationRequested()) {
          job->snapshot.state = PlanningJobState::Cancelled;
          job->snapshot.cancellation_requested = true;
        } else if (result.error) {
          job->snapshot.state = PlanningJobState::Failed;
          job->snapshot.error = result.error;
        } else {
          job->snapshot.state = PlanningJobState::Completed;
          job->snapshot.progress = 1.0;
          job->result = std::move(result.value);
        }
      }
      Publish(job->snapshot.id);
    }
  }

  void Shutdown() {
    {
      std::lock_guard<std::mutex> lock(mutex);
      if (stopping) return;
      stopping = true;
      for (const auto& [id, job] : jobs) {
        if (!Terminal(job->snapshot.state)) {
          job->cancellation->requested.store(true, std::memory_order_relaxed);
          job->snapshot.cancellation_requested = true;
        }
      }
    }
    changed.notify_all();
    for (auto& worker : workers)
      if (worker.joinable()) worker.join();
    workers.clear();
  }

  mutable std::mutex mutex;
  std::condition_variable changed;
  std::unordered_map<std::string, std::shared_ptr<PlanningProvider>> providers;
  std::unordered_map<std::string, std::shared_ptr<Job>> jobs;
  std::deque<std::shared_ptr<Job>> queue;
  std::vector<std::thread> workers;
  std::shared_ptr<ApplicationEventStream> events;
  const std::size_t maximum_jobs;
  std::uint64_t next_id = 1;
  bool stopping = false;
};

InProcessPlanningJobService::InProcessPlanningJobService(
    std::shared_ptr<ApplicationEventStream> events, std::size_t worker_count,
    std::size_t maximum_jobs)
    : impl_(std::make_unique<Impl>(std::move(events), worker_count,
                                   maximum_jobs)) {}

InProcessPlanningJobService::~InProcessPlanningJobService() { Shutdown(); }

bool InProcessPlanningJobService::RegisterProvider(
    std::shared_ptr<PlanningProvider> provider) {
  if (!provider || provider->Capability().empty()) return false;
  std::lock_guard<std::mutex> lock(impl_->mutex);
  if (impl_->stopping) return false;
  return impl_->providers.emplace(provider->Capability(), std::move(provider))
      .second;
}

bool InProcessPlanningJobService::UnregisterProvider(
    const std::string& capability) {
  std::vector<std::string> cancelled;
  bool active = false;
  {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    // Stop accepting work before asking in-flight callbacks to stop. Each job
    // owns a shared provider reference, so the adapter remains valid until Run
    // returns even though it is no longer discoverable.
    impl_->providers.erase(capability);
    for (const auto& [id, job] : impl_->jobs) {
      if (job->snapshot.provider_capability != capability ||
          Terminal(job->snapshot.state))
        continue;
      active = true;
      job->cancellation->requested.store(true, std::memory_order_relaxed);
      job->snapshot.cancellation_requested = true;
      job->snapshot.updated_time = Clock::now();
      cancelled.push_back(id);
    }
  }
  for (const auto& id : cancelled) impl_->Publish(id);
  return !active;
}

std::vector<std::string> InProcessPlanningJobService::ProviderCapabilities()
    const {
  std::lock_guard<std::mutex> lock(impl_->mutex);
  std::vector<std::string> result;
  for (const auto& [capability, provider] : impl_->providers)
    result.push_back(capability);
  std::sort(result.begin(), result.end());
  return result;
}

Result<PlanningJobSnapshot> InProcessPlanningJobService::Submit(
    const PlanningRequest& request, const std::string& owner_id) {
  std::shared_ptr<Impl::Job> job;
  PlanningJobSnapshot submitted_snapshot;
  {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    if (impl_->stopping)
      return Result<PlanningJobSnapshot>::FromError(
          "not_ready", "Planning job service is shutting down");
    const auto provider = impl_->providers.find(request.provider_capability);
    if (provider == impl_->providers.end())
      return Result<PlanningJobSnapshot>::FromError(
          "provider_unavailable",
          "Requested planning capability is unavailable");
    if (impl_->jobs.size() >= impl_->maximum_jobs) {
      auto oldest = impl_->jobs.end();
      for (auto candidate = impl_->jobs.begin(); candidate != impl_->jobs.end();
           ++candidate) {
        if (!Terminal(candidate->second->snapshot.state)) continue;
        if (oldest == impl_->jobs.end() ||
            candidate->second->snapshot.updated_time <
                oldest->second->snapshot.updated_time)
          oldest = candidate;
      }
      if (oldest != impl_->jobs.end()) impl_->jobs.erase(oldest);
    }
    if (impl_->jobs.size() >= impl_->maximum_jobs)
      return Result<PlanningJobSnapshot>::FromError(
          "resource_limit", "Maximum retained planning jobs reached");
    job = std::make_shared<Impl::Job>();
    job->request = request;
    job->provider = provider->second;
    job->cancellation = std::make_shared<AtomicPlanningCancellation>();
    job->snapshot.id = "plan-" + std::to_string(impl_->next_id++);
    job->snapshot.owner_id = owner_id;
    job->snapshot.provider_capability = request.provider_capability;
    job->snapshot.submitted_time = Clock::now();
    job->snapshot.updated_time = job->snapshot.submitted_time;
    impl_->jobs[job->snapshot.id] = job;
    impl_->queue.push_back(job);
    submitted_snapshot = job->snapshot;
  }
  impl_->changed.notify_one();
  impl_->Publish(job->snapshot.id);
  return Result<PlanningJobSnapshot>::FromValue(std::move(submitted_snapshot));
}

Result<PlanningJobSnapshot> InProcessPlanningJobService::Get(
    const std::string& id, const std::string& owner_id) const {
  std::lock_guard<std::mutex> lock(impl_->mutex);
  const auto found = impl_->jobs.find(id);
  if (found == impl_->jobs.end() ||
      found->second->snapshot.owner_id != owner_id)
    return Result<PlanningJobSnapshot>::FromError("not_found",
                                                  "Planning job not found");
  return Result<PlanningJobSnapshot>::FromValue(found->second->snapshot);
}

Result<PlanningJobSnapshot> InProcessPlanningJobService::Cancel(
    const std::string& id, const std::string& owner_id) {
  PlanningJobSnapshot snapshot;
  {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    const auto found = impl_->jobs.find(id);
    if (found == impl_->jobs.end() ||
        found->second->snapshot.owner_id != owner_id)
      return Result<PlanningJobSnapshot>::FromError("not_found",
                                                    "Planning job not found");
    auto& job = *found->second;
    if (!Terminal(job.snapshot.state)) {
      job.cancellation->requested.store(true, std::memory_order_relaxed);
      job.snapshot.cancellation_requested = true;
      job.snapshot.updated_time = Clock::now();
      if (job.snapshot.state == PlanningJobState::Queued)
        job.snapshot.state = PlanningJobState::Cancelled;
    }
    snapshot = job.snapshot;
  }
  impl_->Publish(id);
  return Result<PlanningJobSnapshot>::FromValue(std::move(snapshot));
}

Result<PlanningResult> InProcessPlanningJobService::GetResult(
    const std::string& id, const std::string& owner_id) const {
  std::lock_guard<std::mutex> lock(impl_->mutex);
  const auto found = impl_->jobs.find(id);
  if (found == impl_->jobs.end() ||
      found->second->snapshot.owner_id != owner_id)
    return Result<PlanningResult>::FromError("not_found",
                                             "Planning job not found");
  const auto& job = *found->second;
  if (job.snapshot.state == PlanningJobState::Failed && job.snapshot.error)
    return Result<PlanningResult>{std::nullopt, job.snapshot.error};
  if (job.snapshot.state == PlanningJobState::Cancelled)
    return Result<PlanningResult>::FromError("cancelled",
                                             "Planning job was cancelled");
  if (!job.result)
    return Result<PlanningResult>::FromError("result_not_ready",
                                             "Planning result is not complete");
  return Result<PlanningResult>::FromValue(*job.result);
}

void InProcessPlanningJobService::Shutdown() {
  if (impl_) impl_->Shutdown();
}

}  // namespace ocpn::control
