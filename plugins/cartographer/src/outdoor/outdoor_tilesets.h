// The one table of outdoor tilesets. Every place that used to name Jungle and Snow reads a row of this table: the planner dispatch
// (outdoor_plan.cpp), the loader's ready list, slot names, level names and LevelTypes (core/tables.cpp), and the per-slot arrays of the
// outdoor hooks (engine/outdoor_hooks.cpp). tools/install.ps1 keeps the same rows in $OutdoorTilesets (plus the level geometry the
// installer needs); tests/outdoor_tests.cpp checks that the two agree. A new outdoor tileset is one row here, one row there, its
// Planner value and its planner. See docs/cartographer/adding-a-tileset.md.
#pragma once

#include <cstddef>
#include <string>
#include <string_view>

#include "ascii.h"

namespace cartographer::outdoor {

// Which planner builds the tileset's map (the switch in planOutdoorMap).
enum class Planner { Jungle, Snow };

struct OutdoorTileset {
	const char *name;       // the maps.txt Tileset
	int levelType;          // the levels.txt LevelType its slot level must have
	const char *slot;       // the slots.txt row name of its slot level
	const char *levelName;  // the levels.txt Name tools\install.ps1 gives that level
	Planner planner;
};

// The row order is the slot index in the outdoor hooks: Jungle is 0 (the default for an unknown tileset), Snow is 1.
inline constexpr OutdoorTileset OutdoorTilesets[] = {
	{"Jungle", 21, "Outdoor", "Cartographer Outdoor", Planner::Jungle},
	{"Snow", 31, "OutdoorSnow", "Cartographer Snow", Planner::Snow},
};
inline constexpr size_t OutdoorTilesetCount = sizeof(OutdoorTilesets) / sizeof(OutdoorTilesets[0]);

// The row for a tileset name (ignoring case), or nullptr.
inline const OutdoorTileset *findOutdoorTileset(std::string_view tileset) noexcept {
	for (const OutdoorTileset &t : OutdoorTilesets)
		if (layout::equalsNoCase(t.name, tileset)) return &t;
	return nullptr;
}

// The row for a slot name (ignoring case), or nullptr.
inline const OutdoorTileset *findOutdoorTilesetBySlot(std::string_view slot) noexcept {
	for (const OutdoorTileset &t : OutdoorTilesets)
		if (layout::equalsNoCase(t.slot, slot)) return &t;
	return nullptr;
}

// The row index for a tileset name; 0 for a name that is not in the table.
inline size_t outdoorTilesetIndex(std::string_view tileset) noexcept {
	for (size_t i = 0; i < OutdoorTilesetCount; ++i)
		if (layout::equalsNoCase(OutdoorTilesets[i].name, tileset)) return i;
	return 0;
}

// "Jungle or Snow", "Jungle, Snow or Lava": the names for a message.
inline std::string outdoorTilesetNames() {
	std::string out;
	for (size_t i = 0; i < OutdoorTilesetCount; ++i) {
		if (i > 0) out += i + 1 == OutdoorTilesetCount ? " or " : ", ";
		out += OutdoorTilesets[i].name;
	}
	return out;
}

} // namespace cartographer::outdoor
