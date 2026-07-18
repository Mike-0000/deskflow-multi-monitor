/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025 Chris Rizzitello <sithlord48@gmail.com>
 * SPDX-FileCopyrightText: (C) 2012 Symless Ltd.
 * SPDX-FileCopyrightText: (C) 2008 Volker Lanz <vl@fidra.de>
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "ServerConfig.h"

#include "Hotkey.h"
#include "common/Settings.h"
#include "server/Config.h"

#include <QAbstractButton>
#include <QFile>
#include <QPushButton>

using enum ScreenConfig::Modifier;
using enum ScreenConfig::SwitchCorner;
using enum ScreenConfig::Fix;

static const struct
{
  int x;
  int y;
  const char *name;
} neighbourDirs[] = {
    {1, 0, "right"},
    {-1, 0, "left"},
    {0, -1, "up"},
    {0, 1, "down"},

};

const int serverDefaultIndex = 7;

ServerConfig::ServerConfig(int columns, int rows) : m_Screens(columns), m_Columns(columns), m_Rows(rows)
{
  recall();
}

bool ServerConfig::save(const QString &fileName) const
{
  QFile file(fileName);
  if (!file.open(QIODevice::WriteOnly | QIODevice::Text))
    return false;

  save(file);
  file.close();

  return true;
}

bool ServerConfig::operator==(const ServerConfig &sc) const
{
  return m_Screens == sc.m_Screens &&                                   //
         m_Columns == sc.m_Columns &&                                   //
         m_Rows == sc.m_Rows &&                                         //
         m_HasHeartbeat == sc.m_HasHeartbeat &&                         //
         m_Heartbeat == sc.m_Heartbeat &&                               //
         m_Protocol == sc.m_Protocol &&                                 //
         m_RelativeMouseMoves == sc.m_RelativeMouseMoves &&             //
         m_Win32KeepForeground == sc.m_Win32KeepForeground &&           //
         m_HasSwitchDelay == sc.m_HasSwitchDelay &&                     //
         m_SwitchDelay == sc.m_SwitchDelay &&                           //
         m_HasSwitchDoubleTap == sc.m_HasSwitchDoubleTap &&             //
         m_SwitchDoubleTap == sc.m_SwitchDoubleTap &&                   //
         m_SwitchCornerSize == sc.m_SwitchCornerSize &&                 //
         m_SwitchCorners == sc.m_SwitchCorners &&                       //
         m_Hotkeys == sc.m_Hotkeys &&                                   //
         m_DefaultLockToScreenState == sc.m_DefaultLockToScreenState && //
         m_DisableLockToScreen == sc.m_DisableLockToScreen &&           //
         m_ClipboardSharing == sc.m_ClipboardSharing &&                 //
         m_ClipboardSharingSize == sc.m_ClipboardSharingSize &&         //
         deskflow::server::Config::workspaceLayoutEqual(m_workspaceLayout, sc.m_workspaceLayout);
}

void ServerConfig::save(QFile &file) const
{
  QTextStream outStream(&file);
  outStream << *this;
}

void ServerConfig::setupScreens()
{
  switchCorners().clear();
  screens().clear();
  hotkeys().clear();

  // m_NumSwitchCorners is used as a fixed size array. See Screen::init()
  for (int i = 0; i < static_cast<int>(NumSwitchCorners); i++)
    switchCorners() << false;

  // There must always be screen objects for each cell in the screens QList.
  // Unused screens are identified by having an empty name.
  for (int i = 0; i < numColumns() * numRows(); i++)
    addScreen(Screen());
}

