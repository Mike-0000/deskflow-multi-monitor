/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "server/SwitchEngine.h"

#include <algorithm>

namespace deskflow::server {

SwitchEngine::SwitchEngine(const WorkspaceLayout &layout) : m_layout(layout), m_router(layout)
{
}

void SwitchEngine::applyLandingInset(TransitionResult &transition, int32_t jumpZoneSize, bool hasReverseNeighbor)
{
  if (!hasReverseNeighbor || transition.m_direction == Direction::NoDirection) {
    return;
  }

  // Inclusive jump-zone probes use <= left+zone and >= right-zone, so landing
  // exactly zone pixels inward still triggers an immediate reverse switch.
  const int32_t inset = std::max<int32_t>(1, jumpZoneSize + 1);
  switch (transition.m_direction) {
  case Direction::Left:
    transition.m_dstX -= inset;
    break;
  case Direction::Right:
    transition.m_dstX += inset;
    break;
  case Direction::Top:
    transition.m_dstY -= inset;
    break;
  case Direction::Bottom:
    transition.m_dstY += inset;
    break;
  case Direction::NoDirection:
    break;
  }
}

bool SwitchEngine::isInJumpZone(
    int32_t x, int32_t y, int32_t localX, int32_t localY, int32_t width, int32_t height, int32_t jumpZoneSize
)
{
  const int32_t zone = std::max<int32_t>(0, jumpZoneSize);
  if (zone <= 0 || width <= 0 || height <= 0) {
    return false;
  }

  const int32_t left = localX;
  const int32_t right = localX + width - 1;
  const int32_t top = localY;
  const int32_t bottom = localY + height - 1;

  return x <= left + zone || x >= right - zone || y <= top + zone || y >= bottom - zone;
}

SwitchCandidate SwitchEngine::candidateFromTransition(
    const TransitionResult &transition, Direction dir, int32_t probeX, int32_t probeY, int32_t xActive, int32_t yActive,
    int32_t jumpZoneSize
) const
{
  SwitchCandidate candidate;
  candidate.m_direction = dir;
  candidate.m_probeX = probeX;
  candidate.m_probeY = probeY;
  candidate.m_xActive = xActive;
  candidate.m_yActive = yActive;
  if (!transition.m_valid) {
    return candidate;
  }

  TransitionResult landed = transition;
  applyLandingInset(landed, jumpZoneSize, transition.m_hasReverseNeighbor);

  const MachineLayout *dstMachine = m_layout.findMachine(landed.m_dstMachine);
  const DisplayRect *dstMonitor =
      dstMachine == nullptr ? nullptr : dstMachine->findMonitorById(landed.m_dstMonitorId);
  if (dstMonitor != nullptr) {
    GeometryRouter::clampToMonitor(*dstMonitor, landed.m_dstX, landed.m_dstY);
  }

  candidate.m_valid = true;
  candidate.m_dstMachine = landed.m_dstMachine;
  candidate.m_dstX = landed.m_dstX;
  candidate.m_dstY = landed.m_dstY;
  return candidate;
}

std::vector<SwitchCandidate> SwitchEngine::collectCandidates(const SwitchEngineCursor &cursor) const
{
  std::vector<SwitchCandidate> candidates;
  if (!m_layout.m_enabled) {
    return candidates;
  }

  const MachineLayout *machine = m_layout.findMachine(cursor.m_machine);
  if (machine == nullptr) {
    return candidates;
  }

  const DisplayRect *monitor = machine->findMonitorAtLocal(cursor.m_x, cursor.m_y);
  if (monitor == nullptr && !cursor.m_absoluteMode) {
    // Secondary overshoot: still try both axes from last known interior point later via probes.
  }

  using enum Direction;
  const int32_t zone = std::max<int32_t>(0, cursor.m_jumpZoneSize);

  if (cursor.m_absoluteMode) {
    if (monitor == nullptr) {
      return candidates;
    }

    const int32_t left = monitor->m_localX;
    const int32_t right = monitor->m_localX + monitor->m_width - 1;
    const int32_t top = monitor->m_localY;
    const int32_t bottom = monitor->m_localY + monitor->m_height - 1;

    int32_t xc = cursor.m_x;
    int32_t yc = cursor.m_y;
    if (xc < left + zone) {
      xc = left;
    } else if (xc >= right - zone + 1) {
      xc = right;
    }
    if (yc < top + zone) {
      yc = top;
    } else if (yc >= bottom - zone + 1) {
      yc = bottom;
    }

    auto tryDir = [&](Direction dir, int32_t probeX, int32_t probeY) {
      const auto transition = m_router.findTransition(cursor.m_machine, probeX, probeY, dir);
      if (!transition.has_value()) {
        return;
      }
      candidates.push_back(
          candidateFromTransition(*transition, dir, probeX, probeY, xc, yc, cursor.m_jumpZoneSize)
      );
    };

    if (cursor.m_x <= left + zone) {
      tryDir(Left, cursor.m_x - zone, cursor.m_y);
    } else if (cursor.m_x >= right - zone) {
      tryDir(Right, cursor.m_x + zone, cursor.m_y);
    }
    if (cursor.m_y <= top + zone) {
      tryDir(Top, cursor.m_x, cursor.m_y - zone);
    } else if (cursor.m_y >= bottom - zone) {
      tryDir(Bottom, cursor.m_x, cursor.m_y + zone);
    }
    return candidates;
  }

  // Secondary / relative mode: cursor may be outside the monitor.
  const DisplayRect *refMonitor = monitor;
  if (refMonitor == nullptr) {
    // Prefer monitor containing the pre-delta point approximation.
    refMonitor = machine->findMonitorAtLocal(cursor.m_x - cursor.m_xDelta, cursor.m_y - cursor.m_yDelta);
  }
  if (refMonitor == nullptr && !machine->m_monitors.empty()) {
    refMonitor = &machine->m_monitors.front();
  }
  if (refMonitor == nullptr) {
    return candidates;
  }

  int32_t xc = cursor.m_x;
  int32_t yc = cursor.m_y;
  GeometryRouter::clampToMonitor(*refMonitor, xc, yc);

  // Prefer the pre-delta interior point for the orthogonal axis. Diagonal
  // overshoot (e.g. left+up) would otherwise clamp Y to the top before
  // mapping and land on the destination's top edge instead of preserving Y.
  const int32_t prevX = cursor.m_x - cursor.m_xDelta;
  const int32_t prevY = cursor.m_y - cursor.m_yDelta;
  int32_t alongX = xc;
  int32_t alongY = yc;
  if (refMonitor->containsLocal(prevX, prevY)) {
    alongX = prevX;
    alongY = prevY;
  }

  const bool pastLeft = cursor.m_x < refMonitor->m_localX;
  const bool pastRight = cursor.m_x > refMonitor->m_localX + refMonitor->m_width - 1;
  const bool pastTop = cursor.m_y < refMonitor->m_localY;
  const bool pastBottom = cursor.m_y > refMonitor->m_localY + refMonitor->m_height - 1;

  auto tryDir = [&](Direction dir) {
    // Keep the overshot coordinate on the exit axis so edge projection still
    // fires; use the along-edge coordinate on the orthogonal axis.
    int32_t probeX = cursor.m_x;
    int32_t probeY = cursor.m_y;
    switch (dir) {
    case Direction::Left:
    case Direction::Right:
      probeY = alongY;
      break;
    case Direction::Top:
    case Direction::Bottom:
      probeX = alongX;
      break;
    case Direction::NoDirection:
      break;
    }

    const auto transition = m_router.findTransition(cursor.m_machine, probeX, probeY, dir);
    if (!transition.has_value()) {
      return;
    }
    candidates.push_back(candidateFromTransition(
        *transition, dir, probeX, probeY, xc, yc, cursor.m_jumpZoneSize
    ));
  };

  // Corner parity with primary: probe both axes when past both edges.
  if (pastLeft) {
    tryDir(Left);
  }
  if (pastRight) {
    tryDir(Right);
  }
  if (pastTop) {
    tryDir(Top);
  }
  if (pastBottom) {
    tryDir(Bottom);
  }

  return candidates;
}

std::optional<TransitionResult> SwitchEngine::resolveDirection(
    const std::string &machine, int32_t localX, int32_t localY, Direction dir
) const
{
  return m_router.findTransition(machine, localX, localY, dir);
}

SwitchEngineResult SwitchEngine::evaluate(SwitchGateState &state, const SwitchEngineRequest &request) const
{
  SwitchEngineResult result;
  const auto candidates = collectCandidates(request.m_cursor);
  const auto best = SwitchGate::pickBestCandidate(candidates, request.m_cursor.m_xDelta, request.m_cursor.m_yDelta);
  if (!best.has_value()) {
    result.m_outcome = SwitchEngineOutcome::None;
    result.m_reason = SwitchGateReason::NoNeighbor;
    SwitchGate::clearPending(state);
    return result;
  }

  result.m_candidate = *best;

  SwitchGateInput input = request.m_gateInput;
  input.m_hasNeighbor = true;
  input.m_direction = best->m_direction;
  input.m_x = best->m_dstX;
  input.m_y = best->m_dstY;
  input.m_xActive = best->m_xActive;
  input.m_yActive = best->m_yActive;
  input.m_xDelta = request.m_cursor.m_xDelta;
  input.m_yDelta = request.m_cursor.m_yDelta;

  const SwitchGateResult gate = SwitchGate::evaluate(state, request.m_settings, input);
  result.m_reason = gate.m_reason;
  switch (gate.m_outcome) {
  case SwitchGateOutcome::Allow:
    result.m_outcome = SwitchEngineOutcome::Allow;
    break;
  case SwitchGateOutcome::Wait:
    result.m_outcome = SwitchEngineOutcome::Wait;
    break;
  case SwitchGateOutcome::Reject:
    result.m_outcome = SwitchEngineOutcome::None;
    break;
  }

  if (result.m_outcome == SwitchEngineOutcome::Allow || result.m_outcome == SwitchEngineOutcome::Wait) {
    const auto transition =
        m_router.findTransition(request.m_cursor.m_machine, best->m_probeX, best->m_probeY, best->m_direction);
    if (transition.has_value()) {
      result.m_hasReverseNeighbor = transition->m_hasReverseNeighbor;
    }
  }

  return result;
}

} // namespace deskflow::server
