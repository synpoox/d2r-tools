// UI probe (part "uiprobe"): design section 11, spike 4. Finds which UI messages the skill tree sends when it opens and
// closes, and whether a plugin can open it. A diagnosis tool, not a player feature: it logs nothing until switched on.
//
// Console command "msprobe":
//   msprobe on | off   log every UI message (target, command, text) to the plugin log and the console
//   msprobe toggle     dispatch PanelManager:TogglePanel:SkillsTreePanel
//   msprobe find       look the skill tree up by name (WidgetService::findPanel) and log its rect
//   msprobe panel      toggle the spike 5 test panel multispec/ProbePanel (a strip left of the skill tree)
//   msprobe dock [off] the test panel opens and closes with the skill tree (PanelManager Open/ClosePanel)
// Clicks on the test panel's buttons (MultiSpec:Probe:<n>) are always logged.
// The listener always returns Continue: the probe never changes what the game does with a message.
#pragma once

#include <D2RLPlugin/api.h>

#include <cstddef>

namespace ms {

// Registers the UI message listener and the msprobe command. Writes one "on: ..." / "off: ..." line to report.
void InstallUiProbe(const D2RL::PluginContext* context, char* report, size_t size) noexcept;

}  // namespace ms
