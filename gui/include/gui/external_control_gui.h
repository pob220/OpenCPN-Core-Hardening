/***************************************************************************
 * Copyright (C) 2026 OpenCPN contributors
 ***************************************************************************/

#ifndef GUI_EXTERNAL_CONTROL_GUI_H_
#define GUI_EXTERNAL_CONTROL_GUI_H_

#include "model/external_control.h"

/** Live, main-thread adapters for the transport-free external-control API. */
ocpn::control::ServiceBundle MakeExternalControlServices();

/**
 * Ask provider jobs to cancel before a plugin is deactivated.
 * Returns false while provider code is still pinned by an active job; callers
 * must leave the plugin loaded and permit deactivation to be retried.
 */
bool PrepareExternalPlanningProviderUnload(const std::string& plugin_name);

#endif  // GUI_EXTERNAL_CONTROL_GUI_H_