void ServerConfig::commit()
{
  qDebug("committing server config");

  settings().beginGroup("internalConfig");
  settings().remove("");

  settings().setValue("numColumns", numColumns());
  settings().setValue("numRows", numRows());

  settings().setValue("hasHeartbeat", hasHeartbeat());
  settings().setValue("heartbeat", heartbeat());
  settings().setValue("relativeMouseMoves", relativeMouseMoves());
  settings().setValue("win32KeepForeground", win32KeepForeground());
  settings().setValue("hasSwitchDelay", hasSwitchDelay());
  settings().setValue("switchDelay", switchDelay());
  settings().setValue("hasSwitchDoubleTap", hasSwitchDoubleTap());
  settings().setValue("switchDoubleTap", switchDoubleTap());
  settings().setValue("switchCornerSize", switchCornerSize());
  settings().setValue("defaultLockToScreenState", defaultLockToScreenState());
  settings().setValue("disableLockToScreen", disableLockToScreen());
  settings().setValue("clipboardSharing", clipboardSharing());
  settings().setValue("clipboardSharingSize", QVariant::fromValue(clipboardSharingSize()));

  writeSettings(settings(), switchCorners(), "switchCorner");

  settings().beginWriteArray("screens");
  for (int i = 0; i < screens().size(); i++) {
    settings().setArrayIndex(i);
    const auto &screen = screens()[i];
    screen.saveSettings(settings());
    auto screenName = Settings::value(Settings::Core::ComputerName).toString();
    if (screen.isServer() && screenName != screen.name()) {
      Settings::setValue(Settings::Core::ComputerName, screen.name());
    }
  }
  settings().endArray();

  settings().beginWriteArray("hotkeys");
  for (int i = 0; i < hotkeys().size(); i++) {
    settings().setArrayIndex(i);
    hotkeys()[i].saveSettings(settings().get());
  }
  settings().endArray();

  settings().setValue("workspaceLayout/enabled", m_workspaceLayout.m_enabled);
  settings().setValue("workspaceLayout/version", m_workspaceLayout.m_version);
  settings().setValue("workspaceLayout/machineCount", static_cast<int>(m_workspaceLayout.m_machines.size()));
  settings().beginWriteArray("workspaceLayout/machines");
  for (int i = 0; i < static_cast<int>(m_workspaceLayout.m_machines.size()); ++i) {
    settings().setArrayIndex(i);
    const auto &machine = m_workspaceLayout.m_machines[static_cast<std::size_t>(i)];
    settings().setValue("name", QString::fromStdString(machine.m_name));
    settings().beginWriteArray("monitors");
    for (int j = 0; j < static_cast<int>(machine.m_monitors.size()); ++j) {
      settings().setArrayIndex(j);
      const auto &monitor = machine.m_monitors[static_cast<std::size_t>(j)];
      settings().setValue("id", QString::fromStdString(monitor.m_id));
      settings().setValue("displayName", QString::fromStdString(monitor.m_name));
      settings().setValue("worldX", monitor.m_worldX);
      settings().setValue("worldY", monitor.m_worldY);
      settings().setValue("width", monitor.m_width);
      settings().setValue("height", monitor.m_height);
      settings().setValue("localX", monitor.m_localX);
      settings().setValue("localY", monitor.m_localY);
      settings().setValue("scale", static_cast<double>(monitor.m_scale));
      settings().setValue("dpi", monitor.m_dpi);
      settings().setValue("layoutWidth", monitor.m_layoutWidth);
      settings().setValue("layoutHeight", monitor.m_layoutHeight);
      settings().setValue("needsPlacement", monitor.m_needsPlacement);
    }
    settings().endArray();
  }
  settings().endArray();

  settings().endGroup();
}

