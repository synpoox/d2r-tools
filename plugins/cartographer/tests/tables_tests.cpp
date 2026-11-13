// Host tests for Cartographer's table loader, gates and signature scanner. No game, no SDK.
#define _CRT_SECURE_NO_WARNINGS
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "core/coexist.h"
#include "core/part_signatures.h"
#include "core/request.h"
#include "core/sig.h"
#include "core/tables.h"
#include "core/tsv.h"
#include "events.h"

#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>

using namespace carto;

namespace {

const char* const Header = "Name\tTileset\tShape\tSize\tLevel\tItem\tBoss\tMonsters\tMinCharLevel\tEventChance\n";

auto Names() -> GameNames {
	GameNames n;
	n.haveMonsters = n.haveSuperUniques = n.haveItems = true;
	n.monsters = {"zombie1", "skeleton1", "Diablo", "andariel"};
	n.haveBossFlags = n.haveMods = true;
	n.bossMonsters = {"andariel"};
	for (int i = 0; i <= 44; ++i) {
		n.monsterMods.push_back(i);
	}
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
	map.tileset = "Cave";
	map.shape = "Corridor";
	map.item = "t20";
	return map;
}

}  // namespace

TEST_CASE("a four-cell map loads with every default") {
	Report report;
	const auto maps = LoadMaps("Name\tTileset\tShape\tItem\nFirst\tCave\tCorridor\tt20\n", Names(), report);
	REQUIRE(maps.size() == 1);
	CHECK(report.problems.empty());
	CHECK(maps[0].name == "First");
	CHECK(maps[0].item == "t20");
	CHECK(maps[0].size == "Medium");
	CHECK(maps[0].level == 85);
	CHECK(maps[0].eventChance == -1);
}

TEST_CASE("our own words ignore case, the game's names do not") {
	Report report;
	const auto maps = Load("a\tCAVE\tcorridor\tsmall\t\tt20\t\t\n", report);
	REQUIRE(maps.size() == 1);
	CHECK(maps[0].tileset == "Cave");
	CHECK(maps[0].shape == "Corridor");
	CHECK(maps[0].size == "Small");
	Report exact;
	const auto bad = Load("b\tCave\tCorridor\t\t\tt21\t\tZombie1\n", exact);
	CHECK(bad.empty());
	CHECK(Joined(exact).find("monster 'Zombie1' is not in monstats.txt") != std::string::npos);
	CHECK(Joined(exact).find("Did you mean 'zombie1'?") != std::string::npos);
}

TEST_CASE("a typo gets a plain-English message with a suggestion and the row number") {
	Report report;
	const auto maps = Load("Good\tCave\tCorridor\t\t\tt20\nBad\tJungel\tCorridor\t\t\tt21\n", report);
	CHECK(maps.size() == 1);  // the bad row is skipped, the good one loads
	const std::string text = Joined(report);
	CHECK(text.find("maps.txt row 3: tileset 'Jungel' is not known. Did you mean 'Jungle'?") != std::string::npos);
	CHECK(report.Errors() == 1);
}

TEST_CASE("a known tileset that cannot be built yet is refused plainly") {
	Report report;
	const auto maps = Load("Barracks One\tBarracks\tWinding\t\t\tt20\n", report);
	CHECK(maps.empty());
	CHECK(Joined(report).find("planned but Cartographer cannot build it yet") != std::string::npos);
}

TEST_CASE("the Open shape loads for Cave, Crypt and Jungle; Snow takes no Shape, so Open is refused there with a report line") {
	Report report;
	const auto maps = Load(
		"A\tCave\tOpen\t\t\tt20\n"
		"B\tCrypt\topen\tExtraSmall\t\tt21\n"
		"C\tJungle\tOpen\t\t\tt22\n"
		"D\tSnow\tOpen\t\t\tt23\n"
		"E\tSnow\tSprawl\t\t\tt24\n",
		report);
	REQUIRE(maps.size() == 4);
	CHECK(maps[0].shape == "Open");
	CHECK(maps[1].shape == "Open");
	CHECK(maps[2].tileset == "Jungle");
	CHECK(maps[3].name == "E");  // Snow ignores every other Shape, so those rows still load
	CHECK(report.Errors() == 1);
	const std::string text = Joined(report);
	CHECK(text.find("'Snow' cannot build the Open shape") != std::string::npos);
	CHECK(text.find("This map is skipped.") != std::string::npos);
}

TEST_CASE("a Jungle map whose shapes.txt Open row has a MinPath the capped grid cannot reach is refused with a report line") {
	Report report;
	const auto shapes = LoadShapes("Name\tBase\tMinPath\tMaxPath\tWidth\tHeight\nLong\tOpen\t20\t60\t20\t20\nWide\tOpen\t\t\t20\t20\n", report);
	REQUIRE(shapes.shapes.size() == 2);
	std::vector<MapDef> maps = LoadMaps(std::string(Header) + "A\tJungle\tLong\tLarge\t\tt20\nB\tJungle\tWide\tLarge\t\tt21\nC\tCave\tLong\tLarge\t\tt22\n", Names(), report, &shapes);
	REQUIRE(maps.size() == 2);
	CHECK(maps[0].name == "B");
	CHECK(maps[1].name == "C");
	const std::string text = Joined(report);
	CHECK(text.find("capped at 9 x 9 cells, and MinPath 20 is too long for it. Lower MinPath in shapes.txt.") != std::string::npos);
	CHECK(text.find("seed") == std::string::npos);
	CHECK(text.find("Width and Height") == std::string::npos);
}

TEST_CASE("a shapes.txt row based on Open is Open for Snow too") {
	Report report;
	const auto shapes = LoadShapes("Name\tBase\nPlains\tOpen\nTwisty\tWinding\n", report);
	REQUIRE(shapes.shapes.size() == 2);
	std::vector<MapDef> maps = LoadMaps(std::string(Header) + "A\tSnow\tPlains\t\t\tt20\nB\tSnow\tTwisty\t\t\tt21\nC\tCave\tPlains\t\t\tt22\n", Names(), report, &shapes);
	REQUIRE(maps.size() == 2);
	CHECK(maps[0].name == "B");
	CHECK(maps[1].name == "C");
	CHECK(Joined(report).find("Open shape") != std::string::npos);
}

TEST_CASE("Cave, Crypt and Lava are accepted tilesets; the settings file knows MazeProbe") {
	Report report;
	const auto maps = Load("Cave One\tcave\tWinding\t\t\tt20\nCrypt One\tCrypt\tLoop\t\t\tt21\n", report);
	CHECK(maps.size() == 2);
	CHECK(maps[0].tileset == "Cave");
	CHECK(report.Errors() == 0);
	Report settingsReport;
	CHECK(LoadSettings("", settingsReport).mazeProbe == 0);
	CHECK(LoadSettings("Setting\tValue\nMazeProbe\t1\n", settingsReport).mazeProbe == 1);
	CHECK(settingsReport.Errors() == 0);
	CHECK(LoadSettings("Setting\tValue\nMazeProbe\t5\n", settingsReport).mazeProbe == 0);
	CHECK(settingsReport.Errors() == 1);
}

TEST_CASE("AutoCheck is off by default; AutoCheckMap keeps the map name as written") {
	Report report;
	const Settings off = LoadSettings("", report);
	CHECK(off.autoCheck == 0);
	CHECK(off.autoCheckMap.empty());
	const Settings on = LoadSettings("Setting\tValue\nAutoCheck\t1\nautocheckmap\tColossal Summit\n", report);
	CHECK(on.autoCheck == 1);
	CHECK(on.autoCheckMap == "Colossal Summit");
	CHECK(report.Errors() == 0);
	CHECK(LoadSettings("Setting\tValue\nAutoCheck\t2\n", report).autoCheck == 0);
	CHECK(report.Errors() == 1);
}

TEST_CASE("bad numbers, shapes, bosses, items and duplicates skip only their row") {
	Report report;
	const auto maps = Load(
		"Ok\tCave\tLoop\t\t\tt20\n"
		"BadShape\tCave\tZigzag\t\t\tt21\n"
		"BadLevel\tCave\tLoop\t\t999\tt22\n"
		"BadBoss\tCave\tLoop\t\t\tt23\tNobody\n"
		"BadItem\tCave\tLoop\t\t\tzzzz\n"
		"ok\tCave\tLoop\t\t\tt24\n",
		report);
	CHECK(maps.size() == 2);  // the Nobody boss is refused, the map still loads
	CHECK(report.Errors() == 5);
	const std::string text = Joined(report);
	CHECK(text.find("shape 'Zigzag'") != std::string::npos);
	CHECK(text.find("Level '999' is not a whole number between 1 and 255") != std::string::npos);
	CHECK(text.find("Boss 'Nobody'") != std::string::npos);
	CHECK(text.find("item code 'zzzz'") != std::string::npos);
	CHECK(text.find("already on row 2") != std::string::npos);
}

namespace {

auto Bosses() -> std::vector<BossDef> {
	Report report;
	const auto bosses = LoadBosses("Name\tBase\tMods\tMinions\nThe Drowned King\tzombie1\t5,9\t3\nGrave Warden\tskeleton1\n", Names(), report);
	REQUIRE(report.problems.empty());
	return bosses;
}

}  // namespace

TEST_CASE("bosses.txt rows load with their mods and minions") {
	const auto bosses = Bosses();
	REQUIRE(bosses.size() == 2);
	CHECK(bosses[0].name == "The Drowned King");
	CHECK(bosses[0].base == "zombie1");
	CHECK(bosses[0].mods == std::vector<int>{5, 9});
	CHECK(bosses[0].minions == 3);
	CHECK(bosses[1].mods.empty());
	CHECK(bosses[1].minions == 0);
}

