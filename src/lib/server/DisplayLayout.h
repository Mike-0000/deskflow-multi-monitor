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

//! Shared abutment tolerance (layout units) for router and editors.
inline constexpr int32_t kEdgeAbutTolerance = 2;

//! Reference DPI for density-independent layout units.
inline constexpr int32_t kReferenceDpi = 96;

//! A single physical display within a machine layout
struct DisplayRect
{
  std::string m_id;
  std::string m_name;
  int32_t m_worldX = 0;
  int32_t m_worldY = 0;
  //! Pixel width/height in the machine's local desktop space.
  int32_t m_width = 0;
  int32_t m_height = 0;
  int32_t m_localX = 0;
  int32_t m_localY = 0;
  float m_scale = 1.0f;
  int32_t m_dpi = kReferenceDpi;
  //! Density-independent layout size in world space. 0 means derive from pixels+dpi.
  int32_t m_layoutWidth = 0;
  int32_t m_layoutHeight = 0;
  bool m_needsPlacement = false;

  bool containsLocal(int32_t x, int32_t y) const;
  bool containsWorld(int32_t x, int32_t y) const;

  int32_t layoutWidth() const;
  int32_t layoutHeight() const;
  void ensureLayoutSizes();

  int32_t localToWorldX(int32_t localX) const;
  int32_t localToWorldY(int32_t localY) const;
  int32_t worldToLocalX(int32_t worldX) const;
  int32_t worldToLocalY(int32_t worldY) const;
};

//! Derive density-independent size from pixel extent and dpi/scale.
int32_t layoutSizeFromPixels(int32_t pixels, int32_t dpi, float scale);

//! All displays attached to one DeskFlow screen/client
struct MachineLayout
{
  std::string m_name;
  std::vector<DisplayRect> m_monitors;

  const DisplayRect *findMonitorAtLocal(int32_t x, int32_t y) const;
  const DisplayRect *findMonitorAtWorld(int32_t x, int32_t y) const;
  const DisplayRect *findMonitorById(const std::string &id) const;
  DisplayRect *findMonitorById(const std::string &id);
  void getBoundingLocal(int32_t &x, int32_t &y, int32_t &w, int32_t &h) const;
  void ensureLayoutSizes();
};

//! Global workspace containing all machine layouts
struct WorkspaceLayout
{
  bool m_enabled = false;
  int32_t m_version = 2;
  std::vector<MachineLayout> m_machines;

  const MachineLayout *findMachine(const std::string &name) const;
  MachineLayout *findMachine(const std::string &name);
  void ensureLayoutSizes();
};

//! Result of a geometry-based screen transition
struct TransitionResult
{
  bool m_valid = false;
  std::string m_dstMachine;
  std::string m_dstMonitorId;
  int32_t m_dstX = 0;
  int32_t m_dstY = 0;
  Direction m_direction = Direction::NoDirection;
  bool m_hasReverseNeighbor = false;
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

  //! True if destination machine has a reverse segment on the entered edge.
  bool hasReverseNeighbor(
      const std::string &dstMachine, const std::string &dstMonitorId, Direction enteredFromSrcExit
  ) const;

private:
  const WorkspaceLayout &m_layout;
};

} // namespace deskflow::server
