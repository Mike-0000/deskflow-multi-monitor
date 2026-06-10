/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "base/DirectionTypes.h"
#include "deskflow/KeyTypes.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace deskflow::server {

enum class SwitchRoutingMode
{
  Legacy,
  Advanced
};

//! A resolved cross-screen transition candidate before gate evaluation.
struct SwitchCandidate
{
  Direction m_direction = Direction::NoDirection;
  int32_t m_dstX = 0;
  int32_t m_dstY = 0;
  int32_t m_probeX = 0;
  int32_t m_probeY = 0;
  int32_t m_xActive = 0;
  int32_t m_yActive = 0;
  bool m_valid = false;
  std::string m_dstMachine;
};

enum class SwitchGateOutcome
{
  Allow,
  Reject,
  Wait
};

enum class SwitchGateReason
{
  None,
  NoNeighbor,
  DoubleTapWaiting,
  SwitchDelayWaiting,
  DeadCorner,
  LockedToScreen,
  MissingModifier,
  StaleRoute
};

struct SwitchGateResult
{
  SwitchGateOutcome m_outcome = SwitchGateOutcome::Reject;
  SwitchGateReason m_reason = SwitchGateReason::None;
};

struct SwitchGateSettings
{
  double m_switchWaitDelay = 0.0;
  double m_switchTwoTapDelay = 0.0;
  int32_t m_switchTwoTapZone = 3;
  int32_t m_jumpZoneSize = 1;
  bool m_switchNeedsShift = false;
  bool m_switchNeedsControl = false;
  bool m_switchNeedsAlt = false;
  uint32_t m_lockedCorners = 0;
  int32_t m_cornerSize = 0;
};

struct SwitchGateState
{
  Direction m_switchDir = Direction::NoDirection;
  bool m_hasPendingTarget = false;
  bool m_switchWaitStarted = false;
  int32_t m_switchWaitX = 0;
  int32_t m_switchWaitY = 0;
  bool m_switchTwoTapEngaged = false;
  bool m_switchTwoTapArmed = false;
};

struct SwitchGateInput
{
  bool m_hasNeighbor = false;
  Direction m_direction = Direction::NoDirection;
  int32_t m_x = 0;
  int32_t m_y = 0;
  int32_t m_xActive = 0;
  int32_t m_yActive = 0;
  int32_t m_cornerRectX = 0;
  int32_t m_cornerRectY = 0;
  int32_t m_cornerRectW = 0;
  int32_t m_cornerRectH = 0;
  bool m_lockedToScreen = false;
  KeyModifierMask m_modifiers = 0;
  int32_t m_xDelta = 0;
  int32_t m_yDelta = 0;
  int32_t m_xDelta2 = 0;
  int32_t m_yDelta2 = 0;
  double m_twoTapElapsed = 0.0;
  int32_t m_shapeX = 0;
  int32_t m_shapeY = 0;
  int32_t m_shapeW = 0;
  int32_t m_shapeH = 0;
  bool m_commitOnly = false;
};

const char *switchGateReasonName(SwitchGateReason reason);

class SwitchGate
{
public:
  static uint32_t getCornerInRect(int32_t x, int32_t y, int32_t ax, int32_t ay, int32_t aw, int32_t ah, int32_t size);

  static SwitchGateResult evaluate(SwitchGateState &state, const SwitchGateSettings &settings, const SwitchGateInput &input);

  static void armTwoTap(SwitchGateState &state, const SwitchGateSettings &settings, const SwitchGateInput &input);

  static void clearPending(SwitchGateState &state);

  static bool shouldDropSecondaryWarpDelta(int32_t dx, int32_t dy, int32_t monitorWidth, int32_t monitorHeight);

  static std::optional<SwitchCandidate> pickBestCandidate(
      const std::vector<SwitchCandidate> &candidates, int32_t xDelta, int32_t yDelta
  );
};

} // namespace deskflow::server
