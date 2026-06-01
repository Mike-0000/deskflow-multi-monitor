/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "DisplayLayoutConfigTests.h"

#include "server/Config.h"

#include <sstream>

using namespace deskflow::server;

void DisplayLayoutConfigTests::parseAdvancedLayoutSection()
{
  const char *configText = R"(
section: screens
	desktop:
	laptop:
end
section: display_layouts
	advancedLayout = true
	desktop:
		display1:
			id = DISPLAY1
			worldX = 0
			worldY = 0
			width = 3840
			height = 2160
			localX = 0
			localY = 0
	laptop:
		display1:
			worldX = 3840
			worldY = 600
			width = 1920
			height = 1200
			localX = 0
			localY = 0
end
)";

  std::istringstream input(configText);
  Config config(nullptr);
  input >> config;

  QVERIFY(config.hasAdvancedLayout());
  QCOMPARE(config.getWorkspaceLayout().m_machines.size(), 2u);
  const auto *desktop = config.getWorkspaceLayout().findMachine("desktop");
  QVERIFY(desktop != nullptr);
  QCOMPARE(desktop->m_monitors.size(), 1u);
  QCOMPARE(desktop->m_monitors.front().m_width, 3840);
}

void DisplayLayoutConfigTests::serializeAdvancedLayoutSection()
{
  Config config(nullptr);
  QVERIFY(config.addScreen("desktop"));
  config.getWorkspaceLayout().m_enabled = true;
  MachineLayout desktop;
  desktop.m_name = "desktop";
  desktop.m_monitors.push_back(DisplayRect{});
  desktop.m_monitors.back().m_id = "DISPLAY1";
  desktop.m_monitors.back().m_worldX = 0;
  desktop.m_monitors.back().m_width = 1920;
  desktop.m_monitors.back().m_height = 1080;
  config.getWorkspaceLayout().m_machines.push_back(desktop);

  std::ostringstream output;
  output << config;
  const std::string text = output.str();
  QVERIFY(text.find("section: display_layouts") != std::string::npos);
  QVERIFY(text.find("advancedLayout = true") != std::string::npos);
}

void DisplayLayoutConfigTests::parseLayoutFromGeneratedServerConfig()
{
  const char *configText = R"(
section: screens
	DESKTOP-U00QJVN:
	MikePC:
end
section: links
	DESKTOP-U00QJVN:
		up = MikePC
	MikePC:
		down = DESKTOP-U00QJVN
end
section: display_layouts
	advancedLayout = false
	MikePC:
		65537:
			id = 65537
			name = 65537
			worldX = 0
			worldY = 0
			width = 2560
			height = 1440
			localX = 0
			localY = 0
	DESKTOP-U00QJVN:
		AW2725DF:
			id = AW2725DF
			name = AW2725DF
			worldX = -1294
			worldY = -147
			width = 2560
			height = 1440
			localX = 0
			localY = 0
		LG SDQHD:
			id = LG SDQHD
			name = LG SDQHD
			worldX = 1266
			worldY = -433
			width = 1707
			height = 1920
			localX = 2560
			localY = 16
			scale = 1.5
end
section: options
	clipboardSharing = true
	clipboardSharingSize = true
	relativeMouseMoves = false
	switchCorners = none
	switchCornerSize = 0
	win32KeepForeground = false
	address =
end
)";

  std::istringstream input(configText);
  Config config(nullptr);
  input >> config;

  const auto *server = config.getWorkspaceLayout().findMachine("DESKTOP-U00QJVN");
  QVERIFY(server != nullptr);
  QCOMPARE(server->m_monitors.size(), 2u);
  QCOMPARE(server->m_monitors.back().m_id, std::string("LG SDQHD"));

  std::ostringstream output;
  output << config;
  const std::string text = output.str();
  QVERIFY(text.find("clipboardSharingSize = true") == std::string::npos);
  QVERIFY(text.find("clipboardSharingSize = 3072") != std::string::npos);
}

QTEST_MAIN(DisplayLayoutConfigTests)
