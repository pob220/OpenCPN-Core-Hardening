/***************************************************************************
 * Copyright (C) 2026 OpenCPN contributors
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 ***************************************************************************/

#ifndef MODEL_RENDERER_CONFIG_H_
#define MODEL_RENDERER_CONFIG_H_

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

enum class RendererBackend {
  Software,
  OpenGLLegacy,
  VulkanExperimental,
};

enum class RendererAdapterPolicy {
  IntelIntegrated,
  CpuSoftware,
  NvidiaExperimental,
};

struct RendererResourceLimits {
  uint32_t texture_cache_mb = 256;
  uint32_t max_upload_mb_per_frame = 32;
  uint32_t max_frames_in_flight = 1;
  uint32_t max_geometry_batch_mb = 16;
  uint32_t startup_timeout_ms = 3000;
  uint32_t device_wait_timeout_ms = 1000;
};

struct RendererConfig {
  RendererBackend backend = RendererBackend::Software;
  RendererAdapterPolicy adapter_policy = RendererAdapterPolicy::IntelIntegrated;
  RendererBackend fallback_backend = RendererBackend::Software;
  bool automatic_restart = false;
  bool diagnostics = false;
  RendererResourceLimits limits;
};

struct RendererAdapterInfo {
  std::string name;
  uint32_t vendor_id = 0;
  uint32_t device_id = 0;
  bool integrated = false;
  bool cpu = false;
  bool supports_presentation = false;
};

struct RendererSelectionResult {
  std::optional<size_t> adapter_index;
  std::string reason;
};

struct RendererRuntimeInfo {
  RendererBackend active_backend = RendererBackend::Software;
  std::string adapter_name = "Software renderer";
  uint32_t vendor_id = 0;
  uint32_t device_id = 0;
  std::string device_type = "CPU";
  std::string driver = "wxWidgets software rendering";
  std::string presentation_mode = "software blit";
  std::string health = "Ready";
  bool fallback_active = false;
  uint64_t frames_presented = 0;
  uint64_t presentation_failures = 0;
  uint64_t device_losses = 0;
  uint64_t renderer_restarts = 0;
  double last_frame_ms = 0.0;
};

enum class RendererFailureKind {
  DeviceCreation,
  UnsupportedAdapter,
  SurfaceLost,
  DeviceLost,
  OutOfMemory,
  PipelineCreation,
  StartupTimeout,
  RendererProcessExit,
  InvalidGeometry,
};

enum class RendererFailureAction {
  RetrySurfaceOnce,
  RestartRendererOnce,
  FallBackToSoftware,
};

const char* RendererBackendName(RendererBackend backend);
const char* RendererAdapterPolicyName(RendererAdapterPolicy policy);
std::optional<RendererBackend> ParseRendererBackend(const std::string& value);
std::optional<RendererAdapterPolicy> ParseRendererAdapterPolicy(
    const std::string& value);

RendererConfig MigrateRendererConfig(bool legacy_opengl_enabled,
                                     bool renderer_key_present,
                                     const RendererConfig& stored);

RendererSelectionResult SelectRendererAdapter(
    const RendererConfig& config,
    const std::vector<RendererAdapterInfo>& adapters);

RendererResourceLimits ClampRendererResourceLimits(
    RendererResourceLimits limits);

RendererFailureAction ChooseRendererFailureAction(RendererFailureKind failure,
                                                  const RendererConfig& config,
                                                  uint32_t attempts);

bool RendererUploadWithinLimits(uint32_t width, uint32_t height,
                                uint32_t bytes_per_pixel,
                                const RendererResourceLimits& limits);

extern RendererConfig g_renderer_config;
extern RendererBackend g_renderer_runtime_backend;
extern RendererRuntimeInfo g_renderer_runtime_info;

#endif  // MODEL_RENDERER_CONFIG_H_
