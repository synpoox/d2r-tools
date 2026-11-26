// A slot map's revealed automap, kept in memory for the game instead of the character's map file (issue #306 follow-up; cartographer/22-automap-memory.md).
// The game appends a record of the newly revealed cells per layer each time it leaves a layer (0xd5fe0), and its read (0xd5710) loads lists 1, 2 and 4 of every
// record of that layer whatever the seed: a map that shares its layer with a game level (Jail 18, Cave 2, ...) got every earlier map's walls, and the game
// level's, back on the return from town. So a map's layer object never reads or writes the file: its saves are taken here and played back on the return.
// No game or SDK in it, so it can be unit tested.
#pragma once

#include <array>
#include <cstdint>
#include <map>
#include <set>

namespace carto {

using AutomapCell = std::array<int16_t, 3>;                 // one saved cell: the three words 0xd7ce0 writes (kind, x, y)
using AutomapLists = std::array<std::set<AutomapCell>, 4>;  // the layer object's four cell lists

enum class ReadVerdict { File, Empty, Replay };

struct MapAutomap {
	int key = -1;  // the map's layer key, learnt on its entry read (-1: not entered yet)
	AutomapLists lists;
};

struct AutomapMemory {
	std::map<int, MapAutomap> maps;  // per slot level: the map built last through it in this game
	int activeKey = -1;              // the current layer object belongs to a map: its key, else -1
	int activeSlot = -1;

	// A new map is built through `slot`: it starts with no cells.
	void NewMap(int slot) { maps[slot] = {}; }

	// The game makes layer `key` current and reads it back; `skipped` says the entry skip (automap_skip.h) took this read, `level` is the player's level.
	auto OnRead(int key, int level, bool skipped) -> ReadVerdict {
		activeKey = activeSlot = -1;  // the object left was saved before this read
		if (key < 0) {
			return ReadVerdict::File;
		}
		if (skipped) {
			MapAutomap& map = maps[level];
			map.key = key;
			activeKey = key;
			activeSlot = level;
			return ReadVerdict::Empty;
		}
		const auto it = maps.find(level);
		if (it == maps.end() || it->second.key != key) {
			return ReadVerdict::File;  // not a map's layer: the game's own read
		}
		activeKey = key;
		activeSlot = level;
		return ReadVerdict::Replay;
	}

	// The game saves the record of layer `key`. True: it is the current map's, taken into `maps` (the caller must not write it to the file).
	template <class Each>
	auto OnSave(int key, Each&& each) -> bool {
		if (activeKey < 0 || key != activeKey) {
			return false;
		}
		AutomapLists& lists = maps[activeSlot].lists;
		each([&lists](int list, const AutomapCell& cell) {
			if (list >= 0 && list < 4) {
				lists[static_cast<size_t>(list)].insert(cell);
			}
		});
		return true;
	}

	auto Cells(int slot) const -> const AutomapLists* {
		const auto it = maps.find(slot);
		return it == maps.end() ? nullptr : &it->second.lists;
	}

	// A game was left. The current layer stays marked: the game may save it after the GameLeft event, and a map's cells must not reach the file then either.
	void Clear() { maps.clear(); }
};

}  // namespace carto