TEST_CASE("a bosses.txt Name is trimmed of space, tab, CR and LF only (tools/install.ps1 does the same)") {
	Report report;
	const auto bosses = LoadBosses("Name\tBase\n  Pad\xA0 \tzombie1\n\xA0Lead\tzombie1\n", Names(), report);
	REQUIRE(bosses.size() == 2);
	CHECK(bosses[0].name == "Pad\xA0");  // a trailing non-breaking space stays, so the installer's key matches
	CHECK(BossKey(bosses[0].name) == "cartoboss:Pad_A0");
	CHECK(bosses[1].name == "\xA0Lead");
}

TEST_CASE("a bosses.txt Name is compared folding only A-Z (tools/install.ps1 does the same)") {
	Report report;
	const auto bosses = LoadBosses("Name\tBase\nRen\xC9\tzombie1\nREN\xE9\tzombie1\nrene\tzombie1\nRENE\tzombie1\n", Names(), report);
	REQUIRE(bosses.size() == 3);  // 0xC9 and 0xE9 are two names, rene and RENE are one
	CHECK(bosses[0].name == "Ren\xC9");
	CHECK(bosses[1].name == "REN\xE9");
	CHECK(bosses[2].name == "rene");
}

TEST_CASE("a bosses.txt entry named like a vanilla boss or monster, or with a bad cell, is skipped") {
	Report report;
	const auto bosses = LoadBosses(
		"Name\tBase\tMods\tMinions\n"
		"Bonebreak\tzombie1\n"
		"diablo\tzombie1\n"
		"NoBase\tnobody\n"
		"BadMods\tzombie1\t1,2,3,4\n"
		"BadMinions\tzombie1\t\t11\n"
		"Fine\tzombie1\n"
		"fine\tskeleton1\n",
		Names(), report);
	REQUIRE(bosses.size() == 1);
	CHECK(bosses[0].name == "Fine");
	CHECK(report.Errors() == 6);
	CHECK(Joined(report).find("vanilla boss or monster") != std::string::npos);
}

TEST_CASE("a bosses.txt Base that is a vanilla boss, a Mods id outside monumod.txt, or a 17th entry is skipped") {
	Report report;
	std::string body = "Name\tBase\tMods\nA\tandariel\nB\tzombie1\t200\nC\tzombie1\t44\n";
	CHECK(LoadBosses(body, Names(), report).size() == 1);
	CHECK(report.Errors() == 2);
	const std::string text = Joined(report);
	CHECK(text.find("Base 'andariel' is a vanilla boss") != std::string::npos);
	CHECK(text.find("not an id in monumod.txt") != std::string::npos);
	Report many;
	body = "Name\tBase\n";
	for (size_t i = 0; i < MaxBosses + 1; ++i) {
		body += "Boss" + std::to_string(i) + "\tzombie1\n";
	}
	CHECK(LoadBosses(body, Names(), many).size() == MaxBosses);
	CHECK(many.Errors() == 1);
}

TEST_CASE("a Boss that names a cartoboss: key gets its own refusal") {
	const auto bosses = Bosses();
	Report report;
	const auto maps = LoadMaps(std::string(Header) + "A\tCave\tLoop\t\t\tt20\tcartoboss:The Drowned King\n", Names(), report, nullptr, &bosses);
	REQUIRE(maps.size() == 1);
	CHECK(maps[0].boss.empty());
	const std::string text = Joined(report);
	CHECK(text.find("superuniques.txt key the installer writes") != std::string::npos);
	CHECK(text.find("vanilla") == std::string::npos);
}

TEST_CASE("table text is UTF-8: Latin-1 and UTF-8 files with the same Boss name give the same name and key") {
	const std::string utf8 = "Name\tBase\nG\xC3\xA9rard \xC3\x96stberg\tzombie1\nR\xC3\xB6n\xC3\xA9, Jr.\tskeleton1\n";
	const std::string latin1 = "Name\tBase\nG\xE9rard \xD6stberg\tzombie1\nR\xF6n\xE9, Jr.\tskeleton1\n";
	bool fromLatin1 = true;
	CHECK(NormalizeTableText(utf8, &fromLatin1) == utf8);
	CHECK_FALSE(fromLatin1);
	CHECK(NormalizeTableText(latin1, &fromLatin1) == utf8);
	CHECK(fromLatin1);
	Report a, b;
	const auto fromUtf8 = LoadBosses(NormalizeTableText(utf8), Names(), a);
	const auto fromAnsi = LoadBosses(NormalizeTableText(latin1), Names(), b);
	REQUIRE(fromUtf8.size() == 2);
	REQUIRE(fromAnsi.size() == 2);
	CHECK(fromAnsi[0].name == "G\xC3\xA9rard \xC3\x96stberg");
	CHECK(fromAnsi[0].name == fromUtf8[0].name);
	CHECK(fromAnsi[1].name == fromUtf8[1].name);
	CHECK(BossKey(fromAnsi[0].name) == "cartoboss:G_C3_A9rard_20_C3_96stberg");
	CHECK(BossKey(fromAnsi[1].name) == BossKey(fromUtf8[1].name));
}

TEST_CASE("table text: a UTF-8 byte order mark is dropped, invalid UTF-8 takes the Latin-1 path, ASCII is untouched") {
	bool fromLatin1 = true;
	CHECK(NormalizeTableText("\xEF\xBB\xBFName\n", &fromLatin1) == "Name\n");
	CHECK_FALSE(fromLatin1);
	CHECK(NormalizeTableText("plain\ttext\n", &fromLatin1) == "plain\ttext\n");
	CHECK_FALSE(fromLatin1);
	CHECK(NormalizeTableText("", &fromLatin1).empty());
	CHECK(IsValidUtf8("\xC3\xA9\xE2\x82\xAC\xF0\x9F\x98\x80"));
	for (const char* bad : {"\xE9", "\xC3", "\xC3(", "\xC0\x80", "\xE0\x80\x80", "\xED\xA0\x80", "\xF4\x90\x80\x80", "\xFF", "\x80", "ab\xE2\x82"}) {
		CHECK_FALSE(IsValidUtf8(bad));
	}
	CHECK(NormalizeTableText("Ren\xE9", &fromLatin1) == "Ren\xC3\xA9");
	CHECK(fromLatin1);
	CHECK(NormalizeTableText("\xA0", &fromLatin1) == "\xC2\xA0");  // a lone Latin-1 non-breaking space
}

TEST_CASE("UTF-8 validator boundaries: invalid first bytes, overlong, surrogate and out-of-range forms are rejected, the edges are kept") {
	// Bytes that can never start a sequence (C0 C1 are always overlong, F5 and above are past U+10FFFF).
	for (const char* bad : {"\xC0\x80", "\xC1\xBF", "\xF5\x80\x80\x80", "\xFF"}) {
		CHECK_FALSE(IsValidUtf8(bad));
	}
	// Second byte outside the range its lead byte allows: overlong E0/F0, surrogates ED A0..BF, F0 below U+10000, F4 above U+10FFFF.
	for (const char* bad : {"\xE0\x9F\xBF", "\xE0\x80\x80", "\xED\xA0\x80", "\xED\xBF\xBF", "\xF0\x8F\xBF\xBF", "\xF0\x80\x80\x80",
	                        "\xF4\x90\x80\x80"}) {
		CHECK_FALSE(IsValidUtf8(bad));
	}
	// The first and last code point of each length class.
	for (const char* good : {"\xC2\x80", "\xDF\xBF", "\xE0\xA0\x80", "\xED\x9F\xBF", "\xEE\x80\x80", "\xEF\xBF\xBF", "\xF0\x90\x80\x80",
	                         "\xF4\x8F\xBF\xBF"}) {
		CHECK(IsValidUtf8(good));
	}
	// A Latin-1 string that happens to be valid UTF-8 is kept as UTF-8 (C3 A9 reads as one character, not as two Latin-1 ones).
	bool fromLatin1 = true;
	CHECK(NormalizeTableText("\xC3\xA9", &fromLatin1) == "\xC3\xA9");
	CHECK_FALSE(fromLatin1);
	// The same kind of bytes with a boundary violation fall back to Latin-1 and are converted.
	CHECK(NormalizeTableText("\xED\xA0\x80", &fromLatin1) == "\xC3\xAD\xC2\xA0\xC2\x80");
	CHECK(fromLatin1);
}

TEST_CASE("a boss entry's superuniques key can never equal a vanilla row") {
	CHECK(BossKey("The Drowned King") == "cartoboss:The_20Drowned_20King");
	CHECK(BossKey("King: a_b \"x\" Ren\xE9") == "cartoboss:King_3A_20a_5Fb_20_22x_22_20Ren_E9");  // ASCII only, one-to-one
	CHECK(BossKey("a_b") != BossKey("a b"));
	for (const std::string& vanilla : Names().superUniques) {
		CHECK(BossKey(vanilla) != vanilla);
	}
}

TEST_CASE("boss keys of UTF-8 names with different case match the installer's") {
	// Same keys tools/install.ps1 wrote for these names on a scratch copy of the vanilla tables (A-Z folded only, U+00A0 kept).
	CHECK(BossKey("Ren\xC3\xA9 Lost") == "cartoboss:Ren_C3_A9_20Lost");
	CHECK(BossKey("REN\xC3\x89 LOST") == "cartoboss:REN_C3_89_20LOST");
	CHECK(BossKey("Nbsp\xC2\xA0") == "cartoboss:Nbsp_C2_A0");
	CHECK(BossKey("\xC2\xA0Lead") == "cartoboss:_C2_A0Lead");
}

