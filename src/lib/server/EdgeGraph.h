/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "base/DirectionTypes.h"
#include "server/DisplayLayout.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace deskflow::server {

//! A cross-machine abutting edge segment in density-independent world space.
struct EdgeSegment
{
  std::string m_srcMachine;
  std::string m_dstMachine;
  std::string m_srcMonitorId;
  std::string m_dstMonitorId;
  Direction m_srcExit = Direction::NoDirection;
  int32_t m_worldAlongStart = 0; //!< inclusive
  int32_t m_worldAlongEnd = 0;   //!< exclusive
  int32_t m_worldAbutCoord = 0;
};

//! Precomputed abutting edge segments for a workspace layout.
class EdgeSegmentGraph
{
public:
  EdgeSegmentGraph() = default;
  explicit EdgeSegmentGraph(const WorkspaceLayout &layout, int32_t abutTolerance = kEdgeAbutTolerance);

  void rebuild(const WorkspaceLayout &layout, int32_t abutTolerance = kEdgeAbutTolerance);

  const std::vector<EdgeSegment> &segments() const
  {
    return m_segments;
  }

  uint32_t activeSidesForMachine(const std::string &machineName) const;

  std::optional<EdgeSegment> findSegmentAtExit(
      const std::string &srcMachine, const std::string &srcMonitorId, Direction exitDir, int32_t worldAlongEdge
  ) const;

  bool hasReverseSegment(
      const std::string &dstMachine, const std::string &dstMonitorId, Direction srcExitDirection
  ) const;

  static Direction oppositeDirection(Direction dir);

private:
  void considerPair(
      const MachineLayout &srcMachine, const DisplayRect &srcMonitor, const MachineLayout &dstMachine,
      const DisplayRect &dstMonitor, int32_t abutTolerance
  );

  std::vector<EdgeSegment> m_segments;
};

} // namespace deskflow::server
