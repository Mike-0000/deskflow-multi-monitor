/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "widgets/AdvancedLayoutWidget.h"

#include "server/DisplayLayout.h"
#include "server/PoseMerge.h"

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
#include <cmath>
#include <limits>

namespace {

constexpr qreal kSceneScale = 0.15;
// Keep in sync with deskflow::server::kEdgeAbutTolerance (+ editor grab distance).
constexpr int32_t kSnapDistance = 48;

class MonitorRectItem : public QGraphicsRectItem
{
public:
  MonitorRectItem(
      qreal x, qreal y, qreal w, qreal h, deskflow::server::DisplayRect *monitor, AdvancedLayoutWidget *owner,
      bool movable
  )
      : QGraphicsRectItem(0, 0, w, h),
        m_monitor(monitor),
        m_owner(owner)
  {
    setPos(x, y);
    m_acceptPositionChanges = true;
    QGraphicsItem::GraphicsItemFlags flags = QGraphicsItem::ItemIsSelectable | QGraphicsItem::ItemSendsGeometryChanges;
    if (movable) {
      flags |= QGraphicsItem::ItemIsMovable;
    }
    setFlags(flags);
    setBrush(QColor(70, 130, 180, 120));
    setPen(QPen(Qt::white, 2));
  }

  QVariant itemChange(GraphicsItemChange change, const QVariant &value) override
  {
    if (!m_acceptPositionChanges) {
      return QGraphicsRectItem::itemChange(change, value);
    }

    if (change == ItemPositionChange && m_monitor != nullptr) {
      QPointF pos = value.toPointF();
      if (m_owner != nullptr) {
        pos = m_owner->snapMonitorPosition(m_monitor, pos);
      }
      m_monitor->m_worldX = static_cast<int32_t>(std::lround(pos.x() / kSceneScale));
      m_monitor->m_worldY = static_cast<int32_t>(std::lround(pos.y() / kSceneScale));
      m_monitor->m_needsPlacement = false;
      return pos;
    }

    const auto result = QGraphicsRectItem::itemChange(change, value);
    if (change == ItemPositionHasChanged && m_monitor != nullptr && m_owner != nullptr) {
      m_owner->onMonitorLayoutEdited();
    }
    return result;
  }

private:
  deskflow::server::DisplayRect *m_monitor = nullptr;
  AdvancedLayoutWidget *m_owner = nullptr;
  bool m_acceptPositionChanges = false;
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

  m_importLocalButton = new QPushButton(tr("Import Local Displays"), this);
  m_importClientButton = new QPushButton(tr("Import from Connected Client"), this);
  controls->addWidget(m_importLocalButton);
  controls->addWidget(m_importClientButton);
  layout->addLayout(controls);

  m_scene = new QGraphicsScene(this);
  m_view = new QGraphicsView(m_scene, this);
  m_view->setMinimumHeight(280);
  m_view->setRenderHint(QPainter::Antialiasing);
  layout->addWidget(m_view);

  connect(m_enableAdvanced, &QCheckBox::toggled, this, &AdvancedLayoutWidget::onAdvancedToggled);
  connect(m_machineCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &AdvancedLayoutWidget::onMachineChanged);
  connect(m_importLocalButton, &QPushButton::clicked, this, &AdvancedLayoutWidget::importLocalDisplays);
  connect(m_importClientButton, &QPushButton::clicked, this, &AdvancedLayoutWidget::importConnectedClientDisplays);
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
  layout.m_version = 2;
  layout.ensureLayoutSizes();
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

  std::vector<deskflow::server::DisplayRect> displays;
  const auto screens = QGuiApplication::screens();
  displays.reserve(static_cast<std::size_t>(screens.size()));
  for (const QScreen *screen : screens) {
    const QRect geo = screen->geometry();

    deskflow::server::DisplayRect monitor;
    monitor.m_id = screen->name().toStdString();
    monitor.m_name = monitor.m_id;
    monitor.m_localX = geo.x();
    monitor.m_localY = geo.y();
    monitor.m_width = geo.width();
    monitor.m_height = geo.height();
    monitor.m_scale = static_cast<float>(screen->devicePixelRatio());
    monitor.m_dpi = static_cast<int32_t>(screen->logicalDotsPerInch());
    monitor.ensureLayoutSizes();
    // Seed world from density-independent local layout for first placement only.
    monitor.m_worldX = deskflow::server::layoutSizeFromPixels(geo.x(), monitor.m_dpi, monitor.m_scale);
    monitor.m_worldY = deskflow::server::layoutSizeFromPixels(geo.y(), monitor.m_dpi, monitor.m_scale);
    displays.push_back(monitor);
  }

