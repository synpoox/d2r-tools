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
	int prevPiece[4];       // the entrance family ("Prev" group) by side W, E, S, N; all 0: no entrance family, the entrance is an ordinary room (every row, #455: a Prev piece holds a level exit tile)
	bool callsRingGrower;   // the tileset's case in the maze builder 0x3e69e0 calls the ring grower 0x3e9820 first
	int offsetX;            // the levels.txt OffsetX tools\install.ps1 gives the slot level (its -<Param>OffsetX default)
	int offsetY;            // the levels.txt OffsetY (its -<Param>OffsetY default)
	int ringCells;          // filler rooms (the closed piece) this many cells around the layout, so no void shows past its outer rooms; 0: none
	bool ownGrower;         // the case calls its own room grower (Arcane, 0x3e7a80) instead of the walk 0x3e96d0: the steered walk runs inside that grower's hook
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
	// No row has a Prev family: its entrance rooms (Cave 83-86, Crypt 139-142, Ice 1018-1021) carry the level's Up stairs, a level exit tile, and a
	// map's only way in and out is its portal (docs/reverse-engineering/cartographer/35-map-link-tiles.md, issue #455). The entrance is an ordinary room.
	// Offsets: y 0 and x 3500, 3000, 2500, 2000 (Cave, Crypt, Lava, Ice; 500 apart), so every level fits the automap range and sits above the first Act 5 level (y 1000).
	{"Cave", 3, "Maze", "Cartographer Maze", 24, 52, {0, 0, 0, 0}, false, 3500, 0, 0},
	{"Crypt", 4, "MazeCrypt", "Cartographer Crypt", 8, 108, {0, 0, 0, 0}, false, 3000, 0, 0},
	// Lava (River of Flame, LevelType 28): 24-tile rooms, "Lava W" is Def 837 (base 836, 837..851). Its case in 0x3e69e0 neither calls the ring grower
	// nor places an entrance or exit (no 0x3eb850 call), so there is no Prev family: the entrance is an ordinary room. The pieces close their own sides with a few tiles of lava and the engine
	// draws nothing past the rooms, so one ring of filler rooms goes around the layout (ringCells, issue #372). docs/reverse-engineering/cartographer/23-maze-lava.md.
	{"Lava", 28, "MazeLava", "Cartographer Lava", 24, 836, {0, 0, 0, 0}, false, 2500, 0, 1},
	// Ice: its case in the builder calls the ring grower first (docs/reverse-engineering/cartographer/24-ice-caves-builder.md).
	{"Ice", 33, "MazeIce", "Cartographer Ice Caves", 16, 1002, {0, 0, 0, 0}, true, 2000, 0, 0},
	// Durance (Act 3 Mephisto 1 and 2, LevelType 22): 12-tile rooms, "Mephisto W" is Def 754 (base 753, 754..768). Offset x 1000, y 0: the next free
	// slot in the band, 500 below Snow's x 1500. docs/cartographer/tilesets/a3-durance-of-hate.md.
	{"Durance", 22, "MazeDurance", "Cartographer Durance", 12, 753, {0, 0, 0, 0}, true, 1000, 0, 0},
	// Arcane Sanctuary (LevelType 19, #280): 12-tile rooms, pieces 509 + mask (510..524; 509 itself is a Lair row, so there is no closed piece and no filler ring: the void
	// around the platforms is the tileset's own look). Its case calls the grower 0x3e7a80, neither the walk nor the ring grower, then one special-room call (the Summoner
	// rooms 525..528, always suppressed). docs/reverse-engineering/cartographer/37-arcane-grower.md. x 500: below the Durance row (x 1000), clear of Snow (1500..1692).
	{"Arcane", 19, "MazeArcane", "Cartographer Arcane Sanctuary", 12, 509, {0, 0, 0, 0}, false, 500, 0, 0, true},
	// Dungeon (Act 3 Dungeon 1 and 2, LevelType 24, #244): 14-tile rooms, "Dungeon W" is Def 665 (base 664, 665..679). Its case calls the ring grower, the walk and one
	// special-room call (the Prev pieces 695..698, never placed). Offset x 0, y 0: the next free slot, clear of Arcane (500..860).
	// docs/reverse-engineering/cartographer/39-dungeon-builder.md, docs/cartographer/tilesets/a3-dungeon.md.
	{"Dungeon", 24, "MazeDungeon", "Cartographer Dungeon", 14, 664, {0, 0, 0, 0}, true, 0, 0, 0},
	// Jail (Act 1 Jail, LevelType 8, #273): 12-tile rooms, "Jail W" is Def 206 (base 205, 206..220). Its case calls the ring grower first, then the walk, then two
	// special-room calls (Prev and Next, both suppressed): docs/reverse-engineering/cartographer/40-jail-case.md. Offset x 0, y 500: the y-0 band is full, so
	// the next rows go in a second band below it (y 500..860, 500 under the first, still above the first Act 5 level at y 1000).
	{"Jail", 8, "MazeJail", "Cartographer Jail", 12, 205, {0, 0, 0, 0}, true, 0, 500, 0},
	// Lair (Act 2 Maggot Lair, LevelType 18, #279): 10-tile rooms, "Lair W" is Def 482 (base 481, 482..496). Its case calls the ring grower, the walk, then two
	// special-room calls (Next and Prev, both suppressed): docs/reverse-engineering/cartographer/41-lair-builder.md. Offset x 500, y 500: the second band, right of Jail
	// (x 0, y 500, #273) and clear of Barracks (x 1000, y 500, #272); y 500..860 stays above the first Act 5 level (y 1000).
	{"Lair", 18, "MazeLair", "Cartographer Lair", 10, 481, {0, 0, 0, 0}, true, 500, 500, 0},
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
