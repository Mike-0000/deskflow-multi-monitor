/*
 * Deskflow -- mouse and keyboard sharing utility
 * SPDX-FileCopyrightText: (C) 2026 Deskflow Developers
 * SPDX-License-Identifier: GPL-2.0-only WITH LicenseRef-OpenSSL-Exception
 */

#pragma once

#include "server/DisplayLayout.h"
#include "server/SwitchDecision.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace deskflow::server {

struct SwitchEngineCursor
{
  std::string m_machine;
  int32_t m_x = 0;
  int32_t m_y = 0;
  int32_t m_xDelta = 0;
  int32_t m_yDelta = 0;
  //! Jump-zone size for primary probes and destination landing inset.
  //! Must be the primary jump-zone size even when the active machine is secondary;
  //! secondary clients report 0 from Server::getJumpZoneSize, which under-insets
  //! landings and leaves the cursor inside the reverse jump zone (oscillation).
  int32_t m_jumpZoneSize = 1;
  bool m_absoluteMode = true; //!< primary uses jump-zone probes; secondary uses overshoot
};

struct SwitchEngineRequest
{
  SwitchEngineCursor m_cursor;
  SwitchGateSettings m_settings;
  SwitchGateInput m_gateInput; //!< shape/modifiers/commitOnly; neighbor fields filled by engine
};

enum class SwitchEngineOutcome
{
  None,
  Wait,
  Allow
};

struct SwitchEngineResult
{
  SwitchEngineOutcome m_outcome = SwitchEngineOutcome::None;
  SwitchGateReason m_reason = SwitchGateReason::None;
  SwitchCandidate m_candidate;
  bool m_hasReverseNeighbor = false;
};

//! Unified primary/secondary switch probe over advanced geometry.
class SwitchEngine
{
public:
  explicit SwitchEngine(const WorkspaceLayout &layout);

  //! Resolve geometry candidates for the current cursor (no gate).
  std::vector<SwitchCandidate> collectCandidates(const SwitchEngineCursor &cursor) const;

  //! Probe geometry, pick best candidate, evaluate gate, apply landing inset policy.
  SwitchEngineResult evaluate(SwitchGateState &state, const SwitchEngineRequest &request) const;

  //! Map a single exit direction (used by switch-wait timeout commit).
  std::optional<TransitionResult> resolveDirection(
      const std::string &machine, int32_t localX, int32_t localY, Direction dir
  ) const;

  //! Apply jump-zone landing inset when a reverse neighbor exists.
  //! Uses inset = max(1, jumpZoneSize + 1) so landings clear an inclusive
  //! jump-zone test (x <= left+zone / x >= right-zone).
  static void applyLandingInset(
      TransitionResult &transition, int32_t jumpZoneSize, bool hasReverseNeighbor
  );

  //! True when (x,y) is inside a monitor jump zone that would probe a switch.
  static bool isInJumpZone(
      int32_t x, int32_t y, int32_t localX, int32_t localY, int32_t width, int32_t height, int32_t jumpZoneSize
  );

private:
  SwitchCandidate candidateFromTransition(
      const TransitionResult &transition, Direction dir, int32_t probeX, int32_t probeY, int32_t xActive, int32_t yActive,
      int32_t jumpZoneSize
  ) const;

  const WorkspaceLayout &m_layout;
  GeometryRouter m_router;
};

} // namespace deskflow::server
