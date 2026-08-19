/***************************************************************************
 * Copyright (C) 2026 OpenCPN contributors
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 ***************************************************************************/

#ifndef GUI_CHART_SAFETY_SERVICE_H
#define GUI_CHART_SAFETY_SERVICE_H

namespace ocpn::chart_safety {

/**
 * Whether optional neighbour prefetch is still permitted by a service-call
 * time budget.  A non-positive budget means unlimited processing.
 *
 * The requested centre tile is authoritative work and may itself exceed the
 * budget.  This policy applies only to speculative neighbours after it.
 */
inline bool MayPrefetchNeighbour(long elapsed_ms, int max_milliseconds) {
  return max_milliseconds <= 0 || elapsed_ms < max_milliseconds;
}

}  // namespace ocpn::chart_safety

#endif  // GUI_CHART_SAFETY_SERVICE_H