void ServerConfig::recall()
{
  qDebug("recalling server config");

  settings().beginGroup("internalConfig");

  setNumColumns(settings().value("numColumns", 5).toInt());
  setNumRows(settings().value("numRows", 3).toInt());

  // we need to know the number of columns and rows before we can set up
  // ourselves
  setupScreens();

  haveHeartbeat(settings().value("hasHeartbeat", false).toBool());
  setHeartbeat(settings().value("heartbeat", 5000).toInt());
  setProtocol(Settings::value(Settings::Server::Protocol).value<NetworkProtocol>());
  setRelativeMouseMoves(settings().value("relativeMouseMoves", false).toBool());
  setWin32KeepForeground(settings().value("win32KeepForeground", false).toBool());
  haveSwitchDelay(settings().value("hasSwitchDelay", false).toBool());
  setSwitchDelay(settings().value("switchDelay", 250).toInt());
  haveSwitchDoubleTap(settings().value("hasSwitchDoubleTap", false).toBool());
  setSwitchDoubleTap(settings().value("switchDoubleTap", 250).toInt());
  setSwitchCornerSize(settings().value("switchCornerSize").toInt());
  setDefaultLockToScreenState(settings().value("defaultLockToScreenState", false).toBool());
  setDisableLockToScreen(settings().value("disableLockToScreen", false).toBool());
  setClipboardSharingSize(
      settings().value("clipboardSharingSize", (int)ServerConfig::defaultClipboardSharingSize()).toULongLong()
  );
  setClipboardSharing(settings().value("clipboardSharing", true).toBool());

  readSettings(settings(), switchCorners(), "switchCorner", false, static_cast<int>(NumSwitchCorners));

  int numScreens = settings().beginReadArray("screens");
  Q_ASSERT(numScreens <= screens().size());
  for (int i = 0; i < numScreens; i++) {
    settings().setArrayIndex(i);
    screens()[i].loadSettings(settings());
    if (getServerName() == screens()[i].name()) {
      screens()[i].markAsServer();
    }
  }
  settings().endArray();

  int numHotkeys = settings().beginReadArray("hotkeys");
  for (int i = 0; i < numHotkeys; i++) {
    settings().setArrayIndex(i);
    Hotkey h;
    h.loadSettings(settings().get());
    hotkeys().append(h);
  }
  settings().endArray();

  m_workspaceLayout.m_enabled = settings().value("workspaceLayout/enabled", false).toBool();
  m_workspaceLayout.m_version = settings().value("workspaceLayout/version", 2).toInt();
  const int machineCount = settings().value("workspaceLayout/machineCount", 0).toInt();
  m_workspaceLayout.m_machines.clear();
  if (machineCount > 0) {
    const int storedMachines = settings().beginReadArray("workspaceLayout/machines");
    for (int i = 0; i < storedMachines; ++i) {
      settings().setArrayIndex(i);
      deskflow::server::MachineLayout machine;
      machine.m_name = settings().value("name").toString().toStdString();
      const int monitorCount = settings().beginReadArray("monitors");
      for (int j = 0; j < monitorCount; ++j) {
        settings().setArrayIndex(j);
        deskflow::server::DisplayRect monitor;
        monitor.m_id = settings().value("id").toString().toStdString();
        monitor.m_name = settings().value("displayName").toString().toStdString();
        monitor.m_worldX = settings().value("worldX", 0).toInt();
        monitor.m_worldY = settings().value("worldY", 0).toInt();
        monitor.m_width = settings().value("width", 0).toInt();
        monitor.m_height = settings().value("height", 0).toInt();
        monitor.m_localX = settings().value("localX", 0).toInt();
        monitor.m_localY = settings().value("localY", 0).toInt();
        monitor.m_scale = static_cast<float>(settings().value("scale", 1.0).toDouble());
        monitor.m_dpi = settings().value("dpi", 96).toInt();
        monitor.m_layoutWidth = settings().value("layoutWidth", 0).toInt();
        monitor.m_layoutHeight = settings().value("layoutHeight", 0).toInt();
        monitor.m_needsPlacement = settings().value("needsPlacement", false).toBool();
        monitor.ensureLayoutSizes();
        machine.m_monitors.push_back(monitor);
      }
      settings().endArray();
      m_workspaceLayout.m_machines.push_back(machine);
    }
    settings().endArray();
  }

  reloadWorkspaceLayoutFromConfigFile();

  settings().endGroup();
}

void ServerConfig::reloadWorkspaceLayoutFromConfigFile()
{
  const QString path = Settings::value(Settings::Server::ExternalConfig).toBool()
                           ? Settings::value(Settings::Server::ExternalConfigFile).toString()
                           : Settings::defaultValue(Settings::Server::ExternalConfigFile).toString();

  QFile file(path);
  if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
    return;
  }

  const QByteArray data = file.readAll();
  if (data.isEmpty()) {
    return;
  }

  std::istringstream input(std::string(data.constData(), static_cast<std::size_t>(data.size())));
  try {
    deskflow::server::Config config(nullptr);
    input >> config;
    const auto &fileLayout = config.getWorkspaceLayout();
    if (fileLayout.m_enabled || !fileLayout.m_machines.empty()) {
      m_workspaceLayout = fileLayout;
    }
  } catch (const deskflow::server::ServerConfigReadException &e) {
    qWarning() << "failed to read display layouts from" << path << e.what();
  }
}

int ServerConfig::adjacentScreenIndex(int idx, int deltaColumn, int deltaRow) const
{
  if (screens()[idx].isNull())
    return -1;

  // if we're at the left or right end of the table, don't find results going
  // further left or right
  if ((deltaColumn > 0 && (idx + 1) % numColumns() == 0) || (deltaColumn < 0 && idx % numColumns() == 0))
    return -1;

  int arrayPos = idx + deltaColumn + deltaRow * numColumns();

  if (arrayPos >= screens().size() || arrayPos < 0)
    return -1;

  return arrayPos;
}