TEST_CASE("a map's Boss resolves to a bosses.txt entry only") {
	const auto bosses = Bosses();
	Report report;
	const auto maps = LoadMaps(std::string(Header) + "A\tCave\tLoop\t\t\tt20\tthe drowned king\n", Names(), report, nullptr, &bosses);
	REQUIRE(maps.size() == 1);
	CHECK(maps[0].boss == "The Drowned King");
	CHECK(report.problems.empty());
}

TEST_CASE("a vanilla or unknown Boss is refused with a message and the map opens without it") {
	const auto bosses = Bosses();
	Report report;
	const auto maps = LoadMaps(std::string(Header) +
		"A\tCave\tLoop\t\t\tt20\tBonebreak\n"
		"B\tCave\tLoop\t\t\tt21\tDiablo\n"
		"C\tCave\tLoop\t\t\tt22\tNobody\n"
		"D\tCave\tLoop\t\t\tt23\n",
		Names(), report, nullptr, &bosses);
	REQUIRE(maps.size() == 4);
	for (const MapDef& map : maps) {
		CHECK(map.boss.empty());
	}
	CHECK(report.Errors() == 3);
	const std::string text = Joined(report);
	CHECK(text.find("Boss 'Bonebreak' is a vanilla boss") != std::string::npos);
	CHECK(text.find("Boss 'Diablo' is a vanilla monster") != std::string::npos);
	CHECK(text.find("Boss 'Nobody' is not an entry in bosses.txt") != std::string::npos);
	CHECK(text.find("The map opens without a Boss.") != std::string::npos);
}

TEST_CASE("without bosses.txt every Boss is refused") {
	Report report;
	const auto maps = Load("A\tCave\tLoop\t\t\tt20\tAnything\n", report);
	REQUIRE(maps.size() == 1);
	CHECK(maps[0].boss.empty());
	CHECK(Joined(report).find("(there are none)") != std::string::npos);
}

TEST_CASE("the boss spawn path: its own row, else the base monster plain, else none") {
	CHECK(PickBossPath(true, true) == BossPath::Superunique);
	CHECK(PickBossPath(true, false) == BossPath::Superunique);
	CHECK(PickBossPath(false, true) == BossPath::PlainMonster);
	CHECK(PickBossPath(false, false) == BossPath::None);
}

TEST_CASE("a missing required column is a whole-file error and loads nothing") {
	Report report;
	CHECK(LoadMaps("Name\tTileset\nA\tCave\n", Names(), report).empty());
	CHECK(Joined(report).find("no 'Shape' column") != std::string::npos);
	Report noItem;
	CHECK(LoadMaps("Name\tTileset\tShape\nA\tCave\tLoop\n", Names(), noItem).empty());
	CHECK(Joined(noItem).find("no 'Item' column") != std::string::npos);
	Report empty;
	CHECK(LoadMaps("", Names(), empty).empty());
	CHECK(empty.Errors() == 1);
}

TEST_CASE("unknown columns are a note, not an error") {
	Report report;
	const auto maps = LoadMaps("Name\tTileset\tShape\tItem\tLvl\nA\tCave\tLoop\tt20\t5\n", Names(), report);
	CHECK(maps.size() == 1);
	CHECK(report.Errors() == 0);
	CHECK(Joined(report).find("note: maps.txt row 1: the column 'Lvl'") != std::string::npos);
}

TEST_CASE("names that cannot be checked are accepted when the game tables were unreadable") {
	Report report;
	const GameNames none;
	const auto maps = LoadMaps("Name\tTileset\tShape\tBoss\tItem\nA\tCave\tLoop\tAnything\tabcd\n", none, report);
	CHECK(maps.size() == 1);
}

TEST_CASE("windows line endings and a byte order mark are fine") {
	Report report;
	const auto maps = LoadMaps("\xEF\xBB\xBFName\tTileset\tShape\tItem\r\nA\tCave\tLoop\tt20\r\n", Names(), report);
	CHECK(maps.size() == 1);
	CHECK(report.problems.empty());
}

TEST_CASE("settings default to 20 percent events, Harrogath") {
	Report report;
	const Settings defaults = LoadSettings("", report);
	CHECK(defaults.eventChance == 20);
	CHECK(defaults.town == "Harrogath");
	CHECK(report.Errors() == 0);
	REQUIRE(report.problems.size() == 1);  // an empty optional file is a note
	CHECK(report.problems[0].severity == Severity::Note);
}

TEST_CASE("settings: blank means default, a bad value keeps it and says why") {
	Report report;
	const Settings s = LoadSettings("Setting\tValue\nEventChance\t\nTown\tLut Gholein\nEventChanse\t3\nPlayerStats\t7\n", report);
	CHECK(s.eventChance == 20);
	CHECK(s.town == "Harrogath");
	CHECK(s.playerStats == 1);
	CHECK(report.Errors() == 3);
	CHECK(Joined(report).find("Did you mean") != std::string::npos);
}

TEST_CASE("settings: PlayerStats is on by default and takes 0 or 1")
{
	Report report;
	CHECK(LoadSettings("Setting\tValue\nMapNames\t1\n", report).playerStats == 1);
	CHECK(LoadSettings("Setting\tValue\nplayerstats\t0\n", report).playerStats == 0);
	CHECK(report.Errors() == 0);
	CHECK(LoadSettings("Setting\tValue\nPlayerStats\t2\n", report).playerStats == 1);
	CHECK(report.Errors() == 1);
}

TEST_CASE("slots load and are found ignoring case") {
	Report report;
	const auto slots = LoadSlots("Slot\tLevel\nMaze\t138\nbroken\tx\n", report);
	REQUIRE(slots.size() == 1);
	CHECK(SlotLevel(slots, "maze") == 138);
	CHECK(SlotLevel(slots, "Other") == -1);
	CHECK(report.Errors() == 1);
}

TEST_CASE("slot level types are read from levels.txt") {
	Report report;
	auto slots = LoadSlots("Slot\tLevel\nMaze\t138\nOther\t139\nGone\t500\n", report);
	ReadSlotLevelTypes(slots, "Name\tId\tLevelType\nA\t138\t3\nB\t139\t4\n");
	REQUIRE(slots.size() == 3);
	CHECK(slots[0].levelType == 3);
	CHECK(slots[1].levelType == 4);
	CHECK(slots[2].levelType == -1);
	ReadSlotLevelTypes(slots, "Name\tId\n");  // no LevelType column: nothing changes
	CHECK(slots[1].levelType == 4);
}

TEST_CASE("the Preset tileset is refused: every map is generated") {
	Report report;
	CHECK(Load("Old\tPreset\tLoop\t\t\tt20\n", report).empty());
	const std::string text = Joined(report);
	CHECK(text.find("Preset maps are gone: use Cave or Crypt.") != std::string::npos);
	CHECK(text.find("is not known") == std::string::npos);
}

namespace {
const char* const SlotLevels =
    "Name\tId\tLevelType\nTown\t109\t1\nCave\t45\t3\nCartographer Maze\t138\t3\nCartographer Maze\t139\t4\n";
}

TEST_CASE("a Maze slot on a level the installer did not write is dropped") {
	Report report;
	auto slots = LoadSlots("Slot\tLevel\nMaze\t140\n", report);
	ValidateSlots(slots, "Name\tId\tLevelType\nCartographer Maze\t138\t3\nSomeone's Level\t140\t3\n", report);
	CHECK(slots.empty());
	CHECK(SlotLevel(slots, MazeSlot) == -1);
	CHECK(Joined(report).find("not the level the installer wrote") != std::string::npos);
	CHECK(report.Errors() == 1);
	Report nameless;
	auto noName = LoadSlots("Slot\tLevel\nMaze\t138\n", nameless);
	ValidateSlots(noName, "Id\tLevelType\n138\t3\n", nameless);  // no Name column: it cannot be told, so the slot stays off
	CHECK(noName.empty());
}

TEST_CASE("a slot on a good installed level is kept") {
	Report report;
	auto slots = LoadSlots("Slot\tLevel\nMaze\t138\n", report);
	ValidateSlots(slots, SlotLevels, report);
	REQUIRE(slots.size() == 1);
	CHECK(SlotLevel(slots, MazeSlot) == 138);
	CHECK(report.Errors() == 0);
}

TEST_CASE("a slot naming a vanilla level or a town is dropped and reported") {
	Report report;
	auto slots = LoadSlots("Slot\tLevel\nMaze\t45\nTown\t109\nLow\t1\nEdge\t137\n", report);
	ValidateSlots(slots, SlotLevels, report);
	CHECK(slots.empty());
	CHECK(SlotLevel(slots, MazeSlot) == -1);
	const std::string text = Joined(report);
	CHECK(text.find("slot 'Maze' names level 45, a level of the original game") != std::string::npos);
	CHECK(text.find("slot 'Town' names level 109, a town") != std::string::npos);
	CHECK(report.Errors() == 4);
}

TEST_CASE("a slot whose level has no levels.txt row is dropped") {
	Report report;
	auto slots = LoadSlots("Slot\tLevel\nMaze\t500\n", report);
	ValidateSlots(slots, SlotLevels, report);
	CHECK(slots.empty());
	CHECK(Joined(report).find("no row in levels.txt") != std::string::npos);
	Report unreadable;
	auto again = LoadSlots("Slot\tLevel\nMaze\t138\n", unreadable);
	ValidateSlots(again, "", unreadable);  // levels.txt could not be read: nothing can be checked, so the slot stays off
	CHECK(again.empty());
	CHECK(unreadable.Errors() == 1);
}

TEST_CASE("an old Preset slot row is dropped with a note") {
	Report report;
	auto slots = LoadSlots("Slot\tLevel\nPreset\t139\nMaze\t138\n", report);
	ValidateSlots(slots, SlotLevels, report);
	REQUIRE(slots.size() == 1);
	CHECK(SlotLevel(slots, "Preset") == -1);
	CHECK(report.Errors() == 0);
	CHECK(Joined(report).find("can be deleted") != std::string::npos);
}

