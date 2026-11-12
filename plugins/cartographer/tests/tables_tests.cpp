// Host tests for Cartographer's table loader, gates and signature scanner. No game, no SDK.
#define _CRT_SECURE_NO_WARNINGS
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "core/part_signatures.h"
#include "core/request.h"
#include "core/sig.h"
#include "core/tables.h"
#include "core/tsv.h"
#include "events.h"
#include "mapstats.h"

#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>

using namespace carto;

namespace {

const char* const Header = "Name\tTileset\tShape\tSize\tLevel\tItem\tBoss\tMonsters\tDifficulty\tMinCharLevel\tProperties\tEventChance\n";

auto Names() -> GameNames {
	GameNames n;
	n.haveMonsters = n.haveSuperUniques = n.haveItems = true;
	n.monsters = {"zombie1", "skeleton1", "Diablo"};
	n.superUniques = {"Bonebreak", "The Countess"};
	n.itemCodes = {"tsc", "shm1", "t20", "t21", "t22", "t23", "t24"};
	return n;
}

auto Load(const std::string& body, Report& report) -> std::vector<MapDef> {
	return LoadMaps(std::string(Header) + body, Names(), report);
}

auto Joined(const Report& report) -> std::string {
	std::string out;
	for (const Problem& p : report.problems) {
		out += Report::Line(p) + "\n";
	}
	return out;
}

auto TestMap() -> MapDef {
	MapDef map;
	map.name = "Test";
	map.tileset = "Preset";
	map.shape = "Corridor";
	map.item = "t20";
	return map;
}

}  // namespace

TEST_CASE("a four-cell map loads with every default") {
	Report report;
	const auto maps = LoadMaps("Name\tTileset\tShape\tItem\nFirst\tPreset\tCorridor\tt20\n", Names(), report);
	REQUIRE(maps.size() == 1);
	CHECK(report.problems.empty());
	CHECK(maps[0].name == "First");
	CHECK(maps[0].item == "t20");
	CHECK(maps[0].size == "Medium");
	CHECK(maps[0].level == 85);
	CHECK(maps[0].difficulty == Hell);
	CHECK(maps[0].properties == -1);
	CHECK(maps[0].eventChance == -1);
}

TEST_CASE("our own words ignore case, the game's names do not") {
	Report report;
	const auto maps = Load("a\tPRESET\tcorridor\tsmall\t\tt20\t\t\thell\n", report);
	REQUIRE(maps.size() == 1);
	CHECK(maps[0].tileset == "Preset");
	CHECK(maps[0].shape == "Corridor");
	CHECK(maps[0].size == "Small");
	Report exact;
	const auto bad = Load("b\tPreset\tCorridor\t\t\tt21\t\tZombie1\n", exact);
	CHECK(bad.empty());
	CHECK(Joined(exact).find("monster 'Zombie1' is not in monstats.txt") != std::string::npos);
	CHECK(Joined(exact).find("Did you mean 'zombie1'?") != std::string::npos);
}

TEST_CASE("a typo gets a plain-English message with a suggestion and the row number") {
	Report report;
	const auto maps = Load("Good\tPreset\tCorridor\t\t\tt20\nBad\tJungel\tCorridor\t\t\tt21\n", report);
	CHECK(maps.size() == 1);  // the bad row is skipped, the good one loads
	const std::string text = Joined(report);
	CHECK(text.find("maps.txt row 3: tileset 'Jungel' is not known. Did you mean 'Jungle'?") != std::string::npos);
	CHECK(report.Errors() == 1);
}

TEST_CASE("a known tileset that cannot be built yet is refused plainly") {
	Report report;
	const auto maps = Load("Cave One\tCave\tWinding\t\t\tt20\n", report);
	CHECK(maps.empty());
	CHECK(Joined(report).find("planned but Cartographer cannot build it yet") != std::string::npos);
}

TEST_CASE("bad numbers, shapes, bosses, items and duplicates skip only their row") {
	Report report;
	const auto maps = Load(
		"Ok\tPreset\tLoop\t\t\tt20\n"
		"BadShape\tPreset\tZigzag\t\t\tt21\n"
		"BadLevel\tPreset\tLoop\t\t999\tt22\n"
		"BadBoss\tPreset\tLoop\t\t\tt23\tNobody\n"
		"BadItem\tPreset\tLoop\t\t\tzzzz\n"
		"ok\tPreset\tLoop\t\t\tt24\n",
		report);
	CHECK(maps.size() == 1);
	CHECK(report.Errors() == 5);
	const std::string text = Joined(report);
	CHECK(text.find("shape 'Zigzag'") != std::string::npos);
	CHECK(text.find("Level '999' is not a whole number between 1 and 255") != std::string::npos);
	CHECK(text.find("Boss 'Nobody'") != std::string::npos);
	CHECK(text.find("item code 'zzzz'") != std::string::npos);
	CHECK(text.find("already on row 2") != std::string::npos);
}

