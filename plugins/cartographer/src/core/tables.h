// Cartographer's tables: maps.txt, cartographer.txt and slots.txt, parsed by this plugin (not the SDK's custom tables,
// which reject a whole table on one bad cell). A bad row is skipped with a plain-English message and the other rows
// still load. Rules: docs/cartographer/tables.md and plan section 3.0 (blank means default; our own words ignore case;
// names from D2's tables must match exactly, with a spelling suggestion).
//
// Pure C++ (no Windows, no SDK): tests/tables_test.cpp runs it on any machine.
#pragma once

#include "events.h"
#include "layout.h"
#include "maplight.h"
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
inline constexpr const char* MapStatsFile = "mapstats.txt";  // retired: only noted when it exists
inline constexpr const char* MapEventsFile = "mapevents.txt";
inline constexpr const char* BossesFile = "bosses.txt";
// The superuniques.txt key (Superunique column) of a boss entry. The prefix is on every key, so it can never equal a vanilla row; tools/install.ps1 writes the same.
inline constexpr const char* BossKeyPrefix = "cartoboss:";

inline constexpr int DefaultMonsterLevel = 85;
inline constexpr int DefaultEventChance = 20;
inline constexpr int MaxMonsters = 10;
inline constexpr int Normal = 0, Nightmare = 1, Hell = 2;

// The item spell (misc.txt pSpell) a map's item must have: the plugin hooks the use of items with this spell.
inline constexpr int MapItemSpell = 16;
// The slot (in slots.txt) of the Cave maze level (levels.txt DrlgType 1, plan section 3.6): the slot every maze map opened through before each maze
// tileset had a slot of its own, so it keeps its name. Each maze tileset (Cave, Crypt) opens through the slot level of its table row in
// maze/maze_tilesets.h, because a level has one LevelType and the tilesets need different ones.
inline constexpr const char* MazeSlot = "Maze";
// Each outdoor tileset (Jungle, Snow) opens through a slot level of its own (levels.txt DrlgType 3, outdoor builder plan section 1.1), because a
// level has one LevelType and the tilesets need different ones. The tileset's slot name, level name and LevelType are one row of the table in
// outdoor/outdoor_tilesets.h. A map's item opens its slot level; the outdoor build hook builds it.
inline constexpr int OutdoorDrlgType = 3;

// Names the loader checks against D2's own tables. An empty list with `have*` false means the table could not be read:
// those cells are then not checked (fail open) and the report says so.
struct GameNames {
	bool haveMonsters = false, haveSuperUniques = false, haveItems = false;
	std::vector<std::string> monsters;      // monstats.txt Id
	std::vector<std::string> superUniques;  // superuniques.txt Superunique
	std::vector<std::string> itemCodes;     // misc.txt, weapons.txt and armor.txt code
	bool haveBossFlags = false;
	std::vector<std::string> bossMonsters;  // monstats.txt Id whose boss column is 1 (act bosses and the like)
	bool haveMods = false;
	std::vector<int> monsterMods;           // monumod.txt id
	bool haveItemSpells = false;
	std::vector<std::pair<std::string, int>> itemSpells;  // misc.txt code and pSpell (0 when blank)
	std::vector<std::string> compactItems;                // misc.txt codes with compactsave 1: such an item carries no quality and no stats
	std::vector<std::string> cursorItems;                 // misc.txt codes with spellicon blank or 0 to 9: the client starts an item cursor on use
};

// misc.txt text: the code and pSpell of every item that has a code (a blank pSpell is 0) go to `spells`, the codes with compactsave 1 to `compact`.
// False when the code or pSpell column is missing (a missing compactsave column is fine: nothing is compact). With `cursor`, the codes with a blank spellicon or one of 0 to 9 go there.
auto ParseItemSpells(std::string_view text, std::vector<std::pair<std::string, int>>* spells, std::vector<std::string>* compact, std::vector<std::string>* cursor = nullptr) -> bool;

// The note for a leftover mapstats.txt (retired: a map's properties are stats on its item). Console and log only, never an error.
void NoteRetiredMapStats(Report& report);

