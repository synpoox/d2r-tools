// Cartographer's tables: maps.txt, cartographer.txt and slots.txt, parsed by this plugin (not the SDK's custom tables,
// which reject a whole table on one bad cell). A bad row is skipped with a plain-English message and the other rows
// still load. Rules: docs/cartographer/tables.md and plan section 3.0 (blank means default; our own words ignore case;
// names from D2's tables must match exactly, with a spelling suggestion).
//
// Pure C++ (no Windows, no SDK): tests/tables_test.cpp runs it on any machine.
#pragma once

#include "events.h"
#include "layout.h"
#include "mapstats.h"
#include "report.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace carto {

inline constexpr const char* MapsFile = "maps.txt";
inline constexpr const char* SettingsFile = "cartographer.txt";
inline constexpr const char* SlotsFile = "slots.txt";
inline constexpr const char* ShapesFile = "shapes.txt";
inline constexpr const char* MapStatsFile = "mapstats.txt";
inline constexpr const char* MapEventsFile = "mapevents.txt";

inline constexpr int DefaultMonsterLevel = 85;
inline constexpr int DefaultProperties = 6;
inline constexpr int DefaultEventChance = 20;
inline constexpr int MaxMonsters = 10;
inline constexpr int Normal = 0, Nightmare = 1, Hell = 2;

// The item spell (misc.txt pSpell) a map's item must have: the plugin hooks the use of items with this spell.
inline constexpr int MapItemSpell = 16;
// The slot (in slots.txt) of the maze level every map opens through (levels.txt DrlgType 1, plan section 3.6).
inline constexpr const char* MazeSlot = "Maze";
// The levels.txt Name of the Maze slot's level, as tools\install.ps1 writes it; a Maze slot on any other level is refused.
inline constexpr const char* MazeLevelName = "Cartographer Maze";
// The slot of the outdoor level a Jungle (later Snow) map opens through (levels.txt DrlgType 3, outdoor builder plan section 1.1). Recognised
// and checked here; no map opens through it yet, because the outdoor builder is not wired in (those tilesets are still refused).
inline constexpr const char* OutdoorSlot = "Outdoor";
inline constexpr int OutdoorDrlgType = 3;

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
	int minCharLevel = 1;
	int properties = -1;   // -1: use the cartographer.txt value
	int eventChance = -1;  // -1: use the cartographer.txt value
	int density = 0, champions = 0, uniques = 0;
};

struct Settings {
	int properties = DefaultProperties;
	int eventChance = DefaultEventChance;
	int itemStats = 0;
	int playerStats = 0;  // PlayerStats: 1 puts the map's player stats on the player (a test slot only until the save check passes)
	int mapNames = 0;   // MapNames: 1 shows a map's own Name on the portal tooltip, the automap title and the entry banner (written, not verified in game)
	int mazeProbe = 0;  // 1: the maze steering hooks log what the game does (level rect, rooms, rotation, arrival room)
	int outdoorBuild = 0;  // 1: the Outdoor slot level is built from the plan (hooks live); 0 (the default) leaves it to the game. Off until the game checks of issue #89 and #142 pass
	int outdoorProbe = 0;  // 1: the outdoor hooks log what the game does (level rect, grid cells, rooms)
	int autoCheck = 0;  // AutoCheck: 1 runs the issue #173 game check by itself in a Hell game (test slots only)
	std::string autoCheckMap;  // AutoCheckMap: the map's Name in maps.txt that AutoCheck opens
	std::string town = "Harrogath";
};

struct Slot {
	int line = 0;       // row in slots.txt
	std::string name;
	int level = 0;      // levels.txt Id
	int levelType = -1; // levels.txt LevelType of that level; -1 when levels.txt could not be read or has no such row
	int drlgType = -1;  // levels.txt DrlgType of that level (1 maze, 2 preset, 3 outdoor); -1 when unknown
	int act = -1;       // levels.txt Act of that level (0 based: 4 is Act 5); -1 when unknown
	// levels.txt OffsetX, OffsetY, SizeX and SizeY of that level, in tiles. haveGeometry is true only when all four columns exist and parse; the
	// outdoor build needs them because the level record's rectangle is still zero when the level build starts.
	bool haveGeometry = false;
	int offsetX = 0, offsetY = 0, sizeX = 0, sizeY = 0;
};

