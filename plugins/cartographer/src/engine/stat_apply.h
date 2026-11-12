// Map stats in the game: when a map opens, draws its properties (stats core) and applies them through the game's own
// stat lists: the monster stats to every monster that spawns in the slot level (hook on the monster setup call), the
// player stats to a player while the player stands in the slot level (from the player tick the portal already hooks).
// Every address is found by byte signature at load; a part that finds no single match, or whose guard does not match,
// is logged and left off (a map then opens without that half of its stats). Nothing here is checked in a game yet; the
// probe lines it logs ("statprobe:") are for the needs-game checks in docs/reverse-engineering/cartographer.md section 6.
#pragma once

#include "engine/portal.h"

#include <D2RLPlugin/api.h>

#include <cstdint>
#include <memory>
#include <vector>

namespace carto {

// Finds the parts and hooks the monster setup. Call after InstallPortal (the player half follows the portal's tick part).
// Returns whether at least one half (monsters or players) can be applied; `parts` gets the parts added by this call.
auto InstallStatApply(const D2RL::PluginContext* context, const std::vector<SignatureOverride>& overrides, std::vector<PartStatus>* parts) -> bool;

// A map was opened for `playerId` (the opening count is how many maps this game has opened, for the seed). Draws the
// map's properties and starts applying them. Game thread. Does nothing when neither half is on.
void StatApplyMapOpened(const MapDef& map, const std::shared_ptr<const Tables>& tables, uint32_t playerId, uint32_t openings);

// The PlayerStats setting of cartographer.txt (off by default). While off, nothing is ever attached to the player: the
// player's list sits on the unit and a Save and Exit or an autosave inside the map may happen before it comes off. Only
// for a test slot until the save check in docs/reverse-engineering/cartographer.md section 7 has passed.
void StatApplySetPlayerStats(bool on) noexcept;

// Called from the portal's player tick hook (game thread), after the game's own tick. `game` and `unit` are the tick's.
void StatApplyPlayerTick(void* game, void* unit) noexcept;

// The game was left: forget the map and every list. Touches no game memory (the game has freed or will free the units).
void StatApplyGameLeft() noexcept;

auto StatApplyPartStatuses() -> std::vector<PartStatus>;

// For 'cartographer check': one line saying what is applied.
auto StatApplySummary() -> std::string;

}  // namespace carto
