/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#include "server/ClientProxy1_9.h"

#include "base/Log.h"
#include "deskflow/ProtocolTypes.h"
#include "deskflow/ProtocolUtil.h"
#include "io/IStream.h"
#include "server/Server.h"

#include <cstring>

ClientProxy1_9::ClientProxy1_9(
    const std::string &name, deskflow::IStream *adoptedStream, Server *server, IEventQueue *events
)
    : ClientProxy1_8(name, adoptedStream, server, events)
{
}

const std::vector<deskflow::server::DisplayRect> &ClientProxy1_9::getReportedDisplays() const
{
  return m_reportedDisplays;
}

bool ClientProxy1_9::parseMessage(const uint8_t *code)
{
  if (memcmp(code, kMsgDDisplayInfo, 4) == 0) {
    return recvDisplayInfo();
  }
  return ClientProxy1_5::parseMessage(code);
}

bool ClientProxy1_9::recvDisplayInfo()
{
  int16_t count = 0;
  if (!ProtocolUtil::readf(getStream(), kMsgDDisplayInfo + 4, &count)) {
    return false;
  }

  if (count < 0 || count > 64) {
    return false;
  }

  m_reportedDisplays.clear();
  m_reportedDisplays.reserve(static_cast<std::size_t>(count));

  for (int16_t i = 0; i < count; ++i) {
    std::string id;
    int16_t localX = 0;
    int16_t localY = 0;
    int16_t width = 0;
    int16_t height = 0;
    int16_t scale100 = 100;
    int16_t dpi = 96;
    if (!ProtocolUtil::readf(
            getStream(), kMsgDDisplayInfoArgs, &id, &localX, &localY, &width, &height, &scale100, &dpi
        )) {
      return false;
    }

    if (width <= 0 || height <= 0) {
      continue;
    }

    deskflow::server::DisplayRect display;
    display.m_id = id;
    display.m_name = id;
    display.m_localX = localX;
    display.m_localY = localY;
    display.m_width = width;
    display.m_height = height;
    display.m_worldX = localX;
    display.m_worldY = localY;
    display.m_scale = static_cast<float>(scale100) / 100.0f;
    display.m_dpi = dpi;
    m_reportedDisplays.push_back(display);
  }

  LOG_DEBUG("received %zu display(s) from client \"%s\"", m_reportedDisplays.size(), getName().c_str());
  return true;
}
