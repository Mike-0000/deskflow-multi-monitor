/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "SwitchDecisionTests.h"

#include "deskflow/KeyTypes.h"
#include "deskflow/OptionTypes.h"
#include "server/SwitchDecision.h"

using namespace deskflow::server;

void SwitchDecisionTests::getCornerInRect()
{
  QCOMPARE(SwitchGate::getCornerInRect(0, 0, 0, 0, 100, 100, 10), s_topLeftCornerMask);
  QCOMPARE(SwitchGate::getCornerInRect(99, 0, 0, 0, 100, 100, 10), s_topRightCornerMask);
  QCOMPARE(SwitchGate::getCornerInRect(50, 50, 0, 0, 100, 100, 10), s_noCornerMask);
}

void SwitchDecisionTests::deadCornerBlocksSwitch()
{
  SwitchGateSettings settings;
  settings.m_lockedCorners = s_topLeftCornerMask;
  settings.m_cornerSize = 10;

  SwitchGateState state;
  SwitchGateInput input;
  input.m_hasNeighbor = true;
  input.m_direction = Direction::Left;
  input.m_xActive = 0;
  input.m_yActive = 0;
  input.m_cornerRectX = 0;
  input.m_cornerRectY = 0;
  input.m_cornerRectW = 100;
  input.m_cornerRectH = 100;

  const auto result = SwitchGate::evaluate(state, settings, input);
  QCOMPARE(result.m_outcome, SwitchGateOutcome::Reject);
  QCOMPARE(result.m_reason, SwitchGateReason::DeadCorner);
}

void SwitchDecisionTests::switchDelayWaits()
{
  SwitchGateSettings settings;
  settings.m_switchWaitDelay = 0.25;

  SwitchGateState state;
  SwitchGateInput input;
  input.m_hasNeighbor = true;
  input.m_direction = Direction::Right;
  input.m_x = 10;
  input.m_y = 20;

  const auto result = SwitchGate::evaluate(state, settings, input);
  QCOMPARE(result.m_outcome, SwitchGateOutcome::Wait);
  QCOMPARE(result.m_reason, SwitchGateReason::SwitchDelayWaiting);
  QVERIFY(state.m_switchWaitStarted);
  QCOMPARE(state.m_switchWaitX, 10);
  QCOMPARE(state.m_switchWaitY, 20);
}

void SwitchDecisionTests::switchDelayOrDoubleTapAllowsEither()
{
  SwitchGateSettings settings;
  settings.m_switchWaitDelay = 0.25;
  settings.m_switchTwoTapDelay = 0.25;

  SwitchGateState state;
  state.m_switchTwoTapEngaged = true;
  state.m_switchTwoTapArmed = true;
  state.m_switchDir = Direction::Right;

  SwitchGateInput input;
  input.m_hasNeighbor = true;
  input.m_direction = Direction::Right;
  input.m_twoTapElapsed = 0.05;

  const auto result = SwitchGate::evaluate(state, settings, input);
  QCOMPARE(result.m_outcome, SwitchGateOutcome::Allow);
}

void SwitchDecisionTests::doubleTapRequiresArm()
{
  SwitchGateSettings settings;
  settings.m_switchTwoTapDelay = 0.25;

  SwitchGateState state;
  state.m_switchTwoTapEngaged = true;
  state.m_switchDir = Direction::Right;

  SwitchGateInput input;
  input.m_hasNeighbor = true;
  input.m_direction = Direction::Right;
  input.m_twoTapElapsed = 0.05;

  const auto result = SwitchGate::evaluate(state, settings, input);
  QCOMPARE(result.m_outcome, SwitchGateOutcome::Wait);
  QCOMPARE(result.m_reason, SwitchGateReason::DoubleTapWaiting);
}

void SwitchDecisionTests::pickBestCandidatePrefersMovementVector()
{
  SwitchCandidate horizontal;
  horizontal.m_valid = true;
  horizontal.m_direction = Direction::Right;
  horizontal.m_dstMachine = "right";

  SwitchCandidate vertical;
  vertical.m_valid = true;
  vertical.m_direction = Direction::Top;
  vertical.m_dstMachine = "top";

  const auto picked = SwitchGate::pickBestCandidate({horizontal, vertical}, 5, 1);
  QVERIFY(picked.has_value());
  QCOMPARE(picked->m_direction, Direction::Right);

  const auto pickedVertical = SwitchGate::pickBestCandidate({horizontal, vertical}, 1, 8);
  QVERIFY(pickedVertical.has_value());
  QCOMPARE(pickedVertical->m_direction, Direction::Top);
}

void SwitchDecisionTests::pickBestCandidateStableFallback()
{
  SwitchCandidate horizontal;
  horizontal.m_valid = true;
  horizontal.m_direction = Direction::Right;

  SwitchCandidate vertical;
  vertical.m_valid = true;
  vertical.m_direction = Direction::Top;

  const auto picked = SwitchGate::pickBestCandidate({horizontal, vertical}, 0, 0);
  QVERIFY(picked.has_value());
  QCOMPARE(picked->m_direction, Direction::Right);
}

void SwitchDecisionTests::shouldDropSecondaryWarpDelta()
{
  QVERIFY(SwitchGate::shouldDropSecondaryWarpDelta(700, 0, 1920, 1080));
  QVERIFY(!SwitchGate::shouldDropSecondaryWarpDelta(10, 5, 1920, 1080));
}

void SwitchDecisionTests::clearPendingResetsState()
{
  SwitchGateState state;
  state.m_hasPendingTarget = true;
  state.m_switchDir = Direction::Left;
  state.m_switchWaitStarted = true;
  state.m_switchTwoTapEngaged = true;
  state.m_switchTwoTapArmed = true;

  SwitchGate::clearPending(state);
  QVERIFY(!state.m_hasPendingTarget);
  QCOMPARE(state.m_switchDir, Direction::NoDirection);
  QVERIFY(!state.m_switchWaitStarted);
  QVERIFY(!state.m_switchTwoTapEngaged);
  QVERIFY(!state.m_switchTwoTapArmed);
}

void SwitchDecisionTests::commitOnlySkipsDelayAndDoubleTap()
{
  SwitchGateSettings settings;
  settings.m_switchWaitDelay = 0.25;
  settings.m_switchTwoTapDelay = 0.25;

  SwitchGateState state;
  SwitchGateInput input;
  input.m_hasNeighbor = true;
  input.m_direction = Direction::Right;
  input.m_commitOnly = true;

  const auto result = SwitchGate::evaluate(state, settings, input);
  QCOMPARE(result.m_outcome, SwitchGateOutcome::Allow);
}

QTEST_MAIN(SwitchDecisionTests)
