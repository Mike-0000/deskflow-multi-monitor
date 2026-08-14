/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Symless Ltd.
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "CoreIpc.h"

#include "CoreIpcServer.h"

#include <QMetaEnum>

void ipcSendToClient(const QString &command, const QString &args)
{
  // AutoConnection: Direct when already on the IPC server's thread (so pause/toggle
  // replies include paused= before the command handler returns), Queued otherwise
  // because QLocalSocket must be written from its owning thread.
  auto &server = deskflow::core::ipc::CoreIpcServer::instance();
  QMetaObject::invokeMethod(
      &server, [command, args] { deskflow::core::ipc::CoreIpcServer::instance().broadcastCommand(command, args); },
      Qt::AutoConnection
  );
}

void ipcSendConnectionState(deskflow::core::ConnectionState state)
{
  const auto metaEnum = QMetaEnum::fromType<deskflow::core::ConnectionState>();
  ipcSendToClient(QStringLiteral("connectionState"), metaEnum.valueToKey(static_cast<int>(state)));
}
