// Spec panel (part "specui"): a slot showing the active spec with an arrow above and below, docked to the skill tree's left
// edge and opened and closed with it (design section 11). An arrow or the mouse wheel steps through the specs by running
// the hidden "spec-panel N" command, so a panel swap takes the same path and the same checks as a typed "spec N" but prints
// nothing unless it fails; the panel only shows the result. Layout: spec_panel_layout.h.
#pragma once

#include <D2RLPlugin/api.h>

#include <cstddef>
#include <cstdint>

namespace ms {

// Registers the panel and the UI message listener. Writes one "on: ..." / "off: ..." line to report.
void InstallSpecPanel(const D2RL::PluginContext* context, int32_t specCount, char* report, size_t size) noexcept;

// UI thread, after any spec command: shows the active spec (0 = unknown), and live arrows in town only.
void UpdateSpecPanel(const D2RL::PluginContext* context, int32_t active, bool inTown) noexcept;

// At unload: gives the game window its own message handler back.
void RemoveSpecPanel() noexcept;

}  // namespace ms
