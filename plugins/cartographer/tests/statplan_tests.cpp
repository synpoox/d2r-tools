// Host tests for the Cartographer apply plan. No game, no SDK: the code is pure C++.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <set>
#include <string>
#include <vector>

#include "statplan.h"

using namespace cartographer::statplan;
using cartographer::stats::MapRow;
using cartographer::stats::Settings;
using cartographer::stats::drawMapStats;

namespace {

const char *kItemStatCost =
	"Stat\t*ID\tSend Other\r\n"
	"strength\t0\t1\r\n"
	"item_maxhp_percent\t76\t1\r\n"
	"fireresist\t39\t1\r\n"
	"hpregen\t74\t1\r\n"
	"\r\n"
	"damagepercent\t25\t1\r\n";

StatIds testIds()
{
	return parseItemStatCost(kItemStatCost);
}

} // namespace

TEST_CASE("itemstatcost: ids are row numbers, blank lines are skipped")
{
	const StatIds ids = testIds();
	CHECK(ids.loaded);
	CHECK(ids.find("item_maxhp_percent") == 1);
	CHECK(ids.find("fireresist") == 2);
	CHECK(ids.find("damagepercent") == 4); // a blank line is not a row
	CHECK(ids.find("nope") == -1);
	CHECK(ids.find("FIRERESIST") == -1); // names must match exactly
}

TEST_CASE("itemstatcost: the *ID column is a comment, the row number wins (vanilla row 212 says 213)")
{
	std::string text = "Stat\t*ID\n";
	for (int i = 0; i < 212; ++i) text += "filler" + std::to_string(i) + "\t" + std::to_string(i) + "\n";
	text += "passive_mastery_gethit_rate\t213\n"; // row 212
	text += "passive_mastery_attack_speed\t213\n"; // row 213
	const StatIds ids = parseItemStatCost(text);
	CHECK(ids.find("passive_mastery_gethit_rate") == 212);
	CHECK(ids.find("passive_mastery_attack_speed") == 213);
}

TEST_CASE("itemstatcost: without an *ID column the row number is the id")
{
	const StatIds ids = parseItemStatCost("Stat\tSend Other\nstrength\t1\nenergy\t1\n\t1\ndexterity\t1\n");
	CHECK(ids.loaded);
	CHECK(ids.find("strength") == 0);
	CHECK(ids.find("energy") == 1);
	CHECK(ids.find("dexterity") == 3); // the nameless row still takes its number
}

TEST_CASE("itemstatcost: no Stat column means not loaded")
{
	CHECK_FALSE(parseItemStatCost("Name\tID\nx\t1\n").loaded);
	CHECK_FALSE(parseItemStatCost("").loaded);
}

TEST_CASE("suggest finds a near name and nothing for a far one")
{
	const StatIds ids = testIds();
	CHECK(ids.suggest("fireresits") == "fireresist");
	CHECK(ids.suggest("zzzzzzzz").empty());
}

TEST_CASE("buildPlan splits by target and resolves ids")
{
	const std::vector<Property> props = {
		{Target::Monsters, "item_maxhp_percent", 30},
		{Target::Players, "fireresist", -20},
		{Target::Monsters, "damagepercent", 15},
	};
	const Plan plan = buildPlan(props, testIds());
	REQUIRE(plan.monsters.size() == 2);
	REQUIRE(plan.players.size() == 1);
	CHECK(plan.monsters[0].stat == 1);
	CHECK(plan.monsters[0].value == 30);
	CHECK(plan.monsters[1].stat == 4);
	CHECK(plan.players[0].stat == 2);
	CHECK(plan.players[0].value == -20);
	CHECK(plan.skipped.empty());
	CHECK(describe(plan) == "monsters: item_maxhp_percent(1)=+30, damagepercent(4)=+15; players: fireresist(2)=-20");
}

TEST_CASE("buildPlan skips what it cannot resolve, with a reason")
{
	const std::vector<Property> props = {
		{Target::Monsters, "fireresits", 10}, // typo
		{Target::Players, "hpregen", 0},      // rolled 0
		{Target::Players, "strength", 5},
	};
	const Plan plan = buildPlan(props, testIds());
	CHECK(plan.monsters.empty());
	REQUIRE(plan.players.size() == 1);
	REQUIRE(plan.skipped.size() == 2);
	CHECK(plan.skipped[0].why.find("Did you mean 'fireresist'") != std::string::npos);
	CHECK(plan.skipped[1].why.find("0") != std::string::npos);
}

TEST_CASE("buildPlan with no itemstatcost applies nothing")
{
	const Plan plan = buildPlan({{Target::Monsters, "fireresist", 10}}, StatIds{});
	CHECK(plan.empty());
	REQUIRE(plan.skipped.size() == 1);
	CHECK(describe(plan) == "no stats");
}