QTextStream &operator<<(QTextStream &outStream, const ServerConfig &config)
{
  outStream << "section: screens" << Qt::endl;

  for (const Screen &s : config.screens()) {
    if (!s.isNull())
      outStream << s.screensSection();
  }

  outStream << "end" << Qt::endl << Qt::endl;

  outStream << "section: aliases" << Qt::endl;

  for (const Screen &s : config.screens()) {
    if (!s.isNull())
      outStream << s.aliasesSection();
  }

  outStream << "end" << Qt::endl << Qt::endl;

  outStream << "section: links" << Qt::endl;

  for (int i = 0; const auto &screen : config.screens()) {
    if (!screen.isNull()) {
      outStream << "\t" << screen.name() << ":\n";
      for (const auto &neighbour : std::as_const(neighbourDirs)) {
        int idx = config.adjacentScreenIndex(i, neighbour.x, neighbour.y);
        if (idx != -1 && !config.screens()[idx].isNull())
          outStream << "\t\t" << neighbour.name << " = " << config.screens()[idx].name() << Qt::endl;
      }
    }
    i++;
  }

  outStream << "end" << Qt::endl << Qt::endl;

  if (config.workspaceLayout().m_enabled || !config.workspaceLayout().m_machines.empty()) {
    outStream << "section: display_layouts" << Qt::endl;
    outStream << "\tversion = " << config.workspaceLayout().m_version << Qt::endl;
    outStream << "\tadvancedLayout = " << (config.workspaceLayout().m_enabled ? "true" : "false") << Qt::endl;
    for (const auto &machine : config.workspaceLayout().m_machines) {
      outStream << "\t" << QString::fromStdString(machine.m_name) << ":" << Qt::endl;
      for (const auto &monitor : machine.m_monitors) {
        const QString monitorId =
            QString::fromStdString(monitor.m_id.empty() ? monitor.m_name : monitor.m_id);
        outStream << "\t\t" << monitorId << ":" << Qt::endl;
        if (!monitor.m_id.empty()) {
          outStream << "\t\t\tid = " << QString::fromStdString(monitor.m_id) << Qt::endl;
        }
        if (!monitor.m_name.empty()) {
          outStream << "\t\t\tname = " << QString::fromStdString(monitor.m_name) << Qt::endl;
        }
        outStream << "\t\t\tworldX = " << monitor.m_worldX << Qt::endl;
        outStream << "\t\t\tworldY = " << monitor.m_worldY << Qt::endl;
        outStream << "\t\t\twidth = " << monitor.m_width << Qt::endl;
        outStream << "\t\t\theight = " << monitor.m_height << Qt::endl;
        outStream << "\t\t\tlocalX = " << monitor.m_localX << Qt::endl;
        outStream << "\t\t\tlocalY = " << monitor.m_localY << Qt::endl;
        if (monitor.m_layoutWidth > 0) {
          outStream << "\t\t\tlayoutWidth = " << monitor.m_layoutWidth << Qt::endl;
        }
        if (monitor.m_layoutHeight > 0) {
          outStream << "\t\t\tlayoutHeight = " << monitor.m_layoutHeight << Qt::endl;
        }
        if (monitor.m_scale != 1.0f) {
          outStream << "\t\t\tscale = " << monitor.m_scale << Qt::endl;
        }
        if (monitor.m_dpi != 96) {
          outStream << "\t\t\tdpi = " << monitor.m_dpi << Qt::endl;
        }
        if (monitor.m_needsPlacement) {
          outStream << "\t\t\tneedsPlacement = true" << Qt::endl;
        }
      }
    }
    outStream << "end" << Qt::endl << Qt::endl;
  }

  outStream << "section: options" << Qt::endl;

  if (config.hasHeartbeat())
    outStream << "\t" << "heartbeat = " << config.heartbeat() << Qt::endl;

  if (config.protocol() == NetworkProtocol::Unknown)
    qFatal("unrecognized protocol when writing config");
  outStream << "\t" << "protocol = " << networkProtocolToOption(config.protocol()) << Qt::endl;

  outStream << "\t"
            << "relativeMouseMoves = " << (config.relativeMouseMoves() ? "true" : "false") << Qt::endl;
  outStream << "\t"
            << "win32KeepForeground = " << (config.win32KeepForeground() ? "true" : "false") << Qt::endl;
  outStream << "\t"
            << "defaultLockToScreenState = " << (config.defaultLockToScreenState() ? "true" : "false") << Qt::endl;
  outStream << "\t"
            << "disableLockToScreen = " << (config.disableLockToScreen() ? "true" : "false") << Qt::endl;
  outStream << "\t"
            << "clipboardSharing = " << (config.clipboardSharing() ? "true" : "false") << Qt::endl;
  outStream << "\t"
            << "clipboardSharingSize = " << config.clipboardSharingSize() << Qt::endl;

  if (config.hasSwitchDelay())
    outStream << "\t"
              << "switchDelay = " << config.switchDelay() << Qt::endl;

  if (config.hasSwitchDoubleTap())
    outStream << "\t"
              << "switchDoubleTap = " << config.switchDoubleTap() << Qt::endl;

  outStream << "\t"
            << "switchCorners = none ";
  for (int i = 0; i < config.switchCorners().size(); i++)
    if (config.switchCorners()[i])
      outStream << "+" << ServerConfig::switchCornerName(i) << " ";
  outStream << Qt::endl;

  outStream << "\t"
            << "switchCornerSize = " << config.switchCornerSize() << Qt::endl;

  for (const Hotkey &hotkey : config.hotkeys())
    outStream << hotkey;

  outStream << "end" << Qt::endl << Qt::endl;

  return outStream;
}

