/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "server/PoseMerge.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace deskflow::server {

namespace {

const DisplayRect *findExistingById(const MachineLayout &machine, const std::string &id)
{
  if (id.empty()) {
    return nullptr;
  }
  return machine.findMonitorById(id);
}

const DisplayRect *findExistingByFingerprint(const MachineLayout &machine, const DisplayRect &reported)
{
  for (const auto &monitor : machine.m_monitors) {
    if (monitor.m_localX == reported.m_localX && monitor.m_localY == reported.m_localY &&
        monitor.m_width == reported.m_width && monitor.m_height == reported.m_height) {
      return &monitor;
    }
  }
  return nullptr;
}

void worldBounds(const WorkspaceLayout &layout, int32_t &minX, int32_t &minY, int32_t &maxX, int32_t &maxY)
{
  minX = std::numeric_limits<int32_t>::max();
  minY = std::numeric_limits<int32_t>::max();
  maxX = std::numeric_limits<int32_t>::min();
  maxY = std::numeric_limits<int32_t>::min();
  bool any = false;
  for (const auto &machine : layout.m_machines) {
    for (const auto &monitor : machine.m_monitors) {
      any = true;
      minX = std::min(minX, monitor.m_worldX);
      minY = std::min(minY, monitor.m_worldY);
      maxX = std::max(maxX, monitor.m_worldX + monitor.layoutWidth());
      maxY = std::max(maxY, monitor.m_worldY + monitor.layoutHeight());
    }
  }
  if (!any) {
    minX = minY = 0;
    maxX = maxY = 0;
  }
}

} // namespace

void parkMonitorAwayFromLayout(WorkspaceLayout &layout, DisplayRect &monitor)
{
  monitor.ensureLayoutSizes();
  int32_t minX = 0;
  int32_t minY = 0;
  int32_t maxX = 0;
  int32_t maxY = 0;
  worldBounds(layout, minX, minY, maxX, maxY);
  // Park to the right with a gap larger than abut tolerance so no false edge forms.
  monitor.m_worldX = maxX + kEdgeAbutTolerance + 64;
  monitor.m_worldY = minY;
  monitor.m_needsPlacement = true;
}

PoseMergeResult mergeReportedDisplays(
    MachineLayout &machine, const std::vector<DisplayRect> &reported, bool overwriteWorld
)
{
  PoseMergeResult result;
  if (reported.empty()) {
    return result;
  }

  std::vector<DisplayRect> merged;
  merged.reserve(reported.size());

  WorkspaceLayout scratch;
  scratch.m_machines.push_back(machine);

  for (const auto &incoming : reported) {
    DisplayRect display = incoming;
    display.ensureLayoutSizes();

    const DisplayRect *existing = findExistingById(machine, display.m_id);
    if (existing == nullptr) {
      existing = findExistingByFingerprint(machine, display);
    }

    if (existing != nullptr && !overwriteWorld) {
      display.m_worldX = existing->m_worldX;
      display.m_worldY = existing->m_worldY;
      if (existing->m_layoutWidth > 0) {
        display.m_layoutWidth = existing->m_layoutWidth;
      }
      if (existing->m_layoutHeight > 0) {
        display.m_layoutHeight = existing->m_layoutHeight;
      }
      display.m_needsPlacement = existing->m_needsPlacement;
    } else if (existing == nullptr && !overwriteWorld) {
      // Never invent world=local adjacency for unknown monitors.
      scratch.m_machines[0].m_monitors = merged;
      parkMonitorAwayFromLayout(scratch, display);
      result.m_parkedCount += 1;
    }

    if (existing == nullptr || existing->m_localX != display.m_localX || existing->m_localY != display.m_localY ||
        existing->m_width != display.m_width || existing->m_height != display.m_height ||
        existing->m_worldX != display.m_worldX || existing->m_worldY != display.m_worldY ||
        existing->m_dpi != display.m_dpi || std::abs(existing->m_scale - display.m_scale) > 0.001f ||
        existing->m_layoutWidth != display.m_layoutWidth || existing->m_layoutHeight != display.m_layoutHeight) {
      result.m_changed = true;
    }

    merged.push_back(std::move(display));
  }

  if (machine.m_monitors.size() != merged.size()) {
    result.m_changed = true;
  }

  machine.m_monitors = std::move(merged);
  machine.ensureLayoutSizes();
  return result;
}

} // namespace deskflow::server