TEST_CASE("a second slot on the same level or with the same name is dropped") {
	Report report;
	auto slots = LoadSlots("Slot\tLevel\nMaze\t138\nOther\t138\nmaze\t139\nThird\t139\n", report);
	ValidateSlots(slots, SlotLevels, report);
	REQUIRE(slots.size() == 2);
	CHECK(SlotLevel(slots, "Maze") == 138);
	CHECK(SlotLevel(slots, "Third") == 139);
	CHECK(report.Errors() == 2);
}

TEST_CASE("the Outdoor slot is refused unless it is a new level that levels.txt puts in Act 5") {
	Report report;
	auto slots = LoadSlots("Slot\tLevel\nOutdoor\t140\nTown\t109\nLow\t137\n", report);
	ReadSlotLevelTypes(slots, "Id\tAct\n140\t4\n109\t4\n137\t4\n");
	CHECK(slots[0].act == 4);
	CHECK(OutdoorSlotRefusal(slots[0]).empty());
	CHECK(OutdoorSlotRefusal(slots[1]).find("vanilla") != std::string::npos);  // a town is a vanilla level
	CHECK(OutdoorSlotRefusal(slots[2]).find("vanilla") != std::string::npos);  // 137 is the last vanilla id

	ReadSlotLevelTypes(slots, "Id\tAct\n140\t2\n");  // another act: the game's own builder for that act would run on the slot
	CHECK(slots[0].act == 2);
	CHECK(OutdoorSlotRefusal(slots[0]).find("not read as Act 5") != std::string::npos);

	auto unread = LoadSlots("Slot\tLevel\nOutdoor\t140\n", report);
	CHECK(unread[0].act == -1);
	CHECK(OutdoorSlotRefusal(unread[0]).find("unknown") != std::string::npos);  // no Act column: refused, not guessed
	ReadSlotLevelTypes(unread, "Id\tDrlgType\n140\t3\n");
	CHECK(!OutdoorSlotRefusal(unread[0]).empty());
}

TEST_CASE("OutdoorProbe defaults to off and takes 0 or 1") {
	Report report;
	CHECK(LoadSettings("", report).outdoorProbe == 0);
	CHECK(LoadSettings("Setting\tValue\nOutdoorProbe\t1\n", report).outdoorProbe == 1);
	CHECK(report.Errors() == 0);
	CHECK(LoadSettings("Setting\tValue\nOutdoorProbe\t2\n", report).outdoorProbe == 0);  // out of range: the default is kept, with an error
	CHECK(report.Errors() == 1);
}

TEST_CASE("an old OutdoorBuild line is ignored with one note, not an error, and the other settings still load") {
	Report report;
	const Settings settings = LoadSettings("Setting\tValue\nOutdoorBuild\t0\noutdoorbuild\t1\nEventChance\t7\n", report);
	CHECK(report.Errors() == 0);
	CHECK(settings.eventChance == 7);
	REQUIRE(report.problems.size() == 1);
	CHECK(report.problems[0].text.find("no longer used") != std::string::npos);
	// Outdoor maps do not depend on any setting (Settings has no switch for them), so a 0 here cannot turn them off.
}

TEST_CASE("slot geometry (origin and declared size) is read from levels.txt only when all four columns parse") {
	Report report;
	auto slots = LoadSlots("Slot\tLevel\nOutdoor\t140\nMaze\t139\n", report);
	ReadSlotLevelTypes(slots, "Id\tSizeX\tSizeY\tOffsetX\tOffsetY\tDrlgType\n140\t384\t320\t6000\t3000\t3\n139\t360\t360\t\t3000\t1\n");
	CHECK(slots[0].haveGeometry);
	CHECK(slots[0].sizeX == 384);
	CHECK(slots[0].sizeY == 320);
	CHECK(slots[0].offsetX == 6000);
	CHECK(slots[0].offsetY == 3000);
	CHECK(slots[0].drlgType == 3);
	CHECK(!slots[1].haveGeometry);  // a blank OffsetX: unknown, not 0
	CHECK(slots[1].drlgType == 1);
	auto noGeometry = LoadSlots("Slot\tLevel\nOutdoor\t140\n", report);
	ReadSlotLevelTypes(noGeometry, "Id\tSizeX\tSizeY\tDrlgType\n140\t384\t320\t3\n");  // no Offset columns
	CHECK(!noGeometry[0].haveGeometry);
	CHECK(noGeometry[0].drlgType == 3);
}

TEST_CASE("slot DrlgType is read from levels.txt too, and the Outdoor slot is checked against the tileset") {
	Report report;
	auto slots = LoadSlots("Slot\tLevel\nPreset\t138\nOutdoor\t140\nMaze\t139\n", report);
	CHECK(slots[1].name == "Outdoor");
	ReadSlotLevelTypes(slots, "Name\tId\tDrlgType\tLevelType\nA\t138\t2\t2\nB\t139\t1\t3\nC\t140\t3\t21\n");
	CHECK(slots[0].drlgType == 2);
	CHECK(slots[1].drlgType == 3);
	CHECK(slots[1].levelType == 21);
	CHECK(slots[2].drlgType == 1);
	ReadSlotLevelTypes(slots, "Name\tId\tDrlgType\nC\t140\t1\n");  // only the DrlgType column: LevelType stays
	CHECK(slots[1].drlgType == 1);
	CHECK(slots[1].levelType == 21);

	CHECK(OutdoorLevelType("jungle") == 21);
	CHECK(OutdoorLevelType("Snow") == 31);
	CHECK(OutdoorLevelType("Cave") == -1);

	MapDef jungle;
	jungle.name = "Shorelines";
	jungle.tileset = "Jungle";
	CHECK(OutdoorSlotProblem(jungle, slots).find("DrlgType 1") != std::string::npos);
	ReadSlotLevelTypes(slots, "Name\tId\tDrlgType\tLevelType\nC\t140\t3\t21\n");
	CHECK(OutdoorSlotProblem(jungle, slots).empty());
	MapDef snow = jungle;
	snow.tileset = "Snow";
	CHECK(OutdoorSlotProblem(snow, slots).empty());  // a Snow map is checked against the OutdoorSnow slot, which this table does not have
	slots.push_back(slots[1]);                       // an OutdoorSnow slot on a level that has the Jungle LevelType
	slots.back().name = "OutdoorSnow";
	const std::string wrong = OutdoorSlotProblem(snow, slots);
	CHECK(wrong.find("OutdoorSnow") != std::string::npos);
	CHECK(wrong.find("LevelType 21") != std::string::npos);
	CHECK(wrong.find("needs 31") != std::string::npos);
	CHECK(wrong.find("Shorelines") != std::string::npos);
	slots.back().levelType = 31;
	CHECK(OutdoorSlotProblem(snow, slots).empty());
	CHECK(OutdoorSlotProblem(jungle, slots).empty());  // the Jungle map still looks at the Outdoor slot only

	MapDef cave = jungle;
	cave.tileset = "Cave";
	CHECK(OutdoorSlotProblem(cave, slots).empty());       // not an outdoor map
	CHECK(OutdoorSlotProblem(jungle, {}).empty());        // no Outdoor slot: the 'not installed' message says so
	auto unread = LoadSlots("Slot\tLevel\nOutdoor\t140\n", report);
	CHECK(OutdoorSlotProblem(jungle, unread).empty());    // levels.txt not read: never refuses on what it does not know
}

TEST_CASE("every map needs an Item: a blank cell is refused") {
	Report report;
	const auto maps = Load("Has\tCave\tLoop\t\t\tt20\nNone\tCave\tLoop\n", report);
	REQUIRE(maps.size() == 1);
	CHECK(maps[0].name == "Has");
	CHECK(report.Errors() == 1);
	CHECK(Joined(report).find("maps.txt row 3: the Item cell is empty") != std::string::npos);
	CHECK(Joined(report).find("This map is skipped.") != std::string::npos);
}

TEST_CASE("two maps may not share an item: the second is refused and names the first") {
	Report report;
	const auto maps = Load("A\tCave\tLoop\t\t\tt20\nB\tCave\tLoop\t\t\tt20\nC\tCave\tLoop\t\t\tt21\n", report);
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
	const auto maps = LoadMaps(std::string(Header) + "Ok\tCave\tLoop\t\t\tt20\nBlank\tCave\tLoop\t\t\tt21\nOther\tCave\tLoop\t\t\tt22\nWeapon\tCave\tLoop\t\t\tshm1\n", names, report);
	REQUIRE(maps.size() == 1);
	CHECK(maps[0].name == "Ok");
	const std::string text = Joined(report);
	CHECK(text.find("Item 't21' cannot open a map: it needs item spell 16 (pSpell in misc.txt), and it has 0.") != std::string::npos);
	CHECK(text.find("it has 29.") != std::string::npos);
	CHECK(text.find("Item 'shm1' cannot open a map") != std::string::npos);
	CHECK(text.find("it is not a misc.txt item.") != std::string::npos);
}

TEST_CASE("a map item with compactsave 1 gets a note, one with compactsave 0 does not") {
	GameNames names = Names();
	names.haveItemSpells = true;
	names.itemSpells = {{"t20", MapItemSpell}, {"t21", MapItemSpell}};
	names.compactItems = {"t21"};
	Report report;
	const auto maps = LoadMaps(std::string(Header) + "A\tCave\tLoop\t\t\tt20\nB\tCave\tLoop\t\t\tt21\n", names, report);
	CHECK(maps.size() == 2);
	const std::string text = Joined(report);
	CHECK(text.find("Item 't21' has compactsave 1") != std::string::npos);
	CHECK(text.find("Item 't20' has compactsave") == std::string::npos);
}

