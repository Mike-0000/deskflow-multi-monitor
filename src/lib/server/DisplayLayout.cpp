/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "server/DisplayLayout.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace deskflow::server {

namespace {

constexpr int32_t kExitEdgeMargin = 1;
constexpr int32_t kExitOvershootTolerance = 256;

bool isAtExitEdge(const DisplayRect &monitor, int32_t localX, int32_t localY, Direction exitDir)
{
  const int32_t left = monitor.m_localX;
  const int32_t right = monitor.m_localX + monitor.m_width - 1;
  const int32_t top = monitor.m_localY;
  const int32_t bottom = monitor.m_localY + monitor.m_height - 1;

  switch (exitDir) {
  case Direction::Left:
    return localX >= left - kExitEdgeMargin && localX <= left + kExitEdgeMargin &&
           localY >= top - kExitEdgeMargin && localY <= bottom + kExitEdgeMargin;
  case Direction::Right:
    return localX >= right - kExitEdgeMargin && localX <= right + kExitEdgeMargin &&
           localY >= top - kExitEdgeMargin && localY <= bottom + kExitEdgeMargin;
  case Direction::Top:
    return localY >= top - kExitEdgeMargin && localY <= top + kExitEdgeMargin &&
           localX >= left - kExitEdgeMargin && localX <= right + kExitEdgeMargin;
  case Direction::Bottom:
    return localY >= bottom - kExitEdgeMargin && localY <= bottom + kExitEdgeMargin &&
           localX >= left - kExitEdgeMargin && localX <= right + kExitEdgeMargin;
  case Direction::NoDirection:
    return false;
  }

  return false;
}

const DisplayRect *findMonitorNearExit(const MachineLayout &machine, int32_t localX, int32_t localY, Direction exitDir)
{
  const DisplayRect *bestMonitor = nullptr;
  int32_t bestDistance = kExitOvershootTolerance + 1;

  auto consider = [&](const DisplayRect &monitor, bool candidate, int32_t distance) {
    if (candidate && distance < bestDistance) {
      bestDistance = distance;
      bestMonitor = &monitor;
    }
  };

  for (const auto &monitor : machine.m_monitors) {
    const int32_t left = monitor.m_localX;
    const int32_t right = monitor.m_localX + monitor.m_width - 1;
    const int32_t top = monitor.m_localY;
    const int32_t bottom = monitor.m_localY + monitor.m_height - 1;
    const bool xOverlaps = localX >= left - kExitOvershootTolerance && localX <= right + kExitOvershootTolerance;
    const bool yOverlaps = localY >= top - kExitOvershootTolerance && localY <= bottom + kExitOvershootTolerance;

    switch (exitDir) {
    case Direction::Left:
      consider(
          monitor, yOverlaps && localX <= left + kExitEdgeMargin && localX >= left - kExitOvershootTolerance,
          std::abs(localX - left)
      );
      break;
    case Direction::Right:
      consider(
          monitor, yOverlaps && localX >= right - kExitEdgeMargin && localX <= right + kExitOvershootTolerance,
          std::abs(localX - right)
      );
      break;
    case Direction::Top:
      consider(
          monitor, xOverlaps && localY <= top + kExitEdgeMargin && localY >= top - kExitOvershootTolerance,
          std::abs(localY - top)
      );
      break;
    case Direction::Bottom:
      consider(
          monitor, xOverlaps && localY >= bottom - kExitEdgeMargin && localY <= bottom + kExitOvershootTolerance,
          std::abs(localY - bottom)
      );
      break;
    case Direction::NoDirection:
      break;
    }
  }

  return bestMonitor;
}

void projectToExitEdge(const DisplayRect &monitor, Direction exitDir, int32_t &localX, int32_t &localY)
{
  localX = std::clamp(localX, monitor.m_localX, monitor.m_localX + monitor.m_width - 1);
  localY = std::clamp(localY, monitor.m_localY, monitor.m_localY + monitor.m_height - 1);

  switch (exitDir) {
  case Direction::Left:
    localX = monitor.m_localX;
    break;
  case Direction::Right:
    localX = monitor.m_localX + monitor.m_width - 1;
    break;
  case Direction::Top:
    localY = monitor.m_localY;
    break;
  case Direction::Bottom:
    localY = monitor.m_localY + monitor.m_height - 1;
    break;
  case Direction::NoDirection:
    break;
  }
}

} // namespace