  importDisplaysForCurrentMachine(displays, true);
}

void AdvancedLayoutWidget::setCurrentMachine(const QString &machineName)
{
  const int index = m_machineCombo->findText(machineName);
  if (index >= 0) {
    m_machineCombo->setCurrentIndex(index);
  }
}

void AdvancedLayoutWidget::importDisplaysForMachine(
    const QString &machineName, const std::vector<deskflow::server::DisplayRect> &displays, bool overwrite
)
{
  setCurrentMachine(machineName);
  importDisplaysForCurrentMachine(displays, overwrite);
}

void AdvancedLayoutWidget::importConnectedClientDisplays()
{
  Q_EMIT requestClientDisplayImport(m_machineCombo->currentText(), true);
}

void AdvancedLayoutWidget::importDisplaysForCurrentMachine(
    const std::vector<deskflow::server::DisplayRect> &displays, bool overwrite
)
{
  auto *machine = currentMachine();
  if (machine == nullptr || displays.empty()) {
    return;
  }

  deskflow::server::mergeReportedDisplays(*machine, displays, overwrite);
  m_layout.m_version = 2;
  m_layout.ensureLayoutSizes();
  refreshScene();
  Q_EMIT layoutChanged();
}

void AdvancedLayoutWidget::onMonitorLayoutEdited()
{
  Q_EMIT layoutChanged();
}

QPointF AdvancedLayoutWidget::snapMonitorPosition(
    const deskflow::server::DisplayRect *movingMonitor, const QPointF &proposedPos
) const
{
  if (movingMonitor == nullptr) {
    return proposedPos;
  }

  const int32_t movingW = movingMonitor->layoutWidth();
  const int32_t movingH = movingMonitor->layoutHeight();
  const int32_t proposedX = static_cast<int32_t>(std::lround(proposedPos.x() / kSceneScale));
  const int32_t proposedY = static_cast<int32_t>(std::lround(proposedPos.y() / kSceneScale));
  const int32_t proposedRight = proposedX + movingW;
  const int32_t proposedBottom = proposedY + movingH;

  int32_t snappedX = proposedX;
  int32_t snappedY = proposedY;
  int32_t bestDx = kSnapDistance + 1;
  int32_t bestDy = kSnapDistance + 1;

  auto intervalsOverlap = [](int32_t startA, int32_t endA, int32_t startB, int32_t endB) {
    return std::max(startA, startB) < std::min(endA, endB);
  };

  auto considerX = [&](int32_t candidateX, const deskflow::server::DisplayRect &other) {
    if (!intervalsOverlap(proposedY, proposedBottom, other.m_worldY, other.m_worldY + other.layoutHeight())) {
      return;
    }
    const int32_t dx = std::abs(candidateX - proposedX);
    if (dx < bestDx) {
      bestDx = dx;
      snappedX = candidateX;
    }
  };

  auto considerY = [&](int32_t candidateY, const deskflow::server::DisplayRect &other) {
    if (!intervalsOverlap(proposedX, proposedRight, other.m_worldX, other.m_worldX + other.layoutWidth())) {
      return;
    }
    const int32_t dy = std::abs(candidateY - proposedY);
    if (dy < bestDy) {
      bestDy = dy;
      snappedY = candidateY;
    }
  };

  for (const auto &machine : m_layout.m_machines) {
    for (const auto &other : machine.m_monitors) {
      if (&other == movingMonitor) {
        continue;
      }

      // Exact abutment candidates (0 gap) — matches EdgeSegmentGraph contract.
      considerX(other.m_worldX - movingW, other);
      considerX(other.m_worldX + other.layoutWidth(), other);
      considerX(other.m_worldX, other);
      considerX(other.m_worldX + other.layoutWidth() - movingW, other);

      considerY(other.m_worldY - movingH, other);
      considerY(other.m_worldY + other.layoutHeight(), other);
      considerY(other.m_worldY, other);
      considerY(other.m_worldY + other.layoutHeight() - movingH, other);
    }
  }

  return QPointF(snappedX * kSceneScale, snappedY * kSceneScale);
}

void AdvancedLayoutWidget::refreshScene()
{
  m_scene->clear();

  int32_t minX = 0;
  int32_t minY = 0;
  int32_t maxX = 1920;
  int32_t maxY = 1080;
  bool first = true;
  for (const auto &machine : m_layout.m_machines) {
    for (const auto &monitor : machine.m_monitors) {
      if (first) {
        minX = monitor.m_worldX;
        minY = monitor.m_worldY;
        maxX = monitor.m_worldX + monitor.layoutWidth();
        maxY = monitor.m_worldY + monitor.layoutHeight();
        first = false;
      } else {
        minX = std::min(minX, monitor.m_worldX);
        minY = std::min(minY, monitor.m_worldY);
        maxX = std::max(maxX, monitor.m_worldX + monitor.layoutWidth());
        maxY = std::max(maxY, monitor.m_worldY + monitor.layoutHeight());
      }
    }
  }

  constexpr int32_t padding = 320;
  m_scene->setSceneRect(
      (minX - padding) * kSceneScale, (minY - padding) * kSceneScale, (maxX - minX + padding * 2) * kSceneScale,
      (maxY - minY + padding * 2) * kSceneScale
  );

  int colorIndex = 0;
  const QString currentMachineName = m_machineCombo->currentText();
  for (auto &machine : m_layout.m_machines) {
    const bool movable = QString::fromStdString(machine.m_name) == currentMachineName;
    for (auto &monitor : machine.m_monitors) {
      const QColor colors[] = {QColor(70, 130, 180), QColor(60, 179, 113), QColor(205, 92, 92), QColor(218, 165, 32)};
      QColor color = colors[colorIndex++ % 4];
      if (!movable) {
        color = QColor(105, 105, 105);
      }
      addMonitorToScene(QString::fromStdString(machine.m_name), monitor, color, movable);
    }
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

void AdvancedLayoutWidget::addMonitorToScene(
    const QString &machineName, deskflow::server::DisplayRect &monitor, const QColor &color, bool movable
)
{
  auto *item = new MonitorRectItem(
      monitor.m_worldX * kSceneScale, monitor.m_worldY * kSceneScale, monitor.layoutWidth() * kSceneScale,
      monitor.layoutHeight() * kSceneScale, &monitor, this, movable
  );
  item->setBrush(color);
  if (!movable) {
    item->setOpacity(0.65);
  }
  m_scene->addItem(item);

  const QString monitorName = QString::fromStdString(monitor.m_name.empty() ? monitor.m_id : monitor.m_name);
  auto *label = new QGraphicsSimpleTextItem(QStringLiteral("%1 / %2").arg(machineName, monitorName));
  label->setParentItem(item);
  label->setPos(4, 4);
}