TEST_CASE("item codes are matched ignoring case in the item tables and the spell check") {
	GameNames names = Names();
	names.haveItemSpells = true;
	names.itemSpells = {{"t20", MapItemSpell}, {"t21", 0}};
	names.itemCodes.push_back("t20");
	names.itemCodes.push_back("t21");
	Report report;
	const auto maps = LoadMaps(std::string(Header) + "Up\tCave\tLoop\t\t\tT20\nBlank\tCave\tLoop\t\t\tT21\n", names, report);
	REQUIRE(maps.size() == 1);
	CHECK(maps[0].name == "Up");
	CHECK(report.Errors() == 1);  // only T21's spell
}

TEST_CASE("an item opens exactly the map that names it, and any other item opens none") {
	Report report;
	const auto maps = Load("Hat\tCave\tLoop\t\t\tshm1\nShore\tCave\tLoop\t\t\tt20\n", report);
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
	s.difficulty = Normal;
	CHECK(CheckGates(map, s) == Gate::Difficulty);
	CHECK(RefusalText(Gate::Difficulty, map, s) == "Cartographer: Test can only be opened in Hell.");
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

TEST_CASE("an old Difficulty column is ignored with a note, not an error") {
	Report report;
	const auto maps = LoadMaps("Name\tTileset\tShape\tItem\tDifficulty\nOld\tCave\tCorridor\tt20\tNormal\nBad\tCave\tLoop\tt21\tSoon\n", Names(), report);
	CHECK(maps.size() == 2);
	CHECK(report.Errors() == 0);
	REQUIRE(report.problems.size() == 1);
	CHECK(report.problems[0].severity == Severity::Note);
	CHECK(Joined(report).find("the column 'Difficulty' is ignored: maps can only be opened in Hell.") != std::string::npos);
	GateState state;
	state.difficulty = Normal;
	state.charLevel = 90;
	state.areaId = state.townId = 109;
	CHECK(CheckGates(maps[0], state) == Gate::Difficulty);
	state.difficulty = Hell;
	CHECK(CheckGates(maps[0], state) == Gate::Open);
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
	                           sig::ClientMessage, sig::OpenRedPortal, sig::GameCounter, sig::UnitStat, sig::UnitCoords, sig::FindFreeSpot, sig::PlayerTick,
	                           sig::LevelBuild, sig::MazeWalk, sig::MazeEntranceExit, sig::MazeRingGrower, sig::MazeAddRoom, sig::MazeLinkRooms, sig::MazePiecePicker, sig::ArrivalFinder, sig::ArrivalRoomFinder, sig::DataTablesBase, sig::NameToRow,
	                           sig::OutdoorEntry, sig::OutdoorAct5Builder, sig::PieceStamp, sig::VoidCell, sig::UnitMove};
	for (const char* text : all) {
		Pattern p{};
		INFO(text);
		CHECK(ParsePattern(text, &p));
		CHECK(p.size >= 24);  // a lower bound only: uniqueness in the game is shown by tools/check_portal_signatures.py
	}
	// The flag test is a 9-byte function (`mov eax,[rcx+0x50]; and eax,0x30000; ret`); its signature stops at the `ret`, so it is short.
	Pattern flag{};
	CHECK(ParsePattern(sig::ArrivalFlagTest, &flag));
	CHECK(flag.size == 9);
}

TEST_CASE("signatures: every map stats part signature parses; the hooked one pins the 14 bytes it cuts") {
	const char* const all[] = {sig::NewStatList, sig::SetListStat, sig::AttachStatList, sig::DetachStatList, sig::FreeStatList, sig::MonsterSetup};
	for (const char* text : all) {
		Pattern p{};
		INFO(text);
		CHECK(ParsePattern(text, &p));
		CHECK(p.size >= 15);
	}
	Pattern hook{};
	REQUIRE(ParsePattern(sig::MonsterSetup, &hook));
	CHECK(hook.size == 21);
	for (size_t i = 0; i < 14; ++i) {
		CHECK_FALSE(hook.wild[i]);  // the guard is the exact entry bytes
	}
}

TEST_CASE("signatures: the hooked maze parts pin every byte their hook cuts") {
	const struct { const char* text; uint32_t cut; } hooked[] = {
		{sig::LevelBuild, 14}, {sig::MazeWalk, 18}, {sig::MazeEntranceExit, 17}, {sig::MazeRingGrower, 14}};
	for (const auto& h : hooked) {
		Pattern p{};
		INFO(h.text);
		REQUIRE(ParsePattern(h.text, &p));
		REQUIRE(p.size >= h.cut);
		for (uint32_t i = 0; i < h.cut; ++i) {
			CHECK(!p.wild[i]);
		}
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
	n.itemSpells = {{"t20", 16}, {"t21", 16}, {"t22", 16}};
	return n;
}

}  // namespace

TEST_CASE("the example pack's maps.txt and cartographer.txt load with no problems") {
	Report report;
	const auto bosses = LoadBosses(ExampleFile("bosses.txt"), ExampleNames(), report);
	const auto maps = LoadMaps(ExampleFile("maps.txt"), ExampleNames(), report, nullptr, &bosses);
	CHECK_MESSAGE(report.problems.empty(), Joined(report));
	REQUIRE(bosses.size() == 1);
	REQUIRE(maps.size() == 3);
	CHECK(maps[0].boss == "Shoreline Warden");
	CHECK(maps[0].name == "Shorelines");
	CHECK(maps[0].item == "t20");
	CHECK(maps[0].size == "Large");
	CHECK(maps[0].level == 90);
	CHECK(maps[0].monsters.size() == 2);
	CHECK(maps[0].minCharLevel == 80);
	CHECK(maps[1].shape == "Loop");
	CHECK(maps[1].level == 85);  // blank cells keep the defaults

	Report settingsReport;
	const Settings settings = LoadSettings(ExampleFile("cartographer.txt"), settingsReport);
	CHECK_MESSAGE(settingsReport.problems.empty(), Joined(settingsReport));
	CHECK(settings.eventChance == 20);
	CHECK(settings.town == "Harrogath");
}

TEST_CASE("MapNames is on unless cartographer.txt turns it off") {
	Report none;
	CHECK(LoadSettings("Setting\tValue\n", none).mapNames == 1);
	Report off;
	CHECK(LoadSettings("Setting\tValue\nMapNames\t0\n", off).mapNames == 0);
}

TEST_CASE("the example pack loads every file: maps, settings, shapes and mapevents") {
	Report report;
	const cartographer::layout::ShapeTable shapes = LoadShapes(ExampleFile("shapes.txt"), report);
	const auto events = LoadMapEvents(ExampleFile("mapevents.txt"), report);
	const auto bosses = LoadBosses(ExampleFile("bosses.txt"), ExampleNames(), report);
	const auto maps = LoadMaps(ExampleFile("maps.txt"), ExampleNames(), report, &shapes, &bosses);
	CHECK_MESSAGE(report.problems.empty(), Joined(report));
	REQUIRE(shapes.shapes.size() == 1);
	CHECK(shapes.shapes[0].name == "Snake");
	REQUIRE(events.size() == 3);
	CHECK(events[0].name == "The Drowned King");
	CHECK(events[0].kind == cartographer::events::Kind::Boss);
	CHECK(events[0].count == 6);
	CHECK(events[0].weight == 2);
	CHECK(events[1].drop.empty());
	CHECK(events[2].drop == "Act 5 (H) Super C");
	CHECK(maps.size() == 3);
}

TEST_CASE("shapes.txt: a good row loads, a bad row is reported with its line and skipped") {
	Report report;
	const auto shapes = LoadShapes("Name\tBase\tMinPath\tMaxPath\nSnake\tWinding\t12\t16\nBroken\tWinding\tmany\t16\nCorridor\n", report);
	REQUIRE(shapes.shapes.size() == 1);
	CHECK(shapes.shapes[0].name == "Snake");
	REQUIRE(report.problems.size() == 2);  // the built-in name Corridor is refused too
	CHECK(report.problems[0].file == "shapes.txt");
	CHECK(report.problems[0].line == 3);
	CHECK_MESSAGE(Report::Line(report.problems[0]).find("shapes.txt row 3: MinPath must be a whole number") == 0, Report::Line(report.problems[0]));
	Report empty;
	CHECK(LoadShapes("", empty).shapes.empty());
	REQUIRE(empty.problems.size() == 1);
	CHECK(empty.problems[0].severity == Severity::Note);
}

TEST_CASE("maps.txt Shape may name a shapes.txt row, ignoring case; an unknown name is an error") {
	Report report;
	const auto shapes = LoadShapes("Name\tBase\nSnake\tWinding\n", report);
	REQUIRE(report.problems.empty());
	const std::string text = std::string(Header) +
	                         "Eel\tCave\tsnake\t\t\tt20\n"
	                         "Worm\tCave\tSerpent\t\t\tt21\n"
	                         "Plain\tCave\tLoop\t\t\tt22\n";
	Report maps;
	const auto loaded = LoadMaps(text, Names(), maps, &shapes);
	REQUIRE(loaded.size() == 2);
	CHECK(loaded[0].shape == "Snake");
	CHECK(loaded[1].shape == "Loop");
	REQUIRE(maps.Errors() == 1);
	CHECK(maps.problems[0].line == 3);
	CHECK(maps.problems[0].text.find("shape 'Serpent' is not known.") == 0);
	CHECK(maps.problems[0].text.find("'Snake'") != std::string::npos);
	// Without a shapes.txt the same name is an error.
	Report without;
	CHECK(LoadMaps(text, Names(), without).size() == 1);
	CHECK(without.Errors() == 2);
}

