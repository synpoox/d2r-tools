#include "tables.h"

#include "outdoor_tilesets.h"
#include "text.h"
#include "tsv.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <optional>

namespace carto {

namespace {

struct Tileset {
	const char* name;
	bool ready;
};

// Plan section 3.5. Cave and Crypt are buildable (through the maze steering hooks, which need the Maze slot); the outdoor tilesets (the table in
// outdoor/outdoor_tilesets.h, listed after these) are buildable through the outdoor build hook, which needs the tileset's slot
// (each is checked when the map opens); the others are accepted names that the loader refuses with a message that says so, until their builders ship.
constexpr Tileset Tilesets[] = {
	{"Cave", true},    {"Crypt", true},    {"Lava", false},     {"Barracks", false}, {"Jail", false},
	{"Lair", false},   {"Durance", false}, {"Dungeon", false}, {"Sewer", false},    {"Ice", false},      {"Harem", false},
	{"Basement", false}, {"Spider", false}, {"Temple", false},
};

const std::vector<std::string> Shapes = {"Corridor", "Winding", "Branching", "Loop", "Sprawl"};
const std::vector<std::string> Sizes = {"ExtraSmall", "Small", "Medium", "Large"};
const std::vector<std::string> MapColumns = {"Name",   "Tileset",      "Shape",       "Size",      "Level",   "Item",
                                             "Boss",   "Monsters",     "MinCharLevel", "Properties", "EventChance",
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

// Item codes are matched ignoring case, like FindMapByItem.
auto ContainsNoCase(const std::vector<std::string>& names, std::string_view word) -> bool {
	return std::any_of(names.begin(), names.end(), [&](const std::string& n) { return EqualsNoCase(n, word); });
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

// A whole number of any size: one beyond int32 is cut to INT32_MIN/INT32_MAX so the range checks that follow can say
// "too large" or cut it with a warning, instead of calling it "not a whole number".
auto ParseWhole(const std::string& cell, int* out) -> bool {
	size_t i = (cell.size() > 1 && (cell[0] == '-' || cell[0] == '+')) ? 1 : 0;
	if (i >= cell.size()) {
		return false;
	}
	for (size_t k = i; k < cell.size(); ++k) {
		if (cell[k] < '0' || cell[k] > '9') {
			return false;
		}
	}
	const bool negative = cell[0] == '-';
	long long v = 0;
	for (; i < cell.size(); ++i) {
		v = v * 10 + (cell[i] - '0');
		if (v > 4000000000LL) {
			v = 4000000000LL;  // far past int32; more digits cannot change the cut result
		}
	}
	v = negative ? -v : v;
	*out = static_cast<int>(v < INT32_MIN ? INT32_MIN : (v > INT32_MAX ? INT32_MAX : v));
	return true;
}

// True when the typed whole number was cut to fit int32, so the value a message shows is not what was typed.
auto WasCut(int value, const std::string& typed) -> bool {
	const std::string shown = std::to_string(value);
	size_t digits = (!typed.empty() && (typed[0] == '-' || typed[0] == '+')) ? 1 : 0;
	while (digits + 1 < typed.size() && typed[digits] == '0') {
		++digits;
	}
	const std::string plain = std::string(!typed.empty() && typed[0] == '-' ? "-" : "") + typed.substr(digits);
	return !typed.empty() && plain != shown && !(shown == "0" && plain == "-0");
}

// A cut number is shown in the message as the cut value; this names what was typed. The message itself is never changed.
auto TypedSuffix(const std::string& typed) -> std::string {
	return " (you typed " + typed + ")";
}

// The header cell is the first of its name (ignoring case): a repeated name is reported once, as a repeat, not again as unused.
auto FirstOfName(const std::vector<std::string>& header, size_t i) -> bool {
	for (size_t j = 0; j < i; ++j) {
		if (EqualsNoCase(header[j], header[i])) {
			return false;
		}
	}
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
	for (const cartographer::outdoor::OutdoorTileset& t : cartographer::outdoor::OutdoorTilesets) {
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
	for (const cartographer::outdoor::OutdoorTileset& t : cartographer::outdoor::OutdoorTilesets) {
		out.emplace_back(t.name);
	}
	return out;
}

namespace {

// A header that names a column twice: only the first is read. An error, like shapes.txt, so nobody edits the wrong one.
void ReportRepeatedColumns(const char* file, const Tsv& table, Report& report) {
	for (size_t i = 0; i < table.header.size(); ++i) {
		if (table.header[i].empty()) {
			continue;
		}
		for (size_t j = 0; j < i; ++j) {
			if (EqualsNoCase(table.header[j], table.header[i])) {
				report.Error(file, 1, "the column '" + table.header[i] + "' appears more than once (columns " + std::to_string(j + 1) + " and " + std::to_string(i + 1) +
				                          "). Only the first is used.");
				break;
			}
		}
	}
}

}  // namespace

auto LoadMaps(std::string_view text, const GameNames& names, Report& report, const cartographer::layout::ShapeTable* shapes, const std::vector<BossDef>* bosses) -> std::vector<MapDef> {
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
	ReportRepeatedColumns(MapsFile, table, report);
	for (size_t i = 0; i < table.header.size(); ++i) {
		const std::string& column = table.header[i];
		if (EqualsNoCase(column, "Difficulty") && FirstOfName(table.header, i)) {
			report.Note(MapsFile, 1, "the column 'Difficulty' is ignored: maps can only be opened in Hell.");
		} else if (Canonical(MapColumns, column) == nullptr && FirstOfName(table.header, i)) {
			report.Note(MapsFile, 1, "the column '" + column + "' is not used and is ignored." + DidYouMean(column, MapColumns));
		}
	}
	const int cName = table.Column("Name"), cTileset = table.Column("Tileset"), cShape = table.Column("Shape");
	const int cSize = table.Column("Size"), cLevel = table.Column("Level"), cItem = table.Column("Item");
	const int cBoss = table.Column("Boss"), cMonsters = table.Column("Monsters");
	const int cMinChar = table.Column("MinCharLevel"), cProps = table.Column("Properties"), cEvent = table.Column("EventChance");
	const int cDensity = table.Column("Density"), cChampions = table.Column("Champions"), cUniques = table.Column("Uniques");

	std::vector<std::string> customShapes;
	if (shapes != nullptr) {
		for (const cartographer::layout::ShapeDef& def : shapes->shapes) {
			customShapes.push_back(def.name);
		}
	}

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
		} else if (EqualsNoCase(tileset, "Preset")) {
			fail("Preset maps are gone: use Cave or Crypt.");
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
		} else if (const std::string* custom = Canonical(customShapes, shape)) {
			map.shape = *custom;
		} else {
			std::vector<std::string> all = Shapes;
			all.insert(all.end(), customShapes.begin(), customShapes.end());
			fail("shape '" + shape + "' is not known." + DidYouMean(shape, all) + " Use " + OneOf(all) + (customShapes.empty() ? "" : " (the last ones are from shapes.txt)") + ".");
		}

		const std::string size = table.Cell(row, cSize);
		if (!size.empty()) {
			if (const std::string* known = Canonical(Sizes, size)) {
				map.size = *known;
			} else {
				fail("size '" + size + "' is not known." + DidYouMean(size, Sizes) + " Use " + OneOf(Sizes) + ", or leave it blank for Medium.");
			}
		}

		number(cLevel, "Level", 1, 255, &map.level);
		number(cMinChar, "MinCharLevel", 1, 99, &map.minCharLevel);
		number(cProps, "Properties", 0, 20, &map.properties);
		number(cEvent, "EventChance", 0, 100, &map.eventChance);
		number(cDensity, "Density", 0, INT32_MAX, &map.density);
		number(cChampions, "Champions", 0, INT32_MAX, &map.champions);
		number(cUniques, "Uniques", 0, INT32_MAX, &map.uniques);

		map.item = table.Cell(row, cItem);
		if (map.item.empty()) {
			fail("the Item cell is empty. A map is opened by its own item: put an item code here, like 't20'.");
		} else if (map.item.size() > 4) {
			fail("Item '" + map.item + "' is too long: an item code has up to 4 letters, like 'shm1'.");
		} else if (names.haveItems && !ContainsNoCase(names.itemCodes, map.item)) {
			fail(NotInTable("item code", map.item, "the item tables (misc.txt, weapons.txt, armor.txt)", names.itemCodes));
		} else {
			bool spellOk = true;
			if (names.haveItemSpells) {
				int spell = -1;
				for (const auto& entry : names.itemSpells) {
					if (EqualsNoCase(entry.first, map.item)) {
						spell = entry.second;
					}
				}
				if (spell != MapItemSpell) {
					spellOk = false;
					fail("Item '" + map.item + "' cannot open a map: it needs item spell " + std::to_string(MapItemSpell) +
					     " (pSpell in misc.txt), and " + (spell < 0 ? std::string("it is not a misc.txt item.") : "it has " + std::to_string(spell) + "."));
				}
			}
			for (const MapDef& other : maps) {
				if (spellOk && EqualsNoCase(other.item, map.item)) {  // a row already refused for its spell is not reported twice
					fail("item '" + map.item + "' already opens the map '" + other.name + "' (row " + std::to_string(other.line) +
					     "). Each map needs an item of its own.");
					break;
				}
			}
		}

		// A Boss names a bosses.txt entry. Anything else is refused with a message, and the map still opens without a Boss.
		if (const std::string boss = table.Cell(row, cBoss); !boss.empty()) {
			static const std::vector<BossDef> none;
			std::string problem;
			if (const BossDef* entry = ResolveBoss(bosses != nullptr ? *bosses : none, boss, names, &problem)) {
				map.boss = entry->name;
			} else {
				report.Error(MapsFile, row.line, problem + " The map opens without a Boss.");
			}
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

auto BossKey(std::string_view name) -> std::string {
	return std::string(BossKeyPrefix) + EscapeKeyBytes(name);
}

auto PickBossPath(bool rowFound, bool baseFound) -> BossPath {
	return rowFound ? BossPath::Superunique : (baseFound ? BossPath::PlainMonster : BossPath::None);
}

auto ResolveBoss(const std::vector<BossDef>& bosses, std::string_view value, const GameNames& names, std::string* problem) -> const BossDef* {
	for (const BossDef& def : bosses) {
		if (EqualsNoCase(def.name, value)) {
			return &def;
		}
	}
	const std::string word(value);
	if (word.size() >= std::string_view(BossKeyPrefix).size() && EqualsNoCase(word.substr(0, std::string_view(BossKeyPrefix).size()), BossKeyPrefix)) {
		*problem = "Boss '" + word + "' is a superuniques.txt key the installer writes, not a name. Put the Name of the bosses.txt entry here.";
		return nullptr;
	}
	const bool vanillaBoss = ContainsNoCase(names.superUniques, word);
	if (vanillaBoss || ContainsNoCase(names.monsters, word)) {
		*problem = "Boss '" + word + "' is a vanilla " + (vanillaBoss ? "boss (superuniques.txt)" : "monster (monstats.txt)") +
		           ". A map's Boss must be an entry of its own in bosses.txt; vanilla bosses are never used.";
	} else {
		std::vector<std::string> own;
		for (const BossDef& def : bosses) {
			own.push_back(def.name);
		}
		*problem = "Boss '" + word + "' is not an entry in bosses.txt" + (bosses.empty() ? " (there are none)." : "." + DidYouMean(word, own));
	}
	return nullptr;
}

namespace {

// Notes for header cells no column uses, with a spelling suggestion.
void NoteUnusedColumns(const char* file, const Tsv& table, const std::vector<std::string>& used, Report& report) {
	for (size_t i = 0; i < table.header.size(); ++i) {
		const std::string& column = table.header[i];
		if (!column.empty() && Canonical(used, column) == nullptr && FirstOfName(table.header, i)) {
			report.Note(file, 1, "the column '" + column + "' is not used and is ignored." + DidYouMean(column, used));
		}
	}
}

// An optional file with nothing in it is the same as no file: a note, and the defaults are used.
auto NoteIfEmpty(const char* file, const Tsv& table, Report& report) -> bool {
	if (!table.header.empty()) {
		return false;
	}
	report.Note(file, 0, "the file is empty, so it is not used.");
	return true;
}

// A required column that is missing makes the whole file unusable.
auto RequireColumns(const char* file, const Tsv& table, const std::vector<std::string>& required, Report& report) -> bool {
	if (table.header.empty()) {
		report.Error(file, 0, "the file is empty. It needs a header row with " + OneOf(required) + ", then one row per entry.");
		return false;
	}
	bool ok = true;
	for (const std::string& column : required) {
		if (table.Column(column) < 0) {
			report.Error(file, 1, "there is no '" + column + "' column in the header row. The file is not used.");
			ok = false;
		}
	}
	return ok;
}

// "shapes.txt line 3: text" or "shapes.txt: text" from a core's message, split into row and text for the report.
void ReportCoreMessage(Report& report, bool error, const char* file, const std::string& message) {
	std::string rest = message;
	int line = 0;
	const std::string prefix = file;
	if (rest.rfind(prefix, 0) == 0) {
		rest.erase(0, prefix.size());
		if (rest.rfind(" line ", 0) == 0) {
			line = std::atoi(rest.c_str() + 6);
			const size_t colon = rest.find(": ");
			rest.erase(0, colon == std::string::npos ? 0 : colon);
		}
		if (rest.rfind(": ", 0) == 0) {
			rest.erase(0, 2);
		}
	}
	if (error) {
		report.Error(file, line, rest);
	} else {
		report.Note(file, line, rest);
	}
}

}  // namespace

auto LoadShapes(std::string_view text, Report& report) -> cartographer::layout::ShapeTable {
	if (NoteIfEmpty(ShapesFile, ParseTsv(text), report)) {
		return {};
	}
	cartographer::layout::ParseResult parsed = cartographer::layout::parseShapesTxt(text, ShapesFile);
	for (const std::string& error : parsed.errors) {
		ReportCoreMessage(report, true, ShapesFile, error);
	}
	return std::move(parsed.table);
}

auto LoadMapStats(std::string_view text, Report& report) -> std::vector<cartographer::stats::PoolRow> {
	using cartographer::stats::PoolRow;
	std::vector<PoolRow> pool;
	std::vector<int> lines;
	std::vector<std::string> weightText;  // what was typed in Weight, for messages after the int32 cut
	const Tsv table = ParseTsv(text);
	if (NoteIfEmpty(MapStatsFile, table, report) || !RequireColumns(MapStatsFile, table, {"Target", "Stat", "Min", "Max"}, report)) {
		return pool;
	}
	ReportRepeatedColumns(MapStatsFile, table, report);
	NoteUnusedColumns(MapStatsFile, table, {"Target", "Stat", "Min", "Max", "Weight"}, report);
	const int cTarget = table.Column("Target"), cStat = table.Column("Stat"), cMin = table.Column("Min");
	const int cMax = table.Column("Max"), cWeight = table.Column("Weight");
	const std::vector<std::string> targets = {"Monsters", "Players"};
	for (const TsvRow& row : table.rows) {
		PoolRow entry;
		bool ok = true;
		const auto fail = [&](const std::string& message) {
			report.Error(MapStatsFile, row.line, message + " This row is skipped.");
			ok = false;
		};
		const std::string target = table.Cell(row, cTarget);
		if (const std::string* known = Canonical(targets, target)) {
			entry.target = *known == "Players" ? cartographer::stats::Target::Players : cartographer::stats::Target::Monsters;
		} else {
			fail("Target '" + target + "' is not known." + DidYouMean(target, targets) + " Use 'Monsters' or 'Players'.");
		}
		entry.stat = table.Cell(row, cStat);
		if (entry.stat.empty()) {
			fail("Stat is empty. Name a stat from the game's itemstatcost.txt.");
		}
		const struct {
			int column;
			const char* name;
			int* value;
		} numbers[] = {{cMin, "Min", &entry.min}, {cMax, "Max", &entry.max}, {cWeight, "Weight", &entry.weight}};
		for (const auto& n : numbers) {
			const std::string cell = table.Cell(row, n.column);
			if (cell.empty() && n.column != cWeight) {
				fail(std::string(n.name) + " is empty. Give a whole number.");  // Min and Max are required; Weight defaults to 1
			} else if (!cell.empty() && !ParseWhole(cell, n.value)) {
				fail(std::string(n.name) + " '" + cell + "' is not a whole number.");
			}
		}
		if (ok) {
			pool.push_back(entry);
			lines.push_back(row.line);
			weightText.push_back(table.Cell(row, cWeight));
		}
	}
	// The core's own checks. A negative Weight is an error there and the row is dropped; the rest are notes.
	std::vector<bool> drop(pool.size(), false);
	for (const cartographer::stats::PoolIssue& issue : cartographer::stats::checkPool(pool)) {
		const std::string lead = "row " + std::to_string(issue.row + 1) + " (" + pool[issue.row].stat + "): ";
		const bool leads = issue.message.rfind(lead, 0) == 0;
		std::string message = leads ? pool[issue.row].stat + ": " + issue.message.substr(lead.size()) : issue.message;
		// Only the core's own Weight wording (right after the lead), never a Stat name that contains "Weight".
		if (leads && issue.message.compare(lead.size(), 7, "Weight ") == 0 && WasCut(pool[issue.row].weight, weightText[issue.row])) {
			message += TypedSuffix(weightText[issue.row]);
		}
		if (issue.error) {
			report.Error(MapStatsFile, lines[issue.row], message);
			drop[issue.row] = true;
		} else {
			report.Note(MapStatsFile, lines[issue.row], message);
		}
	}
	std::vector<PoolRow> kept;
	for (size_t i = 0; i < pool.size(); ++i) {
		if (!drop[i]) {
			kept.push_back(pool[i]);
		}
	}
	return kept;
}

auto LoadBosses(std::string_view text, const GameNames& names, Report& report) -> std::vector<BossDef> {
	std::vector<BossDef> out;
	const Tsv table = ParseTsv(text);
	if (NoteIfEmpty(BossesFile, table, report) || !RequireColumns(BossesFile, table, {"Name", "Base"}, report)) {
		return out;
	}
	ReportRepeatedColumns(BossesFile, table, report);
	NoteUnusedColumns(BossesFile, table, {"Name", "Base", "Mods", "Minions"}, report);
	const int cName = table.Column("Name"), cBase = table.Column("Base"), cMods = table.Column("Mods"), cMinions = table.Column("Minions");
	for (const TsvRow& r : table.rows) {
		BossDef def;
		def.line = r.line;
		bool ok = true;
		const auto fail = [&](const std::string& message) {
			report.Error(BossesFile, r.line, message + " This row is skipped.");
			ok = false;
		};
		def.name = table.Cell(r, cName);
		def.base = table.Cell(r, cBase);
		if (def.name.empty()) {
			fail("the Name cell is empty.");
		} else if (ContainsNoCase(names.superUniques, def.name) || ContainsNoCase(names.monsters, def.name)) {
			fail("'" + def.name + "' is the name of a vanilla boss or monster. A boss entry needs a name of its own.");
		} else {
			for (const BossDef& other : out) {
				if (EqualsNoCase(other.name, def.name)) {
					fail("a boss called '" + def.name + "' is already on row " + std::to_string(other.line) + ".");
					break;
				}
			}
		}
		if (def.base.empty()) {
			fail("the Base cell is empty. Put a monstats.txt Id here.");
		} else if (names.haveMonsters && !Contains(names.monsters, def.base)) {
			fail(NotInTable("Base monster", def.base, "monstats.txt", names.monsters));
		} else if (names.haveBossFlags && Contains(names.bossMonsters, def.base)) {
			fail("Base '" + def.base + "' is a vanilla boss (its monstats.txt boss column is 1). A boss entry needs an ordinary monster as its Base.");
		}
		const std::string mods = table.Cell(r, cMods);
		if (!mods.empty()) {
			for (const std::string& part : Split(mods, ',')) {
				int v = 0;
				if (!ParseInt(part, 1, 255, &v)) {
					fail("Mods entry '" + part + "' is not a whole number " + Range(1, 255) + ".");
					break;
				}
				if (names.haveMods && std::find(names.monsterMods.begin(), names.monsterMods.end(), v) == names.monsterMods.end()) {
					fail("Mods entry '" + part + "' is not an id in monumod.txt.");
					break;
				}
				def.mods.push_back(v);
			}
			if (ok && def.mods.size() > 3) {
				fail("Mods has " + std::to_string(def.mods.size()) + " entries; the limit is 3.");
			}
		}
		const std::string minions = table.Cell(r, cMinions);
		if (!minions.empty() && !ParseInt(minions, 0, 10, &def.minions)) {
			fail("Minions '" + minions + "' is not a whole number " + Range(0, 10) + ".");
		}
		if (ok && out.size() >= MaxBosses) {
			fail("bosses.txt may have at most " + std::to_string(MaxBosses) + " entries (the game's once-per-game spawn bit list has an unknown length).");
		}
		if (ok) {
			out.push_back(std::move(def));
		}
	}
	return out;
}

auto LoadMapEvents(std::string_view text, Report& report) -> std::vector<cartographer::events::Row> {
	using cartographer::events::Row;
	std::vector<Row> rows;
	const Tsv table = ParseTsv(text);
	if (NoteIfEmpty(MapEventsFile, table, report) || !RequireColumns(MapEventsFile, table, {"Name", "Kind", "Spawn"}, report)) {
		return rows;
	}
	ReportRepeatedColumns(MapEventsFile, table, report);
	NoteUnusedColumns(MapEventsFile, table, {"Name", "Kind", "Spawn", "Count", "Waves", "Drop", "Message", "Weight"}, report);
	const int cName = table.Column("Name"), cKind = table.Column("Kind"), cSpawn = table.Column("Spawn");
	const int cCount = table.Column("Count"), cWaves = table.Column("Waves"), cDrop = table.Column("Drop");
	const int cMessage = table.Column("Message"), cWeight = table.Column("Weight");
	for (const TsvRow& r : table.rows) {
		Row row;
		bool ok = true;
		struct Typed {
			const char* name;
			int value;
			std::string text;
		};
		std::vector<Typed> typed;  // what was typed, for messages after the int32 cut
		row.line = r.line;
		row.name = table.Cell(r, cName);
		row.kindText = table.Cell(r, cKind);
		if (const auto kind = cartographer::events::parseKindName(row.kindText)) {
			row.kind = *kind;
		}
		row.spawn = table.Cell(r, cSpawn);
		row.drop = table.Cell(r, cDrop);
		row.message = table.Cell(r, cMessage);
		const struct {
			int column;
			const char* name;
			std::optional<int>* value;
		} numbers[] = {{cCount, "Count", &row.count}, {cWaves, "Waves", &row.waves}, {cWeight, "Weight", &row.weight}};
		for (const auto& n : numbers) {
			const std::string cell = table.Cell(r, n.column);
			int v = 0;
			if (cell.empty() || (n.column == cWaves && row.kind != cartographer::events::Kind::Ambush)) {
				continue;  // blank, or Waves on a row that is not an Ambush: not looked at
			}
			if (ParseWhole(cell, &v)) {
				*n.value = v;
				typed.push_back({n.name, v, cell});
			} else {
				report.Error(MapEventsFile, r.line, "'" + (row.name.empty() ? std::string("this event") : row.name) + "' has " + n.name + " '" + cell + "', which is not a whole number. This row is skipped.");
				ok = false;
			}
		}
		if (!ok) {
			continue;
		}
		// Name the typed number only for the label the message is about: the text after the quoted name's "' has " (user text can hold "' has " too).
		const auto echo = [&](std::string message, bool skipped) {
			if (message.empty()) {
				return message;
			}
			const std::string quoted = "'" + row.name + "' has ";
			const size_t found = message.find(quoted);
			if (found != std::string::npos) {
				const size_t has = found + quoted.size();
				for (const Typed& t : typed) {
					if (message.compare(has, std::strlen(t.name) + 1, std::string(t.name) + " ") == 0 && WasCut(t.value, t.text)) {
						if (skipped) {
							message += " This row is skipped.";
						}
						return message + TypedSuffix(t.text);
					}
				}
			}
			return skipped ? message + " This row is skipped." : message;
		};
		const std::string error = echo(cartographer::events::validateRow(row, r.line), true);
		if (!error.empty()) {
			ReportCoreMessage(report, true, MapEventsFile, error);
			continue;
		}
		const std::string warning = echo(cartographer::events::warnRow(row, r.line), false);
		if (!warning.empty()) {
			ReportCoreMessage(report, false, MapEventsFile, warning);
		}
		rows.push_back(std::move(row));
	}
	return rows;
}

auto LoadSettings(std::string_view text, Report& report) -> Settings {
	Settings settings;
	const Tsv table = ParseTsv(text);
	if (NoteIfEmpty(SettingsFile, table, report)) {
		return settings;
	}
	const int cSetting = table.Column("Setting"), cValue = table.Column("Value");
	if (cSetting < 0 || cValue < 0) {
		report.Error(SettingsFile, 1, "the header row needs the columns 'Setting' and 'Value'. The defaults are used.");
		return settings;
	}
	const std::vector<std::string> known = {"Properties", "EventChance", "ItemStats", "PlayerStats", "MapNames", "Town", "MazeProbe", "OutdoorProbe", "AutoCheck", "AutoCheckMap"};
	bool retiredNoted = false;
	for (const TsvRow& row : table.rows) {
		const std::string key = table.Cell(row, cSetting), value = table.Cell(row, cValue);
		if (EqualsNoCase(key, "OutdoorBuild")) {  // retired: outdoor maps are always built
			if (!retiredNoted) {
				report.Note(SettingsFile, row.line, "setting 'OutdoorBuild' is no longer used: outdoor maps are always built. The line is ignored.");
				retiredNoted = true;
			}
			continue;
		}
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
		if (*name == "AutoCheckMap") {
			settings.autoCheckMap = value;  // checked against maps.txt by the engine (autocheck::resolveConfig), which says why a name is unusable
			continue;
		}
		int lo = 0, hi = 20, *target = &settings.properties;
		if (*name == "EventChance") {
			hi = 100;
			target = &settings.eventChance;
		} else if (*name == "PlayerStats") {
			hi = 1;
			target = &settings.playerStats;
		} else if (*name == "MapNames") {
			hi = 1;
			target = &settings.mapNames;
		} else if (*name == "ItemStats") {
			hi = 1;
			target = &settings.itemStats;
		} else if (*name == "MazeProbe") {
			hi = 1;
			target = &settings.mazeProbe;
		} else if (*name == "OutdoorProbe") {
			hi = 1;
			target = &settings.outdoorProbe;
		} else if (*name == "AutoCheck") {
			hi = 1;
			target = &settings.autoCheck;
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
		slot.line = row.line;
		if (slot.name.empty() || !ParseInt(table.Cell(row, cLevel), 1, 65535, &slot.level)) {
			report.Error(SlotsFile, row.line, "the slot needs a name and a level number. Run the installer again (tools\\install.ps1). This slot is skipped.");
			continue;
		}
		slots.push_back(std::move(slot));
	}
	return slots;
}

void ValidateSlots(std::vector<Slot>& slots, std::string_view levelsText, Report& report) {
	constexpr int Towns[] = {1, 40, 75, 103, 109};
	const Tsv levels = ParseTsv(levelsText);
	const int cId = levels.Column("Id"), cName = levels.Column("Name");
	std::vector<int> ids;
	std::vector<std::string> names;  // the levels.txt Name of each id above
	if (cId >= 0) {
		for (const TsvRow& row : levels.rows) {
			int id = 0;
			if (ParseInt(levels.Cell(row, cId), 0, 65535, &id)) {
				ids.push_back(id);
				names.push_back(cName >= 0 ? std::string(levels.Cell(row, cName)) : std::string());
			}
		}
	}
	std::vector<Slot> kept;
	for (Slot& slot : slots) {
		const std::string who = "slot '" + slot.name + "' names level " + std::to_string(slot.level);
		const auto drop = [&](const std::string& why) {
			report.Error(SlotsFile, slot.line, who + ", " + why + ". The slot is not used, so no map opens through it.");
		};
		if (EqualsNoCase(slot.name, "Preset")) {
			report.Note(SlotsFile, slot.line, "the Preset slot is not used any more (every map is generated). The row, and its level in levels.txt and lvlprest.txt, can be deleted.");
		} else if (std::find(std::begin(Towns), std::end(Towns), slot.level) != std::end(Towns)) {
			drop("a town");
		} else if (slot.level <= LastOriginalLevel) {
			drop("a level of the original game. Run the installer again (tools\\install.ps1)");
		} else if (std::find(ids.begin(), ids.end(), slot.level) == ids.end()) {
			drop(cId < 0 ? "but levels.txt could not be read" : "which has no row in levels.txt. Run the installer again (tools\\install.ps1)");
		} else if (EqualsNoCase(slot.name, MazeSlot) &&
		           !EqualsNoCase(names[static_cast<size_t>(std::find(ids.begin(), ids.end(), slot.level) - ids.begin())], MazeLevelName)) {
			drop(std::string("which is not the level the installer wrote (its levels.txt Name must be '") + MazeLevelName + "'). Run the installer again (tools\\install.ps1)");
		} else if (IsOutdoorSlotName(slot.name) &&
		           !EqualsNoCase(names[static_cast<size_t>(std::find(ids.begin(), ids.end(), slot.level) - ids.begin())], OutdoorSlotLevelName(slot.name))) {
			drop("which is not the level the installer wrote (its levels.txt Name must be '" + OutdoorSlotLevelName(slot.name) + "'). Run the installer again (tools\\install.ps1)");
		} else if (std::any_of(kept.begin(), kept.end(), [&](const Slot& k) { return k.level == slot.level; })) {
			drop("which another slot already uses");
		} else if (std::any_of(kept.begin(), kept.end(), [&](const Slot& k) { return EqualsNoCase(k.name, slot.name); })) {
			drop("but a slot of that name is already above");
		} else {
			kept.push_back(std::move(slot));
		}
	}
	slots = std::move(kept);
}

void ReadSlotLevelTypes(std::vector<Slot>& slots, std::string_view levelsText) {
	const Tsv table = ParseTsv(levelsText);
	const int cId = table.Column("Id"), cType = table.Column("LevelType"), cDrlg = table.Column("DrlgType"), cAct = table.Column("Act");
	const int cOffX = table.Column("OffsetX"), cOffY = table.Column("OffsetY"), cSizeX = table.Column("SizeX"), cSizeY = table.Column("SizeY");
	const bool geometryColumns = cOffX >= 0 && cOffY >= 0 && cSizeX >= 0 && cSizeY >= 0;
	if (cId < 0 || (cType < 0 && cDrlg < 0 && cAct < 0 && !geometryColumns)) {
		return;
	}
	for (const TsvRow& row : table.rows) {
		int id = 0, type = 0, drlg = 0;
		if (!ParseInt(table.Cell(row, cId), 0, 65535, &id)) {
			continue;
		}
		const bool haveType = cType >= 0 && ParseInt(table.Cell(row, cType), 0, 255, &type);
		const bool haveDrlg = cDrlg >= 0 && ParseInt(table.Cell(row, cDrlg), 0, 255, &drlg);
		int act = 0;
		const bool haveAct = cAct >= 0 && ParseInt(table.Cell(row, cAct), 0, 255, &act);
		int offX = 0, offY = 0, sizeX = 0, sizeY = 0;
		const bool haveGeometry = geometryColumns && ParseInt(table.Cell(row, cOffX), -1000000, 1000000, &offX) &&
		                          ParseInt(table.Cell(row, cOffY), -1000000, 1000000, &offY) && ParseInt(table.Cell(row, cSizeX), 0, 1000000, &sizeX) &&
		                          ParseInt(table.Cell(row, cSizeY), 0, 1000000, &sizeY);
		for (Slot& slot : slots) {
			if (slot.level == id) {
				if (haveGeometry) {
					slot.haveGeometry = true;
					slot.offsetX = offX;
					slot.offsetY = offY;
					slot.sizeX = sizeX;
					slot.sizeY = sizeY;
				}
				if (haveType) {
					slot.levelType = type;
				}
				if (haveDrlg) {
					slot.drlgType = drlg;
				}
				if (haveAct) {
					slot.act = act;
				}
			}
		}
	}
}

auto IsOutdoorSlotName(std::string_view slot) -> bool {
	return cartographer::outdoor::findOutdoorTilesetBySlot(slot) != nullptr;
}

auto HasOutdoorSlot(const std::vector<Slot>& slots) -> bool {
	for (const cartographer::outdoor::OutdoorTileset& t : cartographer::outdoor::OutdoorTilesets) {
		if (SlotLevel(slots, t.slot) > 0) {
			return true;
		}
	}
	return false;
}

auto OutdoorSlotLevelName(std::string_view slot) -> std::string {
	const cartographer::outdoor::OutdoorTileset* row = cartographer::outdoor::findOutdoorTilesetBySlot(slot);
	return row != nullptr ? std::string(row->levelName) : std::string();
}

auto OutdoorSlotRefusal(const Slot& slot) -> std::string {
	const std::string id = std::to_string(slot.level);
	if (slot.level <= LastOriginalLevel) {
		return "the " + slot.name + " slot level " + id + " is a vanilla level (the slot must be a level the installer added, above " + std::to_string(LastOriginalLevel) + ")";
	}
	if (slot.act != 4) {
		return "the " + slot.name + " slot level " + id + " is not read as Act 5 in levels.txt (Act is " + (slot.act < 0 ? std::string("unknown") : std::to_string(slot.act)) +
		       ", Act 5 is 4)";
	}
	return {};
}

auto OutdoorLevelType(std::string_view tileset) -> int {
	const cartographer::outdoor::OutdoorTileset* row = cartographer::outdoor::findOutdoorTileset(tileset);
	return row != nullptr ? row->levelType : -1;
}

auto OutdoorSlotProblem(const MapDef& map, const std::vector<Slot>& slots) -> std::string {
	const int want = OutdoorLevelType(map.tileset);
	if (want < 0) {
		return {};
	}
	const std::string slotName = MapSlotName(map);
	for (const Slot& slot : slots) {
		if (!EqualsNoCase(slot.name, slotName)) {
			continue;
		}
		if (slot.drlgType >= 0 && slot.drlgType != OutdoorDrlgType) {
			return "the " + slotName + " slot level " + std::to_string(slot.level) + " has DrlgType " + std::to_string(slot.drlgType) +
			       " in levels.txt, but an outdoor map needs " + std::to_string(OutdoorDrlgType) + ", so the map '" + map.name + "' is refused.";
		}
		if (slot.levelType >= 0 && slot.levelType != want) {
			return "the " + slotName + " slot level " + std::to_string(slot.level) + " has LevelType " + std::to_string(slot.levelType) +
			       " in levels.txt, but a " + map.tileset + " map needs " + std::to_string(want) + ", so the map '" + map.name +
			       "' is refused. One outdoor slot serves one tileset.";
		}
		return {};
	}
	return {};
}

auto MapIsOutdoor(const MapDef& map) -> bool {
	return OutdoorLevelType(map.tileset) >= 0;
}

auto MapSlotName(const MapDef& map) -> const char* {
	const cartographer::outdoor::OutdoorTileset* row = cartographer::outdoor::findOutdoorTileset(map.tileset);
	return row != nullptr ? row->slot : MazeSlot;
}

auto OutdoorOpenConflict(const MapDef& map, std::string_view pendingTileset) -> std::string {
	if (!MapIsOutdoor(map) || pendingTileset.empty() || EqualsNoCase(pendingTileset, map.tileset)) {
		return {};
	}
	return "Cartographer: a " + std::string(pendingTileset) + " map is already open and not entered yet, so " + map.name + " (" + map.tileset +
	       ") cannot be opened in this game. Leave the game and start a new one to open it.";
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
	if (state.difficulty != Hell) {
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
	case Gate::Difficulty: return who + "can only be opened in Hell.";
	case Gate::CharLevel: return who + "needs character level " + std::to_string(map.minCharLevel) + " (you are " + std::to_string(state.charLevel) + ").";
	case Gate::AlreadyOpen: return who + "cannot be opened: a map has already been opened in this game.";
	case Gate::NoSpot: return who + "needs a free spot for the portal: step aside and try again.";
	case Gate::Open: break;
	}
	return who + "is open.";
}

}  // namespace carto
