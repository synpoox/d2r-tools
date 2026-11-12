// Cartographer's tables: maps.txt, cartographer.txt and slots.txt, parsed by this plugin (not the SDK's custom tables,
// which reject a whole table on one bad cell). A bad row is skipped with a plain-English message and the other rows
// still load. Rules: docs/cartographer/tables.md and plan section 3.0 (blank means default; our own words ignore case;
// names from D2's tables must match exactly, with a spelling suggestion).
//
// Pure C++ (no Windows, no SDK): tests/tables_test.cpp runs it on any machine.
#pragma once

#include "report.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace carto {

inline constexpr const char* MapsFile = "maps.txt";
inline constexpr const char* SettingsFile = "cartographer.txt";
inline constexpr const char* SlotsFile = "slots.txt";

inline constexpr int DefaultMonsterLevel = 85;
inline constexpr int DefaultProperties = 6;
inline constexpr int DefaultEventChance = 20;
inline constexpr int MaxMonsters = 10;
inline constexpr int Normal = 0, Nightmare = 1, Hell = 2;

// The item spell (misc.txt pSpell) a map's item must have: the plugin hooks the use of items with this spell.
inline constexpr int MapItemSpell = 16;
// The slot (in slots.txt) of the map level every map opens through. Later phases add slots for built layouts.
inline constexpr const char* PresetSlot = "Preset";

// Names the loader checks against D2's own tables. An empty list with `have*` false means the table could not be read:
// those cells are then not checked (fail open) and the report says so.
struct GameNames {
	bool haveMonsters = false, haveSuperUniques = false, haveItems = false;
	std::vector<std::string> monsters;      // monstats.txt Id
	std::vector<std::string> superUniques;  // superuniques.txt Superunique
	std::vector<std::string> itemCodes;     // misc.txt, weapons.txt and armor.txt code
	bool haveItemSpells = false;
	std::vector<std::pair<std::string, int>> itemSpells;  // misc.txt code and pSpell (0 when blank)
};

struct MapDef {
	int line = 0;  // row in maps.txt
	std::string name, tileset, shape, size = "Medium", item, boss;
	std::vector<std::string> monsters;
	int level = DefaultMonsterLevel;
	int difficulty = Hell;
	int minCharLevel = 1;
	int properties = -1;   // -1: use the cartographer.txt value
	int eventChance = -1;  // -1: use the cartographer.txt value
	int density = 0, champions = 0, uniques = 0;
};

struct Settings {
	int properties = DefaultProperties;
	int eventChance = DefaultEventChance;
	int itemStats = 0;
	std::string town = "Harrogath";
};

struct Slot {
	std::string name;
	int level = 0;  // levels.txt Id
};

// Tilesets, shapes, sizes and difficulties as the author writes them.
auto TilesetNames() -> std::vector<std::string>;
auto ReadyTilesetNames() -> std::vector<std::string>;

// maps.txt. Rows with a problem are left out and described in `report`. Every map needs its own Item code.
auto LoadMaps(std::string_view text, const GameNames& names, Report& report) -> std::vector<MapDef>;

// cartographer.txt (Setting, Value). Every setting has a default; a bad value keeps it.
auto LoadSettings(std::string_view text, Report& report) -> Settings;

// slots.txt (Slot, Level), written by the installer.
auto LoadSlots(std::string_view text, Report& report) -> std::vector<Slot>;

// The slot level id for `slot`, or -1.
auto SlotLevel(const std::vector<Slot>& slots, std::string_view slot) -> int;

// The map whose Item is `itemCode` (ignoring case), or nullptr: an item that opens no map is not a map item.
auto FindMapByItem(const std::vector<MapDef>& maps, std::string_view itemCode) -> const MapDef*;

// The map called `name` (ignoring case), or nullptr.
auto FindMap(const std::vector<MapDef>& maps, std::string_view name) -> const MapDef*;

// --- Gates (plan section 2): Hell, Harrogath, one map per game, plus the map's own difficulty and character level ----

struct GateState {
	int difficulty = 0;  // 0 Normal, 1 Nightmare, 2 Hell
	int charLevel = 0;
	int areaId = 0;      // the level the player stands in
	int townId = 0;      // the level the portal opens from
	int openings = 0;    // maps already opened in this game
	bool spotFree = true;
};

enum class Gate { Open, Difficulty, CharLevel, NotInTown, AlreadyOpen, NoSpot };

auto CheckGates(const MapDef& map, const GateState& state) -> Gate;

// The sentence shown to the player and logged when a gate refuses.
auto RefusalText(Gate gate, const MapDef& map, const GateState& state) -> std::string;

}  // namespace carto