TEST_CASE("retired: the Properties column of maps.txt and the Properties and ItemStats settings are noted and ignored") {
	Report maps;
	const auto loaded = LoadMaps("Name\tTileset\tShape\tItem\tProperties\nA\tCave\tLoop\tt20\t4\n", Names(), maps);
	CHECK(loaded.size() == 1);
	CHECK(maps.Errors() == 0);
	REQUIRE(maps.problems.size() == 1);
	CHECK(maps.problems[0].text.find("'Properties' is no longer used") != std::string::npos);
	Report settings;
	LoadSettings("Setting\tValue\nProperties\t8\nItemStats\t1\nEventChance\t30\n", settings);
	CHECK(settings.Errors() == 0);
	REQUIRE(settings.problems.size() == 2);
	CHECK(settings.problems[0].severity == Severity::Note);
	CHECK(settings.problems[1].text.find("'ItemStats' is no longer used") != std::string::npos);
}

TEST_CASE("repeated header columns are reported in maps and mapevents") {
	Report events;
	CHECK(LoadMapEvents("Name\tKind\tSpawn\tName\nA\tBoss\tx\tB\n", events).size() == 1);
	CHECK(events.Errors() == 1);
	Report maps;
	LoadMaps(std::string("Name\tTileset\tShape\tItem\tItem\nA\tJungle\tLoop\tt20\tt21\n"), Names(), maps);
	bool seen = false;
	for (const Problem& p : maps.problems) {
		seen = seen || p.text.find("appears more than once") != std::string::npos;
	}
	CHECK(seen);
}

TEST_CASE("mapevents.txt: rows load; bad rows are skipped with the row's line") {
	Report report;
	const auto events = LoadMapEvents(
	    "Name\tKind\tSpawn\tCount\tWaves\tDrop\tMessage\tWeight\n"
	    "Ambush A\tAmbush\tzombie1\t5\t4\t\t\t1\n"
	    "Bad kind\tAmbsh\tzombie1\t\t\t\t\t\n"
	    "Bad count\tHorde\tzombie1\tlots\t\t\t\t\n"
	    "No spawn\tBoss\t\t\t\t\t\t\n"
	    "Hoard\tTreasure\tchest\t\t\t\t\t\n",
	    report);
	REQUIRE(events.size() == 2);
	CHECK(events[0].waves == 4);
	CHECK(events[1].name == "Hoard");
	CHECK(report.Errors() == 3);
	REQUIRE(report.problems.size() == 4);
	CHECK(report.problems[0].line == 3);
	CHECK_MESSAGE(report.problems[0].text.find("'Bad kind' has Kind 'Ambsh', which is not a Kind. Did you mean Ambush?") == 0, report.problems[0].text);
	CHECK(report.problems[1].line == 4);
	CHECK(report.problems[1].text.find("Count 'lots'") != std::string::npos);
	CHECK(report.problems[2].line == 5);
	CHECK(report.problems[3].severity == Severity::Note);  // a Treasure with no Drop
	CHECK(report.problems[3].line == 6);

	Report none;
	CHECK(LoadMapEvents("", none).empty());
	REQUIRE(none.problems.size() == 1);
	CHECK(none.problems[0].severity == Severity::Note);
}

TEST_CASE("mapevents.txt: Waves is only read on Ambush rows; a number beyond int32 gets the range message") {
	Report report;
	const auto events = LoadMapEvents(
	    "Name\tKind\tSpawn\tCount\tWaves\n"
	    "Boss row\tBoss\tDiablo\t\tabc\n"
	    "Ambush bad\tAmbush\tzombie1\t\tabc\n"
	    "Big\tHorde\tzombie1\t3000000000\t\n",
	    report);
	REQUIRE(events.size() == 1);
	CHECK(events[0].name == "Boss row");
	CHECK(!events[0].waves.has_value());
	REQUIRE(report.Errors() == 2);
	CHECK(report.problems[0].line == 3);
	CHECK(report.problems[0].text.find("Waves 'abc'") != std::string::npos);
	CHECK(report.problems[1].line == 4);
	CHECK(report.problems[1].text.find("not a whole number") == std::string::npos);
}

TEST_CASE("a value cut to int32 gets a suffix naming what was typed; the message is not changed") {
	Report events;
	LoadMapEvents("Name\tKind\tSpawn\tCount\nBig\tHorde\tzombie1\t3000000000\n", events);
	REQUIRE(events.Errors() == 1);
	CHECK_MESSAGE(events.problems[0].text.find("has Count 2147483647.") != std::string::npos, events.problems[0].text);
	CHECK_MESSAGE(events.problems[0].text.find("(you typed 3000000000)") != std::string::npos, events.problems[0].text);
}

TEST_CASE("no suffix for a value that was not cut, or an empty Weight") {
	Report events;
	LoadMapEvents("Name\tKind\tSpawn\tCount\nCount 0\tHorde\tzombie1\t+0\n", events);
	REQUIRE(events.Errors() == 1);
	CHECK_MESSAGE(events.problems[0].text.find("you typed") == std::string::npos, events.problems[0].text);
	CHECK_MESSAGE(events.problems[0].text.find("'Count 0'") != std::string::npos, events.problems[0].text);
}

TEST_CASE("the suffix names only the label the message is about; a Name is kept as typed") {
	Report a;
	LoadMapEvents("Name\tKind\tSpawn\tCount\tWeight\nWeight 2147483647\tHorde\tzombie1\t0\t99999999999\n", a);
	REQUIRE(a.Errors() == 1);
	CHECK_MESSAGE(a.problems[0].text.find("'Weight 2147483647' has Count 0.") != std::string::npos, a.problems[0].text);
	CHECK_MESSAGE(a.problems[0].text.find("you typed") == std::string::npos, a.problems[0].text);

	Report b;
	LoadMapEvents("Name\tKind\tSpawn\tCount\nCount 2147483647\tBogus\tzombie1\t3000000000\n", b);
	REQUIRE(b.Errors() == 1);
	CHECK_MESSAGE(b.problems[0].text.find("'Count 2147483647' has Kind 'Bogus'") != std::string::npos, b.problems[0].text);
	CHECK_MESSAGE(b.problems[0].text.find("you typed") == std::string::npos, b.problems[0].text);
}

TEST_CASE("a Kind that holds \"' has Count\" does not get the Count suffix") {
	Report report;
	LoadMapEvents("Name\tKind\tSpawn\tCount\nx\tBogus' has Count x\tzombie1\t3000000000\n", report);
	REQUIRE(report.Errors() == 1);
	CHECK_MESSAGE(report.problems[0].text.find("'x' has Kind 'Bogus' has Count x'") != std::string::npos, report.problems[0].text);
	CHECK_MESSAGE(report.problems[0].text.find("you typed") == std::string::npos, report.problems[0].text);
}

TEST_CASE("a repeated unknown column is reported once") {
	const GameNames names;
	Report report;
	LoadMaps("Name\tTileset\tShape\tItem\tColour\tcolour\nA\tCave\tCorridor\tt20\t\t\n", names, report, nullptr);
	int unused = 0;
	for (const Problem& p : report.problems) {
		unused += p.text.find("is not used") != std::string::npos ? 1 : 0;
	}
	CHECK(unused == 1);
}

TEST_CASE("no notice when core-maps is not loaded") {
	CHECK(carto::CoreMapsNotice(false).empty());
}

TEST_CASE("the notice names core-maps and says to use one") {
	const std::string text = carto::CoreMapsNotice(true);
	CHECK(text.find("core-maps") != std::string::npos);
	CHECK(text.find("Remove one") != std::string::npos);
}

TEST_CASE("Jungle and Snow are ready tilesets and open through the Outdoor and OutdoorSnow slots; Cave, Crypt and Lava through their own maze slots") {
	Report report;
	const auto maps = Load("Wild\tJungle\tWinding\t\t\tt20\nDeep\tCave\tLoop\t\t\tt21\n", report);
	REQUIRE(maps.size() == 2);
	CHECK(report.Errors() == 0);
	CHECK(MapIsOutdoor(maps[0]));
	CHECK(std::string(MapSlotName(maps[0])) == "Outdoor");
	CHECK(!MapIsOutdoor(maps[1]));
	CHECK(std::string(MapSlotName(maps[1])) == MazeSlot);
	const auto ready = ReadyTilesetNames();
	CHECK(std::find(ready.begin(), ready.end(), "Jungle") != ready.end());
	CHECK(std::find(ready.begin(), ready.end(), "Snow") != ready.end());
	Report snowReport;
	const auto snowMaps = Load("Frost\tSnow\tWinding\t\t\tt22\n", snowReport);
	REQUIRE(snowMaps.size() == 1);
	CHECK(snowReport.Errors() == 0);
	CHECK(MapIsOutdoor(snowMaps[0]));
	CHECK(std::string(MapSlotName(snowMaps[0])) == "OutdoorSnow");
}

TEST_CASE("a maze open is refused while a map of another maze tileset is open and unbuilt") {
	Report report;
	const auto maps = Load("Deep\tCave\tLoop\t\t\tt21\nTomb\tCrypt\tLoop\t\t\tt23\nWild\tJungle\tWinding\t\t\tt20\nBurn\tLava\tLoop\t\t\tt24\n", report);
	REQUIRE(maps.size() == 4);
	const MapDef& cave = maps[0];
	const MapDef& crypt = maps[1];
	const MapDef& jungle = maps[2];
	CHECK(MazeOpenConflict(maps[3], "Crypt").find("Crypt map is already open") != std::string::npos);
	CHECK(MazeOpenConflict(cave, "Cave").empty());
	CHECK(MazeOpenConflict(cave, "").empty());          // nothing pending
	CHECK(MazeOpenConflict(cave, "Cave").empty());      // the same tileset replaces its own job
	CHECK(MazeOpenConflict(cave, "cave").empty());
	CHECK(MazeOpenConflict(crypt, "Cave").find("Cave map is already open") != std::string::npos);  // Cave pending, then Crypt
	CHECK(MazeOpenConflict(cave, "Crypt").find("Crypt map is already open") != std::string::npos);  // and the reverse
	CHECK(MazeOpenConflict(crypt, "Cave").find("Leave the game and start a new one") != std::string::npos);
	CHECK(MazeOpenConflict(jungle, "Cave").empty());    // an outdoor map has its own job
}

