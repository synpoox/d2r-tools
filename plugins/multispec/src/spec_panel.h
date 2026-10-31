// Spec panel (part "specui"): numbered spec slots docked to the skill tree's left edge, opened and closed with it
// (design section 11). A click runs the "spec N" console command, so a panel swap takes the same path and the same checks
// as a typed one; the panel only shows the result. Layout: spec_panel_layout.h.
#pragma once

#include <D2RLPlugin/api.h>

#include <cstddef>
#include <cstdint>

namespace ms {

// Registers the panel and the UI message listener. Writes one "on: ..." / "off: ..." line to report.
void InstallSpecPanel(const D2RL::PluginContext* context, int32_t specCount, char* report, size_t size) noexcept;

// UI thread, after any spec command: marks the active spec (0 = unknown) and enables the slots in town only.
void UpdateSpecPanel(const D2RL::PluginContext* context, int32_t active, bool inTown) noexcept;

}  // namespace ms
