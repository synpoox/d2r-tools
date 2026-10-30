// Part "hud": the lines as native D2R text boxes in a plugin panel (findings (f)). The plugin's layout is served from
// memory (ResourceService) under its own name, the panel is opened with PanelService, and the text is set with the
// game's own TextBoxWidget setter. Replaces the overlay for drawing: D2RLoader's overlay draws nothing (findings (e)).
//
// NOT RUN IN GAME YET: needs live check throughout (TESTING.md).
#pragma once

#include "bxb/settings.h"

#include <D2RLPlugin/api.h>

#include <cstddef>
#include <string>
#include <vector>

namespace hud_text {

// At plugin load, after the config is read: byte checks, layout resource (built from the settings) and panel.
void Install(const D2RL::PluginContext* context, const bxb::Settings& settings, char* report, size_t size) noexcept;
auto Installed() noexcept -> bool;

// UI thread only. Shows the lines (opening the panel when needed) or, with show = false or no lines, closes it.
// While the stock XP bar is hovered the lines are shown anyway. Returns true when the player clicked the stock XP bar
// since the last call: the caller flips `show` for later calls (this call already used the flipped value).
auto Update(const std::vector<std::string>& lines, bool show) noexcept -> bool;

// One status fragment for the status command.
void Status(char* out, size_t size) noexcept;

}  // namespace hud_text