TEST_CASE("a boss may be a monstats Id or a superunique name") {
	Report report;
	const auto maps = Load("A\tPreset\tLoop\t\t\tt20\tBonebreak\nB\tPreset\tLoop\t\t\tt21\tDiablo\n", report);
	CHECK(maps.size() == 2);
	CHECK(report.problems.empty());
}

TEST_CASE("a missing required column is a whole-file error and loads nothing") {
	Report report;
	CHECK(LoadMaps("Name\tTileset\nA\tPreset\n", Names(), report).empty());
	CHECK(Joined(report).find("no 'Shape' column") != std::string::npos);
	Report noItem;
	CHECK(LoadMaps("Name\tTileset\tShape\nA\tPreset\tLoop\n", Names(), noItem).empty());
	CHECK(Joined(noItem).find("no 'Item' column") != std::string::npos);
	Report empty;
	CHECK(LoadMaps("", Names(), empty).empty());
	CHECK(empty.Errors() == 1);
}

TEST_CASE("unknown columns are a note, not an error") {
	Report report;
	const auto maps = LoadMaps("Name\tTileset\tShape\tItem\tLvl\nA\tPreset\tLoop\tt20\t5\n", Names(), report);
	CHECK(maps.size() == 1);
	CHECK(report.Errors() == 0);
	CHECK(Joined(report).find("note: maps.txt row 1: the column 'Lvl'") != std::string::npos);
}

TEST_CASE("names that cannot be checked are accepted when the game tables were unreadable") {
	Report report;
	const GameNames none;
	const auto maps = LoadMaps("Name\tTileset\tShape\tBoss\tItem\nA\tPreset\tLoop\tAnything\tabcd\n", none, report);
	CHECK(maps.size() == 1);
}

TEST_CASE("windows line endings and a byte order mark are fine") {
	Report report;
	const auto maps = LoadMaps("\xEF\xBB\xBFName\tTileset\tShape\tItem\r\nA\tPreset\tLoop\tt20\r\n", Names(), report);
	CHECK(maps.size() == 1);
	CHECK(report.problems.empty());
}

TEST_CASE("settings default to 6 properties, 20 percent events, no item stats, Harrogath") {
	Report report;
	const Settings defaults = LoadSettings("", report);
	CHECK(defaults.properties == 6);
	CHECK(defaults.eventChance == 20);
	CHECK(defaults.itemStats == 0);
	CHECK(defaults.town == "Harrogath");
	CHECK(report.problems.empty());
}

TEST_CASE("settings: blank means default, a bad value keeps it and says why") {
	Report report;
	const Settings s = LoadSettings("Setting\tValue\nproperties\t8\nEventChance\t\nItemStats\t7\nTown\tLut Gholein\nPropertes\t3\n", report);
	CHECK(s.properties == 8);
	CHECK(s.eventChance == 20);
	CHECK(s.itemStats == 0);
	CHECK(s.town == "Harrogath");
	CHECK(report.Errors() == 3);
	CHECK(Joined(report).find("Did you mean") != std::string::npos);
}

TEST_CASE("slots load and are found ignoring case") {
	Report report;
	const auto slots = LoadSlots("Slot\tLevel\nPreset\t138\nbroken\tx\n", report);
	REQUIRE(slots.size() == 1);
	CHECK(SlotLevel(slots, "preset") == 138);
	CHECK(SlotLevel(slots, "Other") == -1);
	CHECK(report.Errors() == 1);
}

TEST_CASE("every map needs an Item: a blank cell is refused") {
	Report report;
	const auto maps = Load("Has\tPreset\tLoop\t\t\tt20\nNone\tPreset\tLoop\n", report);
	REQUIRE(maps.size() == 1);
	CHECK(maps[0].name == "Has");
	CHECK(report.Errors() == 1);
	CHECK(Joined(report).find("maps.txt row 3: the Item cell is empty") != std::string::npos);
	CHECK(Joined(report).find("This map is skipped.") != std::string::npos);
}

