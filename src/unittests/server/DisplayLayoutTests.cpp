/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "DisplayLayoutTests.h"

#include "server/DisplayLayout.h"

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
  return monitor;
}

static WorkspaceLayout makeTwoMachineLayout()
{
  WorkspaceLayout layout;
  layout.m_enabled = true;

  MachineLayout desktop;
  desktop.m_name = "desktop";
  desktop.m_monitors.push_back(makeMonitor("display1", 0, 0, 3840, 2160));
  desktop.m_monitors.push_back(makeMonitor("display2", 3840, 400, 2560, 1440));

  MachineLayout laptop;
  laptop.m_name = "laptop";
  laptop.m_monitors.push_back(makeMonitor("display1", 3840, 600, 1920, 1200));

  layout.m_machines.push_back(desktop);
  layout.m_machines.push_back(laptop);
  return layout;
}

void DisplayLayoutTests::fullEdgeOverlap()
{
  const auto layout = makeTwoMachineLayout();
  GeometryRouter router(layout);

  const auto result = router.findTransition("desktop", 3840, 1000, Direction::Right);
  QVERIFY(result.has_value());
  QCOMPARE(result->m_dstMachine, std::string("laptop"));
  QCOMPARE(result->m_dstX, 0);
  QCOMPARE(result->m_dstY, 400);
}

void DisplayLayoutTests::partialOverlap()
{
  const auto layout = makeTwoMachineLayout();
  GeometryRouter router(layout);

  const auto inOverlap = router.findTransition("desktop", 3840, 1000, Direction::Right);
  QVERIFY(inOverlap.has_value());
  QCOMPARE(inOverlap->m_dstMachine, std::string("laptop"));

  const auto belowOverlap = router.findTransition("desktop", 3840, 500, Direction::Right);
  QVERIFY(!belowOverlap.has_value());
}

void DisplayLayoutTests::noOverlapGap()
{
  WorkspaceLayout layout;
  layout.m_enabled = true;

  MachineLayout left;
  left.m_name = "left";
  left.m_monitors.push_back(makeMonitor("a", 0, 0, 1920, 1080));

  MachineLayout right;
  right.m_name = "right";
  right.m_monitors.push_back(makeMonitor("b", 2000, 0, 1920, 1080));

  layout.m_machines.push_back(left);
  layout.m_machines.push_back(right);

  GeometryRouter router(layout);
  const auto result = router.findTransition("left", 1919, 540, Direction::Right);
  QVERIFY(!result.has_value());
}

void DisplayLayoutTests::sameMachineNoSwitch()
{
  const auto layout = makeTwoMachineLayout();
  GeometryRouter router(layout);

  const auto result = router.findTransition("desktop", 2000, 1000, Direction::Right);
  QVERIFY(!result.has_value());
}

void DisplayLayoutTests::stackedMonitors()
{
  WorkspaceLayout layout;
  layout.m_enabled = true;

  MachineLayout host;
  host.m_name = "host";
  host.m_monitors.push_back(makeMonitor("top", 0, 0, 1920, 1080));
  host.m_monitors.push_back(makeMonitor("bottom", 0, 1080, 1920, 1080, 0, 1080));

  MachineLayout remote;
  remote.m_name = "remote";
  remote.m_monitors.push_back(makeMonitor("only", 1920, 800, 1280, 800));

  layout.m_machines.push_back(host);
  layout.m_machines.push_back(remote);

  GeometryRouter router(layout);
  const auto result = router.findTransition("host", 1919, 1200, Direction::Right);
  QVERIFY(result.has_value());
  QCOMPARE(result->m_dstMachine, std::string("remote"));
}

void DisplayLayoutTests::lShapedServerCanSwitchToClientAboveLeft()
{
  WorkspaceLayout layout;
  layout.m_enabled = true;

  MachineLayout server;
  server.m_name = "server";
  server.m_monitors.push_back(makeMonitor("left", 0, 300, 1920, 1080, 0, 300));
  server.m_monitors.push_back(makeMonitor("right", 1920, 0, 1440, 1440, 1920, 0));

  MachineLayout client;
  client.m_name = "client";
  client.m_monitors.push_back(makeMonitor("only", 0, -780, 1920, 1080));

  layout.m_machines.push_back(server);
  layout.m_machines.push_back(client);

  GeometryRouter router(layout);

  const auto fromTopOfLeft = router.findTransition("server", 1000, 300, Direction::Top);
  QVERIFY(fromTopOfLeft.has_value());
  QCOMPARE(fromTopOfLeft->m_dstMachine, std::string("client"));
  QCOMPARE(fromTopOfLeft->m_dstX, 1000);
  QCOMPARE(fromTopOfLeft->m_dstY, 1079);

  const auto fromUpperLeftOfRight = router.findTransition("server", 1920, 100, Direction::Left);
  QVERIFY(fromUpperLeftOfRight.has_value());
  QCOMPARE(fromUpperLeftOfRight->m_dstMachine, std::string("client"));
  QCOMPARE(fromUpperLeftOfRight->m_dstX, 1919);
  QCOMPARE(fromUpperLeftOfRight->m_dstY, 880);
}

void DisplayLayoutTests::negativeCoordinates()
{
  WorkspaceLayout layout;
  layout.m_enabled = true;

  MachineLayout main;
  main.m_name = "main";
  main.m_monitors.push_back(makeMonitor("primary", 0, 0, 2560, 1440));
  main.m_monitors.push_back(makeMonitor("left", -1920, 200, 1920, 1080, -1920, 200));

  MachineLayout client;
  client.m_name = "client";
  client.m_monitors.push_back(makeMonitor("only", -3840, 200, 1920, 1080));

  layout.m_machines.push_back(main);
  layout.m_machines.push_back(client);

  GeometryRouter router(layout);
  const auto result = router.findTransition("main", -1920, 500, Direction::Left);
  QVERIFY(result.has_value());
  QCOMPARE(result->m_dstMachine, std::string("client"));
}

void DisplayLayoutTests::detectExitDirectionCorner()
{
  const DisplayRect monitor = makeMonitor("m", 0, 0, 100, 100);
  QCOMPARE(GeometryRouter::detectExitDirection(monitor, 0, 0), Direction::Left);
  QCOMPARE(GeometryRouter::detectExitDirection(monitor, 99, 0), Direction::Right);
  QCOMPARE(GeometryRouter::detectExitDirection(monitor, 50, 0), Direction::Top);
}

void DisplayLayoutTests::clampToMonitor()
{
  const DisplayRect monitor = makeMonitor("m", 0, 0, 100, 100, 10, 20);
  int32_t x = 5;
  int32_t y = 200;
  GeometryRouter::clampToMonitor(monitor, x, y);
  QCOMPARE(x, 10);
  QCOMPARE(y, 119);
}

QTEST_MAIN(DisplayLayoutTests)