TEST_CASE("an outdoor open is refused while a map of the other outdoor tileset is open and unbuilt") {
	Report report;
	const auto maps = Load("Wild\tJungle\tWinding\t\t\tt20\nFrost\tSnow\tWinding\t\t\tt22\nDeep\tCave\tLoop\t\t\tt21\n", report);
	REQUIRE(maps.size() == 3);
	const MapDef& jungle = maps[0];
	const MapDef& snow = maps[1];
	const MapDef& cave = maps[2];
	CHECK(OutdoorOpenConflict(jungle, "").empty());        // nothing pending
	CHECK(OutdoorOpenConflict(snow, "").empty());
	CHECK(OutdoorOpenConflict(jungle, "Jungle").empty());  // the same slot replaces its own job
	CHECK(OutdoorOpenConflict(snow, "Snow").empty());
	CHECK(OutdoorOpenConflict(snow, "Jungle").find("Jungle map is already open") != std::string::npos);  // Jungle pending, then Snow
	CHECK(OutdoorOpenConflict(jungle, "Snow").find("Snow map is already open") != std::string::npos);    // and the reverse
	CHECK(OutdoorOpenConflict(jungle, "Snow").find("Leave the game and start a new one") != std::string::npos);
	CHECK(OutdoorOpenConflict(jungle, "Snow").find("Enter that portal") == std::string::npos);
	CHECK(OutdoorOpenConflict(cave, "Jungle").empty());    // a maze map has its own job
}

TEST_CASE("an Outdoor slot on a level the installer did not write is dropped; the installer's level is kept") {
	const std::string levels = "Name\tId\tLevelType\nCartographer Maze\t138\t3\nCartographer Outdoor\t139\t21\nSomeone's Level\t140\t21\n";
	Report bad;
	auto other = LoadSlots("Slot\tLevel\nOutdoor\t140\n", bad);
	ValidateSlots(other, levels, bad);
	CHECK(other.empty());
	CHECK(Joined(bad).find("Cartographer Outdoor") != std::string::npos);
	CHECK(bad.Errors() == 1);
	Report good;
	auto both = LoadSlots("Slot\tLevel\nMaze\t138\nOutdoor\t139\n", good);
	ValidateSlots(both, levels, good);
	CHECK(good.Errors() == 0);
	CHECK(SlotLevel(both, "Outdoor") == 139);
	CHECK(SlotLevel(both, MazeSlot) == 138);
	Report swapped;
	auto crossed = LoadSlots("Slot\tLevel\nMaze\t139\n", swapped);  // the Outdoor level under the Maze name: refused by the Maze name rule
	ValidateSlots(crossed, levels, swapped);
	CHECK(crossed.empty());
}

TEST_CASE("the Outdoor level the installer writes passes the slot checks for a Jungle map") {
	// The row tools\install.ps1 writes: id 139, Act 5 (4), DrlgType 3, LevelType 21, 384 x 384 at (6000, 3000).
	const std::string levels =
	    "Name\tId\tAct\tDrlgType\tLevelType\tSizeX\tSizeY\tOffsetX\tOffsetY\n"
	    "Cartographer Outdoor\t139\t4\t3\t21\t384\t384\t6000\t3000\n";
	Report report;
	auto slots = LoadSlots("Slot\tLevel\nOutdoor\t139\n", report);
	ValidateSlots(slots, levels, report);
	ReadSlotLevelTypes(slots, levels);
	REQUIRE(slots.size() == 1);
	CHECK(report.Errors() == 0);
	CHECK(OutdoorSlotRefusal(slots[0]).empty());
	CHECK(slots[0].haveGeometry);
	CHECK(slots[0].sizeX == 384);
	CHECK(slots[0].offsetX == 6000);
	CHECK(slots[0].offsetY == 3000);
	MapDef jungle;
	jungle.name = "Wild";
	jungle.tileset = "Jungle";
	CHECK(OutdoorSlotProblem(jungle, slots).empty());
	jungle.tileset = "Snow";  // a Snow map needs the OutdoorSnow slot, not this one
	CHECK(OutdoorSlotProblem(jungle, slots).empty());
	CHECK(std::string(MapSlotName(jungle)) == "OutdoorSnow");
	jungle.tileset = "Jungle";
	slots[0].levelType = 31;  // the Outdoor slot on a Snow LevelType: refused for a Jungle map
	CHECK(!OutdoorSlotProblem(jungle, slots).empty());
}

TEST_CASE("the OutdoorSnow slot must be the level the installer wrote, and the installer's Snow level passes the slot checks") {
	// The rows tools\install.ps1 writes: Jungle 139 (LevelType 21, 384) and Snow 140 (LevelType 31, 192 x 192 at (6000, 3600)).
	const std::string levels =
	    "Name\tId\tAct\tDrlgType\tLevelType\tSizeX\tSizeY\tOffsetX\tOffsetY\n"
	    "Cartographer Maze\t138\t4\t1\t3\t360\t360\t6000\t0\n"
	    "Cartographer Outdoor\t139\t4\t3\t21\t384\t384\t6000\t3000\n"
	    "Cartographer Snow\t140\t4\t3\t31\t192\t192\t6000\t3600\n";
	Report report;
	auto slots = LoadSlots("Slot\tLevel\nMaze\t138\nOutdoor\t139\nOutdoorSnow\t140\n", report);
	ValidateSlots(slots, levels, report);
	ReadSlotLevelTypes(slots, levels);
	REQUIRE(slots.size() == 3);
	CHECK(report.Errors() == 0);
	CHECK(SlotLevel(slots, "OutdoorSnow") == 140);
	CHECK(OutdoorSlotRefusal(slots[2]).empty());
	CHECK(slots[2].levelType == 31);
	CHECK(slots[2].sizeX == 192);
	CHECK(slots[2].offsetY == 3600);
	MapDef snow;
	snow.name = "Frost";
	snow.tileset = "Snow";
	CHECK(OutdoorSlotProblem(snow, slots).empty());
	snow.tileset = "Jungle";
	CHECK(OutdoorSlotProblem(snow, slots).empty());

	// The Snow slot on the Jungle level, or the Outdoor slot on the Snow level, is dropped with the name the slot needs.
	Report swapped;
	auto crossed = LoadSlots("Slot\tLevel\nOutdoorSnow\t139\nOutdoor\t140\n", swapped);
	ValidateSlots(crossed, levels, swapped);
	CHECK(crossed.empty());
	CHECK(swapped.Errors() == 2);
	CHECK(Joined(swapped).find("Cartographer Snow") != std::string::npos);
	CHECK(Joined(swapped).find("Cartographer Outdoor") != std::string::npos);

	// A vanilla level or a non-Act 5 level under the Snow name is refused by the same rule as the Outdoor slot, naming the slot.
	auto other = LoadSlots("Slot\tLevel\nOutdoorSnow\t117\n", report);
	ReadSlotLevelTypes(other, "Id\tAct\n117\t4\n");
	CHECK(OutdoorSlotRefusal(other[0]).find("OutdoorSnow") != std::string::npos);
	CHECK(OutdoorSlotRefusal(other[0]).find("vanilla") != std::string::npos);
	CHECK(IsOutdoorSlotName("outdoorsnow"));
	CHECK(OutdoorSlotLevelName("Maze").empty());
}

TEST_CASE("the tileset names, the ready list and the outdoor slot helpers follow the outdoor tileset table")
{
	const auto all = TilesetNames();
	REQUIRE(all.size() == 17);
	CHECK(all[13] == "Temple");
	CHECK(all[14] == "Jungle");  // the outdoor rows come last, in table order
	CHECK(all[15] == "Snow");
	CHECK(all[16] == "Siege");
	const auto ready = ReadyTilesetNames();
	const std::vector<std::string> want = {"Cave", "Crypt", "Lava", "Ice", "Jungle", "Snow", "Siege"};
	CHECK(ready == want);
	CHECK(OutdoorLevelType("Jungle") == 21);
	CHECK(OutdoorLevelType("snow") == 31);
	CHECK(OutdoorLevelType("Siege") == 30);
	CHECK(OutdoorSlotLevelName("outdoorsiege") == "Cartographer Siege");
	CHECK(OutdoorSlotLevelName("Outdoor") == "Cartographer Outdoor");
	CHECK(OutdoorSlotLevelName("outdoorsnow") == "Cartographer Snow");
	CHECK(IsOutdoorSlotName("Outdoor"));
	CHECK_FALSE(IsOutdoorSlotName("Maze"));
	CHECK_FALSE(HasOutdoorSlot({}));
	Slot maze;
	maze.name = "Maze";
	maze.level = 138;
	Slot snow;
	snow.name = "OutdoorSnow";
	snow.level = 140;
	CHECK_FALSE(HasOutdoorSlot({maze}));
	CHECK(HasOutdoorSlot({maze, snow}));
}