TEST_CASE("buildPlan refuses a stat id above the limit")
{
	StatIds ids;
	ids.loaded = true;
	ids.byName = {{"huge", kMaxStatId + 1}, {"ok", kMaxStatId}};
	const Plan plan = buildPlan({{Target::Monsters, "huge", 1}, {Target::Monsters, "ok", 1}}, ids);
	CHECK(plan.monsters.size() == 1);
	CHECK(plan.skipped.size() == 1);
}

TEST_CASE("a drawn map yields the same plan for the same seed")
{
	const std::vector<PoolRow> pool = {
		{Target::Monsters, "item_maxhp_percent", 10, 20, 1},
		{Target::Monsters, "damagepercent", 10, 20, 1},
		{Target::Players, "fireresist", -30, -10, 1},
		{Target::Players, "hpregen", -30, -10, 1},
	};
	Settings s;
	s.properties = 3;
	const StatIds ids = testIds();
	const Plan a = buildPlan(drawMapStats(42, MapRow{}, s, pool), ids);
	const Plan b = buildPlan(drawMapStats(42, MapRow{}, s, pool), ids);
	CHECK(describe(a) == describe(b));
	CHECK(a.monsters.size() + a.players.size() + a.skipped.size() == 3);
	for (const Entry &e : a.monsters) CHECK(e.value >= 10);
	for (const Entry &e : a.players) CHECK(e.value <= -10);
}

TEST_CASE("checkPoolStats warns about unknown stats, only for rows that can be drawn")
{
	const std::vector<PoolRow> pool = {
		{Target::Monsters, "fireresist", 1, 2, 1},
		{Target::Monsters, "fireresits", 1, 2, 1},
		{Target::Monsters, "madeup", 1, 2, 0},  // off: never drawn
		{Target::Monsters, "madeup", 1, 2, -1}, // error row: already reported
	};
	const auto issues = checkPoolStats(pool, testIds());
	REQUIRE(issues.size() == 1);
	CHECK(issues[0].row == 1);
	CHECK_FALSE(issues[0].error);
	CHECK(issues[0].message.find("fireresist") != std::string::npos);
	CHECK(checkPoolStats(pool, StatIds{}).empty()); // table not read: unchecked
}

TEST_CASE("the player's list goes on in the map, off when it no longer applies")
{
	CHECK(decidePlayer(true, true, false, true) == PlayerAction::Apply);
	CHECK(decidePlayer(true, true, true, true) == PlayerAction::None);
	CHECK(decidePlayer(true, false, true, true) == PlayerAction::Remove);  // left the level
	CHECK(decidePlayer(false, true, true, true) == PlayerAction::Remove);  // map closed
	CHECK(decidePlayer(false, false, true, true) == PlayerAction::Remove);
	CHECK(decidePlayer(true, false, false, true) == PlayerAction::None);
	CHECK(decidePlayer(false, true, false, true) == PlayerAction::None);   // a map with no player stats
	// PlayerStats switched to 0 on reload: nothing new, and a list that is on comes off while still in the map
	CHECK(decidePlayer(true, true, false, false) == PlayerAction::None);
	CHECK(decidePlayer(true, true, true, false) == PlayerAction::Remove);
}

TEST_CASE("monsters get stats only in the slot level while a map is active")
{
	CHECK(monsterGetsStats(true, 7, 7));
	CHECK_FALSE(monsterGetsStats(true, 8, 7));
	CHECK_FALSE(monsterGetsStats(false, 7, 7));
	CHECK_FALSE(monsterGetsStats(true, 0, 0));   // no slot installed
	CHECK_FALSE(monsterGetsStats(true, -1, -1));
}

TEST_CASE("PlayerLists tracks, replaces, removes and fills up")
{
	PlayerLists lists;
	CHECK(lists.empty());
	CHECK_FALSE(lists.add(1, 0)); // a null list is not a list
	CHECK(lists.add(1, 0x1000));
	CHECK(lists.add(2, 0x2000));
	CHECK(lists.find(1) == 0x1000);
	CHECK(lists.find(3) == 0);
	CHECK(lists.add(1, 0x1800)); // same unit: replaced, not added
	CHECK(lists.find(1) == 0x1800);
	CHECK(lists.size() == 2);
	lists.remove(1);
	CHECK(lists.find(1) == 0);
	CHECK(lists.size() == 1);
	lists.clear();
	CHECK(lists.empty());
	for (uint32_t i = 0; i < PlayerLists::kMax; ++i) CHECK(lists.add(i + 1, 0x100 + i));
	CHECK_FALSE(lists.add(999, 0x9999));
	CHECK(lists.find(999) == 0);
}

TEST_CASE("mixSeed differs by clock, player and opening count")
{
	std::set<uint64_t> seen;
	for (uint64_t clock : {1ull, 2ull})
		for (uint32_t player : {10u, 11u})
			for (uint32_t opening : {0u, 1u}) seen.insert(mixSeed(clock, player, opening));
	CHECK(seen.size() == 8);
	CHECK(mixSeed(5, 6, 7) == mixSeed(5, 6, 7));
}

