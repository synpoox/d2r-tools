// Map events in the game (phase 6a): the entry message, the event decision at map open, and the Boss and Horde events.
//   * Entry message: when the local player arrives in the map's level, a banner with the map's name and, when an event fired
//     and can be applied, the event's Name and Message (overlay service, fading; the console when there is no overlay).
//   * Decision: at map open, with the pure events core (EventChance, the row draw by Weight, the map seed).
//   * Boss and Horde: spawned in the event room once that room is populated. The hook is on the room populator (0x50acf0),
//     the one function both activation paths (0x4042a0 and 0x408670) call for each room not yet populated; the original
//     runs first, so the room is populated before anything is spawned.
// Every address is found by byte signature at load; a part that finds no single match, or whose guard does not match, is
// logged and left off (the event is then neither spawned nor announced; the map and the banner still work). Nothing here is
// checked in a game yet; the probe lines it logs ("eventprobe:") are for the needs-game checks in
// docs/reverse-engineering/cartographer.md section 9.
#pragma once

#include "engine/portal.h"

#include <D2RLPlugin/api.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace carto {

// Finds the parts, hooks the room populator and registers the banner. Call after InstallPortal. Returns whether events can
// be spawned; `parts` gets the parts added by this call.
auto InstallEventApply(const D2RL::PluginContext* context, const std::vector<SignatureOverride>& overrides, std::vector<PartStatus>* parts) -> bool;

// A map was opened for `playerId` through slot level `level` (`maze`: a Cave or Crypt map, whose event room comes from the
// plan; `player` is the player unit, whose data flag picks the game's tables). Decides the event, resolving its Spawn name from the
// game's loaded tables when they check out (else from the mod's .txt files), and arms the entry message. Game thread.
void EventsMapOpened(const MapDef& map, const std::shared_ptr<const Tables>& tables, uint32_t playerId, uint32_t openings, int level, bool maze, void* player);

// Called from the portal's player tick hook (game thread), after the game's own tick: shows the entry message.
void EventsPlayerTick(void* game, void* unit, uint32_t localPlayer) noexcept;

// The monstats row of every `ids` entry in the game's own table for `dataFlag` (same proven route as the event spawn names), in
// order, and the weight of each (the byte at monstats + 0x64, which the game's region builder copies into its monster groups).
// False, both empty and `why` set when the table cannot be read or an Id is not in it. Game thread, at map open.
auto GameMonsterRows(uint32_t dataFlag, const std::vector<std::string>& ids, std::vector<int>* rows, std::vector<int>* weights, std::string* why) noexcept -> bool;

// The number of rows in the game's levels table (the 64-bit count at tables + 0x1468, the bound the region builder uses) for `dataFlag`.
// False, with `why`, when it cannot be read. Game thread.
auto GameLevelCount(uint32_t dataFlag, int64_t* count, std::string* why) noexcept -> bool;

// The game was left: forget the map and the event. Touches no game memory.
void EventsGameLeft() noexcept;

auto EventPartStatuses() -> std::vector<PartStatus>;

// For 'cartographer check': one line saying what is applied.
auto EventsSummary() -> std::string;

}  // namespace carto
