/***************************************************************************
 * Copyright (C) 2026 OpenCPN contributors
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 ***************************************************************************/

#include "model/renderer_config.h"

#include <algorithm>

namespace {
constexpr uint32_t kIntelVendorId = 0x8086;
constexpr uint32_t kNvidiaVendorId = 0x10de;
}  // namespace

RendererConfig g_renderer_config;
RendererBackend g_renderer_runtime_backend = RendererBackend::Software;
RendererRuntimeInfo g_renderer_runtime_info;

const char* RendererBackendName(RendererBackend backend) {
  switch (backend) {
    case RendererBackend::Software:
      return "software";
    case RendererBackend::OpenGLLegacy:
      return "opengl-legacy";
    case RendererBackend::VulkanExperimental:
      return "vulkan-experimental";
  }
  return "software";
}

const char* RendererAdapterPolicyName(RendererAdapterPolicy policy) {
  switch (policy) {
    case RendererAdapterPolicy::IntelIntegrated:
      return "intel-integrated";
    case RendererAdapterPolicy::CpuSoftware:
      return "cpu-software";
    case RendererAdapterPolicy::NvidiaExperimental:
      return "nvidia-experimental";
  }
  return "intel-integrated";
}

std::optional<RendererBackend> ParseRendererBackend(const std::string& value) {
  if (value == "software") return RendererBackend::Software;
  if (value == "opengl" || value == "opengl-legacy")
    return RendererBackend::OpenGLLegacy;
  if (value == "vulkan" || value == "vulkan-experimental")
    return RendererBackend::VulkanExperimental;
  return std::nullopt;
}

std::optional<RendererAdapterPolicy> ParseRendererAdapterPolicy(
    const std::string& value) {
  if (value == "intel" || value == "intel-integrated")
    return RendererAdapterPolicy::IntelIntegrated;
  if (value == "cpu" || value == "cpu-software")
    return RendererAdapterPolicy::CpuSoftware;
  if (value == "nvidia" || value == "nvidia-experimental")
    return RendererAdapterPolicy::NvidiaExperimental;
  return std::nullopt;
}

RendererConfig MigrateRendererConfig(bool legacy_opengl_enabled,
                                     bool renderer_key_present,
                                     const RendererConfig& stored) {
  if (renderer_key_present) return stored;
  RendererConfig migrated = stored;
  migrated.backend = legacy_opengl_enabled ? RendererBackend::OpenGLLegacy
                                           : RendererBackend::Software;
  migrated.fallback_backend = RendererBackend::Software;
  return migrated;
}

RendererSelectionResult SelectRendererAdapter(
    const RendererConfig& config,
    const std::vector<RendererAdapterInfo>& adapters) {
  auto find = [&](auto predicate) -> std::optional<size_t> {
    for (size_t i = 0; i < adapters.size(); ++i) {
      if (adapters[i].supports_presentation && predicate(adapters[i])) return i;
    }
    return std::nullopt;
  };

  switch (config.adapter_policy) {
    case RendererAdapterPolicy::IntelIntegrated: {
      auto index = find([](const RendererAdapterInfo& adapter) {
        return adapter.vendor_id == kIntelVendorId && adapter.integrated;
      });
      return {index, index ? "selected Intel integrated adapter"
                           : "no present-capable Intel integrated adapter"};
    }
    case RendererAdapterPolicy::CpuSoftware: {
      auto index =
          find([](const RendererAdapterInfo& adapter) { return adapter.cpu; });
      return {index, index ? "selected CPU Vulkan adapter"
                           : "no present-capable CPU Vulkan adapter"};
    }
    case RendererAdapterPolicy::NvidiaExperimental: {
      auto index = find([](const RendererAdapterInfo& adapter) {
        return adapter.vendor_id == kNvidiaVendorId;
      });
      return {index, index ? "selected explicitly requested NVIDIA adapter"
                           : "no present-capable NVIDIA adapter"};
    }
  }
  return {std::nullopt, "unsupported adapter policy"};
}

RendererResourceLimits ClampRendererResourceLimits(
    RendererResourceLimits limits) {
  limits.texture_cache_mb = std::clamp(limits.texture_cache_mb, 32u, 2048u);
  limits.max_upload_mb_per_frame =
      std::clamp(limits.max_upload_mb_per_frame, 1u, 256u);
  limits.max_frames_in_flight = std::clamp(limits.max_frames_in_flight, 1u, 3u);
  limits.max_geometry_batch_mb =
      std::clamp(limits.max_geometry_batch_mb, 1u, 128u);
  limits.startup_timeout_ms =
      std::clamp(limits.startup_timeout_ms, 250u, 15000u);
  limits.device_wait_timeout_ms =
      std::clamp(limits.device_wait_timeout_ms, 100u, 5000u);
  return limits;
}

RendererFailureAction ChooseRendererFailureAction(RendererFailureKind failure,
                                                  const RendererConfig& config,
                                                  uint32_t attempts) {
  if (failure == RendererFailureKind::SurfaceLost && attempts == 0)
    return RendererFailureAction::RetrySurfaceOnce;
  if (failure == RendererFailureKind::RendererProcessExit &&
      config.automatic_restart && attempts == 0)
    return RendererFailureAction::RestartRendererOnce;
  return RendererFailureAction::FallBackToSoftware;
}

bool RendererUploadWithinLimits(uint32_t width, uint32_t height,
                                uint32_t bytes_per_pixel,
                                const RendererResourceLimits& limits) {
  if (width == 0 || height == 0 || bytes_per_pixel == 0) return false;
  constexpr uint64_t kAbsoluteDimensionLimit = 32768;
  if (width > kAbsoluteDimensionLimit || height > kAbsoluteDimensionLimit)
    return false;
  const uint64_t bytes =
      static_cast<uint64_t>(width) * height * bytes_per_pixel;
  const uint64_t limit =
      static_cast<uint64_t>(limits.max_upload_mb_per_frame) * 1024 * 1024;
  return bytes <= limit;
}
