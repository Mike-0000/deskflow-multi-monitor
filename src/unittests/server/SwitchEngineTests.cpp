/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "SwitchEngineTests.h"

#include "server/SwitchEngine.h"

using namespace deskflow::server;

static DisplayRect makeMonitor(
    const std::string &id, int32_t worldX, int32_t worldY, int32_t w, int32_t h, int32_t localX = 0, int32_t localY = 0
)
{
  DisplayRect monitor;
  monitor.m_id = id;
  monitor.m_worldX = worldX;
  monitor.m_worldY = worldY;
  monitor.m_width = w;
  monitor.m_height = h;
  monitor.m_localX = localX;
  monitor.m_localY = localY;
  monitor.ensureLayoutSizes();
  return monitor;
}

void SwitchEngineTests::secondaryCornerParityPrefersVertical()
{
  WorkspaceLayout layout;
  layout.m_enabled = true;

  MachineLayout center;
  center.m_name = "center";
  center.m_monitors.push_back(makeMonitor("c", 1000, 1000, 1000, 1000, 0, 0));

  MachineLayout above;
  above.m_name = "above";
  above.m_monitors.push_back(makeMonitor("a", 1000, 0, 1000, 1000, 0, 0));

  MachineLayout left;
  left.m_name = "left";
  left.m_monitors.push_back(makeMonitor("l", 0, 1000, 1000, 1000, 0, 0));

  layout.m_machines.push_back(center);
  layout.m_machines.push_back(above);
  layout.m_machines.push_back(left);

  SwitchEngine engine(layout);

  SwitchEngineCursor cursor;
  cursor.m_machine = "center";
  cursor.m_absoluteMode = false;
  // Outside top-left corner; previous point was interior.
  cursor.m_x = -10;
  cursor.m_y = -10;
  cursor.m_xDelta = -5;
  cursor.m_yDelta = -20;
  cursor.m_jumpZoneSize = 1;

  const auto candidates = engine.collectCandidates(cursor);
  QVERIFY(candidates.size() >= 2);

  const auto best = SwitchGate::pickBestCandidate(candidates, cursor.m_xDelta, cursor.m_yDelta);
  QVERIFY(best.has_value());
  QCOMPARE(best->m_direction, Direction::Top);
  QCOMPARE(best->m_dstMachine, std::string("above"));
}

void SwitchEngineTests::landingInsetSkippedWithoutReverse()
{
  WorkspaceLayout layout;
  layout.m_enabled = true;

  MachineLayout left;
  left.m_name = "left";
  left.m_monitors.push_back(makeMonitor("a", 0, 0, 1000, 1000));

  // One-way feel: right machine is shorter and only abuts mid — still has reverse for that edge.
  // Use a destination with no reverse by placing a third machine that left can exit into,
  // while destination has no segment back... Actually graph is always bidirectional for abut pairs.
  // So reverse exists whenever forward exists. Test applyLandingInset API directly.
  TransitionResult transition;
  transition.m_valid = true;
  transition.m_direction = Direction::Right;
  transition.m_dstX = 0;
  transition.m_dstY = 100;
  transition.m_hasReverseNeighbor = false;

  SwitchEngine::applyLandingInset(transition, 4, false);
  QCOMPARE(transition.m_dstX, 0);
}

void SwitchEngineTests::landingInsetAppliedWithReverse()
{
  TransitionResult transition;
  transition.m_valid = true;
  transition.m_direction = Direction::Right;
  transition.m_dstX = 0;
  transition.m_dstY = 100;
  transition.m_hasReverseNeighbor = true;

  SwitchEngine::applyLandingInset(transition, 4, true);
  QCOMPARE(transition.m_dstX, 5);
}

