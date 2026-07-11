/***************************************************************************
 * Copyright (C) 2026 OpenCPN contributors
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 ***************************************************************************/

#ifndef GUI_VULKAN_PRESENTER_H_
#define GUI_VULKAN_PRESENTER_H_

#include <memory>
#include <string>

#include <wx/bitmap.h>
#include <wx/window.h>

#include "model/renderer_config.h"

enum class RendererPresentStatus {
  Presented,
  TemporarilyUnavailable,
  PermanentFailure,
};

struct RendererPresentResult {
  RendererPresentStatus status = RendererPresentStatus::PermanentFailure;
  std::string message;
};

class VulkanPresenter {
public:
  VulkanPresenter(wxWindow* window, const RendererConfig& config,
                  const std::string& state_directory);
  ~VulkanPresenter();

  VulkanPresenter(const VulkanPresenter&) = delete;
  VulkanPresenter& operator=(const VulkanPresenter&) = delete;

  RendererPresentResult Present(const wxBitmap& frame);
  bool IsReady() const;
  static bool IsCompiled();

private:
  class Impl;
  std::unique_ptr<Impl> m_impl;
};

#endif  // GUI_VULKAN_PRESENTER_H_
