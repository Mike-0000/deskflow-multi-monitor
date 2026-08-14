/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025-2026 Symless Ltd.
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "CoreIpcServer.h"

#include "base/Log.h"
#include "common/Constants.h"

#include <QLocalSocket>

namespace deskflow::core::ipc {

static CoreIpcServer *s_instance = nullptr;

CoreIpcServer::CoreIpcServer(QObject *parent) : IpcServer(parent, kCoreIpcName, QStringLiteral("core"))
{
  assert(s_instance == nullptr);
  s_instance = this;
}

CoreIpcServer &CoreIpcServer::instance()
{
  assert(s_instance != nullptr);
  return *s_instance;
}

void CoreIpcServer::processCommand(QLocalSocket *clientSocket, const QString &command, const QStringList &parts)
{
  Q_UNUSED(parts)
  if (command == QStringLiteral("stop")) {
    LOG_DEBUG("core ipc server got stop message");
    writeToClientSocket(clientSocket, QStringLiteral("ok"));
    broadcastCommand(QStringLiteral("bye"));
    Q_EMIT stopProcessRequested();
    return;
  }
  if (command == QStringLiteral("pause") || command == QStringLiteral("resume")) {
    const bool paused = command == QStringLiteral("pause");
    LOG_DEBUG("core ipc server got %s message", command.toUtf8().constData());
    writeToClientSocket(clientSocket, QStringLiteral("ok=%1").arg(command));
    Q_EMIT pauseSwitchingRequested(paused);
    return;
  }
  if (command == QStringLiteral("toggle")) {
    LOG_DEBUG("core ipc server got toggle message");
    writeToClientSocket(clientSocket, QStringLiteral("ok=toggle"));
    Q_EMIT toggleSwitchingRequested();
    return;
  }
  if (command == QStringLiteral("status")) {
    LOG_DEBUG("core ipc server got status message");
    bool paused = false;
    Q_EMIT pauseStatusRequested(&paused);
    writeToClientSocket(clientSocket, QStringLiteral("ok=status"));
    writeToClientSocket(clientSocket, QStringLiteral("paused=%1").arg(paused ? QStringLiteral("true") : QStringLiteral("false")));
    return;
  }
  LOG_WARN("core ipc server got unknown command: %s", command.toUtf8().constData());
}

} // namespace deskflow::core::ipc