void SwitchEngineTests::secondaryToPrimaryLandingAvoidsOscillation()
{
  // Host | Client — exit left from client must land past host's right jump zone
  // so the next primary probe cannot immediately reverse-switch.
  WorkspaceLayout layout;
  layout.m_enabled = true;

  MachineLayout host;
  host.m_name = "host";
  host.m_monitors.push_back(makeMonitor("h", 0, 0, 1920, 1080));

  MachineLayout client;
  client.m_name = "client";
  client.m_monitors.push_back(makeMonitor("c", 1920, 0, 1920, 1080));

  layout.m_machines.push_back(host);
  layout.m_machines.push_back(client);

  SwitchEngine engine(layout);

  SwitchEngineCursor cursor;
  cursor.m_machine = "client";
  cursor.m_absoluteMode = false;
  cursor.m_x = -1;
  cursor.m_y = 540;
  cursor.m_xDelta = -8;
  cursor.m_yDelta = 0;
  // Primary jump-zone size (must not use secondary's 0).
  cursor.m_jumpZoneSize = 1;

  const auto candidates = engine.collectCandidates(cursor);
  QCOMPARE(candidates.size(), 1u);
  QCOMPARE(candidates.front().m_dstMachine, std::string("host"));
  QCOMPARE(candidates.front().m_direction, Direction::Left);

  const int32_t landX = candidates.front().m_dstX;
  const int32_t landY = candidates.front().m_dstY;
  QVERIFY(!SwitchEngine::isInJumpZone(landX, landY, 0, 0, 1920, 1080, 1));

  // From that landing, host must not immediately offer a reverse candidate.
  SwitchEngineCursor onHost;
  onHost.m_machine = "host";
  onHost.m_absoluteMode = true;
  onHost.m_x = landX;
  onHost.m_y = landY;
  onHost.m_xDelta = 0;
  onHost.m_yDelta = 0;
  onHost.m_jumpZoneSize = 1;
  QVERIFY(engine.collectCandidates(onHost).empty());
}

void SwitchEngineTests::underInsetLandingStaysInJumpZone()
{
  // Documents the oscillation root cause: jumpZoneSize=0 yields inset=1,
  // which lands on x=width-2 — still inside an inclusive zone of size 1.
  TransitionResult underInset;
  underInset.m_valid = true;
  underInset.m_direction = Direction::Left;
  underInset.m_dstX = 1919;
  underInset.m_dstY = 540;
  underInset.m_hasReverseNeighbor = true;

  SwitchEngine::applyLandingInset(underInset, 0, true);
  QCOMPARE(underInset.m_dstX, 1918);
  QVERIFY(SwitchEngine::isInJumpZone(underInset.m_dstX, underInset.m_dstY, 0, 0, 1920, 1080, 1));

  TransitionResult fixed;
  fixed.m_valid = true;
  fixed.m_direction = Direction::Left;
  fixed.m_dstX = 1919;
  fixed.m_dstY = 540;
  fixed.m_hasReverseNeighbor = true;
  SwitchEngine::applyLandingInset(fixed, 1, true);
  QCOMPARE(fixed.m_dstX, 1917);
  QVERIFY(!SwitchEngine::isInJumpZone(fixed.m_dstX, fixed.m_dstY, 0, 0, 1920, 1080, 1));
}

void SwitchEngineTests::landingPreservesOrthogonalAxisLeftRight()
{
  WorkspaceLayout layout;
  layout.m_enabled = true;

  MachineLayout left;
  left.m_name = "left";
  left.m_monitors.push_back(makeMonitor("l", 0, 0, 1920, 1080));

  MachineLayout right;
  right.m_name = "right";
  right.m_monitors.push_back(makeMonitor("r", 1920, 0, 1920, 1080));

  layout.m_machines.push_back(left);
  layout.m_machines.push_back(right);

  SwitchEngine engine(layout);
  const int32_t zone = 1;

  // Primary absolute: exit right at y=700 → land near left edge of right, same Y.
  SwitchEngineCursor fromLeft;
  fromLeft.m_machine = "left";
  fromLeft.m_absoluteMode = true;
  fromLeft.m_x = 1919;
  fromLeft.m_y = 700;
  fromLeft.m_xDelta = 4;
  fromLeft.m_yDelta = 0;
  fromLeft.m_jumpZoneSize = zone;

  auto candidates = engine.collectCandidates(fromLeft);
  QCOMPARE(candidates.size(), 1u);
  QCOMPARE(candidates.front().m_direction, Direction::Right);
  QCOMPARE(candidates.front().m_dstMachine, std::string("right"));
  QCOMPARE(candidates.front().m_dstX, zone + 1); // inset past reverse jump zone
  QCOMPARE(candidates.front().m_dstY, 700);

  // Secondary relative: exit left at y=300 → land near right edge of left, same Y.
  SwitchEngineCursor fromRight;
  fromRight.m_machine = "right";
  fromRight.m_absoluteMode = false;
  fromRight.m_x = -8;
  fromRight.m_y = 300;
  fromRight.m_xDelta = -8;
  fromRight.m_yDelta = 0;
  fromRight.m_jumpZoneSize = zone;

  candidates = engine.collectCandidates(fromRight);
  QCOMPARE(candidates.size(), 1u);
  QCOMPARE(candidates.front().m_direction, Direction::Left);
  QCOMPARE(candidates.front().m_dstMachine, std::string("left"));
  QCOMPARE(candidates.front().m_dstX, 1919 - (zone + 1));
  QCOMPARE(candidates.front().m_dstY, 300);
}

