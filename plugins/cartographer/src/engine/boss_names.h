// A dedicated map Boss shows its own bosses.txt Name in game (issue #174). The superuniques.txt Name cell the installer writes is
// a key in the plugin's string namespace (cartographer:boss_<Name>); this registers a text for each bosses.txt entry through the
// SDK's resource service while the plugin loads, before the game compiles the table. Written, not verified in game. Notes:
// docs/reverse-engineering/cartographer/10-portal-tooltip.md.
#pragma once

#include "engine/portal.h"

#include <D2RLPlugin/api.h>

#include <memory>
#include <string>

namespace carto {

// Finds the resource service and registers the boss strings for the tables set so far. Call after SetTables, as early as possible
// in plugin load. Fails open: without the service the boss shows the game's text for an unknown key.
void InstallBossNames(const D2RL::PluginContext* context);

// The tables changed (load or reload): registers the strings again when the names changed. Any thread.
void BossNamesSetTables(const std::shared_ptr<const Tables>& tables);

// What the registration holds, for 'cartographer check'.
auto BossNamesSummary() -> std::string;

// Removes the registration (unload).
void BossNamesUnload() noexcept;

}  // namespace carto
