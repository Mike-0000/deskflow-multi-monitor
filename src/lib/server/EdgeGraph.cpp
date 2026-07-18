/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "server/EdgeGraph.h"

#include <algorithm>
#include <cmath>

namespace deskflow::server {

namespace {

bool intervalsOverlap(int32_t startA, int32_t endA, int32_t startB, int32_t endB, int32_t &overlapStart, int32_t &overlapEnd)
{
  overlapStart = std::max(startA, startB);
  overlapEnd = std::min(endA, endB);
  return overlapEnd > overlapStart;
}

} // namespace

Direction EdgeSegmentGraph::oppositeDirection(Direction dir)
{
  using enum Direction;
  switch (dir) {
  case Left:
    return Right;
  case Right:
    return Left;
  case Top:
    return Bottom;
  case Bottom:
    return Top;
  case NoDirection:
    return NoDirection;
  }
  return NoDirection;
}

EdgeSegmentGraph::EdgeSegmentGraph(const WorkspaceLayout &layout, int32_t abutTolerance)
{
  rebuild(layout, abutTolerance);
}

void EdgeSegmentGraph::rebuild(const WorkspaceLayout &layout, int32_t abutTolerance)
{
  m_segments.clear();
  if (!layout.m_enabled) {
    return;
  }

  for (const auto &srcMachine : layout.m_machines) {
    for (const auto &srcMonitor : srcMachine.m_monitors) {
      for (const auto &dstMachine : layout.m_machines) {
        if (dstMachine.m_name == srcMachine.m_name) {
          continue;
        }
        for (const auto &dstMonitor : dstMachine.m_monitors) {
          considerPair(srcMachine, srcMonitor, dstMachine, dstMonitor, abutTolerance);
        }
      }
    }
  }
}

void EdgeSegmentGraph::considerPair(
    const MachineLayout &srcMachine, const DisplayRect &srcMonitor, const MachineLayout &dstMachine,
    const DisplayRect &dstMonitor, int32_t abutTolerance
)
{
  using enum Direction;

  const int32_t srcLeft = srcMonitor.m_worldX;
  const int32_t srcRight = srcMonitor.m_worldX + srcMonitor.layoutWidth();
  const int32_t srcTop = srcMonitor.m_worldY;
  const int32_t srcBottom = srcMonitor.m_worldY + srcMonitor.layoutHeight();

  const int32_t dstLeft = dstMonitor.m_worldX;
  const int32_t dstRight = dstMonitor.m_worldX + dstMonitor.layoutWidth();
  const int32_t dstTop = dstMonitor.m_worldY;
  const int32_t dstBottom = dstMonitor.m_worldY + dstMonitor.layoutHeight();

  auto emitIfOverlap = [&](Direction exitDir, int32_t srcEdge, int32_t dstEdge, int32_t srcAlongStart,
                           int32_t srcAlongEnd, int32_t dstAlongStart, int32_t dstAlongEnd) {
    if (std::abs(srcEdge - dstEdge) > abutTolerance) {
      return;
    }
    int32_t overlapStart = 0;
    int32_t overlapEnd = 0;
    if (!intervalsOverlap(srcAlongStart, srcAlongEnd, dstAlongStart, dstAlongEnd, overlapStart, overlapEnd)) {
      return;
    }

    EdgeSegment segment;
    segment.m_srcMachine = srcMachine.m_name;
    segment.m_dstMachine = dstMachine.m_name;
    segment.m_srcMonitorId = srcMonitor.m_id.empty() ? srcMonitor.m_name : srcMonitor.m_id;
    segment.m_dstMonitorId = dstMonitor.m_id.empty() ? dstMonitor.m_name : dstMonitor.m_id;
    segment.m_srcExit = exitDir;
    segment.m_worldAlongStart = overlapStart;
    segment.m_worldAlongEnd = overlapEnd;
    segment.m_worldAbutCoord = srcEdge;
    m_segments.push_back(std::move(segment));
  };

  // Leave src Right → enter dst Left
  emitIfOverlap(Right, srcRight, dstLeft, srcTop, srcBottom, dstTop, dstBottom);
  // Leave src Left → enter dst Right
  emitIfOverlap(Left, srcLeft, dstRight, srcTop, srcBottom, dstTop, dstBottom);
  // Leave src Bottom → enter dst Top
  emitIfOverlap(Bottom, srcBottom, dstTop, srcLeft, srcRight, dstLeft, dstRight);
  // Leave src Top → enter dst Bottom
  emitIfOverlap(Top, srcTop, dstBottom, srcLeft, srcRight, dstLeft, dstRight);
}

uint32_t EdgeSegmentGraph::activeSidesForMachine(const std::string &machineName) const
{
  using enum DirectionMask;
  uint32_t sides = static_cast<uint32_t>(NoDirMask);
  for (const auto &segment : m_segments) {
    if (segment.m_srcMachine != machineName) {
      continue;
    }
    switch (segment.m_srcExit) {
    case Direction::Left:
      sides |= static_cast<uint32_t>(LeftMask);
      break;
    case Direction::Right:
      sides |= static_cast<uint32_t>(RightMask);
      break;
    case Direction::Top:
      sides |= static_cast<uint32_t>(TopMask);
      break;
    case Direction::Bottom:
      sides |= static_cast<uint32_t>(BottomMask);
      break;
    case Direction::NoDirection:
      break;
    }
  }
  return sides;
}

std::optional<EdgeSegment> EdgeSegmentGraph::findSegmentAtExit(
    const std::string &srcMachine, const std::string &srcMonitorId, Direction exitDir, int32_t worldAlongEdge
) const
{
  const EdgeSegment *best = nullptr;
  int32_t bestOverlap = 0;

  for (const auto &segment : m_segments) {
    if (segment.m_srcMachine != srcMachine || segment.m_srcExit != exitDir) {
      continue;
    }
    if (!srcMonitorId.empty() && segment.m_srcMonitorId != srcMonitorId) {
      continue;
    }
    if (worldAlongEdge < segment.m_worldAlongStart || worldAlongEdge >= segment.m_worldAlongEnd) {
      continue;
    }
    const int32_t overlap = segment.m_worldAlongEnd - segment.m_worldAlongStart;
    if (overlap > bestOverlap) {
      bestOverlap = overlap;
      best = &segment;
    }
  }

  if (best == nullptr) {
    return std::nullopt;
  }
  return *best;
}

bool EdgeSegmentGraph::hasReverseSegment(
    const std::string &dstMachine, const std::string &dstMonitorId, Direction srcExitDirection
) const
{
  const Direction reverseExit = oppositeDirection(srcExitDirection);
  for (const auto &segment : m_segments) {
    if (segment.m_srcMachine == dstMachine && segment.m_srcExit == reverseExit &&
        (dstMonitorId.empty() || segment.m_srcMonitorId == dstMonitorId)) {
      return true;
    }
  }
  return false;
}

} // namespace deskflow::server
