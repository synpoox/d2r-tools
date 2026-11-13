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
};

// The row order is the slot index in the maze hooks. Cave keeps the slot "Maze" of the installs before the table existed.
inline constexpr MazeTileset MazeTilesets[] = {
	{"Cave", 3, "Maze", "Cartographer Maze", 24, 52, {83, 84, 85, 86}, false},
	{"Crypt", 4, "MazeCrypt", "Cartographer Crypt", 8, 108, {139, 140, 141, 142}, false},
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