void SwitchEngineTests::landingPreservesOrthogonalAxisTopBottom()
{
  WorkspaceLayout layout;
  layout.m_enabled = true;

  MachineLayout top;
  top.m_name = "top";
  top.m_monitors.push_back(makeMonitor("t", 0, 0, 1920, 1080));

  MachineLayout bottom;
  bottom.m_name = "bottom";
  bottom.m_monitors.push_back(makeMonitor("b", 0, 1080, 1920, 1080));

  layout.m_machines.push_back(top);
  layout.m_machines.push_back(bottom);

  SwitchEngine engine(layout);
  const int32_t zone = 1;

  SwitchEngineCursor fromTop;
  fromTop.m_machine = "top";
  fromTop.m_absoluteMode = true;
  fromTop.m_x = 640;
  fromTop.m_y = 1079;
  fromTop.m_xDelta = 0;
  fromTop.m_yDelta = 4;
  fromTop.m_jumpZoneSize = zone;

  auto candidates = engine.collectCandidates(fromTop);
  QCOMPARE(candidates.size(), 1u);
  QCOMPARE(candidates.front().m_direction, Direction::Bottom);
  QCOMPARE(candidates.front().m_dstMachine, std::string("bottom"));
  QCOMPARE(candidates.front().m_dstX, 640);
  QCOMPARE(candidates.front().m_dstY, zone + 1);

  SwitchEngineCursor fromBottom;
  fromBottom.m_machine = "bottom";
  fromBottom.m_absoluteMode = false;
  fromBottom.m_x = 900;
  fromBottom.m_y = -6;
  fromBottom.m_xDelta = 0;
  fromBottom.m_yDelta = -6;
  fromBottom.m_jumpZoneSize = zone;

  candidates = engine.collectCandidates(fromBottom);
  QCOMPARE(candidates.size(), 1u);
  QCOMPARE(candidates.front().m_direction, Direction::Top);
  QCOMPARE(candidates.front().m_dstMachine, std::string("top"));
  QCOMPARE(candidates.front().m_dstX, 900);
  QCOMPARE(candidates.front().m_dstY, 1079 - (zone + 1));
}

void SwitchEngineTests::cornerOvershootLeftPreservesY()
{
  // Fast diagonal leave: cursor is past left AND above the top. Without the
  // orthogonal-axis fix, projectToExitEdge clamps Y to 0 and the destination
  // landing snaps to the top instead of the pre-delta Y.
  WorkspaceLayout layout;
  layout.m_enabled = true;

  MachineLayout left;
  left.m_name = "left";
  left.m_monitors.push_back(makeMonitor("l", 0, 0, 1920, 1080));

  MachineLayout right;
  right.m_name = "right";
  right.m_monitors.push_back(makeMonitor("r", 1920, 0, 1920, 1080));

  layout.m_machines.push_back(left);
  layout.m_machines.push_back(right);

  SwitchEngine engine(layout);

  SwitchEngineCursor cursor;
  cursor.m_machine = "right";
  cursor.m_absoluteMode = false;
  // Current overshoots both axes; previous point (5, 400) was interior.
  cursor.m_x = -10;
  cursor.m_y = -5;
  cursor.m_xDelta = -20;
  cursor.m_yDelta = -405;
  cursor.m_jumpZoneSize = 1;

  const auto candidates = engine.collectCandidates(cursor);
  QVERIFY(candidates.size() >= 1);

  const SwitchCandidate *leftExit = nullptr;
  for (const auto &candidate : candidates) {
    if (candidate.m_direction == Direction::Left) {
      leftExit = &candidate;
      break;
    }
  }
  QVERIFY(leftExit != nullptr);
  QCOMPARE(leftExit->m_dstMachine, std::string("left"));
  QCOMPARE(leftExit->m_dstX, 1917); // right edge minus inset (zone+1)
  QCOMPARE(leftExit->m_dstY, 400);  // preserved from pre-delta Y, not clamped to top
}

QTEST_MAIN(SwitchEngineTests)