TEST_CASE("Size accepts ExtraSmall, and Density, Champions and Uniques have no 1000 cap") {
	Report report;
	const auto maps = LoadMaps(
		"Name\tTileset\tShape\tSize\tItem\tDensity\tChampions\tUniques\n"
		"A\tCave\tCorridor\tExtraSmall\tt20\t5000\t2000\t1000000\n"
		"B\tCave\tCorridor\textrasmall\tt21\t2147483647\t\t\n"
		"C\tCave\tCorridor\t\tt22\t-1\t\t\n"
		"D\tCave\tCorridor\t\tt23\t2147483648\t\t\n"
		"E\tCave\tCorridor\t\tt24\t99999999999999999999\t\t\n",
		Names(), report);
	REQUIRE(maps.size() == 2);
	CHECK(maps[0].size == "ExtraSmall");
	CHECK(maps[0].density == 5000);
	CHECK(maps[0].champions == 2000);
	CHECK(maps[0].uniques == 1000000);
	CHECK(maps[1].size == "ExtraSmall");
	CHECK(maps[1].density == 2147483647);
	CHECK(Joined(report).find("Density '-1' must be a whole number, 0 or more") != std::string::npos);
	CHECK(Joined(report).find("Density '2147483648' must be") != std::string::npos);
	CHECK(Joined(report).find("Density '99999999999999999999' must be") != std::string::npos);
}

TEST_CASE("a shapes.txt shape that does not fit ExtraSmall is refused at load for an ExtraSmall row only") {
	Report shapeReport;
	const auto shapes = LoadShapes("Name\tBase\tMinPath\tMaxPath\nSnake\tWinding\t20\t24\n", shapeReport);
	REQUIRE(shapes.shapes.size() == 1);  // fine at Small, Medium and Large
	Report report;
	const auto maps = LoadMaps(
		"Name\tTileset\tShape\tSize\tItem\n"
		"Tiny\tCave\tSnake\tExtraSmall\tt20\n"
		"Roomy\tCave\tSnake\tSmall\tt21\n"
		"Plain\tCave\tWinding\tExtraSmall\tt22\n",
		Names(), report, &shapes);
	REQUIRE(maps.size() == 2);
	CHECK(maps[0].name == "Roomy");
	CHECK(maps[1].name == "Plain");
	CHECK(Joined(report).find("Snake") != std::string::npos);
	CHECK(Joined(report).find("does not fit Size ExtraSmall") != std::string::npos);
}

TEST_CASE("Cave, Crypt and Lava open through the Maze, MazeCrypt and MazeLava slots, each on the level the installer wrote") {
	Report report;
	const auto maps = Load("Deep\tCave\tLoop\t\t\tt21\nTomb\tCrypt\tLoop\t\t\tt23\n", report);
	REQUIRE(maps.size() == 2);
	CHECK(report.Errors() == 0);
	CHECK(std::string(MapSlotName(maps[0])) == "Maze");
	CHECK(std::string(MapSlotName(maps[1])) == "MazeCrypt");
	CHECK_FALSE(MapIsOutdoor(maps[1]));
	CHECK(IsMazeSlotName("MazeCrypt"));
	CHECK(IsMazeSlotName("maze"));
	CHECK_FALSE(IsMazeSlotName("Outdoor"));
	CHECK(MazeSlotLevelName("MazeCrypt") == "Cartographer Crypt");
	CHECK(IsMazeSlotName("MazeIce"));
	CHECK(MazeSlotLevelName("MazeIce") == "Cartographer Ice Caves");
	CHECK(MazeSlotLevelName("Maze") == "Cartographer Maze");
	CHECK(MazeSlotLevelName("Outdoor").empty());

	const std::string levels = "Name\tId\tLevelType\nCartographer Maze\t138\t3\nCartographer Crypt\t141\t4\nSomeone's Level\t150\t4\n";
	Report good;
	auto both = LoadSlots("Slot\tLevel\nMaze\t138\nMazeCrypt\t141\n", good);
	ValidateSlots(both, levels, good);
	CHECK(good.Errors() == 0);
	CHECK(SlotLevel(both, "Maze") == 138);
	CHECK(SlotLevel(both, "MazeCrypt") == 141);
	CHECK(HasMazeSlot(both));
	CHECK_FALSE(HasMazeSlot({}));

	Report crossed;  // the Crypt level under the Maze name, and a level of someone else under the Crypt name: both refused
	auto wrong = LoadSlots("Slot\tLevel\nMaze\t141\nMazeCrypt\t150\n", crossed);
	ValidateSlots(wrong, levels, crossed);
	CHECK(wrong.empty());
	CHECK(crossed.Errors() == 2);
	CHECK(Joined(crossed).find("Cartographer Crypt") != std::string::npos);
}

TEST_CASE("each maze slot reads the LevelType of its own level") {
	const std::string levels = "Name\tId\tLevelType\nCartographer Maze\t138\t3\nCartographer Crypt\t141\t4\n";
	Report r;
	auto slots = LoadSlots("Slot\tLevel\nMaze\t138\nMazeCrypt\t141\n", r);
	ValidateSlots(slots, levels, r);
	ReadSlotLevelTypes(slots, levels);
	REQUIRE(slots.size() == 2);
	CHECK(slots[0].levelType == 3);
	CHECK(slots[1].levelType == 4);
}

TEST_CASE("ParseItemSpells reads code, pSpell and compactsave from misc.txt text, the way the plugin does") {
	std::vector<std::pair<std::string, int>> spells;
	std::vector<std::string> compact;
	const std::string misc = "name\tcode\tpSpell\tcompactsave\r\nMap A\tt20\t16\t0\r\nMap B\tt21\t16\t1\r\nBlank\tt22\t\t\r\nNo code\t\t5\t1\r\n";
	REQUIRE(ParseItemSpells(misc, &spells, &compact));
	REQUIRE(spells.size() == 3);
	CHECK(spells[0] == std::make_pair(std::string("t20"), 16));
	CHECK(spells[1] == std::make_pair(std::string("t21"), 16));
	CHECK(spells[2] == std::make_pair(std::string("t22"), 0));
	REQUIRE(compact.size() == 1);
	CHECK(compact[0] == "t21");
	// A table without a compactsave column is read, with nothing compact; without pSpell it is not read.
	spells.clear();
	compact.clear();
	CHECK(ParseItemSpells("code\tpSpell\r\nt20\t16\r\n", &spells, &compact));
	CHECK(spells.size() == 1);
	CHECK(compact.empty());
	CHECK_FALSE(ParseItemSpells("code\tcompactsave\r\nt20\t1\r\n", &spells, &compact));
	// And the result drives the note LoadMaps prints.
	GameNames names = Names();
	REQUIRE(ParseItemSpells(misc, &names.itemSpells, &names.compactItems));
	names.haveItemSpells = true;
	names.itemCodes.push_back("t20");
	names.itemCodes.push_back("t21");
	Report report;
	const auto maps = LoadMaps(std::string(Header) + "A\tCave\tLoop\t\t\tt20\nB\tCave\tLoop\t\t\tt21\n", names, report);
	CHECK(maps.size() == 2);
	const std::string text = Joined(report);
	CHECK(text.find("Item 't21' has compactsave 1") != std::string::npos);
	CHECK(text.find("Item 't20' has compactsave") == std::string::npos);
}

TEST_CASE("CheckMonSpeedRows notes a mon-speed mod that reaches -100 or lower in an affix or unique row") {
	Report report;
	CheckMonSpeedRows("Name\tmod1code\tmod1min\tmod1max\tmod2code\tmod2min\tmod2max\r\nSlow\tmon-speed\t-100\t-50\t\t\t\r\nOk\tmon-speed\t-99\t-10\t\t\t\r\nLate\tmon-life\t10\t20\tmon-speed\t-300\t-200\r\nOther\tmon-life\t-500\t0\t\t\t\r\n",
	                  "magicprefix.txt", report);
	const std::string text = Joined(report);
	CHECK(report.problems.size() == 2);
	CHECK(report.Errors() == 0);
	CHECK(text.find("magicprefix.txt row 2: mon-speed goes down to -100") != std::string::npos);
	CHECK(text.find("row 4: mon-speed goes down to -300") != std::string::npos);
	Report unique;
	CheckMonSpeedRows("index\tprop1\tpar1\tmin1\tmax1\tprop2\tpar2\tmin2\tmax2\r\nU\tmon-life\t\t5\t5\tmon-speed\t\t-120\t-120\r\n", "uniqueitems.txt", unique);
	REQUIRE(unique.problems.size() == 1);
	CHECK(Joined(unique).find("mon-speed goes down to -120") != std::string::npos);
	Report reversed;  // max below min: the lower one counts; a capitalised code matches; cubemain.txt and setitems.txt columns are read
	CheckMonSpeedRows("Name\tmod1code\tmod1min\tmod1max\r\nR\tmon-speed\t-50\t-150\r\n", "magicsuffix.txt", reversed);
	CHECK(reversed.problems.size() == 1);
	Report cube;
	CheckMonSpeedRows("description\tmod 1\tmod 1 min\tmod 1 max\tb mod 2\tb mod 2 min\tb mod 2 max\r\nup\tMon-Speed\t-100\t-100\tmon-speed\t-20\t-130\r\nok\tmon-speed\t-20\t-20\t\t\t\r\n", "cubemain.txt", cube);
	CHECK(cube.problems.size() == 2);
	Report set;
	CheckMonSpeedRows("index\tprop1\tmin1\tmax1\taprop1a\tamin1a\tamax1a\r\nS\tmon-life\t1\t1\tmon-speed\t-200\t-200\r\n", "setitems.txt", set);
	CHECK(set.problems.size() == 1);
	Report none;
	CheckMonSpeedRows("Name\tmod1code\r\nx\tmon-speed\r\n", "magicsuffix.txt", none);  // no min or max column: nothing to read
	CHECK(none.problems.empty());
}