// The last level of the original game (3.3, Colossal Summit); a slot must be a level above it.
inline constexpr int LastOriginalLevel = 137;

// Tilesets, shapes and sizes as the author writes them.
auto TilesetNames() -> std::vector<std::string>;
auto ReadyTilesetNames() -> std::vector<std::string>;

// maps.txt. Rows with a problem are left out and described in `report`. Every map needs its own Item code.
// `shapes` (from LoadShapes) lets the Shape cell name a shapes.txt row as well as a built-in shape; null means built-ins only.
auto LoadMaps(std::string_view text, const GameNames& names, Report& report, const cartographer::layout::ShapeTable* shapes = nullptr) -> std::vector<MapDef>;

// shapes.txt (optional). A row that cannot be built is left out and described in `report`; the other rows load.
auto LoadShapes(std::string_view text, Report& report) -> cartographer::layout::ShapeTable;

// mapstats.txt (optional): the pool of random properties. A bad row is left out; an empty result means the built-in pool.
// Stat names are not checked against the game's tables yet (they are used when properties are applied).
auto LoadMapStats(std::string_view text, Report& report) -> std::vector<cartographer::stats::PoolRow>;

// mapevents.txt (optional): the events a map can roll. A bad row is left out. Spawn names are not resolved yet.
auto LoadMapEvents(std::string_view text, Report& report) -> std::vector<cartographer::events::Row>;

// cartographer.txt (Setting, Value). Every setting has a default; a bad value keeps it.
auto LoadSettings(std::string_view text, Report& report) -> Settings;

// slots.txt (Slot, Level), written by the installer.
auto LoadSlots(std::string_view text, Report& report) -> std::vector<Slot>;

// Drops every slot that could aim a hook at a level that is not the installer's own: a level of the original game
// (id 137 or below, 3.3), a town, a level with no levels.txt row (or no readable levels.txt), a repeated level or name.
// An old "Preset" row is dropped with a note. Each drop is described in `report`; a dropped slot stays closed.
void ValidateSlots(std::vector<Slot>& slots, std::string_view levelsText, Report& report);

// Fills in each slot's levelType, drlgType and act from the mod's levels.txt (columns Id, LevelType, DrlgType and Act; each is read when its column
// exists). A slot with no row keeps -1.
void ReadSlotLevelTypes(std::vector<Slot>& slots, std::string_view levelsText);

// Empty, or why the Outdoor slot level must be left to the game: the id is a vanilla level (137 or below, towns included), or levels.txt does not
// say Act 5 (Act 4, zero based) for it. Unlike OutdoorSlotProblem this refuses on what it does not know: the outdoor build writes into the level
// and calls the game, so it runs only for a level it has seen to be in Act 5.
auto OutdoorSlotRefusal(const Slot& slot) -> std::string;

// The levels.txt LevelType an outdoor tileset's slot level must have (Jungle 21, Snow 31), or -1 for a tileset that is not outdoor.
auto OutdoorLevelType(std::string_view tileset) -> int;

// Empty, or why the Outdoor slot cannot build `map`: the level has another LevelType than the tileset needs, or is not an outdoor (DrlgType 3)
// level. Empty too for a map that is not outdoor, when there is no Outdoor slot (the "not installed" message says so) and when levels.txt
// was not read (-1): this check never refuses on what it does not know.
auto OutdoorSlotProblem(const MapDef& map, const std::vector<Slot>& slots) -> std::string;

// The slot level id for `slot`, or -1.
auto SlotLevel(const std::vector<Slot>& slots, std::string_view slot) -> int;

// The map whose Item is `itemCode` (ignoring case), or nullptr: an item that opens no map is not a map item.
auto FindMapByItem(const std::vector<MapDef>& maps, std::string_view itemCode) -> const MapDef*;

// The map called `name` (ignoring case), or nullptr.
auto FindMap(const std::vector<MapDef>& maps, std::string_view name) -> const MapDef*;

// --- Gates (plan section 2): Hell, Harrogath, one map per game, plus the map's character level ----

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
