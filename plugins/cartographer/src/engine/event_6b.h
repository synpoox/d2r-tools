// Map events, phase 6b, first half: the Shrine event kind and the Boss `Drop` column.
//   * Shrine: the event's object (a shrine, well or chest, by objects.txt row) is created at a free spot near the centre of the
//     event room, once that room is populated (the same moment and room as the Boss and Horde spawns). The Spawn name is the row's
//     line in objects.txt, accepted only when exactly one line has it and the line count equals the game's row count; else the
//     Spawn can be the row number. The free-spot search (0x402f50) and the create (0x490410) are the calls the game's debug
//     command `SpawnObject` makes.
//   * Boss Drop: the boss the event spawned is remembered by unit id; when it dies, the game's drop roller (0x441300) is called once
//     more with the Drop treasure class, from a hook on the monster death drop (0x447b80) that runs the original first.
// Every address is found by byte signature at load (0x402f50 as the call target inside the SpawnObject handler); a part that does not
// match is left off and logged, the rest keep working, and the event is simply not announced (Shrine) or has no extra drop (Boss).
// Nothing here is checked in a game yet; the "eventprobe:" lines it logs are for the needs-game checks in
// docs/reverse-engineering/cartographer.md section 13. State is cleared when the game is left.
#pragma once

#include "engine/portal.h"
#include "engine/scan.h"

#include <D2RLPlugin/api.h>

#include "events.h"
#include "eventplan.h"

#include <cstdint>
#include <string>
#include <vector>

namespace carto {

// Finds the parts and installs the death hook (at load only, chained through the loader). `dataTables` and `nameToRow` are the parts
// event_apply.cpp already resolved (shared, not scanned for twice); `populatorOn` is whether its room populator hook is in.
void InstallEvent6b(const D2RL::PluginContext* context, const std::vector<SignatureOverride>& overrides, const Part& dataTables, const Part& nameToRow, bool populatorOn);

// A map was opened: forget the previous map's Boss Drop and Shrine, whatever the new map's event is. Game thread.
void Event6bMapOpened() noexcept;

auto Event6bPartStatuses() -> std::vector<PartStatus>;

// One line for 'cartographer check'.
auto Event6bSummary() -> std::string;

// A Shrine was decided: resolve its Spawn to an objects row. False (with `why`) when the event cannot be placed, so it is neither
// announced nor waited for. `player` is the player unit (its data flag picks the game's tables). Game thread, at map open.
auto Event6bPrepareShrine(const cartographer::events::Event& event, const std::vector<std::string>& objectNames, void* player, std::string* why) -> bool;

// A Boss event was accepted: resolve its Drop treasure class and arm the death hook. Does nothing without a Drop. Game thread, at map open.
void Event6bArmBossDrop(const cartographer::events::Event& event, int level, void* player);

// The event room was populated: place the prepared Shrine object near the centre of the server rect. `game` and `room` are the
// populator's arguments. Game thread.
void Event6bPlaceShrine(void* game, void* room, const cartographer::eventplan::Rect& serverRect, int scale, uint64_t seed) noexcept;

// The Boss event spawned its boss (`unit`, the spawn's return value): remember it for the Drop.
void Event6bBossSpawned(void* unit) noexcept;

// The game was left: forget the shrine and the tracked boss. Touches no game memory.
void Event6bGameLeft() noexcept;

}  // namespace carto
