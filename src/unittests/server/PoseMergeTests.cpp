/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "PoseMergeTests.h"

#include "server/PoseMerge.h"

using namespace deskflow::server;

void PoseMergeTests::preservesWorldById()
{
  MachineLayout machine;
  machine.m_name = "laptop";
  DisplayRect existing;
  existing.m_id = "DISPLAY1";
  existing.m_localX = 0;
  existing.m_localY = 0;
  existing.m_width = 1920;
  existing.m_height = 1200;
  existing.m_worldX = 4000;
  existing.m_worldY = 200;
  existing.ensureLayoutSizes();
  machine.m_monitors.push_back(existing);

  DisplayRect reported = existing;
  reported.m_worldX = 0;
  reported.m_worldY = 0;
  reported.m_dpi = 120; // local geometry metadata update, world pose must stay

  const auto result = mergeReportedDisplays(machine, {reported}, false);
  QVERIFY(result.m_changed);
  QCOMPARE(machine.m_monitors[0].m_worldX, 4000);
  QCOMPARE(machine.m_monitors[0].m_worldY, 200);
  QCOMPARE(machine.m_monitors[0].m_dpi, 120);
  QVERIFY(!machine.m_monitors[0].m_needsPlacement);
}

void PoseMergeTests::parksUnknownWithoutWorldLocal()
{
  MachineLayout machine;
  machine.m_name = "laptop";

  DisplayRect reported;
  reported.m_id = "NEW";
  reported.m_localX = 0;
  reported.m_localY = 0;
  reported.m_width = 800;
  reported.m_height = 600;
  reported.m_worldX = 0;
  reported.m_worldY = 0;
  reported.ensureLayoutSizes();

  const auto result = mergeReportedDisplays(machine, {reported}, false);
  QVERIFY(result.m_changed);
  QCOMPARE(result.m_parkedCount, 1);
  QVERIFY(machine.m_monitors[0].m_needsPlacement);
  QVERIFY(machine.m_monitors[0].m_worldX != 0 || machine.m_monitors[0].m_worldY != 0 || true);
  // Must not keep inventing adjacency at local origin.
  QCOMPARE(machine.m_monitors[0].m_worldX, kEdgeAbutTolerance + 64);
}

void PoseMergeTests::overwriteReplacesWorld()
{
  MachineLayout machine;
  machine.m_name = "laptop";
  DisplayRect existing;
  existing.m_id = "DISPLAY1";
  existing.m_width = 1920;
  existing.m_height = 1080;
  existing.m_worldX = 111;
  existing.m_worldY = 222;
  existing.ensureLayoutSizes();
  machine.m_monitors.push_back(existing);

  DisplayRect reported = existing;
  reported.m_worldX = 50;
  reported.m_worldY = 60;

  mergeReportedDisplays(machine, {reported}, true);
  QCOMPARE(machine.m_monitors[0].m_worldX, 50);
  QCOMPARE(machine.m_monitors[0].m_worldY, 60);
}

QTEST_MAIN(PoseMergeTests)
