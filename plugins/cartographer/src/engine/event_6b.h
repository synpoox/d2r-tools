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
// docs/reverse-engineering/cartographer/13-ambush-treasure-shrine.md. State is cleared when the game is left.
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

// A map was opened: forget the previous map's Boss Drop, Shrine, Treasure and Ambush, whatever the new map's event is. Game thread.
void Event6bMapOpened() noexcept;

auto Event6bPartStatuses() -> std::vector<PartStatus>;

// One line for 'cartographer check'.
auto Event6bSummary() -> std::string;

// A Shrine was decided: resolve its Spawn to an objects row. False (with `why`) when the event cannot be placed, so it is neither
// announced nor waited for. `player` is the player unit (its data flag picks the game's tables). Game thread, at map open.
auto Event6bPrepareShrine(const cartographer::events::Event& event, const std::vector<std::string>& objectClasses, const std::vector<std::string>& objectNames, void* player, std::string* why) -> bool;

// A Boss event or a Treasure monster was accepted: resolve its Drop treasure class and arm the death hook. Does nothing without a Drop.
// Game thread, at map open.
void Event6bArmBossDrop(const cartographer::events::Event& event, int level, void* player);

// Phase 6b, second half: Treasure and Ambush.
//   * Treasure: a Spawn that is a monster is spawned; its death rolls Drop like a Boss's.
//     Any other Spawn is an objects.txt chest, placed like a Shrine; when it is opened (a hook on the chest operate handler, 0x58e2f0)
//     the Drop is rolled with the chest roller (0x4404f0).
//   * Ambush: when the local player is in the event room, `Waves` waves of `Count` x `Spawn` are spawned one after another; a wave ends when
//     all its units died (the SDK monster death listener, by unit id) or on a time-out.

// True when the event is placed as an object (a Shrine, or a Treasure whose Spawn is not a monster), not spawned as monsters.
auto Event6bPlacesObject(const cartographer::events::Event& event, cartographer::events::NameKind spawnKind) noexcept -> bool;

// A Treasure chest was accepted: resolve its objects row (like a Shrine) and its Drop. False (with `why`) when the chest cannot be placed.
// A Drop that cannot be used does not stop the chest. Game thread, at map open.
auto Event6bPrepareChest(const cartographer::events::Event& event, const std::vector<std::string>& objectClasses, const std::vector<std::string>& objectNames, int level, void* player, std::string* why) -> bool;

// The first spawn of a Boss or Treasure event (`unit`, the spawn's return value): track it for the Drop.
void Event6bFirstSpawned(void* game, cartographer::events::Kind kind, void* unit) noexcept;

// An Ambush event room was populated: remember one wave's spawns (`spawns`, planned for the room) and the room's server rect, by which the
// player's room is recognised later (no room pointer is kept; `room` is only logged). Spawns nothing yet.
void Event6bArmAmbush(const cartographer::events::Event& event, const std::vector<cartographer::eventplan::Spawn>& spawns, const cartographer::eventplan::Rect& serverRect, void* room) noexcept;

// Spawns one wave unit: true when the game gave a unit (returned in `unit`). Supplied by event_apply.cpp, which owns the spawn calls.
using AmbushSpawnFn = bool (*)(void* game, void* room, const cartographer::eventplan::Spawn& spawn, void** unit) noexcept;

// Reads a room's server rect; false when it cannot be read. Supplied by event_apply.cpp.
using AmbushRectFn = bool (*)(void* room, cartographer::eventplan::Rect* serverRect) noexcept;

// Each player tick: starts the next wave when the player (in `playerRoom`) is in the event room (matched by its server rect), and ends the
// Ambush when its waves are done. Does nothing without a game. True once, when the first wave made a unit: the Ambush has then happened.
bool Event6bAmbushTick(void* game, void* playerRoom, AmbushRectFn roomRect, AmbushSpawnFn spawn) noexcept;

// The event room was populated: place the prepared Shrine object near the centre of the server rect. `game` and `room` are the
// populator's arguments. True only when the game created the object, so a Shrine that was not placed is never announced. Game thread.
bool Event6bPlaceShrine(void* game, void* room, const cartographer::eventplan::Rect& serverRect) noexcept;

// The Boss event spawned its boss (`unit`, the spawn's return value): remember it for the Drop.
void Event6bBossSpawned(void* unit) noexcept;

// The game was left: forget the shrine, the tracked boss, chest and Ambush. Touches no game memory.
void Event6bGameLeft() noexcept;

}  // namespace carto
