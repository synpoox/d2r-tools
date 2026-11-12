// Host tests for Cartographer's table loader, gates and signature scanner. No game, no SDK.
#define _CRT_SECURE_NO_WARNINGS
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "core/part_signatures.h"
#include "core/sig.h"
#include "core/tables.h"
#include "core/tsv.h"

#include <string>

using namespace carto;

namespace {

const char* const Header = "Name\tTileset\tShape\tSize\tLevel\tItem\tBoss\tMonsters\tDifficulty\tMinCharLevel\tProperties\tEventChance\n";

auto Names() -> GameNames {
	GameNames n;
	n.haveMonsters = n.haveSuperUniques = n.haveItems = true;
	n.monsters = {"zombie1", "skeleton1", "Diablo"};
	n.superUniques = {"Bonebreak", "The Countess"};
	n.itemCodes = {"cmap", "tsc", "shm1"};
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

}  // namespace

TEST_CASE("a three-cell map loads with every default") {
	Report report;
	const auto maps = LoadMaps("Name\tTileset\tShape\nFirst\tPreset\tCorridor\n", Names(), report);
	REQUIRE(maps.size() == 1);
	CHECK(report.problems.empty());
	CHECK(maps[0].name == "First");
	CHECK(maps[0].size == "Medium");
	CHECK(maps[0].level == 85);
	CHECK(maps[0].difficulty == Hell);
	CHECK(maps[0].properties == -1);
	CHECK(maps[0].eventChance == -1);
}

TEST_CASE("our own words ignore case, the game's names do not") {
	Report report;
	const auto maps = Load("a\tPRESET\tcorridor\tsmall\t\t\t\t\thell\n", report);
	REQUIRE(maps.size() == 1);
	CHECK(maps[0].tileset == "Preset");
	CHECK(maps[0].shape == "Corridor");
	CHECK(maps[0].size == "Small");
	Report exact;
	const auto bad = Load("b\tPreset\tCorridor\t\t\t\t\tZombie1\n", exact);
	CHECK(bad.empty());
	CHECK(Joined(exact).find("monster 'Zombie1' is not in monstats.txt") != std::string::npos);
	CHECK(Joined(exact).find("Did you mean 'zombie1'?") != std::string::npos);
}

TEST_CASE("a typo gets a plain-English message with a suggestion and the row number") {
	Report report;
	const auto maps = Load("Good\tPreset\tCorridor\nBad\tJungel\tCorridor\n", report);
	CHECK(maps.size() == 1);  // the bad row is skipped, the good one loads
	const std::string text = Joined(report);
	CHECK(text.find("maps.txt row 3: tileset 'Jungel' is not known. Did you mean 'Jungle'?") != std::string::npos);
	CHECK(report.Errors() == 1);
}

TEST_CASE("a known tileset that cannot be built yet is refused plainly") {
	Report report;
	const auto maps = Load("Cave One\tCave\tWinding\n", report);
	CHECK(maps.empty());
	CHECK(Joined(report).find("planned but Cartographer cannot build it yet") != std::string::npos);
}

TEST_CASE("bad numbers, shapes, bosses, items and duplicates skip only their row") {
	Report report;
	const auto maps = Load(
		"Ok\tPreset\tLoop\n"
		"BadShape\tPreset\tZigzag\n"
		"BadLevel\tPreset\tLoop\t\t999\n"
		"BadBoss\tPreset\tLoop\t\t\t\tNobody\n"
		"BadItem\tPreset\tLoop\t\t\tzzzz\n"
		"ok\tPreset\tLoop\n",
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
	const auto maps = Load("A\tPreset\tLoop\t\t\t\tBonebreak\nB\tPreset\tLoop\t\t\t\tDiablo\n", report);
	CHECK(maps.size() == 2);
	CHECK(report.problems.empty());
}

TEST_CASE("a missing required column is a whole-file error and loads nothing") {
	Report report;
	CHECK(LoadMaps("Name\tTileset\nA\tPreset\n", Names(), report).empty());
	CHECK(Joined(report).find("no 'Shape' column") != std::string::npos);
	Report empty;
	CHECK(LoadMaps("", Names(), empty).empty());
	CHECK(empty.Errors() == 1);
}

TEST_CASE("unknown columns are a note, not an error") {
	Report report;
	const auto maps = LoadMaps("Name\tTileset\tShape\tLvl\nA\tPreset\tLoop\t5\n", Names(), report);
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
	const auto maps = LoadMaps("\xEF\xBB\xBFName\tTileset\tShape\r\nA\tPreset\tLoop\r\n", Names(), report);
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

TEST_CASE("the built-in preset map is always offered, unless maps.txt replaces it") {
	CHECK(MergeWithBuiltin({}).size() == 1);
	CHECK(MergeWithBuiltin({})[0].name == "Preset");
	Report report;
	const auto own = MergeWithBuiltin(Load("preset\tPreset\tLoop\n", report));
	REQUIRE(own.size() == 1);
	CHECK(own[0].shape == "Loop");
	CHECK(FindMap(MergeWithBuiltin({}), "PRESET") != nullptr);
}

TEST_CASE("an item opens the maps that name it, else the maps with a blank Item") {
	Report report;
	const auto maps = MergeWithBuiltin(Load("Hat\tPreset\tLoop\t\t\tshm1\nPlain\tPreset\tLoop\n", report));
	CHECK(ChooseMap(maps, "shm1", 0)->name == "Hat");
	// Preset and Plain both have a blank Item
	const MapDef* a = ChooseMap(maps, "cmap", 0);
	const MapDef* b = ChooseMap(maps, "cmap", 1);
	REQUIRE(a != nullptr);
	REQUIRE(b != nullptr);
	CHECK(a != b);
	CHECK(ChooseMap({}, "cmap", 0) == nullptr);
}

TEST_CASE("gates: Hell, Harrogath, level, one map per game, a free spot") {
	const MapDef map = BuiltinMap();
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
	const MapDef map = BuiltinMap();
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
	                           sig::ClientMessage, sig::OpenRedPortal, sig::GameCounter, sig::UnitStat, sig::UnitCoords, sig::FindFreeSpot};
	for (const char* text : all) {
		Pattern p{};
		INFO(text);
		CHECK(ParsePattern(text, &p));
		CHECK(p.size >= 24);  // a lower bound only: uniqueness in the game is shown by tools/check_portal_signatures.py
	}
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
