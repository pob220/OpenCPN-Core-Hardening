/***************************************************************************
 * Copyright (C) 2026 OpenCPN contributors
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 ***************************************************************************/

#include "vulkan_presenter.h"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <limits>
#include <vector>

#include <wx/log.h>

#ifdef ocpnUSE_VULKAN
#define VK_USE_PLATFORM_WAYLAND_KHR
#define VK_USE_PLATFORM_XLIB_KHR
#include <vulkan/vulkan.h>

#include <gtk/gtk.h>
#ifdef GDK_WINDOWING_WAYLAND
#include <gdk/gdkwayland.h>
#endif
#ifdef GDK_WINDOWING_X11
#include <gdk/gdkx.h>
#endif
#endif

namespace fs = std::filesystem;

namespace {
constexpr uint64_t kNanosecondsPerMillisecond = 1000000ULL;

std::string MarkerPath(const std::string& state_directory) {
  return (fs::path(state_directory) / "renderer-startup.pending").string();
}

void WriteMarker(const std::string& path) {
  std::ofstream marker(path, std::ios::trunc);
  marker << "vulkan-experimental\n";
}

void RemoveMarker(const std::string& path) {
  std::error_code error;
  fs::remove(path, error);
}
}  // namespace

class VulkanPresenter::Impl {
public:
  Impl(wxWindow* window, const RendererConfig& config,
       const std::string& state_directory)
      : m_window(window),
        m_config(config),
        m_marker_path(MarkerPath(state_directory)) {
#ifdef ocpnUSE_VULKAN
    WriteMarker(m_marker_path);
    m_ready = Initialize();
    if (!m_ready) Shutdown();
#else
    m_failure = "Vulkan support was not compiled into OpenCPN";
#endif
  }

  ~Impl() {
#ifdef ocpnUSE_VULKAN
    Shutdown();
#endif
  }

  bool IsReady() const { return m_ready; }

  RendererPresentResult Present(const wxBitmap& frame) {
#ifndef ocpnUSE_VULKAN
    return {RendererPresentStatus::PermanentFailure, m_failure};
#else
    if (!m_ready) return {RendererPresentStatus::PermanentFailure, m_failure};
    const auto started = std::chrono::steady_clock::now();
    const int width = frame.GetWidth();
    const int height = frame.GetHeight();
    if (width <= 0 || height <= 0)
      return Fail("invalid frame dimensions", false);
    const uint64_t bytes = static_cast<uint64_t>(width) * height * 4;
    if (!RendererUploadWithinLimits(width, height, 4, m_config.limits))
      return Fail("frame exceeds configured upload limit", false);

    if (m_extent.width != static_cast<uint32_t>(width) ||
        m_extent.height != static_cast<uint32_t>(height)) {
      if (!RecreateSwapchain(static_cast<uint32_t>(width),
                             static_cast<uint32_t>(height)))
        return Fail(m_failure, false);
      if (m_extent.width != static_cast<uint32_t>(width) ||
          m_extent.height != static_cast<uint32_t>(height))
        return Fail("chart frame and Vulkan surface pixel extents differ",
                    false);
    }
    if (bytes > m_staging_size && !CreateStagingBuffer(bytes))
      return Fail(m_failure, false);

    const uint64_t wait_ns =
        static_cast<uint64_t>(m_config.limits.device_wait_timeout_ms) *
        kNanosecondsPerMillisecond;
    VkResult result = vkWaitForFences(m_device, 1, &m_fence, VK_TRUE, wait_ns);
    if (result == VK_TIMEOUT) {
      m_abandon_device = true;
      return Fail("renderer fence timeout", false);
    }
    if (result != VK_SUCCESS) return VkFailure("waiting for frame", result);

    uint32_t image_index = 0;
    result =
        vkAcquireNextImageKHR(m_device, m_swapchain, wait_ns, m_image_available,
                              VK_NULL_HANDLE, &image_index);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
      if (!RecreateSwapchain(width, height)) return Fail(m_failure, false);
      return {RendererPresentStatus::TemporarilyUnavailable,
              "surface changed; swapchain recreated"};
    }
    if (result != VK_SUCCESS)
      return VkFailure("acquiring swapchain image", result);