int ServerConfig::numScreens() const
{
  int rval = 0;

  for (const Screen &s : screens()) {
    if (!s.isNull())
      rval++;
  }

  return rval;
}

QString ServerConfig::getServerName() const
{
  return Settings::value(Settings::Core::ComputerName).toString();
}

void ServerConfig::updateServerName()
{
  for (auto &screen : screens()) {
    if (screen.isServer()) {
      screen.setName(Settings::value(Settings::Core::ComputerName).toString());
      break;
    }
  }
}

QString ServerConfig::configFile() const
{
  return Settings::value(Settings::Server::ExternalConfigFile).toString();
}

bool ServerConfig::useExternalConfig() const
{
  return Settings::value(Settings::Server::ExternalConfig).toBool();
}

bool ServerConfig::isFull() const
{
  bool isFull = true;

  for (const auto &screen : screens()) {
    if (screen.isNull()) {
      isFull = false;
      break;
    }
  }

  return isFull;
}

bool ServerConfig::screenExists(const QString &screenName) const
{
  bool isExists = false;

  for (const auto &screen : screens()) {
    if (!screen.isNull() && screen.name() == screenName) {
      isExists = true;
      break;
    }
  }

  return isExists;
}

void ServerConfig::addClient(const QString &clientName)
{
  int serverIndex = -1;
  const auto screenName = Settings::value(Settings::Core::ComputerName).toString();

  if (findScreenName(screenName, serverIndex)) {
    m_Screens[serverIndex].markAsServer();
  } else {
    fixNoServer(screenName, serverIndex);
  }

  m_Screens.addScreenByPriority(Screen(clientName));
}

void ServerConfig::setConfigFile(const QString &configFile) const
{
  Settings::setValue(Settings::Server::ExternalConfigFile, configFile);
}

void ServerConfig::setUseExternalConfig(bool useExternalConfig) const
{
  Settings::setValue(Settings::Server::ExternalConfig, useExternalConfig);
}

bool ServerConfig::findScreenName(const QString &name, int &index)
{
  bool found = false;
  for (int i = 0; i < screens().size(); i++) {
    if (!screens()[i].isNull() && screens()[i].name().compare(name) == 0) {
      index = i;
      found = true;
      break;
    }
  }
  return found;
}

bool ServerConfig::fixNoServer(const QString &name, int &index)
{
  bool fixed = false;
  if (screens()[serverDefaultIndex].isNull()) {
    m_Screens[serverDefaultIndex].setName(name);
    m_Screens[serverDefaultIndex].markAsServer();
    index = serverDefaultIndex;
    fixed = true;
  }

  return fixed;
}

size_t ServerConfig::defaultClipboardSharingSize()
{
  return 3 * 1024; // 3 MiB
}

size_t ServerConfig::setClipboardSharingSize(size_t size)
{
  if (size) {
    size += 512; // Round up to the nearest megabyte
    size /= 1024;
    size *= 1024;
    setClipboardSharing(true);
  } else {
    setClipboardSharing(false);
  }
  using std::swap;
  swap(size, m_ClipboardSharingSize);
  return size;
}

QSettingsProxy &ServerConfig::settings()
{
  return Settings::proxy();
}