// Text of an item table with mods (magicprefix, magicsuffix, uniqueitems, setitems or cubemain; `file` names it in the report): a note for each
// mon-speed mod whose lowest value (min or max) is -100 or lower, because what -100 or lower does to monsters has not been checked in game (the plugin does not limit the value).
void CheckMonSpeedRows(std::string_view text, const std::string& file, Report& report);

// One row of bosses.txt: a boss that belongs to Cartographer, never a vanilla one.
struct BossDef {
	int line = 0;  // row in bosses.txt
	std::string name;           // shown in game; maps.txt Boss names it
	std::string base;           // monstats.txt Id the boss is made from
	std::vector<int> mods;      // up to 3 monster mods (monumod.txt ids)
	int minions = 0;            // 0 to 10; the game's own group size for the superunique row
};

// The most entries bosses.txt may have. The game keeps a once-per-game "already spawned" bit per superuniques row in a bitmap of
// unknown length (docs/reverse-engineering/cartographer/01-addresses.md), so the number of added rows is kept small.
inline constexpr size_t MaxBosses = 16;

// The superuniques.txt key of the entry called `name`: BossKeyPrefix plus EscapeKeyBytes(name), ASCII only (install.ps1 Get-BossEscaped).
auto BossKey(std::string_view name) -> std::string;

struct MapDef {
	int line = 0;  // row in maps.txt
	std::string name, tileset, shape, size = "Large", item, boss;
	std::vector<std::string> monsters;
	int level = DefaultMonsterLevel;
	int minCharLevel = 1;
	int eventChance = -1;  // -1: use the cartographer.txt value
	int density = 0, champions = 0, uniques = 0;
	cartographer::maplight::Look look;  // Brightness, Warmth: the map's own look (blank: the tileset's)
};

// A maps.txt row that was skipped at load, kept so using its item can say so instead of doing nothing.
struct SkippedMap {
	int line = 0;       // row in maps.txt
	std::string name;   // the Name cell, may be empty
	std::string item;   // the Item cell (up to 4 letters)
	std::string reason; // the first thing wrong with the row, as reported at load
};