    void* mapped = nullptr;
    result = vkMapMemory(m_device, m_staging_memory, 0, bytes, 0, &mapped);
    if (result != VK_SUCCESS)
      return VkFailure("mapping staging memory", result);
    wxImage image = frame.ConvertToImage();
    if (!image.IsOk()) {
      vkUnmapMemory(m_device, m_staging_memory);
      return Fail("could not convert chart bitmap for presentation", false);
    }
    const unsigned char* rgb = image.GetData();
    auto* output = static_cast<unsigned char*>(mapped);
    const bool bgra = m_format == VK_FORMAT_B8G8R8A8_UNORM ||
                      m_format == VK_FORMAT_B8G8R8A8_SRGB;
    for (uint64_t pixel = 0; pixel < static_cast<uint64_t>(width) * height;
         ++pixel) {
      const unsigned char r = rgb[pixel * 3];
      const unsigned char g = rgb[pixel * 3 + 1];
      const unsigned char b = rgb[pixel * 3 + 2];
      output[pixel * 4] = bgra ? b : r;
      output[pixel * 4 + 1] = g;
      output[pixel * 4 + 2] = bgra ? r : b;
      output[pixel * 4 + 3] = 255;
    }
    vkUnmapMemory(m_device, m_staging_memory);

    vkResetFences(m_device, 1, &m_fence);
    vkResetCommandBuffer(m_command_buffer, 0);
    VkCommandBufferBeginInfo begin{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO};
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    if (vkBeginCommandBuffer(m_command_buffer, &begin) != VK_SUCCESS)
      return Fail("could not begin Vulkan command buffer", false);

