/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "server/ClientProxy1_8.h"
#include "server/DisplayLayout.h"

class ClientProxy1_9 : public ClientProxy1_8
{
public:
  ClientProxy1_9(const std::string &name, deskflow::IStream *adoptedStream, Server *server, IEventQueue *events);
  ~ClientProxy1_9() override = default;

  const std::vector<deskflow::server::DisplayRect> &getReportedDisplays() const override;

protected:
  bool parseMessage(const uint8_t *code) override;

private:
  bool recvDisplayInfo();

  std::vector<deskflow::server::DisplayRect> m_reportedDisplays;
};
