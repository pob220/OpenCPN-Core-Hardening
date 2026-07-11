#include <gtest/gtest.h>

#include "model/renderer_config.h"

namespace {
RendererAdapterInfo Adapter(const char* name, uint32_t vendor, bool integrated,
                            bool cpu, bool present = true) {
  RendererAdapterInfo adapter;
  adapter.name = name;
  adapter.vendor_id = vendor;
  adapter.integrated = integrated;
  adapter.cpu = cpu;
  adapter.supports_presentation = present;
  return adapter;
}
}  // namespace

TEST(RendererConfig, MigratesLegacyOpenGlWithoutChangingExplicitConfig) {
  RendererConfig stored;
  stored.backend = RendererBackend::VulkanExperimental;
  EXPECT_EQ(RendererBackend::OpenGLLegacy,
            MigrateRendererConfig(true, false, stored).backend);
  EXPECT_EQ(RendererBackend::Software,
            MigrateRendererConfig(false, false, stored).backend);
  EXPECT_EQ(RendererBackend::VulkanExperimental,
            MigrateRendererConfig(true, true, stored).backend);
}

TEST(RendererConfig, IntelPolicyNeverFallsThroughToNvidia) {
  RendererConfig config;
  config.adapter_policy = RendererAdapterPolicy::IntelIntegrated;
  const std::vector<RendererAdapterInfo> adapters = {
      Adapter("NVIDIA", 0x10de, false, false),
      Adapter("Lavapipe", 0x10005, false, true),
  };
  const auto result = SelectRendererAdapter(config, adapters);
  EXPECT_FALSE(result.adapter_index.has_value());
}

TEST(RendererConfig, IntelPolicySelectsIntegratedIntelNotDiscreteNvidia) {
  RendererConfig config;
  config.adapter_policy = RendererAdapterPolicy::IntelIntegrated;
  const std::vector<RendererAdapterInfo> adapters = {
      Adapter("NVIDIA", 0x10de, false, false),
      Adapter("Intel Iris Xe", 0x8086, true, false),
  };
  const auto result = SelectRendererAdapter(config, adapters);
  ASSERT_TRUE(result.adapter_index.has_value());
  EXPECT_EQ(1u, *result.adapter_index);
}

TEST(RendererConfig, NvidiaRequiresExplicitPolicy) {
  const std::vector<RendererAdapterInfo> adapters = {
      Adapter("NVIDIA", 0x10de, false, false),
  };
  RendererConfig config;
  EXPECT_FALSE(SelectRendererAdapter(config, adapters).adapter_index);
  config.adapter_policy = RendererAdapterPolicy::NvidiaExperimental;
  ASSERT_TRUE(SelectRendererAdapter(config, adapters).adapter_index);
}

TEST(RendererConfig, CpuPolicyDoesNotSelectHardware) {
  RendererConfig config;
  config.adapter_policy = RendererAdapterPolicy::CpuSoftware;
  const std::vector<RendererAdapterInfo> adapters = {
      Adapter("Intel", 0x8086, true, false),
      Adapter("Lavapipe", 0x10005, false, true),
  };
  const auto result = SelectRendererAdapter(config, adapters);
  ASSERT_TRUE(result.adapter_index.has_value());
  EXPECT_EQ(1u, *result.adapter_index);
}

TEST(RendererConfig, RejectsAdaptersWithoutPresentationSupport) {
  RendererConfig config;
  const std::vector<RendererAdapterInfo> adapters = {
      Adapter("Intel", 0x8086, true, false, false),
  };
  EXPECT_FALSE(SelectRendererAdapter(config, adapters).adapter_index);
}

TEST(RendererConfig, ClampsResourceLimits) {
  RendererResourceLimits limits;
  limits.texture_cache_mb = 0;
  limits.max_upload_mb_per_frame = 1000;
  limits.max_frames_in_flight = 99;
  limits.max_geometry_batch_mb = 0;
  limits.startup_timeout_ms = 1;
  limits.device_wait_timeout_ms = 9000;
  limits = ClampRendererResourceLimits(limits);
  EXPECT_EQ(32u, limits.texture_cache_mb);
  EXPECT_EQ(256u, limits.max_upload_mb_per_frame);
  EXPECT_EQ(3u, limits.max_frames_in_flight);
  EXPECT_EQ(1u, limits.max_geometry_batch_mb);
  EXPECT_EQ(250u, limits.startup_timeout_ms);
  EXPECT_EQ(5000u, limits.device_wait_timeout_ms);
}

TEST(RendererConfig, ParsesStableConfigurationNames) {
  EXPECT_EQ(RendererBackend::Software, *ParseRendererBackend("software"));
  EXPECT_EQ(RendererBackend::OpenGLLegacy,
            *ParseRendererBackend("opengl-legacy"));
  EXPECT_EQ(RendererBackend::VulkanExperimental,
            *ParseRendererBackend("vulkan-experimental"));
  EXPECT_FALSE(ParseRendererBackend("fastest"));
}

TEST(RendererConfig, DeviceLossAndOomAlwaysFallBackToSoftware) {
  RendererConfig config;
  config.automatic_restart = true;
  EXPECT_EQ(
      RendererFailureAction::FallBackToSoftware,
      ChooseRendererFailureAction(RendererFailureKind::DeviceLost, config, 0));
  EXPECT_EQ(
      RendererFailureAction::FallBackToSoftware,
      ChooseRendererFailureAction(RendererFailureKind::OutOfMemory, config, 0));
}

TEST(RendererConfig, SurfaceAndProcessRetriesAreBounded) {
  RendererConfig config;
  config.automatic_restart = true;
  EXPECT_EQ(
      RendererFailureAction::RetrySurfaceOnce,
      ChooseRendererFailureAction(RendererFailureKind::SurfaceLost, config, 0));
  EXPECT_EQ(
      RendererFailureAction::FallBackToSoftware,
      ChooseRendererFailureAction(RendererFailureKind::SurfaceLost, config, 1));
  EXPECT_EQ(RendererFailureAction::RestartRendererOnce,
            ChooseRendererFailureAction(
                RendererFailureKind::RendererProcessExit, config, 0));
  EXPECT_EQ(RendererFailureAction::FallBackToSoftware,
            ChooseRendererFailureAction(
                RendererFailureKind::RendererProcessExit, config, 1));
}

TEST(RendererConfig, UploadValidationRejectsMalformedOrExcessiveFrames) {
  RendererResourceLimits limits;
  limits.max_upload_mb_per_frame = 4;
  EXPECT_TRUE(RendererUploadWithinLimits(512, 512, 4, limits));
  EXPECT_FALSE(RendererUploadWithinLimits(0, 512, 4, limits));
  EXPECT_FALSE(RendererUploadWithinLimits(50000, 1, 4, limits));
  EXPECT_FALSE(RendererUploadWithinLimits(2048, 2048, 4, limits));
  EXPECT_FALSE(RendererUploadWithinLimits(UINT32_MAX, UINT32_MAX, 4, limits));
}
