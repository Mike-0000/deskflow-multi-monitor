/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "server/DisplayLayout.h"

#include "server/EdgeGraph.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace deskflow::server {

namespace {

constexpr int32_t kExitEdgeMargin = 1;
constexpr int32_t kExitOvershootTolerance = 256;

int32_t scaleCoord(int32_t value, int32_t fromExtent, int32_t toExtent)
{
  if (fromExtent <= 0) {
    return 0;
  }
  // Round half away from zero for stable bijective-ish mapping.
  const int64_t num = static_cast<int64_t>(value) * static_cast<int64_t>(toExtent);
  const int64_t den = static_cast<int64_t>(fromExtent);
  if (num >= 0) {
    return static_cast<int32_t>((num + den / 2) / den);
  }
  return static_cast<int32_t>((num - den / 2) / den);
}

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

int32_t layoutSizeFromPixels(int32_t pixels, int32_t dpi, float scale)
{
  if (pixels <= 0) {
    return 0;
  }
  if (scale > 0.0f && std::abs(scale - 1.0f) > 0.001f) {
    return std::max(1, static_cast<int32_t>(std::lround(static_cast<double>(pixels) / static_cast<double>(scale))));
  }
  const int32_t safeDpi = dpi > 0 ? dpi : kReferenceDpi;
  return std::max(1, static_cast<int32_t>((static_cast<int64_t>(pixels) * kReferenceDpi + safeDpi / 2) / safeDpi));
}

bool DisplayRect::containsLocal(int32_t x, int32_t y) const
{
  return x >= m_localX && x < m_localX + m_width && y >= m_localY && y < m_localY + m_height;
}

bool DisplayRect::containsWorld(int32_t x, int32_t y) const
{
  return x >= m_worldX && x < m_worldX + layoutWidth() && y >= m_worldY && y < m_worldY + layoutHeight();
}

int32_t DisplayRect::layoutWidth() const
{
  if (m_layoutWidth > 0) {
    return m_layoutWidth;
  }
  return layoutSizeFromPixels(m_width, m_dpi, m_scale);
}

int32_t DisplayRect::layoutHeight() const
{
  if (m_layoutHeight > 0) {
    return m_layoutHeight;
  }
  return layoutSizeFromPixels(m_height, m_dpi, m_scale);
}

void DisplayRect::ensureLayoutSizes()
{
  if (m_layoutWidth <= 0) {
    m_layoutWidth = layoutSizeFromPixels(m_width, m_dpi, m_scale);
  }
  if (m_layoutHeight <= 0) {
    m_layoutHeight = layoutSizeFromPixels(m_height, m_dpi, m_scale);
  }
}

int32_t DisplayRect::localToWorldX(int32_t localX) const
{
  return m_worldX + scaleCoord(localX - m_localX, m_width, layoutWidth());
}

int32_t DisplayRect::localToWorldY(int32_t localY) const
{
  return m_worldY + scaleCoord(localY - m_localY, m_height, layoutHeight());
}

int32_t DisplayRect::worldToLocalX(int32_t worldX) const
{
  return m_localX + scaleCoord(worldX - m_worldX, layoutWidth(), m_width);
}

int32_t DisplayRect::worldToLocalY(int32_t worldY) const
{
  return m_localY + scaleCoord(worldY - m_worldY, layoutHeight(), m_height);
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

const DisplayRect *MachineLayout::findMonitorById(const std::string &id) const
{
  if (id.empty()) {
    return nullptr;
  }
  for (const auto &monitor : m_monitors) {
    if (monitor.m_id == id || monitor.m_name == id) {
      return &monitor;
    }
  }
  return nullptr;
}

DisplayRect *MachineLayout::findMonitorById(const std::string &id)
{
  return const_cast<DisplayRect *>(static_cast<const MachineLayout *>(this)->findMonitorById(id));
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

void MachineLayout::ensureLayoutSizes()
{
  for (auto &monitor : m_monitors) {
    monitor.ensureLayoutSizes();
  }
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

void WorkspaceLayout::ensureLayoutSizes()
{
  for (auto &machine : m_machines) {
    machine.ensureLayoutSizes();
  }
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

  const int32_t worldX = srcMonitor->localToWorldX(localX);
  const int32_t worldY = srcMonitor->localToWorldY(localY);
  const int32_t exitAlongEdge = (exitDir == Direction::Left || exitDir == Direction::Right) ? worldY : worldX;
  const std::string srcMonitorId = srcMonitor->m_id.empty() ? srcMonitor->m_name : srcMonitor->m_id;

  EdgeSegmentGraph graph(m_layout);
  const auto segment = graph.findSegmentAtExit(srcMachine, srcMonitorId, exitDir, exitAlongEdge);
  if (!segment.has_value()) {
    return std::nullopt;
  }

  const MachineLayout *dstMachine = m_layout.findMachine(segment->m_dstMachine);
  if (dstMachine == nullptr) {
    return std::nullopt;
  }
  const DisplayRect *dstMonitor = dstMachine->findMonitorById(segment->m_dstMonitorId);
  if (dstMonitor == nullptr) {
    return std::nullopt;
  }

  TransitionResult result;
  result.m_valid = true;
  result.m_dstMachine = segment->m_dstMachine;
  result.m_dstMonitorId = segment->m_dstMonitorId;
  result.m_direction = exitDir;
  result.m_hasReverseNeighbor =
      graph.hasReverseSegment(segment->m_dstMachine, segment->m_dstMonitorId, exitDir);

  using enum Direction;
  switch (exitDir) {
  case Left:
    result.m_dstX = dstMonitor->m_localX + dstMonitor->m_width - 1;
    result.m_dstY = dstMonitor->worldToLocalY(worldY);
    break;
  case Right:
    result.m_dstX = dstMonitor->m_localX;
    result.m_dstY = dstMonitor->worldToLocalY(worldY);
    break;
  case Top:
    result.m_dstX = dstMonitor->worldToLocalX(worldX);
    result.m_dstY = dstMonitor->m_localY + dstMonitor->m_height - 1;
    break;
  case Bottom:
    result.m_dstX = dstMonitor->worldToLocalX(worldX);
    result.m_dstY = dstMonitor->m_localY;
    break;
  default:
    return std::nullopt;
  }

  clampToMonitor(*dstMonitor, result.m_dstX, result.m_dstY);
  return result;
}

bool GeometryRouter::hasReverseNeighbor(
    const std::string &dstMachine, const std::string &dstMonitorId, Direction enteredFromSrcExit
) const
{
  EdgeSegmentGraph graph(m_layout);
  return graph.hasReverseSegment(dstMachine, dstMonitorId, enteredFromSrcExit);
}

uint32_t GeometryRouter::getActiveSidesForMachine(const std::string &machineName) const
{
  if (!m_layout.m_enabled) {
    return static_cast<uint32_t>(DirectionMask::NoDirMask);
  }
  EdgeSegmentGraph graph(m_layout);
  return graph.activeSidesForMachine(machineName);
}

} // namespace deskflow::server
