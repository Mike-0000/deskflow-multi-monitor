/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "widgets/AdvancedLayoutWidget.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGraphicsRectItem>
#include <QGraphicsScene>
#include <QGraphicsSimpleTextItem>
#include <QGraphicsView>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QScreen>
#include <QVBoxLayout>

#include <algorithm>

namespace {

class MonitorRectItem : public QGraphicsRectItem
{
public:
  MonitorRectItem(qreal x, qreal y, qreal w, qreal h, deskflow::server::DisplayRect *monitor)
      : QGraphicsRectItem(x, y, w, h),
        m_monitor(monitor)
  {
    setFlags(QGraphicsItem::ItemIsMovable | QGraphicsItem::ItemIsSelectable | QGraphicsItem::ItemSendsGeometryChanges);
    setBrush(QColor(70, 130, 180, 120));
    setPen(QPen(Qt::white, 2));
  }

  QVariant itemChange(GraphicsItemChange change, const QVariant &value) override
  {
    if (change == ItemPositionChange && m_monitor != nullptr) {
      const QPointF pos = value.toPointF();
      m_monitor->m_worldX = static_cast<int32_t>(pos.x());
      m_monitor->m_worldY = static_cast<int32_t>(pos.y());
    }
    return QGraphicsRectItem::itemChange(change, value);
  }

private:
  deskflow::server::DisplayRect *m_monitor = nullptr;
};

} // namespace

AdvancedLayoutWidget::AdvancedLayoutWidget(QWidget *parent) : QWidget(parent)
{
  auto *layout = new QVBoxLayout(this);

  m_enableAdvanced = new QCheckBox(tr("Enable advanced monitor layout"), this);
  layout->addWidget(m_enableAdvanced);

  auto *controls = new QHBoxLayout();
  m_machineCombo = new QComboBox(this);
  controls->addWidget(new QLabel(tr("Machine:"), this));
  controls->addWidget(m_machineCombo, 1);

  auto *importButton = new QPushButton(tr("Import Local Displays"), this);
  controls->addWidget(importButton);
  layout->addLayout(controls);

  m_scene = new QGraphicsScene(this);
  m_view = new QGraphicsView(m_scene, this);
  m_view->setMinimumHeight(280);
  m_view->setRenderHint(QPainter::Antialiasing);
  layout->addWidget(m_view);

  connect(m_enableAdvanced, &QCheckBox::toggled, this, &AdvancedLayoutWidget::onAdvancedToggled);
  connect(m_machineCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &AdvancedLayoutWidget::onMachineChanged);
  connect(importButton, &QPushButton::clicked, this, &AdvancedLayoutWidget::importLocalDisplays);
}

void AdvancedLayoutWidget::setKnownMachines(const QStringList &machines)
{
  m_machines = machines;
  rebuildMachineList();
}

void AdvancedLayoutWidget::setWorkspaceLayout(const deskflow::server::WorkspaceLayout &layout)
{
  m_layout = layout;
  m_enableAdvanced->setChecked(layout.m_enabled);
  rebuildMachineList();
  refreshScene();
}

deskflow::server::WorkspaceLayout AdvancedLayoutWidget::workspaceLayout() const
{
  deskflow::server::WorkspaceLayout layout = m_layout;
  layout.m_enabled = m_enableAdvanced->isChecked();
  return layout;
}

void AdvancedLayoutWidget::onAdvancedToggled(bool enabled)
{
  m_layout.m_enabled = enabled;
  m_view->setEnabled(enabled);
  m_machineCombo->setEnabled(enabled);
  Q_EMIT layoutChanged();
}

void AdvancedLayoutWidget::onMachineChanged(int)
{
  refreshScene();
}

