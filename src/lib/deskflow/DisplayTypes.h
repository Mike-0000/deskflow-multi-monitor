/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct PlatformDisplayInfo
{
  std::string m_id;
  std::string m_name;
  int32_t m_x = 0;
  int32_t m_y = 0;
  int32_t m_width = 0;
  int32_t m_height = 0;
  float m_scale = 1.0f;
  int32_t m_dpi = 96;
};

using PlatformDisplayList = std::vector<PlatformDisplayInfo>;