bool DisplayRect::containsLocal(int32_t x, int32_t y) const
{
  return x >= m_localX && x < m_localX + m_width && y >= m_localY && y < m_localY + m_height;
}

bool DisplayRect::containsWorld(int32_t x, int32_t y) const
{
  return x >= m_worldX && x < m_worldX + m_width && y >= m_worldY && y < m_worldY + m_height;
}

int32_t DisplayRect::localToWorldX(int32_t localX) const
{
  return m_worldX + (localX - m_localX);
}

int32_t DisplayRect::localToWorldY(int32_t localY) const
{
  return m_worldY + (localY - m_localY);
}

int32_t DisplayRect::worldToLocalX(int32_t worldX) const
{
  return m_localX + (worldX - m_worldX);
}

int32_t DisplayRect::worldToLocalY(int32_t worldY) const
{
  return m_localY + (worldY - m_worldY);
}

const DisplayRect *MachineLayout::findMonitorAtLocal(int32_t x, int32_t y) const
{
  for (const auto &monitor : m_monitors) {
    if (monitor.containsLocal(x, y)) {
      return &monitor;
    }
  }
  return nullptr;
}

const DisplayRect *MachineLayout::findMonitorAtWorld(int32_t x, int32_t y) const
{
  for (const auto &monitor : m_monitors) {
    if (monitor.containsWorld(x, y)) {
      return &monitor;
    }
  }
  return nullptr;
}

void MachineLayout::getBoundingLocal(int32_t &x, int32_t &y, int32_t &w, int32_t &h) const
{
  if (m_monitors.empty()) {
    x = y = w = h = 0;
    return;
  }

  int32_t minX = std::numeric_limits<int32_t>::max();
  int32_t minY = std::numeric_limits<int32_t>::max();
  int32_t maxX = std::numeric_limits<int32_t>::min();
  int32_t maxY = std::numeric_limits<int32_t>::min();

  for (const auto &monitor : m_monitors) {
    minX = std::min(minX, monitor.m_localX);
    minY = std::min(minY, monitor.m_localY);
    maxX = std::max(maxX, monitor.m_localX + monitor.m_width);
    maxY = std::max(maxY, monitor.m_localY + monitor.m_height);
  }

  x = minX;
  y = minY;
  w = maxX - minX;
  h = maxY - minY;
}

const MachineLayout *WorkspaceLayout::findMachine(const std::string &name) const
{
  for (const auto &machine : m_machines) {
    if (machine.m_name == name) {
      return &machine;
    }
  }
  return nullptr;
}

MachineLayout *WorkspaceLayout::findMachine(const std::string &name)
{
  for (auto &machine : m_machines) {
    if (machine.m_name == name) {
      return &machine;
    }
  }
  return nullptr;
}

GeometryRouter::GeometryRouter(const WorkspaceLayout &layout) : m_layout(layout)
{
}

const DisplayRect *GeometryRouter::findMonitorAtLocal(const MachineLayout &machine, int32_t x, int32_t y)
{
  return machine.findMonitorAtLocal(x, y);
}

Direction GeometryRouter::detectExitDirection(const DisplayRect &monitor, int32_t localX, int32_t localY, int32_t margin)
{
  using enum Direction;

  const int32_t left = monitor.m_localX;
  const int32_t right = monitor.m_localX + monitor.m_width - 1;
  const int32_t top = monitor.m_localY;
  const int32_t bottom = monitor.m_localY + monitor.m_height - 1;

  const bool atLeft = localX <= left + margin;
  const bool atRight = localX >= right - margin;
  const bool atTop = localY <= top + margin;
  const bool atBottom = localY >= bottom - margin;

  if (atLeft && atTop) {
    return Left;
  }
  if (atRight && atTop) {
    return Right;
  }
  if (atLeft && atBottom) {
    return Left;
  }
  if (atRight && atBottom) {
    return Right;
  }
  if (atLeft) {
    return Left;
  }
  if (atRight) {
    return Right;
  }
  if (atTop) {
    return Top;
  }
  if (atBottom) {
    return Bottom;
  }

  return NoDirection;
}

