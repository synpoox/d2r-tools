// The map's monster columns in the game (issue #156). When a map opens, `Level` is written into the slot level's own row of the
// game's levels table (the monster level fields, which the game reads on each call), and `Density`, `Uniques` and `Monsters` into
// the slot level's per-game region record (`*(game + 0x1d0)` indexed by level: the density, the unique pack counts and the monster
// groups, which the game copied from the row when the game was created and its spawn code reads). The row's originals are put back
// on leaving the game, on unload and when the portal does not open; the record belongs to the game, so a failed open puts it back
// and leaving the game only forgets it. `Champions` has no field and is not applied. Only a slot level (above the last vanilla level)
// is ever written. Each part is switched off with a log line when its memory does not look as described. Written, not verified in a
// game. Notes: docs/reverse-engineering/cartographer.md section 15; pure part: src/levelrow.
#pragma once

#include "core/tables.h"

#include <cstdint>
#include <string>

namespace carto {

// Called on the game thread just before the game opens the portal for `map` through slot level `level` (`slotLevel` is the level
// the slots table gives for the map's slot, `dataFlag` the byte at game + 0x106). Returns whether anything was changed.
auto LevelRowBeforeOpen(const MapDef& map, int level, int slotLevel, const void* game, uint32_t dataFlag) noexcept -> bool;

// The portal for `level` did not open after LevelRowBeforeOpen wrote it: puts back only what that call wrote.
void LevelRowOpenFailed(int level) noexcept;

// GameLeft: puts the original value back in every row field written, and forgets the record fields (the record is the game's and
// goes with it). Safe to call at any time, any number of times.
void LevelRowRestore() noexcept;

// The same for unload. It does not call the game: it writes through the row pointers kept at write time, after checking the memory,
// and only into fields that still hold the plugin's values.
void LevelRowUnload() noexcept;

// What the columns do now, for 'cartographer check'.
auto LevelRowSummary() -> std::string;

}  // namespace carto