TEST_CASE("buildPlan refuses player stats the game bakes into the saved base value")
{
	StatIds ids;
	ids.loaded = true;
	ids.byName = {{"statpts", 4}, {"newskills", 5}, {"hitpoints", 6}, {"maxhp", 7}, {"mana", 8}, {"stamina", 10}, {"level", 12},
	              {"experience", 13}, {"gold", 14}, {"goldbank", 15}, {"fireresist", 39}, {"strength", 0}, {"renamed", 14}};
	std::vector<Property> props;
	for (const char *n : {"statpts", "newskills", "hitpoints", "mana", "stamina", "level", "experience", "gold", "goldbank", "renamed"})
		props.push_back({Target::Players, n, 5});
	props.push_back({Target::Players, "maxhp", 5});    // an item stat: allowed
	props.push_back({Target::Players, "strength", 5}); // an item stat: allowed
	props.push_back({Target::Players, "fireresist", 5});
	const Plan plan = buildPlan(props, ids);
	CHECK(plan.players.size() == 3);
	REQUIRE(plan.skipped.size() == 10);
	for (const Skipped &s : plan.skipped) {
		CHECK(s.why.find("refused on players") != std::string::npos);
		CHECK(s.why.find("'" + s.stat + "'") != std::string::npos);
	}
}

TEST_CASE("on monsters level and the current values are refused, the rest is fine")
{
	StatIds ids;
	ids.loaded = true;
	ids.byName = {{"gold", 14}, {"level", 12}, {"hitpoints", 6}, {"mana", 8}, {"stamina", 10}, {"maxhp", 7}, {"renamed", 6}};
	const Plan plan = buildPlan({{Target::Monsters, "gold", 5}, {Target::Monsters, "maxhp", 5}, {Target::Monsters, "level", 5},
	                             {Target::Monsters, "hitpoints", 5}, {Target::Monsters, "mana", 5}, {Target::Monsters, "stamina", 5},
	                             {Target::Monsters, "renamed", 5}}, ids);
	CHECK(plan.monsters.size() == 2);
	REQUIRE(plan.skipped.size() == 5);
	for (const Skipped &s : plan.skipped) CHECK(s.why.find("refused on monsters") != std::string::npos);
	CHECK(plan.skipped[1].why.find("current value") != std::string::npos);
	CHECK(refusedReason(Target::Players, "fireresist", 39).empty());
	CHECK_FALSE(refusedReason(Target::Monsters, "hitpoints", 6).empty());
	CHECK_FALSE(refusedReason(Target::Players, "x", 12).empty()); // by id alone
}

TEST_CASE("stats that need a skill or state number are refused on both targets")
{
	StatIds ids;
	ids.loaded = true;
	ids.byName = {{"state", 100}, {"item_aura", 101}, {"item_charged_skill", 102}, {"item_skillonhit", 103}, {"item_skillondeath", 104}, {"fireresist", 39}};
	std::vector<Property> props;
	for (const Target t : {Target::Monsters, Target::Players})
		for (const char *n : {"state", "item_aura", "item_charged_skill", "item_skillonhit", "item_skillondeath", "fireresist"}) props.push_back({t, n, 5});
	const Plan plan = buildPlan(props, ids);
	CHECK(plan.monsters.size() == 1);
	CHECK(plan.players.size() == 1);
	REQUIRE(plan.skipped.size() == 10);
	for (const Skipped &s : plan.skipped) CHECK(s.why.find("skill or state number") != std::string::npos);
}

TEST_CASE("checkPoolStats names each refused row")
{
	StatIds ids;
	ids.loaded = true;
	ids.byName = {{"gold", 14}, {"fireresist", 39}};
	const std::vector<PoolRow> pool = {
		{Target::Players, "fireresist", 1, 2, 1},
		{Target::Players, "gold", 1, 2, 1},
		{Target::Monsters, "gold", 1, 2, 1}, // fine on monsters
		{Target::Players, "gold", 1, 2, 0},  // off: not reported
	};
	const auto issues = checkPoolStats(pool, ids);
	REQUIRE(issues.size() == 1);
	CHECK(issues[0].row == 1);
	CHECK(issues[0].message.find("gold") != std::string::npos);
	CHECK(issues[0].message.find("refused on players") != std::string::npos);
}

TEST_CASE("PlayerStats off: no player entry is planned, monsters still are")
{
	const std::vector<Property> props = {{Target::Monsters, "damagepercent", 15}, {Target::Players, "fireresist", -20}};
	const Plan off = buildPlan(props, testIds(), false);
	CHECK(off.monsters.size() == 1);
	CHECK(off.players.empty());
	REQUIRE(off.skipped.size() == 1);
	CHECK(off.skipped[0].why.find("PlayerStats is 0") != std::string::npos);
	CHECK(buildPlan(props, testIds()).players.size() == 1);
}
