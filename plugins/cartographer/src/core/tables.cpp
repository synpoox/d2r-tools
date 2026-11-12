#include "tables.h"

#include "text.h"
#include "tsv.h"

#include <algorithm>
#include <cstdlib>

namespace carto {

namespace {

struct Tileset {
	const char* name;
	bool ready;
};

// Plan section 3.5. Only Preset is buildable in phase 1; the others are accepted names that the loader refuses with a
// message that says so, until their builders ship.
constexpr Tileset Tilesets[] = {
	{"Preset", true},  {"Cave", false},   {"Crypt", false},    {"Lava", false},     {"Barracks", false}, {"Jail", false},
	{"Lair", false},   {"Durance", false}, {"Dungeon", false}, {"Sewer", false},    {"Ice", false},      {"Harem", false},
	{"Basement", false}, {"Spider", false}, {"Temple", false}, {"Jungle", false},    {"Snow", false},
};

const std::vector<std::string> Shapes = {"Corridor", "Winding", "Branching", "Loop", "Sprawl"};
const std::vector<std::string> Sizes = {"Small", "Medium", "Large"};
const std::vector<std::string> Difficulties = {"Normal", "Nightmare", "Hell"};
const std::vector<std::string> MapColumns = {"Name",   "Tileset",      "Shape",       "Size",      "Level",   "Item",
                                             "Boss",   "Monsters",     "Difficulty",  "MinCharLevel", "Properties", "EventChance",
                                             "Density", "Champions",   "Uniques"};

auto Canonical(const std::vector<std::string>& names, std::string_view word) -> const std::string* {
	for (const std::string& n : names) {
		if (EqualsNoCase(n, word)) {
			return &n;
		}
	}
	return nullptr;
}

auto Contains(const std::vector<std::string>& names, std::string_view word) -> bool {
	return std::find(names.begin(), names.end(), word) != names.end();
}

// A whole number in [lo, hi].
auto ParseInt(const std::string& cell, int lo, int hi, int* out) -> bool {
	if (cell.empty()) {
		return false;
	}
	char* end = nullptr;
	const long v = std::strtol(cell.c_str(), &end, 10);
	if (end == cell.c_str() || *end != '\0' || v < lo || v > hi) {
		return false;
	}
	*out = static_cast<int>(v);
	return true;
}

auto Range(int lo, int hi) -> std::string {
	return "between " + std::to_string(lo) + " and " + std::to_string(hi);
}

// "monster 'x' is not in monstats.txt. Did you mean 'y'?" -- D2's names must match exactly, case included.
auto NotInTable(const char* what, const std::string& word, const char* table, const std::vector<std::string>& names) -> std::string {
	return std::string(what) + " '" + word + "' is not in " + table + "." + DidYouMean(word, names) +
	       " Names from the game's tables must match exactly, including capital letters.";
}

}  // namespace

auto TilesetNames() -> std::vector<std::string> {
	std::vector<std::string> out;
	for (const Tileset& t : Tilesets) {
		out.emplace_back(t.name);
	}
	return out;
}

auto ReadyTilesetNames() -> std::vector<std::string> {
	std::vector<std::string> out;
	for (const Tileset& t : Tilesets) {
		if (t.ready) {
			out.emplace_back(t.name);
		}
	}
	return out;
}

auto LoadMaps(std::string_view text, const GameNames& names, Report& report) -> std::vector<MapDef> {
	std::vector<MapDef> maps;
	const Tsv table = ParseTsv(text);
	if (table.header.empty()) {
		report.Error(MapsFile, 0, "the file is empty. It needs a header row with Name, Tileset, Shape and Item, then one row per map.");
		return maps;
	}
	bool missing = false;
	for (const char* required : {"Name", "Tileset", "Shape", "Item"}) {
		if (table.Column(required) < 0) {
			report.Error(MapsFile, 1, std::string("there is no '") + required + "' column in the header row. Every map needs Name, Tileset, Shape and Item.");
			missing = true;
		}
	}
	if (missing) {
		return maps;
	}
	for (const std::string& column : table.header) {
		if (Canonical(MapColumns, column) == nullptr) {
			report.Note(MapsFile, 1, "the column '" + column + "' is not used and is ignored." + DidYouMean(column, MapColumns));
		}
	}
	const int cName = table.Column("Name"), cTileset = table.Column("Tileset"), cShape = table.Column("Shape");
	const int cSize = table.Column("Size"), cLevel = table.Column("Level"), cItem = table.Column("Item");
	const int cBoss = table.Column("Boss"), cMonsters = table.Column("Monsters"), cDifficulty = table.Column("Difficulty");
	const int cMinChar = table.Column("MinCharLevel"), cProps = table.Column("Properties"), cEvent = table.Column("EventChance");
	const int cDensity = table.Column("Density"), cChampions = table.Column("Champions"), cUniques = table.Column("Uniques");

	for (const TsvRow& row : table.rows) {
		MapDef map;
		map.line = row.line;
		bool ok = true;
		const auto fail = [&](const std::string& message) {
			report.Error(MapsFile, row.line, message + " This map is skipped.");
			ok = false;
		};
		// An optional whole-number column: blank keeps the default.
		const auto number = [&](int column, const char* label, int lo, int hi, int* out) {
			const std::string cell = table.Cell(row, column);
			if (!cell.empty() && !ParseInt(cell, lo, hi, out)) {
				fail(std::string(label) + " '" + cell + "' is not a whole number " + Range(lo, hi) + ".");
			}
		};

		map.name = table.Cell(row, cName);
		if (map.name.empty()) {
			fail("the Name cell is empty.");
		} else {
			for (const MapDef& other : maps) {
				if (EqualsNoCase(other.name, map.name)) {
					fail("a map called '" + map.name + "' is already on row " + std::to_string(other.line) + ".");
					break;
				}
			}
		}

		const std::string tileset = table.Cell(row, cTileset);
		if (tileset.empty()) {
			fail("the Tileset cell is empty. Use " + OneOf(ReadyTilesetNames()) + ".");
		} else {
			const std::vector<std::string> all = TilesetNames();
			const std::string* known = Canonical(all, tileset);
			if (known == nullptr) {
				fail("tileset '" + tileset + "' is not known." + DidYouMean(tileset, all) + " Known tilesets: " + OneOf(all) + ".");
			} else if (!Contains(ReadyTilesetNames(), *known)) {
				fail("tileset '" + *known + "' is planned but Cartographer cannot build it yet. Use " + OneOf(ReadyTilesetNames()) + " for now.");
			} else {
				map.tileset = *known;
			}
		}

		const std::string shape = table.Cell(row, cShape);
		if (const std::string* known = Canonical(Shapes, shape)) {
			map.shape = *known;
		} else {
			fail("shape '" + shape + "' is not known." + DidYouMean(shape, Shapes) + " Use " + OneOf(Shapes) + ".");
		}

		const std::string size = table.Cell(row, cSize);
		if (!size.empty()) {
			if (const std::string* known = Canonical(Sizes, size)) {
				map.size = *known;
			} else {
				fail("size '" + size + "' is not known." + DidYouMean(size, Sizes) + " Use " + OneOf(Sizes) + ", or leave it blank for Medium.");
			}
		}

		const std::string difficulty = table.Cell(row, cDifficulty);
		if (!difficulty.empty()) {
			if (const std::string* known = Canonical(Difficulties, difficulty)) {
				map.difficulty = static_cast<int>(known - Difficulties.data());
			} else {
				fail("difficulty '" + difficulty + "' is not known." + DidYouMean(difficulty, Difficulties) + " Use " + OneOf(Difficulties) +
				     ", or leave it blank for Hell.");
			}
		}

		number(cLevel, "Level", 1, 255, &map.level);
		number(cMinChar, "MinCharLevel", 1, 99, &map.minCharLevel);
		number(cProps, "Properties", 0, 20, &map.properties);
		number(cEvent, "EventChance", 0, 100, &map.eventChance);
		number(cDensity, "Density", 0, 1000, &map.density);
		number(cChampions, "Champions", 0, 1000, &map.champions);
		number(cUniques, "Uniques", 0, 1000, &map.uniques);

		map.item = table.Cell(row, cItem);
		if (map.item.empty()) {
			fail("the Item cell is empty. A map is opened by its own item: put an item code here, like 't20'.");
		} else if (map.item.size() > 4) {
			fail("Item '" + map.item + "' is too long: an item code has up to 4 letters, like 'shm1'.");
		} else if (names.haveItems && !Contains(names.itemCodes, map.item)) {
			fail(NotInTable("item code", map.item, "the item tables (misc.txt, weapons.txt, armor.txt)", names.itemCodes));
		} else {
			if (names.haveItemSpells) {
				int spell = -1;
				for (const auto& entry : names.itemSpells) {
					if (entry.first == map.item) {
						spell = entry.second;
					}
				}
				if (spell != MapItemSpell) {
					fail("Item '" + map.item + "' cannot open a map: it needs item spell " + std::to_string(MapItemSpell) +
					     " (pSpell in misc.txt), and " + (spell < 0 ? std::string("it is not a misc.txt item.") : "it has " + std::to_string(spell) + "."));
				}
			}
			for (const MapDef& other : maps) {
				if (EqualsNoCase(other.item, map.item)) {
					fail("item '" + map.item + "' already opens the map '" + other.name + "' (row " + std::to_string(other.line) +
					     "). Each map needs an item of its own.");
					break;
				}
			}
		}

		map.boss = table.Cell(row, cBoss);
		if (!map.boss.empty() && names.haveMonsters && names.haveSuperUniques && !Contains(names.monsters, map.boss) &&
		    !Contains(names.superUniques, map.boss)) {
			std::vector<std::string> both = names.monsters;
			both.insert(both.end(), names.superUniques.begin(), names.superUniques.end());
			fail(NotInTable("Boss", map.boss, "monstats.txt (Id) or superuniques.txt (Superunique)", both));
		}

		const std::string monsters = table.Cell(row, cMonsters);
		if (!monsters.empty()) {
			for (const std::string& id : Split(monsters, ',')) {
				if (id.empty()) {
					fail("the Monsters list has an empty entry (two commas in a row?).");
				} else if (names.haveMonsters && !Contains(names.monsters, id)) {
					fail(NotInTable("monster", id, "monstats.txt", names.monsters));
				} else {
					map.monsters.push_back(id);
				}
			}
			if (static_cast<int>(map.monsters.size()) > MaxMonsters) {
				fail("the Monsters list has " + std::to_string(map.monsters.size()) + " entries; the limit is " + std::to_string(MaxMonsters) + ".");
			}
		}

		if (ok) {
			maps.push_back(std::move(map));
		}
	}
	return maps;
}

auto LoadSettings(std::string_view text, Report& report) -> Settings {
	Settings settings;
	if (text.empty()) {
		return settings;
	}
	const Tsv table = ParseTsv(text);
	const int cSetting = table.Column("Setting"), cValue = table.Column("Value");
	if (cSetting < 0 || cValue < 0) {
		report.Error(SettingsFile, 1, "the header row needs the columns 'Setting' and 'Value'. The defaults are used.");
		return settings;
	}
	const std::vector<std::string> known = {"Properties", "EventChance", "ItemStats", "Town"};
	for (const TsvRow& row : table.rows) {
		const std::string key = table.Cell(row, cSetting), value = table.Cell(row, cValue);
		const std::string* name = Canonical(known, key);
		const auto fail = [&](const std::string& message) { report.Error(SettingsFile, row.line, message + " The default is kept."); };
		if (name == nullptr) {
			fail("setting '" + key + "' is not known." + DidYouMean(key, known) + " Use " + OneOf(known) + ".");
			continue;
		}
		if (value.empty()) {
			continue;  // blank means default
		}
		if (*name == "Town") {
			// Slot levels are in Act 5 and a portal only reaches its own act.
			if (EqualsNoCase(value, "Harrogath")) {
				settings.town = "Harrogath";
			} else {
				fail("town '" + value + "' is not supported: maps are in Act 5, and a portal only reaches its own act. Use 'Harrogath'.");
			}
			continue;
		}
		int lo = 0, hi = 20, *target = &settings.properties;
		if (*name == "EventChance") {
			hi = 100;
			target = &settings.eventChance;
		} else if (*name == "ItemStats") {
			hi = 1;
			target = &settings.itemStats;
		}
		if (!ParseInt(value, lo, hi, target)) {
			fail(*name + " '" + value + "' is not a whole number " + Range(lo, hi) + ".");
		}
	}
	return settings;
}

auto LoadSlots(std::string_view text, Report& report) -> std::vector<Slot> {
	std::vector<Slot> slots;
	const Tsv table = ParseTsv(text);
	const int cSlot = table.Column("Slot"), cLevel = table.Column("Level");
	if (cSlot < 0 || cLevel < 0) {
		report.Error(SlotsFile, 1, "the header row needs the columns 'Slot' and 'Level'. Run the installer again (tools\\install.ps1).");
		return slots;
	}
	for (const TsvRow& row : table.rows) {
		Slot slot;
		slot.name = table.Cell(row, cSlot);
		if (slot.name.empty() || !ParseInt(table.Cell(row, cLevel), 1, 65535, &slot.level)) {
			report.Error(SlotsFile, row.line, "the slot needs a name and a level number. Run the installer again (tools\\install.ps1). This slot is skipped.");
			continue;
		}
		slots.push_back(std::move(slot));
	}
	return slots;
}

auto SlotLevel(const std::vector<Slot>& slots, std::string_view slot) -> int {
	for (const Slot& s : slots) {
		if (EqualsNoCase(s.name, slot)) {
			return s.level;
		}
	}
	return -1;
}

auto FindMap(const std::vector<MapDef>& maps, std::string_view name) -> const MapDef* {
	for (const MapDef& m : maps) {
		if (EqualsNoCase(m.name, name)) {
			return &m;
		}
	}
	return nullptr;
}

auto FindMapByItem(const std::vector<MapDef>& maps, std::string_view itemCode) -> const MapDef* {
	for (const MapDef& m : maps) {
		if (!m.item.empty() && EqualsNoCase(m.item, itemCode)) {
			return &m;
		}
	}
	return nullptr;
}

auto CheckGates(const MapDef& map, const GateState& state) -> Gate {
	if (state.areaId != state.townId) {
		return Gate::NotInTown;
	}
	if (state.difficulty != map.difficulty) {
		return Gate::Difficulty;
	}
	if (state.charLevel < map.minCharLevel) {
		return Gate::CharLevel;
	}
	if (state.openings > 0) {
		return Gate::AlreadyOpen;
	}
	return state.spotFree ? Gate::Open : Gate::NoSpot;
}

auto RefusalText(Gate gate, const MapDef& map, const GateState& state) -> std::string {
	const std::string who = "Cartographer: " + map.name + " ";
	switch (gate) {
	case Gate::NotInTown: return who + "can only be opened in town (Harrogath).";
	case Gate::Difficulty: return who + "can only be opened in " + Difficulties[static_cast<size_t>(map.difficulty)] + ".";
	case Gate::CharLevel: return who + "needs character level " + std::to_string(map.minCharLevel) + " (you are " + std::to_string(state.charLevel) + ").";
	case Gate::AlreadyOpen: return who + "cannot be opened: a map has already been opened in this game.";
	case Gate::NoSpot: return who + "needs a free spot for the portal: step aside and try again.";
	case Gate::Open: break;
	}
	return who + "is open.";
}

}  // namespace carto