TEST_CASE("two maps may not share an item: the second is refused and names the first") {
	Report report;
	const auto maps = Load("A\tPreset\tLoop\t\t\tt20\nB\tPreset\tLoop\t\t\tt20\nC\tPreset\tLoop\t\t\tt21\n", report);
	REQUIRE(maps.size() == 2);
	CHECK(maps[0].name == "A");
	CHECK(maps[1].name == "C");
	CHECK(report.Errors() == 1);
	CHECK(Joined(report).find("item 't20' already opens the map 'A' (row 2)") != std::string::npos);
}

TEST_CASE("a map's item needs item spell 16 when misc.txt could be read") {
	GameNames names = Names();
	names.haveItemSpells = true;
	names.itemSpells = {{"t20", MapItemSpell}, {"t21", 0}, {"t22", 29}};
	Report report;
	const auto maps = LoadMaps(std::string(Header) + "Ok\tPreset\tLoop\t\t\tt20\nBlank\tPreset\tLoop\t\t\tt21\nOther\tPreset\tLoop\t\t\tt22\nWeapon\tPreset\tLoop\t\t\tshm1\n", names, report);
	REQUIRE(maps.size() == 1);
	CHECK(maps[0].name == "Ok");
	const std::string text = Joined(report);
	CHECK(text.find("Item 't21' cannot open a map: it needs item spell 16 (pSpell in misc.txt), and it has 0.") != std::string::npos);
	CHECK(text.find("it has 29.") != std::string::npos);
	CHECK(text.find("Item 'shm1' cannot open a map") != std::string::npos);
	CHECK(text.find("it is not a misc.txt item.") != std::string::npos);
}

TEST_CASE("item codes are matched ignoring case in the item tables and the spell check") {
	GameNames names = Names();
	names.haveItemSpells = true;
	names.itemSpells = {{"t20", MapItemSpell}, {"t21", 0}};
	names.itemCodes.push_back("t20");
	names.itemCodes.push_back("t21");
	Report report;
	const auto maps = LoadMaps(std::string(Header) + "Up\tPreset\tLoop\t\t\tT20\nBlank\tPreset\tLoop\t\t\tT21\n", names, report);
	REQUIRE(maps.size() == 1);
	CHECK(maps[0].name == "Up");
	CHECK(report.Errors() == 1);  // only T21's spell
}

TEST_CASE("an item opens exactly the map that names it, and any other item opens none") {
	Report report;
	const auto maps = Load("Hat\tPreset\tLoop\t\t\tshm1\nShore\tPreset\tLoop\t\t\tt20\n", report);
	REQUIRE(maps.size() == 2);
	CHECK(FindMapByItem(maps, "shm1")->name == "Hat");
	CHECK(FindMapByItem(maps, "t20")->name == "Shore");
	CHECK(FindMapByItem(maps, "T20")->name == "Shore");
	CHECK(FindMapByItem(maps, "tsc") == nullptr);
	CHECK(FindMapByItem(maps, "") == nullptr);
	CHECK(FindMapByItem({}, "t20") == nullptr);
	// a map is still found by name for "cartographer open"
	CHECK(FindMap(maps, "shore")->item == "t20");
}

TEST_CASE("gates: Hell, Harrogath, level, one map per game, a free spot") {
	const MapDef map = TestMap();
	GateState ok;
	ok.difficulty = Hell;
	ok.charLevel = 90;
	ok.areaId = ok.townId = 109;
	CHECK(CheckGates(map, ok) == Gate::Open);

	GateState s = ok;
	s.difficulty = Nightmare;
	CHECK(CheckGates(map, s) == Gate::Difficulty);
	s = ok;
	s.areaId = 40;
	CHECK(CheckGates(map, s) == Gate::NotInTown);
	s = ok;
	s.openings = 1;
	CHECK(CheckGates(map, s) == Gate::AlreadyOpen);
	s = ok;
	s.spotFree = false;
	CHECK(CheckGates(map, s) == Gate::NoSpot);

	MapDef high = map;
	high.minCharLevel = 80;
	s = ok;
	s.charLevel = 70;
	CHECK(CheckGates(high, s) == Gate::CharLevel);
	CHECK(RefusalText(Gate::CharLevel, high, s).find("needs character level 80 (you are 70)") != std::string::npos);
	CHECK(RefusalText(Gate::AlreadyOpen, map, s).find("already been opened in this game") != std::string::npos);
}

