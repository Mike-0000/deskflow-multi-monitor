/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "server/DisplayLayout.h"

#include <vector>

namespace deskflow::server {

struct PoseMergeResult
{
  bool m_changed = false;
  int32_t m_parkedCount = 0;
};

//! Merge client-reported local geometry into a machine layout without scrambling world poses.
PoseMergeResult mergeReportedDisplays(
    MachineLayout &machine, const std::vector<DisplayRect> &reported, bool overwriteWorld
);

//! Park a monitor away from existing world geometry so it does not invent false adjacency.
void parkMonitorAwayFromLayout(WorkspaceLayout &layout, DisplayRect &monitor);

} // namespace deskflow::server
