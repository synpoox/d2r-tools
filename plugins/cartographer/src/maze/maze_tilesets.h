// The one table of maze tilesets, like outdoor/outdoor_tilesets.h. Every place that used to name Cave and Crypt reads a row of this table: the
// numbers the planner needs (builtinTileset in steer.cpp), the loader's ready list, slot names, level names and LevelTypes (core/tables.cpp), and
// the per-slot level ids of the maze hooks (engine/maze_hooks.cpp). tools/install.ps1 keeps the same rows in $MazeTilesets (plus the level
// geometry the installer needs); tests/maze_tests.cpp checks that the two agree. A level's LevelType fixes its tileset, so each maze tileset has
// a Maze slot level of its own. A new maze tileset is one row here, one row there and its numbers in docs/cartographer/tilesets/. See
// docs/cartographer/adding-a-tileset.md.
#pragma once

#include <cstddef>
#include <string>
#include <string_view>

#include "ascii.h"

namespace cartographer::maze {

struct MazeTileset {
	const char *name;       // the maps.txt Tileset
	int levelType;          // the levels.txt LevelType its slot level must have
	const char *slot;       // the slots.txt row name of its slot level
	const char *levelName;  // the levels.txt Name tools\install.ps1 gives that level
	int roomTiles;          // LvlMaze SizeX = SizeY: a room is roomTiles x roomTiles tiles
	int pieceBase;          // a room's piece is pieceBase + exit mask
	int prevPiece[4];       // the entrance family ("Prev" group) by side W, E, S, N; all 0: no entrance family, the entrance is an ordinary room
	bool callsRingGrower;   // the tileset's case in the maze builder 0x3e69e0 calls the ring grower 0x3e9820 first
	int offsetX;            // the levels.txt OffsetX tools\install.ps1 gives the slot level (its -<Param>OffsetX default)
	int offsetY;            // the levels.txt OffsetY (its -<Param>OffsetY default)
	int ringCells;          // filler rooms (the closed piece) this many cells around the layout, so no void shows past its outer rooms; 0: none
};

// Every maze slot level is levelTiles x levelTiles (SizeX and SizeY in levels.txt; the largest plan is 360 tiles on a side).
inline constexpr int levelTiles = 360;

// The game saves a level's automap cells as signed 16-bit numbers when the player leaves it (x = (tx - ty) * 8, y = (tx + ty) * 4) and a level whose
// cells do not fit loses its revealed map after a trip to town (docs/reverse-engineering/cartographer/22-automap-memory.md). True when all four
// corners of the size x size level at (offsetX, offsetY) fit: tx - ty from -4096 to 4095 and tx + ty up to 8191. tools/install.ps1 has the same test
// (Test-AutomapRange) and refuses an offset that fails it.
constexpr bool automapInRange(int offsetX, int offsetY, int size) noexcept {
	return offsetX - offsetY + size <= 4095 && offsetY - offsetX + size <= 4096 && offsetX + offsetY + 2 * size <= 8191;
}

// The row order is the slot index in the maze hooks. Cave keeps the slot "Maze" of the installs before the table existed.
inline constexpr MazeTileset MazeTilesets[] = {
	// Offsets: y 0 and x 3500, 3000, 2500, 2000 (Cave, Crypt, Lava, Ice; 500 apart), so every level fits the automap range and sits above the first Act 5 level (y 1000).
	{"Cave", 3, "Maze", "Cartographer Maze", 24, 52, {83, 84, 85, 86}, false, 3500, 0, 0},
	{"Crypt", 4, "MazeCrypt", "Cartographer Crypt", 8, 108, {139, 140, 141, 142}, false, 3000, 0, 0},
	// Lava (River of Flame, LevelType 28): 24-tile rooms, "Lava W" is Def 837 (base 836, 837..851). Its case in 0x3e69e0 neither calls the ring grower
	// nor places an entrance or exit (no 0x3eb850 call), so there is no Prev family: the entrance is an ordinary room. The pieces close their own sides with a few tiles of lava and the engine
	// draws nothing past the rooms, so one ring of filler rooms goes around the layout (ringCells, issue #372). docs/reverse-engineering/cartographer/23-maze-lava.md.
	{"Lava", 28, "MazeLava", "Cartographer Lava", 24, 836, {0, 0, 0, 0}, false, 2500, 0, 1},
};
inline constexpr size_t MazeTilesetCount = sizeof(MazeTilesets) / sizeof(MazeTilesets[0]);

// The row for a tileset name (ignoring case), or nullptr.
inline const MazeTileset *findMazeTileset(std::string_view tileset) noexcept {
	for (const MazeTileset &t : MazeTilesets)
		if (layout::equalsNoCase(t.name, tileset)) return &t;
	return nullptr;
}

// The row for a slot name (ignoring case), or nullptr.
inline const MazeTileset *findMazeTilesetBySlot(std::string_view slot) noexcept {
	for (const MazeTileset &t : MazeTilesets)
		if (layout::equalsNoCase(t.slot, slot)) return &t;
	return nullptr;
}

// The row index for a tileset name, or MazeTilesetCount for a name that is not in the table.
inline size_t mazeTilesetIndex(std::string_view tileset) noexcept {
	for (size_t i = 0; i < MazeTilesetCount; ++i)
		if (layout::equalsNoCase(MazeTilesets[i].name, tileset)) return i;
	return MazeTilesetCount;
}

// "Cave or Crypt", "Cave, Crypt or Ice": the names for a message.
inline std::string mazeTilesetNames() {
	std::string out;
	for (size_t i = 0; i < MazeTilesetCount; ++i) {
		if (i > 0) out += i + 1 == MazeTilesetCount ? " or " : ", ";
		out += MazeTilesets[i].name;
	}
	return out;
}

} // namespace cartographer::maze