TEST_CASE("the second open is refused after the first counts") {
	const MapDef map = TestMap();
	GateState state;
	state.difficulty = Hell;
	state.charLevel = 99;
	state.areaId = state.townId = 109;
	CHECK(CheckGates(map, state) == Gate::Open);
	state.openings = 1;  // OpenRedPortal counted the first
	CHECK(CheckGates(map, state) == Gate::AlreadyOpen);
}

TEST_CASE("signatures: parse, wildcards, and a unique match") {
	Pattern p{};
	CHECK(ParsePattern("48 8B ?? 05", &p));
	CHECK(p.size == 4);
	CHECK_FALSE(ParsePattern("?? 8B", &p));
	CHECK_FALSE(ParsePattern("4", &p));
	CHECK_FALSE(ParsePattern("GG 11", &p));
	CHECK_FALSE(ParsePattern("", &p));
	REQUIRE(ParsePattern("48 8B ?? 05", &p));
	const uint8_t data[] = {0x00, 0x48, 0x8B, 0x11, 0x05, 0x48, 0x8B, 0x22, 0x06, 0x48, 0x8B, 0x33, 0x05};
	size_t at = 0;
	CHECK(FindPattern(data, sizeof(data), p, &at) == 2);  // two matches: a part with this signature stays off
	CHECK(at == 1);
	Pattern q{};
	REQUIRE(ParsePattern("48 8B 22 06", &q));
	CHECK(FindPattern(data, sizeof(data), q, &at) == 1);
	CHECK(at == 5);
	Pattern none{};
	REQUIRE(ParsePattern("DE AD", &none));
	CHECK(FindPattern(data, sizeof(data), none, &at) == 0);
}

TEST_CASE("signatures: every recorded part signature parses and has at least 24 bytes") {
	const char* const all[] = {sig::ItemSpellCheck, sig::ItemSpellUse, sig::ItemRecord, sig::UnitClassOf, sig::UnitRoom, sig::RoomLevelId,
	                           sig::ClientMessage, sig::OpenRedPortal, sig::GameCounter, sig::UnitStat, sig::UnitCoords, sig::FindFreeSpot, sig::PlayerTick};
	for (const char* text : all) {
		Pattern p{};
		INFO(text);
		CHECK(ParsePattern(text, &p));
		CHECK(p.size >= 24);  // a lower bound only: uniqueness in the game is shown by tools/check_portal_signatures.py
	}
}

TEST_CASE("an open request is served once, in time, and expires when the game never serves it") {
	OpenRequest request;
	CHECK(request.Take(0, 5000).state == OpenRequest::State::None);
	request.Post("Preset", 1000);
	CHECK(request.Pending());
	const OpenRequest::Taken ready = request.Take(2000, 5000);
	CHECK(ready.state == OpenRequest::State::Ready);
	CHECK(ready.map == "Preset");
	CHECK_FALSE(request.Pending());
	CHECK(request.Take(2001, 5000).state == OpenRequest::State::None);  // served once
	request.Post("Preset", 1000);
	CHECK(request.Take(6001, 5000).state == OpenRequest::State::Expired);
	request.Post("S", 100);
	CHECK_FALSE(request.Stale(5100, 5000));
	CHECK(request.Stale(5101, 5000));
	request.Clear();
	CHECK_FALSE(request.Stale(99999, 5000));
	request.Post("A", 0);
	request.Post("B", 10);  // a newer request replaces the older one
	CHECK(request.Take(20, 5000).map == "B");
	request.Post("C", 0);
	request.Clear();
	CHECK(request.Take(1, 5000).state == OpenRequest::State::None);
}

TEST_CASE("spelling suggestions") {
	CHECK(Suggest("Jungel", TilesetNames()) == "Jungle");
	CHECK(Suggest("zzzzzz", TilesetNames()).empty());
	CHECK(DidYouMean("lopp", {"Loop", "Winding"}) == " Did you mean 'Loop'?");
}

TEST_CASE("long words get no suggestion, and a report stops listing after 200 problems") {
	CHECK(Suggest(std::string(5000, 'a'), {"aaaa"}).empty());
	Report report;
	for (int i = 1; i <= 500; ++i) {
		report.Error("maps.txt", i, "bad");
	}
	CHECK(report.Errors() == 200);
	CHECK(report.problems.size() == 201);
	Report other;
	other.Error("cartographer.txt", 1, "bad");
	CHECK(other.Errors() == 1);
}

// --- The example pack (examples/first-map) is what docs/cartographer/tables.md and the README show. -------------------

