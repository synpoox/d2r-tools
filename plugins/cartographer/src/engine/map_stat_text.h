// The tooltip text of the map level stat (cartographer_map_level in itemstatcost.txt). tools\install.ps1 writes the stat's description
// as the plugin's own string key (cartographer:map_level_stat); this registers its text through the SDK's resource service while the
// plugin loads, before the game compiles the table. Without the service the game shows its text for an unknown key.
#pragma once

#include <D2RLPlugin/api.h>

#include <string>

namespace carto {

void InstallMapStatText(const D2RL::PluginContext* context);

// For 'cartographer check'.
auto MapStatTextSummary() -> std::string;

// Removes the registration (unload).
void MapStatTextUnload() noexcept;

}  // namespace carto
