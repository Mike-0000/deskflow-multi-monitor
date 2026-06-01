/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QTest>

class DisplayLayoutTests : public QObject
{
  Q_OBJECT

private Q_SLOTS:
  void fullEdgeOverlap();
  void partialOverlap();
  void noOverlapGap();
  void sameMachineNoSwitch();
  void stackedMonitors();
  void negativeCoordinates();
  void detectExitDirectionCorner();
  void clampToMonitor();
};