namespace {

auto ExampleFile(const char* name) -> std::string {
	std::ifstream in(std::string(CARTOGRAPHER_EXAMPLE_DIR) + "/" + name, std::ios::binary);
	REQUIRE_MESSAGE(in.good(), name);
	std::ostringstream text;
	text << in.rdbuf();
	return text.str();
}

auto ExampleNames() -> GameNames {
	GameNames n = Names();
	n.monsters.push_back("quillrat1");
	n.superUniques.push_back("Bishibosh");
	n.haveItemSpells = true;
	n.itemSpells = {{"t20", 16}, {"t21", 16}};
	return n;
}

}  // namespace

TEST_CASE("the example pack's maps.txt and cartographer.txt load with no problems") {
	Report report;
	const auto maps = LoadMaps(ExampleFile("maps.txt"), ExampleNames(), report);
	CHECK_MESSAGE(report.problems.empty(), Joined(report));
	REQUIRE(maps.size() == 2);
	CHECK(maps[0].name == "Shorelines");
	CHECK(maps[0].item == "t20");
	CHECK(maps[0].size == "Large");
	CHECK(maps[0].level == 90);
	CHECK(maps[0].monsters.size() == 2);
	CHECK(maps[0].minCharLevel == 80);
	CHECK(maps[0].properties == 4);
	CHECK(maps[1].shape == "Loop");
	CHECK(maps[1].level == 85);  // blank cells keep the defaults

	Report settingsReport;
	const Settings settings = LoadSettings(ExampleFile("cartographer.txt"), settingsReport);
	CHECK_MESSAGE(settingsReport.problems.empty(), Joined(settingsReport));
	CHECK(settings.properties == 6);
	CHECK(settings.eventChance == 20);
	CHECK(settings.itemStats == 0);
	CHECK(settings.town == "Harrogath");
}

TEST_CASE("the example pack's mapstats.txt and mapevents.txt pass the real stats and events checks") {
	// The plugin does not read these two tables yet; the pack is run through the same checks the later phases use.
	const Tsv stats = ParseTsv(ExampleFile("mapstats.txt"));
	REQUIRE(stats.header == std::vector<std::string>{"Target", "Stat", "Min", "Max", "Weight"});
	std::vector<cartographer::stats::PoolRow> pool;
	for (const TsvRow& row : stats.rows) {
		const std::string target = stats.Cell(row, 0);
		CHECK((EqualsNoCase(target, "Players") || EqualsNoCase(target, "Monsters")));
		cartographer::stats::PoolRow p;
		p.target = EqualsNoCase(target, "Players") ? cartographer::stats::Target::Players : cartographer::stats::Target::Monsters;
		p.stat = stats.Cell(row, 1);
		p.min = std::atoi(stats.Cell(row, 2).c_str());
		p.max = std::atoi(stats.Cell(row, 3).c_str());
		p.weight = std::atoi(stats.Cell(row, 4).c_str());
		pool.push_back(p);
	}
	CHECK(pool.size() == 4);
	for (const auto& issue : cartographer::stats::checkPool(pool)) {
		FAIL_CHECK(issue.message);
	}

	const Tsv events = ParseTsv(ExampleFile("mapevents.txt"));
	REQUIRE(events.header == std::vector<std::string>{"Name", "Kind", "Spawn", "Count", "Waves", "Drop", "Message", "Weight"});
	std::vector<cartographer::events::Row> rows;
	for (const TsvRow& r : events.rows) {
		cartographer::events::Row row;
		row.name = events.Cell(r, 0);
		row.kindText = events.Cell(r, 1);
		if (const auto kind = cartographer::events::parseKindName(row.kindText)) {
			row.kind = *kind;
		}
		row.spawn = events.Cell(r, 2);
		if (!events.Cell(r, 3).empty()) {
			row.count = std::atoi(events.Cell(r, 3).c_str());
		}
		if (!events.Cell(r, 4).empty()) {
			row.waves = std::atoi(events.Cell(r, 4).c_str());
		}
		row.drop = events.Cell(r, 5);
		row.message = events.Cell(r, 6);
		if (!events.Cell(r, 7).empty()) {
			row.weight = std::atoi(events.Cell(r, 7).c_str());
		}
		rows.push_back(row);
	}
	CHECK(rows.size() == 3);
	for (const std::string& problem : cartographer::events::validateRows(rows)) {
		FAIL_CHECK(problem);
	}
	for (const std::string& note : cartographer::events::warnRows(rows)) {
		FAIL_CHECK(note);
	}
}
