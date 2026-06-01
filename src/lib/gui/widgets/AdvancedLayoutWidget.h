/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "server/DisplayLayout.h"

#include <QPointF>
#include <QWidget>

class QCheckBox;
class QComboBox;
class QGraphicsView;
class QGraphicsScene;
class QPushButton;

class AdvancedLayoutWidget : public QWidget
{
  Q_OBJECT

public:
  explicit AdvancedLayoutWidget(QWidget *parent = nullptr);

  void setKnownMachines(const QStringList &machines);
  void setWorkspaceLayout(const deskflow::server::WorkspaceLayout &layout);
  void setCurrentMachine(const QString &machineName);
  void importDisplaysForMachine(const QString &machineName, const std::vector<deskflow::server::DisplayRect> &displays, bool overwrite);
  deskflow::server::WorkspaceLayout workspaceLayout() const;

  void onMonitorLayoutEdited();
  QPointF snapMonitorPosition(const deskflow::server::DisplayRect *movingMonitor, const QPointF &proposedPos) const;

Q_SIGNALS:
  void layoutChanged();
  void requestClientDisplayImport(const QString &machineName, bool overwrite);

private Q_SLOTS:
  void onAdvancedToggled(bool enabled);
  void onMachineChanged(int index);
  void importLocalDisplays();
  void importConnectedClientDisplays();
  void refreshScene();

private:
  void rebuildMachineList();
  deskflow::server::MachineLayout *currentMachine();
  const deskflow::server::MachineLayout *currentMachine() const;
  void addMonitorToScene(const QString &machineName, deskflow::server::DisplayRect &monitor, const QColor &color, bool movable);
  void importDisplaysForCurrentMachine(const std::vector<deskflow::server::DisplayRect> &displays, bool overwrite);

  QCheckBox *m_enableAdvanced = nullptr;
  QPushButton *m_importLocalButton = nullptr;
  QPushButton *m_importClientButton = nullptr;
  QComboBox *m_machineCombo = nullptr;
  QGraphicsView *m_view = nullptr;
  QGraphicsScene *m_scene = nullptr;
  deskflow::server::WorkspaceLayout m_layout;
  QStringList m_machines;
};
