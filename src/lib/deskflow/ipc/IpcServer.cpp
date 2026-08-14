/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2025-2026 Symless Ltd.
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "IpcServer.h"

#include "base/Log.h"
#include "common/VersionInfo.h"

#include <QLocalServer>
#include <QLocalSocket>

namespace deskflow::core::ipc {

IpcServer::IpcServer(QObject *parent, const QString &serverName, const QString &typeName)
    : QObject(parent),
      m_server{new QLocalServer(this)}, // NOSONAR - Qt memory
      m_serverName(serverName),
      m_typeName(typeName.toUtf8())
{
  // do nothing
}

IpcServer::~IpcServer()
{
  m_server->close();
}

void IpcServer::listen()
{
  // IPC server normally runs as system, but GUI runs as regular user, so we need to allow world access.
  m_server->setSocketOptions(QLocalServer::WorldAccessOption);

  connect(m_server, &QLocalServer::newConnection, this, &IpcServer::handleNewConnection);
  QLocalServer::removeServer(m_serverName);
  if (m_server->listen(m_serverName)) {
    LOG_DEBUG("%s ipc server listening on: %s", m_typeName.constData(), m_serverName.toUtf8().constData());
  } else {
    LOG_ERR("%s ipc server failed to listen on: %s", m_typeName.constData(), m_serverName.toUtf8().constData());
  }
}

void IpcServer::handleNewConnection()
{
  QLocalSocket *clientSocket = m_server->nextPendingConnection();
  if (!clientSocket) {
    LOG_ERR("%s ipc server failed to get new connection", m_typeName.constData());
    return;
  }

  LOG_DEBUG("%s ipc server got new connection", m_typeName.constData());
  m_clients.insert(clientSocket);
  m_clientBuffers.insert(clientSocket, QByteArray{});

  connect(clientSocket, &QLocalSocket::readyRead, this, &IpcServer::handleReadyRead);
  connect(clientSocket, &QLocalSocket::disconnected, this, &IpcServer::handleDisconnected);
  connect(clientSocket, &QLocalSocket::errorOccurred, this, &IpcServer::handleErrorOccurred);
}

void IpcServer::handleReadyRead()
{
  const auto clientSocket = qobject_cast<QLocalSocket *>(sender());
  LOG_VERBOSE("%s ipc server ready to read data", m_typeName.constData());

  const QByteArray incoming = clientSocket->readAll();
  if (incoming.isEmpty()) {
    LOG_WARN("%s ipc server got empty message", m_typeName.constData());
    return;
  }

  auto &data = m_clientBuffers[clientSocket];
  data.append(incoming);

  // each message is delimited by a newline to keep the protocol super simple.
  while (data.contains('\n')) {
    const auto index = data.indexOf('\n');
    QByteArray messageData = data.left(index);
    data.remove(0, index + 1);
    QString message = QString::fromUtf8(messageData);
    processMessage(clientSocket, message);
  }

  if (data.size() > 1024 * 1024) {
    LOG_WARN("%s ipc server client exceeded receive buffer limit", m_typeName.constData());
    data.clear();
    writeToClientSocket(clientSocket, QStringLiteral("error=message too large"));
    clientSocket->flush();
    clientSocket->disconnectFromServer();
  }
}

void IpcServer::handleDisconnected()
{
  const auto clientSocket = qobject_cast<QLocalSocket *>(sender());
  LOG_DEBUG("%s ipc server client disconnected", m_typeName.constData());
  m_clients.remove(clientSocket);
  m_authenticatedClients.remove(clientSocket);
  m_mismatchedClients.remove(clientSocket);
  m_clientBuffers.remove(clientSocket);
  clientSocket->deleteLater();
}

void IpcServer::handleErrorOccurred()
{
  const auto clientSocket = qobject_cast<QLocalSocket *>(sender());
  LOG_ERR("%s ipc server client error: %s", m_typeName.constData(), clientSocket->errorString().toUtf8().constData());
  m_clients.remove(clientSocket);
  m_authenticatedClients.remove(clientSocket);
  m_mismatchedClients.remove(clientSocket);
  m_clientBuffers.remove(clientSocket);
  clientSocket->deleteLater();
}

void IpcServer::processMessage(QLocalSocket *clientSocket, const QString &message)
{
  LOG_VERBOSE("%s ipc server got message: %s", m_typeName.constData(), message.toUtf8().constData());
  const auto separator = message.indexOf('=');
  const auto command = separator >= 0 ? message.left(separator) : message;
  const QStringList parts = separator >= 0 ? QStringList{command, message.mid(separator + 1)} : QStringList{command};
  if (command.isEmpty()) {
    LOG_ERR("%s ipc server got invalid message: %s", m_typeName.constData(), message.toUtf8().constData());
    writeToClientSocket(clientSocket, QStringLiteral("error"));
    return;
  }

  if (command == QStringLiteral("hello")) {
    if (parts.size() < 2) {
      LOG_ERR("%s ipc client hello missing version", m_typeName.constData());
      writeToClientSocket(clientSocket, "error=missing version");
      clientSocket->flush();
      clientSocket->disconnectFromServer();
      return;
    }

    const auto versionId = QStringLiteral("%1+%2").arg(kVersion, kVersionGitSha);
    const auto clientVersion = parts.at(1);
    LOG_DEBUG("%s ipc server got hello message (version: %s)", m_typeName.constData(), versionId.toUtf8().constData());

    if (clientVersion != versionId) {
      LOG_WARN(
          "%s ipc client version mismatch (client: %s, server: %s)", m_typeName.constData(),
          clientVersion.toUtf8().constData(), versionId.toUtf8().constData()
      );
      m_mismatchedClients.insert(clientSocket);
      m_authenticatedClients.remove(clientSocket);
      writeToClientSocket(clientSocket, QStringLiteral("versionMismatch=%1").arg(versionId));
      clientSocket->flush();
      // Do not replay pending state; client must stop this process and reinstall matching binaries.
      return;
    }

    m_mismatchedClients.remove(clientSocket);
    m_authenticatedClients.insert(clientSocket);
    LOG_DEBUG("%s ipc server sending hello back", m_typeName.constData());
    writeToClientSocket(clientSocket, QStringLiteral("hello=%1").arg(versionId));

    // Replay messages that were queued before any clients connected.
    LOG_VERBOSE("ipc server replaying %d pending messages", m_pendingMessages.size());
    for (const auto &pending : std::as_const(m_pendingMessages)) {
      LOG_VERBOSE("%s ipc server replaying: %s", m_typeName.constData(), pending.toUtf8().constData());
      writeToClientSocket(clientSocket, pending);
    }
    m_pendingMessages.clear();
  } else if (m_mismatchedClients.contains(clientSocket)) {
    // After version skew, only accept stop so the GUI can tear down the stale peer.
    if (command == QStringLiteral("stop")) {
      processCommand(clientSocket, command, parts);
    } else {
      LOG_WARN(
          "%s ipc rejecting '%s' from version-mismatched client (reinstall matching binaries)", m_typeName.constData(),
          command.toUtf8().constData()
      );
      writeToClientSocket(clientSocket, QStringLiteral("error=version mismatch"));
    }
  } else if (!m_authenticatedClients.contains(clientSocket)) {
    LOG_WARN("%s ipc client sent command before hello", m_typeName.constData());
    writeToClientSocket(clientSocket, QStringLiteral("error=handshake required"));
    clientSocket->flush();
    clientSocket->disconnectFromServer();
    return;
  } else if (command == QStringLiteral("noop")) {
    LOG_DEBUG("%s ipc server got noop message", m_typeName.constData());
    writeToClientSocket(clientSocket, QStringLiteral("ok=noop"));
  } else {
    processCommand(clientSocket, command, parts);
  }

  clientSocket->flush();
}

void IpcServer::broadcastCommand(const QString &command, const QString &args)
{
  const auto message = args.isEmpty() ? command : QStringLiteral("%1=%2").arg(command, args);

  if (m_authenticatedClients.isEmpty()) {
    LOG_VERBOSE(
        "%s ipc server has no clients, message queued: %s", m_typeName.constData(), message.toUtf8().constData()
    );
    m_pendingMessages.append(message);
    return;
  }

  LOG_VERBOSE(
      "%s ipc server broadcasting message to %d clients: %s", m_typeName.constData(), m_authenticatedClients.size(),
      message.toUtf8().constData()
  );
  for (auto *client : std::as_const(m_authenticatedClients)) {
    writeToClientSocket(client, message);
    client->flush();
  }
}

void IpcServer::writeToClientSocket(QLocalSocket *clientSocket, const QString &message) const
{
  QByteArray messageData = message.toUtf8() + '\n';
  qint64 bytesWritten = clientSocket->write(messageData);
  if (bytesWritten != messageData.size()) {
    LOG_ERR("%s ipc server failed to write full message to client socket", m_typeName.constData());
  } else {
    LOG_VERBOSE(
        "%s ipc server wrote message to client socket: %s", m_typeName.constData(), message.toUtf8().constData()
    );
  }
}

} // namespace deskflow::core::ipc
