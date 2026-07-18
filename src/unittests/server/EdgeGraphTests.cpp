/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "EdgeGraphTests.h"

#include "server/EdgeGraph.h"

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

void EdgeGraphTests::buildsPartialOverlapSegments()
{
  WorkspaceLayout layout;
  layout.m_enabled = true;

  MachineLayout desktop;
  desktop.m_name = "desktop";
  desktop.m_monitors.push_back(makeMonitor("a", 0, 0, 3840, 2160));

  MachineLayout laptop;
  laptop.m_name = "laptop";
  laptop.m_monitors.push_back(makeMonitor("b", 3840, 600, 1920, 1200));

  layout.m_machines.push_back(desktop);
  layout.m_machines.push_back(laptop);

  EdgeSegmentGraph graph(layout);
  QVERIFY(graph.segments().size() >= 2);

  const auto hit = graph.findSegmentAtExit("desktop", "a", Direction::Right, 1000);
  QVERIFY(hit.has_value());
  QCOMPARE(hit->m_dstMachine, std::string("laptop"));

  const auto miss = graph.findSegmentAtExit("desktop", "a", Direction::Right, 100);
  QVERIFY(!miss.has_value());
}

void EdgeGraphTests::rejectsGapBeyondTolerance()
{
  WorkspaceLayout layout;
  layout.m_enabled = true;

  MachineLayout left;
  left.m_name = "left";
  left.m_monitors.push_back(makeMonitor("a", 0, 0, 1000, 1000));

  MachineLayout right;
  right.m_name = "right";
  right.m_monitors.push_back(makeMonitor("b", 1000 + kEdgeAbutTolerance + 1, 0, 1000, 1000));

  layout.m_machines.push_back(left);
  layout.m_machines.push_back(right);

  EdgeSegmentGraph graph(layout);
  QVERIFY(!graph.findSegmentAtExit("left", "a", Direction::Right, 500).has_value());
}

void EdgeGraphTests::acceptsWithinTolerance()
{
  WorkspaceLayout layout;
  layout.m_enabled = true;

  MachineLayout left;
  left.m_name = "left";
  left.m_monitors.push_back(makeMonitor("a", 0, 0, 1000, 1000));

  MachineLayout right;
  right.m_name = "right";
  right.m_monitors.push_back(makeMonitor("b", 1000 + kEdgeAbutTolerance, 0, 1000, 1000));

  layout.m_machines.push_back(left);
  layout.m_machines.push_back(right);

  EdgeSegmentGraph graph(layout);
  QVERIFY(graph.findSegmentAtExit("left", "a", Direction::Right, 500).has_value());
}

void EdgeGraphTests::activeSidesWithoutMidpoint()
{
  // Neighbor overlaps only the top strip of a tall monitor — midpoint would miss.
  WorkspaceLayout layout;
  layout.m_enabled = true;

  MachineLayout tall;
  tall.m_name = "tall";
  tall.m_monitors.push_back(makeMonitor("a", 0, 0, 1000, 2000));

  MachineLayout shortNeighbor;
  shortNeighbor.m_name = "short";
  shortNeighbor.m_monitors.push_back(makeMonitor("b", 1000, 0, 800, 400));

  layout.m_machines.push_back(tall);
  layout.m_machines.push_back(shortNeighbor);

  EdgeSegmentGraph graph(layout);
  using enum DirectionMask;
  const uint32_t sides = graph.activeSidesForMachine("tall");
  QVERIFY((sides & static_cast<uint32_t>(RightMask)) != 0);

  QVERIFY(graph.findSegmentAtExit("tall", "a", Direction::Right, 100).has_value());
  QVERIFY(!graph.findSegmentAtExit("tall", "a", Direction::Right, 1000).has_value());
}

void EdgeGraphTests::multiSegmentSameSide()
{
  WorkspaceLayout layout;
  layout.m_enabled = true;

  MachineLayout host;
  host.m_name = "host";
  host.m_monitors.push_back(makeMonitor("a", 0, 0, 1000, 2000));

  MachineLayout topClient;
  topClient.m_name = "topClient";
  topClient.m_monitors.push_back(makeMonitor("b", 1000, 0, 800, 400));

  MachineLayout bottomClient;
  bottomClient.m_name = "bottomClient";
  bottomClient.m_monitors.push_back(makeMonitor("c", 1000, 1600, 800, 400));

  layout.m_machines.push_back(host);
  layout.m_machines.push_back(topClient);
  layout.m_machines.push_back(bottomClient);

  EdgeSegmentGraph graph(layout);
  const auto top = graph.findSegmentAtExit("host", "a", Direction::Right, 100);
  const auto bottom = graph.findSegmentAtExit("host", "a", Direction::Right, 1800);
  QVERIFY(top.has_value());
  QVERIFY(bottom.has_value());
  QCOMPARE(top->m_dstMachine, std::string("topClient"));
  QCOMPARE(bottom->m_dstMachine, std::string("bottomClient"));
}

void EdgeGraphTests::reverseNeighborDetection()
{
  WorkspaceLayout layout;
  layout.m_enabled = true;

  MachineLayout left;
  left.m_name = "left";
  left.m_monitors.push_back(makeMonitor("a", 0, 0, 1000, 1000));

  MachineLayout right;
  right.m_name = "right";
  right.m_monitors.push_back(makeMonitor("b", 1000, 0, 1000, 1000));

  layout.m_machines.push_back(left);
  layout.m_machines.push_back(right);

  EdgeSegmentGraph graph(layout);
  QVERIFY(graph.hasReverseSegment("right", "b", Direction::Right));
  QVERIFY(!graph.hasReverseSegment("right", "b", Direction::Top));
}

QTEST_MAIN(EdgeGraphTests)
