// Host tests for the Cartographer apply plan. No game, no SDK: the code is pure C++.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <cstring>
#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include "statplan.h"

using namespace cartographer::statplan;

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

TEST_CASE("buildPlan keeps the first 16 player stats in plan order and says why")
{
	std::string text = "Stat\tSend Other\n";
	for (int i = 0; i < 20; ++i) text += "filler" + std::to_string(i) + "\t1\n"; // ids 0..19 include refused ones
	std::vector<Property> props;
	for (int i = 0; i < 18; ++i) {
		text += "capstat" + std::to_string(i) + "\t1\n";
		props.push_back({Target::Players, "capstat" + std::to_string(i), i + 1});
	}
	props.push_back({Target::Monsters, "capstat0", 5});
	const Plan plan = buildPlan(props, parseItemStatCost(text), true);
	REQUIRE(plan.players.size() == 16);
	for (size_t i = 0; i < 16; ++i) CHECK(plan.players[i].name == "capstat" + std::to_string(i));
	CHECK(plan.monsters.size() == 1);
	REQUIRE(plan.skipped.size() == 2);
	CHECK(plan.skipped[0].stat == "capstat16");
	CHECK(plan.skipped[1].stat == "capstat17");
	CHECK(plan.skipped[0].why == "the game sends at most 16 stats of a list to the character sheet");
	CHECK(plan.skipped[1].why == plan.skipped[0].why);
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

TEST_CASE("a player list still on the unit and carrying the plugin's fields comes off, even when the state lookup fails")
{
	CHECK(decideRemoval(true, true, true) == RemoveAction::StateOffDetachFree);
	CHECK(decideRemoval(true, false, true) == RemoveAction::DetachFree);  // finder lost it: still detached and freed
	CHECK(decideRemoval(false, true, true) == RemoveAction::Forget);      // not held by the unit: never touched
	CHECK(decideRemoval(false, false, true) == RemoveAction::Forget);
	CHECK(decideRemoval(false, false, false) == RemoveAction::Forget);
}

TEST_CASE("a list on the chain that is not the plugin's is forgotten, never freed")
{
	// The game freed ours and gave the address to one of its own lists on the same unit.
	CHECK(decideRemoval(true, false, false) == RemoveAction::Forget);
	CHECK(decideRemoval(true, true, false) == RemoveAction::Forget);
}

TEST_CASE("a list is the plugin's only with the flags and state read when it was attached")
{
	CHECK(ownsList(0, 77, 0, 77, 77));
	CHECK_FALSE(ownsList(2, 77, 0, 77, 77));   // an engine list (flags 2) that has our state
	CHECK_FALSE(ownsList(0, 12, 0, 77, 77));   // another state
	CHECK_FALSE(ownsList(0x80000000u, 77, 0, 77, 77));
	CHECK_FALSE(ownsList(0, 0, 0, 0, 0));      // no state recorded
	CHECK_FALSE(ownsList(0, 0xffffffffu, 0, 0xffffffffu, -1));
}

TEST_CASE("an attach that set flag bits still leaves the list owned, so the normal removal detaches it")
{
	// The attach set bits in the flags; the plugin stored them, so the same values read at removal match.
	PlayerLists lists;
	REQUIRE(lists.add(5, 0x5000, 77, 0x2004u, 77));
	CHECK(lists.flagsOf(5) == 0x2004u);
	CHECK(lists.rawStateOf(5) == 77u);
	CHECK(lists.flagsOf(9) == 0u);
	const bool owned = ownsList(0x2004u, 77, lists.flagsOf(5), lists.rawStateOf(5), lists.stateOf(5));
	CHECK(owned);
	CHECK(decideRemoval(true, true, owned) == RemoveAction::StateOffDetachFree);
	CHECK(decideRemoval(true, false, owned) == RemoveAction::DetachFree);
	// A game list at the reused address with other flags is not ours.
	CHECK_FALSE(ownsList(0, 77, lists.flagsOf(5), lists.rawStateOf(5), lists.stateOf(5)));
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
	CHECK(lists.stateOf(1) == 0);
	CHECK(lists.add(2, 0x2000, 39));
	CHECK(lists.stateOf(2) == 39); // the state is kept with the list
	CHECK(lists.stateOf(7) == 0);
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
	// The Save Param Bits rows of the vanilla 3.3.0 itemstatcost.txt: name and row id.
	const struct { const char *name; int id; } param[] = {
		{"item_addclassskills", 83}, {"item_nonclassskill", 97}, {"state", 98}, {"item_singleskill", 107}, {"item_elemskill", 126},
		{"item_aura", 151}, {"item_reanimate", 155}, {"attack_vs_montype", 179}, {"damage_vs_montype", 180}, {"item_addskill_tab", 188},
		{"item_skillonattack", 195}, {"item_skillonkill", 196}, {"item_skillondeath", 197}, {"item_skillonhit", 198},
		{"item_skillonlevelup", 199}, {"item_skillongethit", 201}, {"item_charged_skill", 204}};
	StatIds ids;
	ids.loaded = true;
	std::vector<Property> props;
	for (const auto &e : param) ids.byName.emplace_back(e.name, e.id);
	ids.byName.emplace_back("fireresist", 39);
	for (const Target t : {Target::Monsters, Target::Players}) {
		for (const auto &e : param) props.push_back({t, e.name, 5});
		props.push_back({t, "fireresist", 5});
	}
	const Plan plan = buildPlan(props, ids);
	CHECK(plan.monsters.size() == 1);
	CHECK(plan.players.size() == 1);
	REQUIRE(plan.skipped.size() == 2 * (sizeof(param) / sizeof(param[0])));
	for (const Skipped &s : plan.skipped) CHECK(s.why.find("skill or state number") != std::string::npos);
	for (const auto &e : param) {
		CHECK_FALSE(refusedReason(Target::Monsters, e.name, e.id).empty());
		CHECK_FALSE(refusedReason(Target::Players, "renamed_by_a_mod", e.id).empty()); // by id alone
	}
	CHECK(refusedReason(Target::Players, "fireresist", 39).empty());
	CHECK(refusedReason(Target::Monsters, "item_skillonxyz", 200).empty()); // the list is exact: no prefix match
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

// --- The state for the player's list (issue #104) -----------------------------------------------------------------------

namespace {

// A states table whose rows are all empty as the compiler stores them (cartographer/07-map-stats-parts.md): the lookups overlay1-4, castoverlay,
// removerlay, pgsvoverlay, stat, cltevent, skill and missile are 0xffff, itemtrans is the byte 0xff, the rest is 0 (also setfunc/remfunc).
std::vector<uint8_t> emptyStates(uint32_t count)
{
	std::vector<uint8_t> rows(static_cast<size_t>(count) * kStateRowSize, 0);
	for (uint32_t i = 0; i < count; ++i) {
		uint8_t *r = rows.data() + static_cast<size_t>(i) * kStateRowSize;
		const uint16_t key = static_cast<uint16_t>(i);
		std::memcpy(r, &key, sizeof(key));
		for (size_t off : {2, 4, 6, 8, 0xa, 0xc, 0xe, 0x18, 0x30, 0x38, 0x3a}) {
			r[off] = 0xff;
			r[off + 1] = 0xff;
		}
		r[0x2c] = 0xff;
	}
	return rows;
}

void setI16(std::vector<uint8_t> &rows, uint32_t state, size_t off, int16_t v)
{
	std::memcpy(rows.data() + static_cast<size_t>(state) * kStateRowSize + off, &v, sizeof(v));
}

std::vector<uint8_t> noSkills(uint32_t count)
{
	std::vector<uint8_t> rows(static_cast<size_t>(count) * kSkillRowSize, 0);
	for (uint32_t i = 0; i < count; ++i) {
		for (size_t off : {0xa0, 0xa2, 0xb4, 0x2d2, 0x2d4, 0x2d6}) {
			rows[static_cast<size_t>(i) * kSkillRowSize + off] = 0xff;
			rows[static_cast<size_t>(i) * kSkillRowSize + off + 1] = 0xff;
		}
	}
	return rows;
}

void setSkill(std::vector<uint8_t> &rows, uint32_t skill, size_t off, int16_t v)
{
	std::memcpy(rows.data() + static_cast<size_t>(skill) * kSkillRowSize + off, &v, sizeof(v));
}

} // namespace

TEST_CASE("pickPlayerState takes the lowest empty state above 0 that is not in the static list")
{
	auto states = emptyStates(40);
	const auto skills = noSkills(3);
	const StatePick pick = pickPlayerState(states.data(), 40, skills.data(), 3);
	CHECK(pick.state == 1);
	// 6 and 7 are in the static list: with 1 to 5 taken the next free one is 8.
	for (uint32_t s : {1u, 2u, 3u, 4u, 5u}) setI16(states, s, 0x18, 3);
	CHECK(pickPlayerState(states.data(), 40, skills.data(), 3).state == 8);
}

TEST_CASE("pickPlayerState skips rows with an overlay, stat, function, group, nosend or disguise bit")
{
	auto states = emptyStates(14);
	setI16(states, 1, 2, 5);     // overlay1
	setI16(states, 2, 0xe, 0);   // pgsvoverlay 0 is a real overlay
	setI16(states, 3, 0x18, 0);  // stat 0 is strength
	setI16(states, 4, 0x1a, 7);  // setfunc
	setI16(states, 5, 0x1c, 9);  // remfunc
	setI16(states, 8, 0x1e, 2);  // group
	states[9 * kStateRowSize + 0x10] = 1;   // nosend
	states[10 * kStateRowSize + 0x12] = 2;  // disguise
	const auto skills = noSkills(1);
	CHECK(pickPlayerState(states.data(), 14, skills.data(), 1).state == 12);
}

TEST_CASE("pickPlayerState: 0 in setfunc and remfunc is empty; a row with any flag bit set is never taken")
{
	auto states = emptyStates(12);
	setI16(states, 1, 0x1a, 0);
	setI16(states, 1, 0x1c, 0);
	states[1 * kStateRowSize + 0x11] = 0x10;  // bit 12 (curse)
	const auto skills = noSkills(1);
	CHECK(pickPlayerState(states.data(), 12, skills.data(), 1).state == 2);
}

TEST_CASE("pickPlayerState has no fallback: when only rows with something set are left it picks nothing")
{
	auto states = emptyStates(12);
	const auto skills = noSkills(1);
	states[1 * kStateRowSize + 0x11] = 0x10;  // flag bit 12 (curse)
	setI16(states, 2, 0x38, 5);               // skill
	setI16(states, 3, 0x3a, 5);               // missile
	states[4 * kStateRowSize + 0x3c] = 1;     // canstack
	states[5 * kStateRowSize + 0x10 + 5] = 1; // flag bit 40 (notondead)
	for (uint32_t s : {8u, 9u, 10u, 11u}) setI16(states, s, 2, 1);
	CHECK(pickPlayerState(states.data(), 12, skills.data(), 1).state == -1);
}

TEST_CASE("pickPlayerState skips a row with any of the function, colour, light, sound, item, graphics or vfx columns set")
{
	const auto skills = noSkills(1);
	struct Col { size_t off; size_t size; uint32_t value; };
	const Col cols[] = {
		{0x32, 2, 1}, {0x34, 2, 1}, {0x36, 2, 1},                          // clteventfunc, cltactivefunc, srvactivefunc
		{0x20, 1, 1}, {0x21, 1, 1}, {0x22, 1, 1}, {0x23, 1, 1}, {0x24, 1, 1}, // colorpri, colorshift, light r/g/b
		{0x26, 2, 3}, {0x28, 2, 3}, {0x2a, 2, 3}, {0x2e, 2, 3}, {0x30, 2, 3}, // onsound, offsound, itemtype, gfxclass, cltevent
		{0x2c, 1, 1}, {0x2d, 1, 1},                                         // itemtrans, gfxtype
		{0x40, 4, 1},                                                       // vfxweaponstate
	};
	for (const Col &c : cols) {
		auto states = emptyStates(8);
		std::memcpy(states.data() + 1 * kStateRowSize + c.off, &c.value, c.size);
		INFO("column at +" << c.off);
		CHECK(pickPlayerState(states.data(), 8, skills.data(), 1).state == 2);
	}
	// The compiled empty values are exact: sound and item columns are 0 (a negative one is not what the compiler writes), cltevent is
	// negative (0 is a real event), itemtrans is 0xff (0 is a real transform), gfxclass is a plain word (0 only).
	auto states = emptyStates(8);
	CHECK(pickPlayerState(states.data(), 8, skills.data(), 1).state == 1);
	setI16(states, 1, 0x26, -1);
	CHECK(pickPlayerState(states.data(), 8, skills.data(), 1).state == 2);
	states = emptyStates(8);
	setI16(states, 1, 0x30, 0);
	CHECK(pickPlayerState(states.data(), 8, skills.data(), 1).state == 2);
	states = emptyStates(8);
	states[1 * kStateRowSize + 0x2c] = 0;
	CHECK(pickPlayerState(states.data(), 8, skills.data(), 1).state == 2);
	states = emptyStates(8);
	setI16(states, 1, 0x2e, -1);
	CHECK(pickPlayerState(states.data(), 8, skills.data(), 1).state == 2);
}

namespace {

// A compiled states row as D2RLoader 1.3.1 stores an empty one (states.bin of build 93847, rows 39, 92, 116, 230, 231 predicted from
// states.txt and checked against the file): key, 14 bytes of 0xff (overlays), 8 zero flag bytes, stat 0xffff, 18 zero bytes
// (setfunc, remfunc, group, colour, light, sounds, itemtype), itemtrans 0xff, 3 zero bytes (gfxtype, gfxclass), cltevent 0xffff,
// 6 zero bytes (three functions), skill and missile 0xffff, 8 zero bytes (canstack, padding, vfxweaponstate).
std::vector<uint8_t> compiledEmptyRow(uint16_t key)
{
	std::vector<uint8_t> r(kStateRowSize, 0);
	r[0] = static_cast<uint8_t>(key & 0xff);
	r[1] = static_cast<uint8_t>(key >> 8);
	for (size_t i = 2; i < 16; ++i) r[i] = 0xff;
	r[0x18] = r[0x19] = 0xff;
	r[0x2c] = 0xff;
	r[0x30] = r[0x31] = 0xff;
	r[0x38] = r[0x39] = r[0x3a] = r[0x3b] = 0xff;
	return r;
}

} // namespace

TEST_CASE("pickPlayerState: the compiled empty rows of vanilla (39, 92, 116, 230, 231) qualify, and 39 is the pick")
{
	const std::vector<uint8_t> empty = compiledEmptyRow(0);
	const size_t sz = kStateRowSize;
	CHECK(empty.size() == sz);
	const auto skills = noSkills(1);
	for (uint16_t key : std::initializer_list<uint16_t>{39, 92, 116, 230, 231}) {
		const std::vector<uint8_t> row = compiledEmptyRow(key);
		// The same row alone in a table of two (row 0 is never taken): the pick is state 1.
		std::vector<uint8_t> table(2 * sz, 0);
		std::memcpy(table.data() + sz, row.data(), sz);
		INFO("key " << key);
		CHECK(pickPlayerState(table.data(), 2, skills.data(), 1).state == 1);
	}
	// A table with the vanilla layout of candidates: 39 is the lowest not static and not named; 92 is static.
	std::vector<uint8_t> table(232 * sz, 0);
	for (uint32_t s = 0; s < 232; ++s) {
		std::vector<uint8_t> row = compiledEmptyRow(static_cast<uint16_t>(s));
		row[0x10] = 1;  // nosend: everything else is taken
		std::memcpy(table.data() + s * sz, row.data(), sz);
	}
	for (uint32_t s : {92u, 116u, 230u, 231u}) {
		const std::vector<uint8_t> row = compiledEmptyRow(static_cast<uint16_t>(s));
		std::memcpy(table.data() + s * sz, row.data(), sz);
	}
	CHECK(pickPlayerState(table.data(), 232, skills.data(), 1).state == 116);  // 92 is in the static list
	const std::vector<uint8_t> row39 = compiledEmptyRow(39);
	std::memcpy(table.data() + 39 * sz, row39.data(), sz);
	CHECK(pickPlayerState(table.data(), 232, skills.data(), 1).state == 39);
}

TEST_CASE("explainNoPick names the first failing column, its offset and value, and counts the empty rows that are static or named")
{
	const size_t sz = kStateRowSize;
	std::vector<uint8_t> table(8 * sz, 0);
	for (uint32_t s = 0; s < 8; ++s) {
		const std::vector<uint8_t> row = compiledEmptyRow(static_cast<uint16_t>(s));
		std::memcpy(table.data() + s * sz, row.data(), sz);
	}
	auto skills = noSkills(1);
	setSkill(skills, 0, 0xa0, 3);          // state 3 is named by a skill
	table[1 * sz + 0x2c] = 0;              // itemtrans 0: the case the first game run hit, with every other column fine
	table[2 * sz + 0x12] = 0x02;           // disguise flag, and also
	setI16(table, 2, 0x38, 5);             // a skill
	table[4 * sz + 0x2c] = 0;              // itemtrans, and
	table[4 * sz + 0x3c] = 1;              // canstack
	table[5 * sz + 0x2c] = 0;
	table[6 * sz + 0x2c] = 0;
	table[7 * sz + 0x2c] = 0;
	const auto lines = explainNoPick(table.data(), 8, skills.data(), 1, 3);
	REQUIRE(lines.size() == 4);
	CHECK(lines[0].find("1 of 7 rows") == 0);       // only state 3 has every column empty
	CHECK(lines[0].find("1 named by a skills row") != std::string::npos);
	CHECK(lines[0].find("0 usable") != std::string::npos);
	// Static states (6, 7) are never listed; rows with the fewest failing columns come first, lowest state first on a tie.
	CHECK(lines[1] == "state 1: 1 column(s) not empty; first itemtrans at +0x2c = 0x00, empty is 0xff");
	CHECK(lines[2] == "state 5: 1 column(s) not empty; first itemtrans at +0x2c = 0x00, empty is 0xff");
	CHECK(lines[3].find("state 2: 2 column(s) not empty; first flags 16-23 at +0x12 = 0x02 (flag disguise)") == 0);
	CHECK(explainNoPick(nullptr, 8, skills.data(), 1).empty());
}

TEST_CASE("pickPlayerState skips the states the game sets by a fixed id, including 11 and 92")
{
	auto states = emptyStates(100);
	const auto skills = noSkills(1);
	for (uint32_t s = 1; s < 100; ++s) {
		if (s != 11 && s != 92) setI16(states, s, 2, 1);
	}
	CHECK(pickPlayerState(states.data(), 100, skills.data(), 1).state == -1);
}

TEST_CASE("pickPlayerState skips a state any skills row names, and returns -1 when nothing is left")
{
	auto states = emptyStates(10);
	auto skills = noSkills(2);
	setSkill(skills, 1, 0xb4, 1);   // passivestate
	setSkill(skills, 0, 0x2d6, 2);  // state3
	setSkill(skills, 0, 0xa0, 3);   // aurastate
	CHECK(pickPlayerState(states.data(), 10, skills.data(), 2).state == 4);
	for (uint32_t s : {4u, 5u, 8u, 9u}) setI16(states, s, 2, 1);
	CHECK(pickPlayerState(states.data(), 10, skills.data(), 2).state == -1);
	CHECK(pickPlayerState(nullptr, 10, skills.data(), 2).state == -1);
	CHECK(pickPlayerState(states.data(), 10, nullptr, 2).state == -1);
	CHECK(pickPlayerState(states.data(), 1, nullptr, 0).state == -1);
}

TEST_CASE("checkTableView accepts only the game's own table")
{
	int rows[4] = {};
	CHECK(checkTableView(rows, 232, kStateRowSize, rows, 232, kStateRowSize).empty());
	CHECK_FALSE(checkTableView(rows, 232, 0x40, rows, 232, kStateRowSize).empty());
	CHECK_FALSE(checkTableView(rows, 232, kStateRowSize, rows + 1, 232, kStateRowSize).empty());
	CHECK_FALSE(checkTableView(rows, 232, kStateRowSize, rows, 233, kStateRowSize).empty());
	CHECK_FALSE(checkTableView(nullptr, 0, kStateRowSize, nullptr, 0, kStateRowSize).empty());
}

TEST_CASE("itemStatsToProperties: monster-side item stats apply their vanilla stat to monsters, the rest are player stats, in the item's order")
{
	const StatIds ids = parseItemStatCost(
	    "Stat\nstrength\nitem_maxhp_percent\nfireresist\ncartographer_mon_life\ncartographer_mon_fire_res\n");  // ids 0 to 4, the row numbers
	const auto r = itemStatsToProperties({{3, 0, 30}, {2, 0, -20}, {0, 0, 7}, {4, 0, 25}}, ids);
	REQUIRE(r.properties.size() == 4);
	CHECK(r.dropped.empty());
	CHECK(r.properties[0] == cartographer::statplan::Property{Target::Monsters, "item_maxhp_percent", 30});
	CHECK(r.properties[1] == cartographer::statplan::Property{Target::Players, "fireresist", -20});
	CHECK(r.properties[2] == cartographer::statplan::Property{Target::Players, "strength", 7});
	CHECK(r.properties[3] == cartographer::statplan::Property{Target::Monsters, "fireresist", 25});
	// A vanilla stat on the item is the player's, never the monsters'.
	const auto plan = buildPlan(r.properties, ids);
	REQUIRE(plan.monsters.size() == 2);
	CHECK(plan.monsters[0].stat == 1);
	CHECK(plan.monsters[1].stat == 2);
	REQUIRE(plan.players.size() == 2);
}

TEST_CASE("itemStatsToProperties: a layer, a zero value and an unknown id are dropped with a line each")
{
	const StatIds ids = parseItemStatCost("Stat\nfireresist\ncartographer_mon_life\n");
	const auto r = itemStatsToProperties({{0, 3, -20}, {0, 0, 0}, {9999, 0, 1}, {1, 0, -5}}, ids);
	REQUIRE(r.properties.size() == 1);
	CHECK(r.properties[0] == cartographer::statplan::Property{Target::Monsters, "item_maxhp_percent", -5});
	REQUIRE(r.dropped.size() == 3);
	CHECK(r.dropped[0].find("layer") != std::string::npos);
	CHECK(r.dropped[1].find("value is 0") != std::string::npos);
	CHECK(r.dropped[2].find("no itemstatcost row has this id") != std::string::npos);
	CHECK(itemStatsToProperties({}, ids).properties.empty());
	// Fail open: with no stat table nothing can be named.
	const auto none = itemStatsToProperties({{0, 0, -20}}, StatIds{});
	CHECK(none.properties.empty());
	REQUIRE(none.dropped.size() == 1);
}

TEST_CASE("every monster-side map stat names a vanilla stat buildPlan accepts on monsters")
{
	StatIds ids;
	ids.loaded = true;
	int next = 100;  // clear of the ids buildPlan refuses
	for (const auto &m : monsterStats()) ids.byName.emplace_back(m.applied, next++);
	for (const auto &m : monsterStats()) {
		const auto plan = buildPlan({{Target::Monsters, m.applied, 10}}, ids);
		CHECK_MESSAGE(plan.monsters.size() == 1, m.applied);
	}
}

TEST_CASE("mapLevelFromItem: the item's stat is the map level, else the maps.txt Level")
{
	const StatIds ids = parseItemStatCost("Stat\t*ID\nstrength\t0\ncartographer_map_level\t400\n");
	const int ID = 1;
	REQUIRE(ids.find(kMapLevelStat) == 1);
	auto pick = mapLevelFromItem({{0, 0, 5}, {ID, 0, 92}}, ids, 85);
	CHECK(pick.fromItem);
	CHECK(pick.level == 92);
	CHECK(pick.note.empty());
	// No stat on the item (an item made before the stat existed): the fallback, with nothing to report.
	pick = mapLevelFromItem({{0, 0, 5}}, ids, 85);
	CHECK_FALSE(pick.fromItem);
	CHECK(pick.level == 85);
	CHECK(pick.note.empty());
	CHECK(mapLevelFromItem({}, ids, 85).level == 85);
	// The row is missing from itemstatcost.txt, or the table could not be read: the fallback.
	CHECK(mapLevelFromItem({{ID, 0, 92}}, testIds(), 85).level == 85);
	CHECK(mapLevelFromItem({{ID, 0, 92}}, StatIds{}, 85).level == 85);
	// A stat with a layer is another stat.
	CHECK(mapLevelFromItem({{ID, 3, 92}}, ids, 85).level == 85);
}

TEST_CASE("mapLevelFromItem: a value of 0 or less falls back, a value above 255 is cut, both with a note")
{
	const StatIds ids = parseItemStatCost("Stat\t*ID\ncartographer_map_level\t400\n");
	const int ID = 0;
	auto pick = mapLevelFromItem({{ID, 0, 0}}, ids, 70);
	CHECK_FALSE(pick.fromItem);
	CHECK(pick.level == 70);
	CHECK_FALSE(pick.note.empty());
	CHECK(mapLevelFromItem({{ID, 0, -4}}, ids, 70).level == 70);
	pick = mapLevelFromItem({{ID, 0, 300}}, ids, 70);
	CHECK(pick.fromItem);
	CHECK(pick.level == 255);
	CHECK_FALSE(pick.note.empty());
	CHECK(mapLevelFromItem({{ID, 0, 255}}, ids, 70).note.empty());
	CHECK(mapLevelFromItem({{ID, 0, 1}}, ids, 70).level == 1);
}

TEST_CASE("withoutMapLevel drops only the map-level stat")
{
	const StatIds ids = parseItemStatCost("Stat\t*ID\nstrength\t0\ncartographer_map_level\t400\n");
	const int ID = 1;
	const auto kept = withoutMapLevel({{0, 0, 5}, {ID, 0, 92}, {0, 0, 6}}, ids);
	REQUIRE(kept.size() == 2);
	CHECK(kept[0].value == 5);
	CHECK(kept[1].value == 6);
	CHECK(withoutMapLevel({{ID, 0, 92}}, StatIds{}).size() == 1);
}

TEST_CASE("isStaticState lists the fixed ids")
{
	for (int s : {0, 6, 7, 11, 18, 22, 24, 52, 62, 92, 100, 107, 109, 118, 154, 164, 165, 172, 184, 186, 189, 209, 219}) CHECK(isStaticState(s));
	for (int s : {1, 39, 108, 116, 230, 231}) CHECK_FALSE(isStaticState(s));
}

TEST_CASE("every monster-side map stat is a row the installer adds, with a description text")
{
	std::string here = __FILE__;
	std::ifstream in(here.substr(0, here.find_last_of("/\\") + 1) + "../tools/install.ps1", std::ios::binary);
	REQUIRE(in.good());
	std::stringstream all;
	all << in.rdbuf();
	for (const auto &m : monsterStats()) {
		CHECK_MESSAGE(all.str().find(std::string("'") + m.itemStat + "'") != std::string::npos, m.itemStat);
	}
}

TEST_CASE("the installer writes and repairs the Map type with Rare on and Magic and Normal off")
{
	std::string here = __FILE__;
	std::ifstream in(here.substr(0, here.find_last_of("/\\") + 1) + "../tools/install.ps1", std::ios::binary);
	REQUIRE(in.good());
	std::stringstream all;
	all << in.rdbuf();
	const std::string text = all.str();
	const std::string magicOff = "Set-CellIf $itemTypes $cells 'Magic' '0'";
	const size_t first = text.find(magicOff);
	REQUIRE(first != std::string::npos);
	CHECK(text.find(magicOff, first + 1) != std::string::npos);  // once when the row is added, once in the repair
	CHECK(text.find("Set-CellIf $itemTypes $cells 'Normal' '0'") != std::string::npos);
	CHECK(text.find("foreach ($col in 'Magic', 'Rare')") == std::string::npos);
}

TEST_CASE("decodeItemStatList reads D2RLoader's 16-byte entries and refuses any other layout")
{
	// The unique map from the check (RE note 14, section 16): {layer, stat, value, 0} seven times.
	const int32_t good[] = {0, 39, 15, 0, 0, 41, 15, 0, 0, 43, 15, 0, 0, 45, 15, 0, 0, 368, 95, 0, 0, 369, 30, 0, 0, 371, -20, 0};
	std::string problem;
	const auto ok = decodeItemStatList(good, 7, 16, &problem);
	REQUIRE(ok.size() == 7);
	CHECK(problem.empty());
	CHECK(ok[0].stat == 39);
	CHECK(ok[0].layer == 0);
	CHECK(ok[0].value == 15);
	CHECK(ok[6].stat == 371);
	CHECK(ok[6].value == -20);

	// Only the first maxStats entries are read.
	CHECK(decodeItemStatList(good, 7, 3, &problem).size() == 3);

	// 8-byte pairs {stat << 16 | layer, value}: the fourth int of an entry is not 0.
	const int32_t pairs[] = {(39 << 16), 15, (41 << 16), 15, (43 << 16), 15, (45 << 16), 15};
	CHECK(decodeItemStatList(pairs, 2, 16, &problem).empty());
	CHECK(problem.find("layout not recognised") != std::string::npos);

	// Junk: a stat id past kMaxStatId, a negative layer.
	problem.clear();
	const int32_t junk[] = {0, 39, 15, 0, 0, 99999, 1, 0};
	CHECK(decodeItemStatList(junk, 2, 16, &problem).empty());
	CHECK(problem.find("entry 1") != std::string::npos);
	problem.clear();
	const int32_t neg[] = {-1, 39, 15, 0};
	CHECK(decodeItemStatList(neg, 1, 16, &problem).empty());
	CHECK(!problem.empty());

	// Nothing to read is not a problem.
	problem.clear();
	CHECK(decodeItemStatList(good, 0, 16, &problem).empty());
	CHECK(problem.empty());
}

TEST_CASE("mapDensityBonus: the item's map-density values are added up over its affixes")
{
	const StatIds ids = parseItemStatCost("Stat\t*ID\nstrength\t0\ncartographer_map_density\t400\nitem_magicbonus\t80\n");
	const int D = 1, MF = 2;
	REQUIRE(ids.find(kMapDensityStat) == D);
	// Two affixes with density (20 and 35) and one with magic find: only density counts.
	CHECK(mapDensityBonus({{D, 0, 20}, {MF, 0, 15}, {D, 0, 35}}, ids) == 55);
	CHECK(mapDensityBonus({{D, 0, 20}, {D, 0, -50}}, ids) == -30);
	CHECK(mapDensityBonus({{MF, 0, 15}}, ids) == 0);
	CHECK(mapDensityBonus({}, ids) == 0);
	// A layer makes it another stat; a missing row or an unreadable table gives nothing.
	CHECK(mapDensityBonus({{D, 3, 20}}, ids) == 0);
	CHECK(mapDensityBonus({{D, 0, 20}}, testIds()) == 0);
	CHECK(mapDensityBonus({{D, 0, 20}}, StatIds{}) == 0);
	// Not capped at 1000: an item saves one value of -524288 to 524287 for the stat, so a sum of 500000 is within what a saved item holds.
	CHECK(mapDensityBonus({{D, 0, 200000}, {D, 0, 200000}, {D, 0, 100000}}, ids) == 500000);
}

TEST_CASE("densityWithBonus adds percentage points, uncapped, never below 0")
{
	CHECK(densityWithBonus(100, 55) == 155);
	CHECK(densityWithBonus(0, 40) == 40);
	CHECK(densityWithBonus(1000, 500) == 1500);
	CHECK(densityWithBonus(100, 0) == 100);
	CHECK(densityWithBonus(50, -80) == 0);
}

TEST_CASE("withoutMapDensity drops only the density stat, so it is not read as a player stat")
{
	const StatIds ids = parseItemStatCost("Stat\t*ID\nstrength\t0\ncartographer_map_density\t400\nitem_magicbonus\t80\n");
	const std::vector<ItemStat> stats = {{1, 0, 20}, {2, 0, 15}, {0, 0, 5}};
	const auto rest = withoutMapDensity(stats, ids);
	REQUIRE(rest.size() == 2);
	CHECK(rest[0].stat == 2);
	CHECK(rest[1].stat == 0);
	// The magic find left over is a player stat on the player under its own name.
	const auto mapped = itemStatsToProperties(rest, ids);
	REQUIRE(mapped.properties.size() == 2);
	CHECK(mapped.properties[0] == Property{Target::Players, "item_magicbonus", 15});
	CHECK(withoutMapDensity(stats, StatIds{}).size() == 3);
}
