/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "server/DisplayLayout.h"

#include <QWidget>

class QCheckBox;
class QComboBox;
class QGraphicsView;
class QGraphicsScene;

class AdvancedLayoutWidget : public QWidget
{
  Q_OBJECT

public:
  explicit AdvancedLayoutWidget(QWidget *parent = nullptr);

  void setKnownMachines(const QStringList &machines);
  void setWorkspaceLayout(const deskflow::server::WorkspaceLayout &layout);
  deskflow::server::WorkspaceLayout workspaceLayout() const;

Q_SIGNALS:
  void layoutChanged();

private Q_SLOTS:
  void onAdvancedToggled(bool enabled);
  void onMachineChanged(int index);
  void importLocalDisplays();
  void refreshScene();

private:
  void rebuildMachineList();
  deskflow::server::MachineLayout *currentMachine();
  const deskflow::server::MachineLayout *currentMachine() const;
  void addMonitorToScene(deskflow::server::DisplayRect &monitor, const QColor &color);

  QCheckBox *m_enableAdvanced = nullptr;
  QComboBox *m_machineCombo = nullptr;
  QGraphicsView *m_view = nullptr;
  QGraphicsScene *m_scene = nullptr;
  deskflow::server::WorkspaceLayout m_layout;
  QStringList m_machines;
};
