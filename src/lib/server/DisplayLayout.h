/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "base/DirectionTypes.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace deskflow::server {

//! A single physical display within a machine layout
struct DisplayRect
{
  std::string m_id;
  std::string m_name;
  int32_t m_worldX = 0;
  int32_t m_worldY = 0;
  int32_t m_width = 0;
  int32_t m_height = 0;
  int32_t m_localX = 0;
  int32_t m_localY = 0;
  float m_scale = 1.0f;
  int32_t m_dpi = 96;

  bool containsLocal(int32_t x, int32_t y) const;
  bool containsWorld(int32_t x, int32_t y) const;
  int32_t localToWorldX(int32_t localX) const;
  int32_t localToWorldY(int32_t localY) const;
  int32_t worldToLocalX(int32_t worldX) const;
  int32_t worldToLocalY(int32_t worldY) const;
};

//! All displays attached to one DeskFlow screen/client
struct MachineLayout
{
  std::string m_name;
  std::vector<DisplayRect> m_monitors;

  const DisplayRect *findMonitorAtLocal(int32_t x, int32_t y) const;
  const DisplayRect *findMonitorAtWorld(int32_t x, int32_t y) const;
  void getBoundingLocal(int32_t &x, int32_t &y, int32_t &w, int32_t &h) const;
};

//! Global workspace containing all machine layouts
struct WorkspaceLayout
{
  bool m_enabled = false;
  std::vector<MachineLayout> m_machines;

  const MachineLayout *findMachine(const std::string &name) const;
  MachineLayout *findMachine(const std::string &name);
};

//! Result of a geometry-based screen transition
struct TransitionResult
{
  bool m_valid = false;
  std::string m_dstMachine;
  int32_t m_dstX = 0;
  int32_t m_dstY = 0;
  Direction m_direction = Direction::NoDirection;
};

//! Routes cursor transitions using per-monitor world geometry
class GeometryRouter
{
public:
  explicit GeometryRouter(const WorkspaceLayout &layout);

  //! Find a cross-machine transition when the cursor exits at local coordinates.
  std::optional<TransitionResult> findTransition(
      const std::string &srcMachine, int32_t localX, int32_t localY, Direction exitDir
  ) const;

  //! Find which monitor on a machine contains local coordinates.
  static const DisplayRect *findMonitorAtLocal(const MachineLayout &machine, int32_t x, int32_t y);

  //! Detect exit direction from monitor edge at local coordinates.
  static Direction detectExitDirection(const DisplayRect &monitor, int32_t localX, int32_t localY, int32_t margin = 1);

  //! Clamp coordinates to monitor bounds.
  static void clampToMonitor(const DisplayRect &monitor, int32_t &x, int32_t &y);

  //! Return a DirectionMask bitmask of edges that can transition to another machine.
  uint32_t getActiveSidesForMachine(const std::string &machineName) const;

private:
  std::optional<TransitionResult> findNeighborAcrossEdge(
      const MachineLayout &srcMachine, const DisplayRect &srcMonitor, int32_t localX, int32_t localY, Direction exitDir
  ) const;

  const WorkspaceLayout &m_layout;
};

} // namespace deskflow::server
