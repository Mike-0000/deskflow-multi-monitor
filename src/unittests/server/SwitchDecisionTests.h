/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <QTest>

class SwitchDecisionTests : public QObject
{
  Q_OBJECT

private Q_SLOTS:
  void getCornerInRect();
  void deadCornerBlocksSwitch();
  void switchDelayWaits();
  void switchDelayOrDoubleTapAllowsEither();
  void doubleTapRequiresArm();
  void pickBestCandidatePrefersMovementVector();
  void pickBestCandidateStableFallback();
  void shouldDropSecondaryWarpDelta();
  void clearPendingResetsState();
  void commitOnlySkipsDelayAndDoubleTap();
};