void GeometryRouter::clampToMonitor(const DisplayRect &monitor, int32_t &x, int32_t &y)
{
  const int32_t left = monitor.m_localX;
  const int32_t top = monitor.m_localY;
  const int32_t right = monitor.m_localX + monitor.m_width - 1;
  const int32_t bottom = monitor.m_localY + monitor.m_height - 1;

  if (x < left) {
    x = left;
  } else if (x > right) {
    x = right;
  }
  if (y < top) {
    y = top;
  } else if (y > bottom) {
    y = bottom;
  }
}

std::optional<TransitionResult> GeometryRouter::findTransition(
    const std::string &srcMachine, int32_t localX, int32_t localY, Direction exitDir
) const
{
  if (!m_layout.m_enabled) {
    return std::nullopt;
  }

  const MachineLayout *src = m_layout.findMachine(srcMachine);
  if (src == nullptr) {
    return std::nullopt;
  }

  bool projectedToEdge = false;
  const DisplayRect *srcMonitor = src->findMonitorAtLocal(localX, localY);
  if (srcMonitor == nullptr) {
    srcMonitor = findMonitorNearExit(*src, localX, localY, exitDir);
    projectedToEdge = srcMonitor != nullptr;
  }

  if (srcMonitor == nullptr) {
    return std::nullopt;
  }

  if (exitDir == Direction::NoDirection) {
    exitDir = detectExitDirection(*srcMonitor, localX, localY);
  }

  if (exitDir == Direction::NoDirection) {
    return std::nullopt;
  }

  if (projectedToEdge) {
    projectToExitEdge(*srcMonitor, exitDir, localX, localY);
  }

  if (!isAtExitEdge(*srcMonitor, localX, localY, exitDir)) {
    return std::nullopt;
  }

  return findNeighborAcrossEdge(*src, *srcMonitor, localX, localY, exitDir);
}