struct Settings {
	int eventChance = DefaultEventChance;
	int playerStats = 1;  // PlayerStats: 1 puts the map's player stats on the player while in the map (removed on leaving and on Save and Exit)
	int mapNames = 1;   // MapNames: 0 turns off a map's own Name on the portal tooltip, the automap title and the entry banner (on by default; seen in game in single player, multiplayer unverified)
	int mazeProbe = 0;  // 1: the maze steering hooks log what the game does (level rect, rooms, rotation, arrival room)
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

// bosses.txt (optional): a bad row is left out and described in `report`. A Name that is a vanilla superunique or monstats Id is refused.
auto LoadBosses(std::string_view text, const GameNames& names, Report& report) -> std::vector<BossDef>;

// The entry a maps.txt Boss value names (ignoring case), or nullptr with `problem` saying why: vanilla boss, or not an entry.
auto ResolveBoss(const std::vector<BossDef>& bosses, std::string_view value, const GameNames& names, std::string* problem) -> const BossDef*;

// Which way the map Boss is spawned. `rowFound`: the game has the entry's superuniques row (the installer ran). `baseFound`: its base monster resolves.
// A superunique row is used when there is one; the game returning no unit falls back to a plain monster of the base id (the engine does that at spawn).
enum class BossPath { Superunique, PlainMonster, None };
auto PickBossPath(bool rowFound, bool baseFound) -> BossPath;

// maps.txt. Rows with a problem are left out and described in `report`. Every map needs its own Item code.
// `bosses` (from LoadBosses) is what a Boss cell may name; null means none. A Boss that is not an entry is refused with a message but the map loads without it.
// `shapes` (from LoadShapes) lets the Shape cell name a shapes.txt row as well as a built-in shape; null means built-ins only.
auto LoadMaps(std::string_view text, const GameNames& names, Report& report, const cartographer::layout::ShapeTable* shapes = nullptr, const std::vector<BossDef>* bosses = nullptr,
              std::vector<SkippedMap>* skipped = nullptr) -> std::vector<MapDef>;

// shapes.txt (optional). A row that cannot be built is left out and described in `report`; the other rows load.
auto LoadShapes(std::string_view text, Report& report) -> cartographer::layout::ShapeTable;

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

// Empty, or why an outdoor slot level (a slot row of the outdoor tileset table) must be left to the game: the id is a vanilla level (137 or below, towns included), or levels.txt does not
// say Act 5 (Act 4, zero based) for it. Unlike OutdoorSlotProblem this refuses on what it does not know: the outdoor build writes into the level
// and calls the game, so it runs only for a level it has seen to be in Act 5.
auto OutdoorSlotRefusal(const Slot& slot) -> std::string;

// True for a maze slot (a slot name in the maze tileset table: Maze, MazeCrypt), ignoring case.
auto IsMazeSlotName(std::string_view slot) -> bool;

// True when `slots` has a row for any maze tileset's slot, with a level id above zero.
auto HasMazeSlot(const std::vector<Slot>& slots) -> bool;

// The levels.txt Name a maze slot's level must have (the table's level name, e.g. "Cartographer Crypt"), or "" for another slot name.
auto MazeSlotLevelName(std::string_view slot) -> std::string;

// True for an outdoor slot (a slot name in the outdoor tileset table), ignoring case.
auto IsOutdoorSlotName(std::string_view slot) -> bool;

// True when `slots` has a row for any outdoor tileset's slot, with a level id above zero.
auto HasOutdoorSlot(const std::vector<Slot>& slots) -> bool;

// The levels.txt Name an outdoor slot's level must have (the table's level name, e.g. "Cartographer Snow"), or "" for another slot name.
auto OutdoorSlotLevelName(std::string_view slot) -> std::string;

// The levels.txt LevelType an outdoor tileset's slot level must have (the tileset's table row: Jungle 21, Snow 31), or -1 for a tileset that is not outdoor.
auto OutdoorLevelType(std::string_view tileset) -> int;

// Empty, or why the outdoor slot of `map` (MapSlotName) cannot build it: the level has another LevelType than the tileset needs, or is not an outdoor (DrlgType 3)
// level. Empty too for a map that is not outdoor, when there is no Outdoor slot (the "not installed" message says so) and when levels.txt
// was not read (-1): this check never refuses on what it does not know.
auto OutdoorSlotProblem(const MapDef& map, const std::vector<Slot>& slots) -> std::string;

// True for a map built on an outdoor slot (Jungle, Snow): its tileset has an outdoor LevelType. Every other map is a maze map.
auto MapIsOutdoor(const MapDef& map) -> bool;

// The slot a map opens through: its tileset's slot in the maze table ("Maze" for Cave, "MazeCrypt" for Crypt) or the outdoor table ("Outdoor" for
// Jungle, "OutdoorSnow" for Snow), otherwise "Maze".
auto MapSlotName(const MapDef& map) -> const char*;

// Empty, or why an outdoor `map` cannot be opened while a map of tileset `pendingTileset` (empty: none) is open and not yet built. The outdoor
// slots (Jungle on Outdoor, Snow on OutdoorSnow) share one open-map job, so a second open on the other slot would leave the first portal's level unplanned.
auto OutdoorOpenConflict(const MapDef& map, std::string_view pendingTileset) -> std::string;

// Empty, or why a maze `map` cannot be opened while a map of tileset `pendingTileset` (empty: none) is open and not yet built. Every maze slot shares
// one open-map job, so a second open of another tileset would leave the first portal's level unplanned (a stock maze).
auto MazeOpenConflict(const MapDef& map, std::string_view pendingTileset) -> std::string;

// The slot level id for `slot`, or -1.
auto SlotLevel(const std::vector<Slot>& slots, std::string_view slot) -> int;

// The map whose Item is `itemCode` (ignoring case), or nullptr: an item that opens no map is not a map item.
auto FindMapByItem(const std::vector<MapDef>& maps, std::string_view itemCode) -> const MapDef*;

// The skipped row whose Item is `itemCode` (ignoring case), or nullptr. Only for a code no loaded map uses (FindMapByItem is tried first).
auto FindSkippedByItem(const std::vector<SkippedMap>& skipped, std::string_view itemCode) -> const SkippedMap*;

// What the player sees when using the item of a skipped row (row number and where to read why; the reason goes to the log, see SkippedMapLog).
auto SkippedMapText(const SkippedMap& skipped) -> std::string;
auto SkippedMapLog(const SkippedMap& skipped) -> std::string;

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