    VkImageMemoryBarrier to_transfer{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    to_transfer.srcAccessMask = 0;
    to_transfer.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    to_transfer.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    to_transfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    to_transfer.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    to_transfer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    to_transfer.image = m_images[image_index];
    to_transfer.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    vkCmdPipelineBarrier(m_command_buffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0,
                         nullptr, 1, &to_transfer);

    VkBufferImageCopy copy{};
    copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
    copy.imageExtent = {static_cast<uint32_t>(width),
                        static_cast<uint32_t>(height), 1};
    vkCmdCopyBufferToImage(m_command_buffer, m_staging_buffer,
                           m_images[image_index],
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);

    VkImageMemoryBarrier to_present = to_transfer;
    to_present.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    to_present.dstAccessMask = 0;
    to_present.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    to_present.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    vkCmdPipelineBarrier(m_command_buffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0,
                         nullptr, 1, &to_present);
    if (vkEndCommandBuffer(m_command_buffer) != VK_SUCCESS)
      return Fail("could not finish Vulkan command buffer", false);

    const VkPipelineStageFlags wait_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    VkSubmitInfo submit{VK_STRUCTURE_TYPE_SUBMIT_INFO};
    submit.waitSemaphoreCount = 1;
    submit.pWaitSemaphores = &m_image_available;
    submit.pWaitDstStageMask = &wait_stage;
    submit.commandBufferCount = 1;
    submit.pCommandBuffers = &m_command_buffer;
    VkSemaphore render_finished = m_render_finished[image_index];
    submit.signalSemaphoreCount = 1;
    submit.pSignalSemaphores = &render_finished;
    result = vkQueueSubmit(m_queue, 1, &submit, m_fence);
    if (result != VK_SUCCESS) return VkFailure("submitting frame", result);

    VkPresentInfoKHR present{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
    present.waitSemaphoreCount = 1;
    present.pWaitSemaphores = &render_finished;
    present.swapchainCount = 1;
    present.pSwapchains = &m_swapchain;
    present.pImageIndices = &image_index;
    result = vkQueuePresentKHR(m_queue, &present);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
      m_extent = {0, 0};
      return {RendererPresentStatus::TemporarilyUnavailable,
              "surface changed; frame will be retried"};
    }
    if (result != VK_SUCCESS) return VkFailure("presenting frame", result);

    if (!m_presented_once) {
      m_presented_once = true;
      RemoveMarker(m_marker_path);
    }
    ++g_renderer_runtime_info.frames_presented;
    g_renderer_runtime_info.last_frame_ms =
        std::chrono::duration<double, std::milli>(
            std::chrono::steady_clock::now() - started)
            .count();
    g_renderer_runtime_info.health = "Presenting normally";
    return {RendererPresentStatus::Presented, "frame presented"};
#endif
  }

private:
#ifdef ocpnUSE_VULKAN
  bool Initialize() {
    if (!m_window || !m_window->GetHandle()) {
      m_failure = "chart window has no native handle";
      return false;
    }

    const char* filter = "*intel*";
    if (m_config.adapter_policy == RendererAdapterPolicy::CpuSoftware)
      filter = "*lvp*,*lavapipe*";
    else if (m_config.adapter_policy ==
             RendererAdapterPolicy::NvidiaExperimental)
      filter = "*nvidia*";
    const char* previous_filter = std::getenv("VK_LOADER_DRIVERS_SELECT");
    const std::string saved_filter = previous_filter ? previous_filter : "";
    const char* previous_layers = std::getenv("VK_LOADER_LAYERS_DISABLE");
    const std::string saved_layers = previous_layers ? previous_layers : "";
    setenv("VK_LOADER_DRIVERS_SELECT", filter, 1);
    setenv("VK_LOADER_LAYERS_DISABLE",
           "VK_LAYER_NV_*,VK_LAYER_MESA_device_select", 1);

    std::vector<const char*> extensions = {VK_KHR_SURFACE_EXTENSION_NAME};
    GtkWidget* widget = static_cast<GtkWidget*>(m_window->GetHandle());
    gtk_widget_realize(widget);
    GdkWindow* gdk_window = gtk_widget_get_window(widget);
    if (!gdk_window) {
      RestoreLoaderFilters(saved_filter, previous_filter != nullptr,
                           saved_layers, previous_layers != nullptr);
      m_failure = "GTK chart window is not realized";
      return false;
    }
#ifdef GDK_WINDOWING_WAYLAND
    if (GDK_IS_WAYLAND_WINDOW(gdk_window))
      extensions.push_back(VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME);
    else
#endif
#ifdef GDK_WINDOWING_X11
        if (GDK_IS_X11_WINDOW(gdk_window))
      extensions.push_back(VK_KHR_XLIB_SURFACE_EXTENSION_NAME);
    else
#endif
    {
      RestoreLoaderFilters(saved_filter, previous_filter != nullptr,
                           saved_layers, previous_layers != nullptr);
      m_failure = "unsupported GTK window system for Vulkan";
      return false;
    }

    VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    app.pApplicationName = "OpenCPN";
    app.applicationVersion = VK_MAKE_VERSION(5, 15, 0);
    app.pEngineName = "OpenCPN software-chart Vulkan presenter";
    app.engineVersion = 1;
    app.apiVersion = VK_API_VERSION_1_0;
    VkInstanceCreateInfo create{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    create.pApplicationInfo = &app;
    create.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    create.ppEnabledExtensionNames = extensions.data();
    const char* validation = "VK_LAYER_KHRONOS_validation";
    if (m_config.diagnostics && ValidationLayerAvailable()) {
      create.enabledLayerCount = 1;
      create.ppEnabledLayerNames = &validation;
      m_validation_enabled = true;
    }
    VkResult result = vkCreateInstance(&create, nullptr, &m_instance);
    if (result != VK_SUCCESS) {
      RestoreLoaderFilters(saved_filter, previous_filter != nullptr,
                           saved_layers, previous_layers != nullptr);
      m_failure = "could not create restricted Vulkan instance";
      return false;
    }

#ifdef GDK_WINDOWING_WAYLAND
    if (GDK_IS_WAYLAND_WINDOW(gdk_window)) {
      VkWaylandSurfaceCreateInfoKHR surface{
          VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR};
      surface.display = gdk_wayland_display_get_wl_display(
          gdk_window_get_display(gdk_window));
      surface.surface = gdk_wayland_window_get_wl_surface(gdk_window);
      result =
          vkCreateWaylandSurfaceKHR(m_instance, &surface, nullptr, &m_surface);
    } else
#endif
#ifdef GDK_WINDOWING_X11
        if (GDK_IS_X11_WINDOW(gdk_window)) {
      VkXlibSurfaceCreateInfoKHR surface{
          VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR};
      surface.dpy =
          gdk_x11_display_get_xdisplay(gdk_window_get_display(gdk_window));
      surface.window = gdk_x11_window_get_xid(gdk_window);
      result =
          vkCreateXlibSurfaceKHR(m_instance, &surface, nullptr, &m_surface);
    } else
#endif
      result = VK_ERROR_EXTENSION_NOT_PRESENT;

    if (result == VK_SUCCESS) result = SelectDeviceAndQueue();
    RestoreLoaderFilters(saved_filter, previous_filter != nullptr, saved_layers,
                         previous_layers != nullptr);
    if (result != VK_SUCCESS) {
      m_failure = "no matching present-capable Vulkan adapter";
      return false;
    }
    if (!CreateDevice() || !CreateCommandResources()) return false;
    const wxSize size = m_window->GetClientSize();
    if (!RecreateSwapchain(size.x, size.y)) return false;

    g_renderer_runtime_info.active_backend =
        RendererBackend::VulkanExperimental;
    g_renderer_runtime_info.fallback_active = false;
    g_renderer_runtime_info.health = "Initialized; waiting for first frame";
    g_renderer_runtime_info.presentation_mode = "Vulkan FIFO transfer";
    wxLogMessage(
        "Renderer: backend=vulkan-experimental adapter=%s vendor=0x%04x "
        "device=0x%04x type=%s driver=%s presentation=FIFO "
        "upload_limit_mb=%u frames_in_flight=1 validation=%s",
        g_renderer_runtime_info.adapter_name, g_renderer_runtime_info.vendor_id,
        g_renderer_runtime_info.device_id, g_renderer_runtime_info.device_type,
        g_renderer_runtime_info.driver, m_config.limits.max_upload_mb_per_frame,
        m_validation_enabled ? "enabled" : "disabled");
    return true;
  }

  static void RestoreLoaderFilters(const std::string& driver_value,
                                   bool driver_existed,
                                   const std::string& layer_value,
                                   bool layer_existed) {
    if (driver_existed)
      setenv("VK_LOADER_DRIVERS_SELECT", driver_value.c_str(), 1);
    else
      unsetenv("VK_LOADER_DRIVERS_SELECT");
    if (layer_existed)
      setenv("VK_LOADER_LAYERS_DISABLE", layer_value.c_str(), 1);
    else
      unsetenv("VK_LOADER_LAYERS_DISABLE");
  }

  bool ValidationLayerAvailable() const {
    uint32_t count = 0;
    vkEnumerateInstanceLayerProperties(&count, nullptr);
    std::vector<VkLayerProperties> layers(count);
    vkEnumerateInstanceLayerProperties(&count, layers.data());
    for (const auto& layer : layers) {
      if (std::strcmp(layer.layerName, "VK_LAYER_KHRONOS_validation") == 0)
        return true;
    }
    return false;
  }

  VkResult SelectDeviceAndQueue() {
    uint32_t count = 0;
    VkResult result = vkEnumeratePhysicalDevices(m_instance, &count, nullptr);
    if (result != VK_SUCCESS || count == 0) return VK_ERROR_DEVICE_LOST;
    std::vector<VkPhysicalDevice> devices(count);
    vkEnumeratePhysicalDevices(m_instance, &count, devices.data());
    std::vector<RendererAdapterInfo> adapters;
    std::vector<uint32_t> queue_indices(count, UINT32_MAX);
    for (uint32_t i = 0; i < count; ++i) {
      VkPhysicalDeviceProperties properties{};
      vkGetPhysicalDeviceProperties(devices[i], &properties);
      uint32_t queue_count = 0;
      vkGetPhysicalDeviceQueueFamilyProperties(devices[i], &queue_count,
                                               nullptr);
      std::vector<VkQueueFamilyProperties> queues(queue_count);
      vkGetPhysicalDeviceQueueFamilyProperties(devices[i], &queue_count,
                                               queues.data());
      bool present = false;
      for (uint32_t q = 0; q < queue_count; ++q) {
        VkBool32 supported = VK_FALSE;
        vkGetPhysicalDeviceSurfaceSupportKHR(devices[i], q, m_surface,
                                             &supported);
        if ((queues[q].queueFlags & VK_QUEUE_GRAPHICS_BIT) && supported) {
          queue_indices[i] = q;
          present = true;
          break;
        }
      }
      RendererAdapterInfo adapter;
      adapter.name = properties.deviceName;
      adapter.vendor_id = properties.vendorID;
      adapter.device_id = properties.deviceID;
      adapter.integrated =
          properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU;
      adapter.cpu = properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU;
      adapter.supports_presentation = present;
      adapters.push_back(adapter);
    }
    const auto selection = SelectRendererAdapter(m_config, adapters);
    if (!selection.adapter_index) return VK_ERROR_FEATURE_NOT_PRESENT;
    const size_t index = *selection.adapter_index;
    m_physical_device = devices[index];
    m_queue_family = queue_indices[index];
    VkPhysicalDeviceProperties properties{};
    vkGetPhysicalDeviceProperties(m_physical_device, &properties);
    g_renderer_runtime_info.adapter_name = properties.deviceName;
    g_renderer_runtime_info.vendor_id = properties.vendorID;
    g_renderer_runtime_info.device_id = properties.deviceID;
    g_renderer_runtime_info.device_type =
        properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU
            ? "Integrated GPU"
        : properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU ? "CPU"
                                                               : "Discrete GPU";
    g_renderer_runtime_info.driver =
        "Vulkan API " +
        std::to_string(VK_VERSION_MAJOR(properties.apiVersion)) + "." +
        std::to_string(VK_VERSION_MINOR(properties.apiVersion)) + ", driver " +
        std::to_string(properties.driverVersion);
    return VK_SUCCESS;
  }

  bool CreateDevice() {
    const float priority = 1.0f;
    VkDeviceQueueCreateInfo queue{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
    queue.queueFamilyIndex = m_queue_family;
    queue.queueCount = 1;
    queue.pQueuePriorities = &priority;
    const char* extension = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
    VkDeviceCreateInfo create{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
    create.queueCreateInfoCount = 1;
    create.pQueueCreateInfos = &queue;
    create.enabledExtensionCount = 1;
    create.ppEnabledExtensionNames = &extension;
    VkResult result =
        vkCreateDevice(m_physical_device, &create, nullptr, &m_device);
    if (result != VK_SUCCESS) {
      m_failure = "could not create Vulkan device";
      return false;
    }
    vkGetDeviceQueue(m_device, m_queue_family, 0, &m_queue);
    return true;
  }

  bool CreateCommandResources() {
    VkCommandPoolCreateInfo pool{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};
    pool.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    pool.queueFamilyIndex = m_queue_family;
    if (vkCreateCommandPool(m_device, &pool, nullptr, &m_command_pool) !=
        VK_SUCCESS) {
      m_failure = "could not create Vulkan command pool";
      return false;
    }
    VkCommandBufferAllocateInfo allocate{
        VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};
    allocate.commandPool = m_command_pool;
    allocate.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocate.commandBufferCount = 1;
    if (vkAllocateCommandBuffers(m_device, &allocate, &m_command_buffer) !=
        VK_SUCCESS) {
      m_failure = "could not allocate Vulkan command buffer";
      return false;
    }
    VkSemaphoreCreateInfo semaphore{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    VkFenceCreateInfo fence{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
    fence.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    if (vkCreateSemaphore(m_device, &semaphore, nullptr, &m_image_available) !=
            VK_SUCCESS ||
        vkCreateFence(m_device, &fence, nullptr, &m_fence) != VK_SUCCESS) {
      m_failure = "could not create Vulkan frame synchronization";
      return false;
    }
    return true;
  }

  bool RecreateSwapchain(uint32_t requested_width, uint32_t requested_height) {
    if (!m_device || requested_width == 0 || requested_height == 0)
      return false;
    vkDeviceWaitIdle(m_device);
    if (m_swapchain) vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);
    for (VkSemaphore semaphore : m_render_finished)
      vkDestroySemaphore(m_device, semaphore, nullptr);
    m_render_finished.clear();
    m_swapchain = VK_NULL_HANDLE;
    m_images.clear();

    VkSurfaceCapabilitiesKHR capabilities{};
    if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
            m_physical_device, m_surface, &capabilities) != VK_SUCCESS) {
      m_failure = "could not query Vulkan surface capabilities";
      return false;
    }
    if (!(capabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT)) {
      m_failure = "surface does not support safe transfer presentation";
      return false;
    }
    uint32_t format_count = 0;
    vkGetPhysicalDeviceSurfaceFormatsKHR(m_physical_device, m_surface,
                                         &format_count, nullptr);
    std::vector<VkSurfaceFormatKHR> formats(format_count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(m_physical_device, m_surface,
                                         &format_count, formats.data());
    if (formats.empty()) {
      m_failure = "surface has no Vulkan formats";
      return false;
    }
    VkSurfaceFormatKHR chosen = formats.front();
    for (const auto& format : formats) {
      if (format.format == VK_FORMAT_B8G8R8A8_UNORM ||
          format.format == VK_FORMAT_R8G8B8A8_UNORM) {
        chosen = format;
        break;
      }
    }
    m_format = chosen.format;
    m_extent = capabilities.currentExtent;
    if (m_extent.width == UINT32_MAX) {
      m_extent.width =
          std::clamp(requested_width, capabilities.minImageExtent.width,
                     capabilities.maxImageExtent.width);
      m_extent.height =
          std::clamp(requested_height, capabilities.minImageExtent.height,
                     capabilities.maxImageExtent.height);
    }
    uint32_t image_count = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount && image_count > capabilities.maxImageCount)
      image_count = capabilities.maxImageCount;
    VkSwapchainCreateInfoKHR create{
        VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
    create.surface = m_surface;
    create.minImageCount = image_count;
    create.imageFormat = chosen.format;
    create.imageColorSpace = chosen.colorSpace;
    create.imageExtent = m_extent;
    create.imageArrayLayers = 1;
    create.imageUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    create.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    create.preTransform = capabilities.currentTransform;
    create.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    create.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    create.clipped = VK_TRUE;
    if (vkCreateSwapchainKHR(m_device, &create, nullptr, &m_swapchain) !=
        VK_SUCCESS) {
      m_failure = "could not create Vulkan swapchain";
      return false;
    }
    vkGetSwapchainImagesKHR(m_device, m_swapchain, &image_count, nullptr);
    m_images.resize(image_count);
    vkGetSwapchainImagesKHR(m_device, m_swapchain, &image_count,
                            m_images.data());
    VkSemaphoreCreateInfo semaphore{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    m_render_finished.resize(image_count, VK_NULL_HANDLE);
    for (VkSemaphore& item : m_render_finished) {
      if (vkCreateSemaphore(m_device, &semaphore, nullptr, &item) !=
          VK_SUCCESS) {
        m_failure = "could not create per-image presentation semaphore";
        return false;
      }
    }
    return true;
  }

  uint32_t FindMemoryType(uint32_t bits, VkMemoryPropertyFlags wanted) const {
    VkPhysicalDeviceMemoryProperties properties{};
    vkGetPhysicalDeviceMemoryProperties(m_physical_device, &properties);
    for (uint32_t i = 0; i < properties.memoryTypeCount; ++i) {
      if ((bits & (1u << i)) &&
          (properties.memoryTypes[i].propertyFlags & wanted) == wanted)
        return i;
    }
    return UINT32_MAX;
  }

  bool CreateStagingBuffer(VkDeviceSize size) {
    if (m_staging_buffer) vkDestroyBuffer(m_device, m_staging_buffer, nullptr);
    if (m_staging_memory) vkFreeMemory(m_device, m_staging_memory, nullptr);
    m_staging_buffer = VK_NULL_HANDLE;
    m_staging_memory = VK_NULL_HANDLE;
    VkBufferCreateInfo buffer{VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
    buffer.size = size;
    buffer.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    buffer.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(m_device, &buffer, nullptr, &m_staging_buffer) !=
        VK_SUCCESS) {
      m_failure = "could not create bounded Vulkan staging buffer";
      return false;
    }
    VkMemoryRequirements requirements{};
    vkGetBufferMemoryRequirements(m_device, m_staging_buffer, &requirements);
    const uint32_t type = FindMemoryType(
        requirements.memoryTypeBits, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                                         VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (type == UINT32_MAX) {
      m_failure = "no coherent host-visible Vulkan memory";
      return false;
    }
    VkMemoryAllocateInfo allocate{VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO};
    allocate.allocationSize = requirements.size;
    allocate.memoryTypeIndex = type;
    if (vkAllocateMemory(m_device, &allocate, nullptr, &m_staging_memory) !=
            VK_SUCCESS ||
        vkBindBufferMemory(m_device, m_staging_buffer, m_staging_memory, 0) !=
            VK_SUCCESS) {
      m_failure = "could not allocate bounded Vulkan staging memory";
      return false;
    }
    m_staging_size = size;
    return true;
  }

  RendererPresentResult VkFailure(const char* operation, VkResult result) {
    if (result == VK_ERROR_DEVICE_LOST) {
      m_abandon_device = true;
      ++g_renderer_runtime_info.device_losses;
      return Fail(std::string(operation) + ": Vulkan device lost", false);
    }
    if (result == VK_ERROR_OUT_OF_HOST_MEMORY ||
        result == VK_ERROR_OUT_OF_DEVICE_MEMORY)
      return Fail(std::string(operation) + ": out of memory", false);
    return Fail(
        std::string(operation) + ": Vulkan error " + std::to_string(result),
        false);
  }

  RendererPresentResult Fail(const std::string& message, bool temporary) {
    m_failure = message;
    ++g_renderer_runtime_info.presentation_failures;
    g_renderer_runtime_info.health = message;
    return {temporary ? RendererPresentStatus::TemporarilyUnavailable
                      : RendererPresentStatus::PermanentFailure,
            message};
  }

  void Shutdown() {
    m_ready = false;
    if (m_abandon_device) {
      wxLogWarning(
          "Renderer: abandoning failed Vulkan device without a blocking "
          "device-idle wait; process teardown will reclaim its resources");
      return;
    }
    if (m_device) vkDeviceWaitIdle(m_device);
    if (m_staging_buffer) vkDestroyBuffer(m_device, m_staging_buffer, nullptr);
    if (m_staging_memory) vkFreeMemory(m_device, m_staging_memory, nullptr);
    if (m_fence) vkDestroyFence(m_device, m_fence, nullptr);
    for (VkSemaphore semaphore : m_render_finished)
      vkDestroySemaphore(m_device, semaphore, nullptr);
    if (m_image_available)
      vkDestroySemaphore(m_device, m_image_available, nullptr);
    if (m_command_pool) vkDestroyCommandPool(m_device, m_command_pool, nullptr);
    if (m_swapchain) vkDestroySwapchainKHR(m_device, m_swapchain, nullptr);
    if (m_device) vkDestroyDevice(m_device, nullptr);
    if (m_surface) vkDestroySurfaceKHR(m_instance, m_surface, nullptr);
    if (m_instance) vkDestroyInstance(m_instance, nullptr);
    m_instance = VK_NULL_HANDLE;
    m_device = VK_NULL_HANDLE;
  }

  VkInstance m_instance = VK_NULL_HANDLE;
  VkSurfaceKHR m_surface = VK_NULL_HANDLE;
  VkPhysicalDevice m_physical_device = VK_NULL_HANDLE;
  VkDevice m_device = VK_NULL_HANDLE;
  VkQueue m_queue = VK_NULL_HANDLE;
  uint32_t m_queue_family = UINT32_MAX;
  VkSwapchainKHR m_swapchain = VK_NULL_HANDLE;
  VkFormat m_format = VK_FORMAT_UNDEFINED;
  VkExtent2D m_extent{};
  std::vector<VkImage> m_images;
  VkCommandPool m_command_pool = VK_NULL_HANDLE;
  VkCommandBuffer m_command_buffer = VK_NULL_HANDLE;
  VkSemaphore m_image_available = VK_NULL_HANDLE;
  std::vector<VkSemaphore> m_render_finished;
  VkFence m_fence = VK_NULL_HANDLE;
  VkBuffer m_staging_buffer = VK_NULL_HANDLE;
  VkDeviceMemory m_staging_memory = VK_NULL_HANDLE;
  VkDeviceSize m_staging_size = 0;
  bool m_validation_enabled = false;
  bool m_abandon_device = false;
#endif

  wxWindow* m_window = nullptr;
  RendererConfig m_config;
  std::string m_marker_path;
  std::string m_failure;
  bool m_ready = false;
  bool m_presented_once = false;
};

VulkanPresenter::VulkanPresenter(wxWindow* window, const RendererConfig& config,
                                 const std::string& state_directory)
    : m_impl(std::make_unique<Impl>(window, config, state_directory)) {}

VulkanPresenter::~VulkanPresenter() = default;

RendererPresentResult VulkanPresenter::Present(const wxBitmap& frame) {
  return m_impl->Present(frame);
}

bool VulkanPresenter::IsReady() const { return m_impl->IsReady(); }

bool VulkanPresenter::IsCompiled() {
#ifdef ocpnUSE_VULKAN
  return true;
#else
  return false;
#endif
}
