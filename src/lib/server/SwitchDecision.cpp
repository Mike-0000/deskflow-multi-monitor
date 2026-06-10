/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "server/SwitchDecision.h"

#include "deskflow/OptionTypes.h"

#include <algorithm>
#include <cmath>

namespace deskflow::server {

const char *switchGateReasonName(SwitchGateReason reason)
{
  switch (reason) {
  case SwitchGateReason::None:
    return "none";
  case SwitchGateReason::NoNeighbor:
    return "no neighbor";
  case SwitchGateReason::DoubleTapWaiting:
    return "double tap waiting";
  case SwitchGateReason::SwitchDelayWaiting:
    return "switch delay waiting";
  case SwitchGateReason::DeadCorner:
    return "dead corner";
  case SwitchGateReason::LockedToScreen:
    return "locked to screen";
  case SwitchGateReason::MissingModifier:
    return "missing modifier";
  case SwitchGateReason::StaleRoute:
    return "stale route";
  }
  return "unknown";
}

uint32_t SwitchGate::getCornerInRect(int32_t x, int32_t y, int32_t ax, int32_t ay, int32_t aw, int32_t ah, int32_t size)
{
  int32_t xSide;
  if (x <= ax) {
    xSide = -1;
  } else if (x >= ax + aw - 1) {
    xSide = 1;
  } else {
    xSide = 0;
  }

  int32_t ySide;
  if (y <= ay) {
    ySide = -1;
  } else if (y >= ay + ah - 1) {
    ySide = 1;
  } else {
    ySide = 0;
  }

  if (xSide != 0) {
    if (y < ay + size) {
      return (xSide < 0) ? s_topLeftCornerMask : s_topRightCornerMask;
    }
    if (y >= ay + ah - size) {
      return (xSide < 0) ? s_bottomLeftCornerMask : s_bottomRightCornerMask;
    }
  }

  if (ySide != 0) {
    if (x < ax + size) {
      return (ySide < 0) ? s_topLeftCornerMask : s_bottomLeftCornerMask;
    }
    if (x >= ax + aw - size) {
      return (ySide < 0) ? s_topRightCornerMask : s_bottomRightCornerMask;
    }
  }

  return s_noCornerMask;
}

SwitchGateResult SwitchGate::evaluate(SwitchGateState &state, const SwitchGateSettings &settings, const SwitchGateInput &input)
{
  SwitchGateResult result;

  if (!input.m_hasNeighbor) {
    clearPending(state);
    result.m_outcome = SwitchGateOutcome::Reject;
    result.m_reason = SwitchGateReason::NoNeighbor;
    return result;
  }

  bool preventSwitch = false;
  bool allowSwitch = false;

  const bool isNewDirection = input.m_direction != state.m_switchDir;
  if (isNewDirection || !state.m_hasPendingTarget) {
    state.m_switchDir = input.m_direction;
    state.m_hasPendingTarget = true;
  }

  if (!input.m_commitOnly && !allowSwitch && settings.m_switchTwoTapDelay > 0.0) {
    const bool twoTapStarted = state.m_switchTwoTapEngaged;
    const bool twoTapReady =
        state.m_switchTwoTapArmed && input.m_twoTapElapsed <= settings.m_switchTwoTapDelay;
    if (isNewDirection || !twoTapStarted || !twoTapReady) {
      preventSwitch = true;
      state.m_switchTwoTapEngaged = true;
      state.m_switchTwoTapArmed = false;
      result.m_reason = SwitchGateReason::DoubleTapWaiting;
    } else {
      allowSwitch = true;
    }
  }

  if (!input.m_commitOnly && !allowSwitch && settings.m_switchWaitDelay > 0.0) {
    if (isNewDirection || !state.m_switchWaitStarted) {
      state.m_switchWaitX = input.m_x;
      state.m_switchWaitY = input.m_y;
      state.m_switchWaitStarted = true;
    }
    preventSwitch = true;
    if (result.m_reason == SwitchGateReason::None) {
      result.m_reason = SwitchGateReason::SwitchDelayWaiting;
    }
  }

  if (settings.m_lockedCorners != 0) {
    const uint32_t corner = getCornerInRect(
        input.m_xActive, input.m_yActive, input.m_cornerRectX, input.m_cornerRectY, input.m_cornerRectW,
        input.m_cornerRectH, settings.m_cornerSize
    );
    if ((corner & settings.m_lockedCorners) != 0) {
      preventSwitch = true;
      clearPending(state);
      result.m_outcome = SwitchGateOutcome::Reject;
      result.m_reason = SwitchGateReason::DeadCorner;
      return result;
    }
  }

  if (!preventSwitch && input.m_lockedToScreen) {
    clearPending(state);
    result.m_outcome = SwitchGateOutcome::Reject;
    result.m_reason = SwitchGateReason::LockedToScreen;
    return result;
  }

  if (!preventSwitch &&
      ((settings.m_switchNeedsShift && (input.m_modifiers & KeyModifierShift) != KeyModifierShift) ||
       (settings.m_switchNeedsControl && (input.m_modifiers & KeyModifierControl) != KeyModifierControl) ||
       (settings.m_switchNeedsAlt && (input.m_modifiers & KeyModifierAlt) != KeyModifierAlt))) {
    clearPending(state);
    result.m_outcome = SwitchGateOutcome::Reject;
    result.m_reason = SwitchGateReason::MissingModifier;
    return result;
  }

  if (preventSwitch) {
    result.m_outcome = SwitchGateOutcome::Wait;
    if (result.m_reason == SwitchGateReason::None) {
      result.m_reason = SwitchGateReason::SwitchDelayWaiting;
    }
    return result;
  }

  clearPending(state);
  result.m_outcome = SwitchGateOutcome::Allow;
  result.m_reason = SwitchGateReason::None;
  return result;
}

void SwitchGate::armTwoTap(SwitchGateState &state, const SwitchGateSettings &settings, const SwitchGateInput &input)
{
  if (!state.m_switchTwoTapEngaged) {
    return;
  }

  if (input.m_twoTapElapsed > settings.m_switchTwoTapDelay) {
    state.m_switchTwoTapEngaged = false;
    state.m_switchTwoTapArmed = false;
    return;
  }

  if (state.m_switchTwoTapArmed) {
    return;
  }

  int32_t tapZone = settings.m_jumpZoneSize;
  if (tapZone < settings.m_switchTwoTapZone) {
    tapZone = settings.m_switchTwoTapZone;
  }

  const int32_t ax = input.m_shapeX;
  const int32_t ay = input.m_shapeY;
  const int32_t aw = input.m_shapeW;
  const int32_t ah = input.m_shapeH;
  if (input.m_x < ax + tapZone || input.m_x >= ax + aw - tapZone || input.m_y < ay + tapZone ||
      input.m_y >= ay + ah - tapZone) {
    return;
  }

  switch (state.m_switchDir) {
  case Direction::Left:
    state.m_switchTwoTapArmed = (input.m_xDelta > 0 && input.m_xDelta2 > 0);
    break;
  case Direction::Right:
    state.m_switchTwoTapArmed = (input.m_xDelta < 0 && input.m_xDelta2 < 0);
    break;
  case Direction::Top:
    state.m_switchTwoTapArmed = (input.m_yDelta > 0 && input.m_yDelta2 > 0);
    break;
  case Direction::Bottom:
    state.m_switchTwoTapArmed = (input.m_yDelta < 0 && input.m_yDelta2 < 0);
    break;
  case Direction::NoDirection:
    break;
  }
}

void SwitchGate::clearPending(SwitchGateState &state)
{
  state.m_hasPendingTarget = false;
  state.m_switchDir = Direction::NoDirection;
  state.m_switchWaitStarted = false;
  state.m_switchTwoTapEngaged = false;
  state.m_switchTwoTapArmed = false;
}

bool SwitchGate::shouldDropSecondaryWarpDelta(int32_t dx, int32_t dy, int32_t monitorWidth, int32_t monitorHeight)
{
  return std::abs(dx) > monitorWidth / 3 || std::abs(dy) > monitorHeight / 3;
}

std::optional<SwitchCandidate> SwitchGate::pickBestCandidate(
    const std::vector<SwitchCandidate> &candidates, int32_t xDelta, int32_t yDelta
)
{
  std::vector<const SwitchCandidate *> valid;
  valid.reserve(candidates.size());
  for (const auto &candidate : candidates) {
    if (candidate.m_valid) {
      valid.push_back(&candidate);
    }
  }

  if (valid.empty()) {
    return std::nullopt;
  }
  if (valid.size() == 1) {
    return *valid.front();
  }

  const int32_t absX = std::abs(xDelta);
  const int32_t absY = std::abs(yDelta);
  const bool preferHorizontal = absX > absY;
  const bool preferVertical = absY > absX;

  auto directionScore = [](Direction direction, bool horizontal, bool vertical) {
    using enum Direction;
    switch (direction) {
    case Left:
    case Right:
      return horizontal ? 2 : 0;
    case Top:
    case Bottom:
      return vertical ? 2 : 0;
    case NoDirection:
      return 0;
    }
    return 0;
  };

  const SwitchCandidate *best = valid.front();
  int bestScore = directionScore(best->m_direction, preferHorizontal, preferVertical);
  for (size_t i = 1; i < valid.size(); ++i) {
    const auto *candidate = valid[i];
    const int score = directionScore(candidate->m_direction, preferHorizontal, preferVertical);
    if (score > bestScore) {
      bestScore = score;
      best = candidate;
    }
  }

  return *best;
}

} // namespace deskflow::server