std::optional<TransitionResult> GeometryRouter::findNeighborAcrossEdge(
    const MachineLayout &srcMachine, const DisplayRect &srcMonitor, int32_t localX, int32_t localY, Direction exitDir
) const
{
  using enum Direction;

  const int32_t worldX = srcMonitor.localToWorldX(localX);
  const int32_t worldY = srcMonitor.localToWorldY(localY);

  int32_t srcEdgeStart = 0;
  int32_t srcEdgeEnd = 0;
  int32_t exitWorldCoord = 0;
  Direction neighborSide = NoDirection;

  switch (exitDir) {
  case Left:
    srcEdgeStart = srcMonitor.m_worldY;
    srcEdgeEnd = srcMonitor.m_worldY + srcMonitor.m_height;
    exitWorldCoord = srcMonitor.m_worldX;
    neighborSide = Right;
    break;
  case Right:
    srcEdgeStart = srcMonitor.m_worldY;
    srcEdgeEnd = srcMonitor.m_worldY + srcMonitor.m_height;
    exitWorldCoord = srcMonitor.m_worldX + srcMonitor.m_width;
    neighborSide = Left;
    break;
  case Top:
    srcEdgeStart = srcMonitor.m_worldX;
    srcEdgeEnd = srcMonitor.m_worldX + srcMonitor.m_width;
    exitWorldCoord = srcMonitor.m_worldY;
    neighborSide = Bottom;
    break;
  case Bottom:
    srcEdgeStart = srcMonitor.m_worldX;
    srcEdgeEnd = srcMonitor.m_worldX + srcMonitor.m_width;
    exitWorldCoord = srcMonitor.m_worldY + srcMonitor.m_height;
    neighborSide = Top;
    break;
  default:
    return std::nullopt;
  }

  const int32_t exitAlongEdge = (exitDir == Left || exitDir == Right) ? worldY : worldX;

  const DisplayRect *bestDst = nullptr;
  const MachineLayout *bestMachine = nullptr;
  int32_t bestOverlap = 0;

  for (const auto &dstMachine : m_layout.m_machines) {
    if (dstMachine.m_name == srcMachine.m_name) {
      continue;
    }

    for (const auto &dstMonitor : dstMachine.m_monitors) {
      int32_t dstEdgeStart = 0;
      int32_t dstEdgeEnd = 0;
      int32_t dstEdgeCoord = 0;

      switch (neighborSide) {
      case Right:
        dstEdgeStart = dstMonitor.m_worldY;
        dstEdgeEnd = dstMonitor.m_worldY + dstMonitor.m_height;
        dstEdgeCoord = dstMonitor.m_worldX + dstMonitor.m_width;
        break;
      case Left:
        dstEdgeStart = dstMonitor.m_worldY;
        dstEdgeEnd = dstMonitor.m_worldY + dstMonitor.m_height;
        dstEdgeCoord = dstMonitor.m_worldX;
        break;
      case Bottom:
        dstEdgeStart = dstMonitor.m_worldX;
        dstEdgeEnd = dstMonitor.m_worldX + dstMonitor.m_width;
        dstEdgeCoord = dstMonitor.m_worldY + dstMonitor.m_height;
        break;
      case Top:
        dstEdgeStart = dstMonitor.m_worldX;
        dstEdgeEnd = dstMonitor.m_worldX + dstMonitor.m_width;
        dstEdgeCoord = dstMonitor.m_worldY;
        break;
      default:
        continue;
      }

      if (std::abs(dstEdgeCoord - exitWorldCoord) > 1) {
        continue;
      }

      const int32_t overlapStart = std::max(srcEdgeStart, dstEdgeStart);
      const int32_t overlapEnd = std::min(srcEdgeEnd, dstEdgeEnd);
      if (overlapEnd <= overlapStart) {
        continue;
      }

      if (exitAlongEdge < overlapStart || exitAlongEdge >= overlapEnd) {
        continue;
      }

      const int32_t overlap = overlapEnd - overlapStart;
      if (overlap > bestOverlap) {
        bestOverlap = overlap;
        bestDst = &dstMonitor;
        bestMachine = &dstMachine;
      }
    }
  }

  if (bestDst == nullptr || bestMachine == nullptr) {
    return std::nullopt;
  }

  TransitionResult result;
  result.m_valid = true;
  result.m_dstMachine = bestMachine->m_name;
  result.m_direction = exitDir;

  switch (neighborSide) {
  case Left:
    result.m_dstX = bestDst->worldToLocalX(exitWorldCoord);
    result.m_dstY = bestDst->worldToLocalY(worldY);
    break;
  case Right:
    result.m_dstX = bestDst->worldToLocalX(exitWorldCoord - 1);
    result.m_dstY = bestDst->worldToLocalY(worldY);
    break;
  case Top:
    result.m_dstX = bestDst->worldToLocalX(worldX);
    result.m_dstY = bestDst->worldToLocalY(exitWorldCoord);
    break;
  case Bottom:
    result.m_dstX = bestDst->worldToLocalX(worldX);
    result.m_dstY = bestDst->worldToLocalY(exitWorldCoord - 1);
    break;
  default:
    return std::nullopt;
  }

  clampToMonitor(*bestDst, result.m_dstX, result.m_dstY);
  return result;
}

uint32_t GeometryRouter::getActiveSidesForMachine(const std::string &machineName) const
{
  using enum DirectionMask;

  if (!m_layout.m_enabled) {
    return static_cast<uint32_t>(NoDirMask);
  }

  const MachineLayout *machine = m_layout.findMachine(machineName);
  if (machine == nullptr) {
    return static_cast<uint32_t>(NoDirMask);
  }

  uint32_t sides = static_cast<uint32_t>(NoDirMask);
  for (const auto &monitor : machine->m_monitors) {
    const int32_t left = monitor.m_localX;
    const int32_t right = monitor.m_localX + monitor.m_width - 1;
    const int32_t top = monitor.m_localY;
    const int32_t bottom = monitor.m_localY + monitor.m_height - 1;
    const int32_t midX = monitor.m_localX + monitor.m_width / 2;
    const int32_t midY = monitor.m_localY + monitor.m_height / 2;

    if (findTransition(machineName, left, midY, Direction::Left)) {
      sides |= static_cast<uint32_t>(LeftMask);
    }
    if (findTransition(machineName, right, midY, Direction::Right)) {
      sides |= static_cast<uint32_t>(RightMask);
    }
    if (findTransition(machineName, midX, top, Direction::Top)) {
      sides |= static_cast<uint32_t>(TopMask);
    }
    if (findTransition(machineName, midX, bottom, Direction::Bottom)) {
      sides |= static_cast<uint32_t>(BottomMask);
    }
  }

  return sides;
}

} // namespace deskflow::server