void AdvancedLayoutWidget::importLocalDisplays()
{
  auto *machine = currentMachine();
  if (machine == nullptr) {
    return;
  }

  machine->m_monitors.clear();
  const auto screens = QGuiApplication::screens();
  for (int i = 0; i < screens.size(); ++i) {
    const QScreen *screen = screens.at(i);
    const QRect geo = screen->geometry();

    deskflow::server::DisplayRect monitor;
    monitor.m_id = screen->name().toStdString();
    monitor.m_name = monitor.m_id;
    monitor.m_localX = geo.x();
    monitor.m_localY = geo.y();
    monitor.m_width = geo.width();
    monitor.m_height = geo.height();
    monitor.m_worldX = geo.x();
    monitor.m_worldY = geo.y();
    monitor.m_scale = static_cast<float>(screen->devicePixelRatio());
    monitor.m_dpi = static_cast<int32_t>(screen->logicalDotsPerInch());
    machine->m_monitors.push_back(monitor);
  }

  refreshScene();
  Q_EMIT layoutChanged();
}

void AdvancedLayoutWidget::refreshScene()
{
  m_scene->clear();

  auto *machine = currentMachine();
  if (machine == nullptr) {
    return;
  }

  int32_t minX = 0;
  int32_t minY = 0;
  int32_t maxX = 1920;
  int32_t maxY = 1080;
  bool first = true;
  for (const auto &monitor : machine->m_monitors) {
    if (first) {
      minX = monitor.m_worldX;
      minY = monitor.m_worldY;
      maxX = monitor.m_worldX + monitor.m_width;
      maxY = monitor.m_worldY + monitor.m_height;
      first = false;
    } else {
      minX = std::min(minX, monitor.m_worldX);
      minY = std::min(minY, monitor.m_worldY);
      maxX = std::max(maxX, monitor.m_worldX + monitor.m_width);
      maxY = std::max(maxY, monitor.m_worldY + monitor.m_height);
    }
  }

  const qreal scale = 0.15;
  m_scene->setSceneRect(minX * scale - 20, minY * scale - 20, (maxX - minX + 40) * scale, (maxY - minY + 40) * scale);

  int colorIndex = 0;
  for (auto &monitor : machine->m_monitors) {
    const QColor colors[] = {QColor(70, 130, 180), QColor(60, 179, 113), QColor(205, 92, 92), QColor(218, 165, 32)};
    addMonitorToScene(monitor, colors[colorIndex++ % 4]);
  }
}

void AdvancedLayoutWidget::rebuildMachineList()
{
  m_machineCombo->blockSignals(true);
  m_machineCombo->clear();

  for (const QString &name : m_machines) {
    m_machineCombo->addItem(name);
    if (m_layout.findMachine(name.toStdString()) == nullptr) {
      deskflow::server::MachineLayout machine;
      machine.m_name = name.toStdString();
      m_layout.m_machines.push_back(machine);
    }
  }

  m_machineCombo->blockSignals(false);
  if (m_machineCombo->count() > 0) {
    m_machineCombo->setCurrentIndex(0);
  }
}

deskflow::server::MachineLayout *AdvancedLayoutWidget::currentMachine()
{
  if (m_machineCombo->currentIndex() < 0) {
    return nullptr;
  }
  return m_layout.findMachine(m_machineCombo->currentText().toStdString());
}

const deskflow::server::MachineLayout *AdvancedLayoutWidget::currentMachine() const
{
  if (m_machineCombo->currentIndex() < 0) {
    return nullptr;
  }
  return m_layout.findMachine(m_machineCombo->currentText().toStdString());
}

void AdvancedLayoutWidget::addMonitorToScene(deskflow::server::DisplayRect &monitor, const QColor &color)
{
  const qreal scale = 0.15;
  auto *item = new MonitorRectItem(
      monitor.m_worldX * scale, monitor.m_worldY * scale, monitor.m_width * scale, monitor.m_height * scale, &monitor
  );
  item->setBrush(color);
  m_scene->addItem(item);

  auto *label = new QGraphicsSimpleTextItem(QString::fromStdString(monitor.m_name.empty() ? monitor.m_id : monitor.m_name));
  label->setParentItem(item);
  label->setPos(4, 4);
}
