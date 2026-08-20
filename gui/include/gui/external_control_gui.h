/***************************************************************************
 * Copyright (C) 2026 OpenCPN contributors
 ***************************************************************************/

#ifndef GUI_EXTERNAL_CONTROL_GUI_H_
#define GUI_EXTERNAL_CONTROL_GUI_H_

#include "model/external_control.h"

/** Live, main-thread adapters for the transport-free external-control API. */
ocpn::control::ServiceBundle MakeExternalControlServices();

#endif  // GUI_EXTERNAL_CONTROL_GUI_H_
