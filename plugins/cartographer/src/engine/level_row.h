// The map's monster columns in the game (issue #156): when a map opens, `Level`, `Monsters`, `Density`, `Champions` and `Uniques`
// are written into the slot level's own row of the game's levels table (the monster level fields, NumMon and the three monster
// lists, MonDen, MonUMin and MonUMax), and the originals are put back on leaving the game, on unload and when the portal does not
// open. Only the slot level's row is ever written. The row is found the way MapNames finds it (the LevelsRow part, called only),
// and each part is switched off with a log line when the row does not look as the table loader describes it. Written, not
// verified in a game. Notes: docs/reverse-engineering/cartographer.md section 10b; pure part: src/levelrow.
#pragma once

#include "core/tables.h"

#include <cstdint>
#include <string>

namespace carto {

// Called on the game thread just before the game opens the portal for `map` through slot level `level` (`dataFlag` is the game's
// data flag, the byte at game + 0x106). Writes what the map asks for into the slot row. Returns whether the row was changed.
auto LevelRowBeforeOpen(const MapDef& map, int level, uint32_t dataFlag) noexcept -> bool;

// The portal for `level` did not open after LevelRowBeforeOpen wrote it: puts back only what that call wrote.
void LevelRowOpenFailed(int level) noexcept;

// Puts the original value back in every field written, in every row changed. Safe to call at any time, any number of times: on GameLeft.
void LevelRowRestore() noexcept;

// The same restore for unload. It does not call the game: it writes through the row pointers kept at write time, after checking
// the memory, and only into fields that still hold the plugin's values.
void LevelRowUnload() noexcept;

// What the columns do now, for 'cartographer check'.
auto LevelRowSummary() -> std::string;

}  // namespace carto
