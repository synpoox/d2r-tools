// Host tests for the Cartographer map-events core. No game, no SDK: plain structs in, a decided event out.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <cstdio>
#include <string>
#include <vector>

#include "events.h"
#include "layout.h"
#include "rng.h"

using namespace cartographer::events;
namespace lay = cartographer::layout;

namespace {

constexpr int kSeeds = 100000;

Row row(const char *name, Kind kind, const char *spawn, NameKind sk = NameKind::Unresolved)
{
	Row r;
	r.name = name;
	r.kind = kind;
	r.spawn = spawn;
	r.spawnKind = sk;
	return r;
}

// One row of each of the five kinds, with every parameter the plan lists.
std::vector<Row> fiveKinds()
{
	std::vector<Row> rows;
	Row boss = row("The Drowned King", Kind::Boss, "Bishibosh", NameKind::Unique);
	boss.count = 6;
	boss.drop = "Act 5 (H) Super C";
	boss.message = "The water churns...";
	rows.push_back(boss);
	Row amb = row("Ambush!", Kind::Ambush, "zombie3", NameKind::Monster);
	amb.count = 8;
	amb.waves = 4;
	rows.push_back(amb);
	Row horde = row("Swarm", Kind::Horde, "quillrat1", NameKind::Monster);
	horde.count = 20;
	rows.push_back(horde);
	Row tr = row("Loot Goblin", Kind::Treasure, "goblin", NameKind::Monster);
	tr.drop = "Act 5 (H) Super C";
	rows.push_back(tr);
	Row shrine = row("Old Well", Kind::Shrine, "well1", NameKind::Object);
	rows.push_back(shrine);
	return rows;
}

const lay::Point kRoom{3, 2};

std::string fingerprint(const std::optional<Event> &e)
{
	return e ? describe(*e) + (e->text.empty() ? "" : " | " + e->text) : std::string("none");
}

} // namespace

TEST_CASE("event rate follows EventChance over many seeds")
{
	const std::vector<Row> rows = fiveKinds();
	for (int chance : {0, 20, 50, 100}) {
		int fired = 0;
		for (int s = 0; s < kSeeds; ++s)
			if (decide(rows, chance, lay::splitmix64(static_cast<uint64_t>(s)), &kRoom)) ++fired;
		const double pct = 100.0 * fired / kSeeds;
		std::printf("EventChance %d: %d of %d fired (%.2f%%)\n", chance, fired, kSeeds, pct);
		if (chance == 0) CHECK(fired == 0);
		else if (chance == 100) CHECK(fired == kSeeds);
		else {
			CHECK(pct > chance - 1.0);
			CHECK(pct < chance + 1.0);
		}
	}
}

TEST_CASE("the default EventChance is 20 and a per-map value wins")
{
	CHECK(kDefaultEventChance == 20);
	CHECK(effectiveEventChance(std::nullopt) == 20);
	CHECK(effectiveEventChance(std::nullopt, 30) == 30);
	CHECK(effectiveEventChance(25, 30) == 25);
	CHECK(effectiveEventChance(0, 30) == 0);
	CHECK(effectiveEventChance(250) == 100);
	CHECK(effectiveEventChance(-5) == 0);
	CHECK(validateEventChance(101) != "");
	CHECK(validateEventChance(-1) != "");
	CHECK(validateEventChance(0) == "");
	CHECK(validateEventChance(100) == "");
}

TEST_CASE("each of the five kinds is selected and parameterised")
{
	const std::vector<Row> rows = fiveKinds();
	int seen[kKindCount + 1] = {};
	for (int s = 0; s < 5000; ++s) {
		auto e = decide(rows, 100, static_cast<uint64_t>(s), &kRoom, 4);
		REQUIRE(e);
		CHECK(e->room == kRoom);
		CHECK(e->roomIdx == 4);
		CHECK(e->rowIndex >= 0);
		CHECK(e->kind == rows[static_cast<size_t>(e->rowIndex)].kind);
		++seen[static_cast<int>(e->kind)];
		switch (e->kind) {
		case Kind::Boss:
			CHECK(e->name == "The Drowned King");
			CHECK(e->spawn == "Bishibosh");
			CHECK(e->spawnKind == NameKind::Unique);
			CHECK(e->minions == 6);
			CHECK(e->drop == "Act 5 (H) Super C");
			CHECK(e->text == "The Drowned King\nThe water churns...");
			CHECK(e->waves == 0);
			break;
		case Kind::Ambush:
			CHECK(e->waveSize == 8);
			CHECK(e->waves == 4);
			CHECK(e->minions == 0);
			CHECK(e->text == "Ambush!");
			break;
		case Kind::Horde:
			CHECK(e->hordeSize == 20);
			CHECK(e->waves == 0);
			break;
		case Kind::Treasure:
			CHECK(e->treasure == TreasureForm::Monster);
			CHECK(e->drop == "Act 5 (H) Super C");
			break;
		case Kind::Shrine:
			CHECK(e->spawn == "well1");
			CHECK(e->minions == 0);
			CHECK(e->hordeSize == 0);
			break;
		default: FAIL("invalid kind drawn");
		}
	}
	for (int k = 1; k <= kKindCount; ++k) CHECK_MESSAGE(seen[k] > 700, "kind " << k << " drawn " << seen[k]);
}

TEST_CASE("table defaults: Count 1, Waves 3, Weight 1")
{
	std::vector<Row> rows{row("a", Kind::Boss, "x"), row("b", Kind::Ambush, "y"), row("c", Kind::Horde, "z")};
	std::vector<Row> only;
	for (const Row &r : rows) {
		only = {r};
		auto e = decide(only, 100, 1, &kRoom);
		REQUIRE(e);
		CHECK(e->minions + e->waveSize + e->hordeSize == 1);
		if (r.kind == Kind::Ambush) CHECK(e->waves == kDefaultWaves);
	}
}

TEST_CASE("Treasure is a fleeing monster or a chest, by what Spawn names")
{
	auto form = [](NameKind k) {
		auto e = decide({row("t", Kind::Treasure, "s", k)}, 100, 7, &kRoom);
		REQUIRE(e);
		return e->treasure;
	};
	CHECK(form(NameKind::Monster) == TreasureForm::Monster);
	CHECK(form(NameKind::Unique) == TreasureForm::Monster);
	CHECK(form(NameKind::Object) == TreasureForm::Chest);
	CHECK(form(NameKind::Unresolved) == TreasureForm::Unresolved);
}

TEST_CASE("mixed Spawn name kinds are representable; a mismatch with the Kind is rejected")
{
	// Both kinds of name are accepted where the plan allows them ...
	CHECK(validateRow(row("a", Kind::Boss, "Bishibosh", NameKind::Unique), 2).empty());
	CHECK(validateRow(row("a", Kind::Boss, "fallenshaman", NameKind::Monster), 2).empty());
	CHECK(validateRow(row("a", Kind::Boss, "Bishibosh", NameKind::Unresolved), 2).empty());
	CHECK(validateRow(row("a", Kind::Treasure, "chest3", NameKind::Object), 2).empty());
	CHECK(validateRow(row("a", Kind::Shrine, "well1", NameKind::Object), 2).empty());
	// ... and a resolved name of the wrong sort is not.
	CHECK(!validateRow(row("a", Kind::Shrine, "zombie3", NameKind::Monster), 2).empty());
	CHECK(!validateRow(row("a", Kind::Horde, "well1", NameKind::Object), 2).empty());
	CHECK(!validateRow(row("a", Kind::Boss, "well1", NameKind::Object), 2).empty());
}

TEST_CASE("Weight steers the draw")
{
	Row a = row("a", Kind::Horde, "x");
	a.weight = 3;
	Row b = row("b", Kind::Shrine, "w", NameKind::Object);
	b.weight = 1;
	const std::vector<Row> rows{a, b};
	int na = 0;
	for (int s = 0; s < kSeeds; ++s) {
		auto e = decide(rows, 100, lay::splitmix64(static_cast<uint64_t>(s)), &kRoom);
		REQUIRE(e);
		if (e->rowIndex == 0) ++na;
	}
	const double pct = 100.0 * na / kSeeds;
	CHECK(pct > 74.0);
	CHECK(pct < 76.0);
}

TEST_CASE("bad rows are rejected with a plain message and never crash decide")
{
	std::vector<Row> bad;
	bad.push_back(row("", Kind::Boss, "x"));                  // no name
	bad.push_back(row("n", Kind::Invalid, "x"));              // no kind
	bad.push_back(row("n", Kind::Boss, ""));                  // no spawn
	bad.push_back(row("n", Kind::Boss, "   "));               // blank spawn
	Row c = row("n", Kind::Horde, "x");
	c.count = 0;
	bad.push_back(c);
	c.count = -3;
	bad.push_back(c);
	c.count = 100000;
	bad.push_back(c);
	Row w = row("n", Kind::Ambush, "x");
	w.waves = 0;
	bad.push_back(w);
	w.waves = 9999;
	bad.push_back(w);
	Row wt = row("n", Kind::Shrine, "x", NameKind::Object);
	wt.weight = 0;
	bad.push_back(wt);
	wt.weight = -1;
	bad.push_back(wt);
	wt.weight = 1 << 30;
	bad.push_back(wt);

	const auto errors = validateRows(bad);
	CHECK(errors.size() == bad.size());
	for (const auto &e : errors) CHECK(e.rfind("mapevents.txt line ", 0) == 0);
	CHECK(errors[0].find("line 2") != std::string::npos);

	// Nothing usable: never an event, never a crash, for any chance or seed.
	for (int s = 0; s < 1000; ++s) CHECK(!decide(bad, 100, static_cast<uint64_t>(s), &kRoom));
	CHECK(!decide({}, 100, 1, &kRoom));
	CHECK(!decide(bad, -50, 1, &kRoom));
	CHECK(!decide(bad, 5000, 1, &kRoom));

	// Bad rows beside a good one: the good one is the only draw.
	bad.push_back(row("good", Kind::Horde, "x"));
	for (int s = 0; s < 200; ++s) {
		auto e = decide(bad, 100, static_cast<uint64_t>(s), &kRoom);
		REQUIRE(e);
		CHECK(e->name == "good");
		CHECK(e->rowIndex == static_cast<int>(bad.size()) - 1);
	}
}

TEST_CASE("chance outside 0..100 is clamped, and no room means no event")
{
	const std::vector<Row> rows = fiveKinds();
	for (int s = 0; s < 200; ++s) {
		CHECK(decide(rows, 1000, static_cast<uint64_t>(s), &kRoom));
		CHECK(!decide(rows, -1000, static_cast<uint64_t>(s), &kRoom));
		CHECK(!decide(rows, 100, static_cast<uint64_t>(s), nullptr));
	}
	lay::Layout none; // eventIdx -1: the layout reserved no event room
	CHECK(!decide(rows, 100, none));
}

TEST_CASE("kind names parse case-insensitively")
{
	CHECK(parseKindName("boss") == Kind::Boss);
	CHECK(parseKindName(" AMBUSH ") == Kind::Ambush);
	CHECK(parseKindName("Horde") == Kind::Horde);
	CHECK(parseKindName("treasure") == Kind::Treasure);
	CHECK(parseKindName("SHRINE") == Kind::Shrine);
	CHECK(!parseKindName("").has_value());
	CHECK(!parseKindName("Trap").has_value());
}

TEST_CASE("decisions are deterministic and independent of the table object")
{
	const std::vector<Row> a = fiveKinds();
	const std::vector<Row> b = fiveKinds();
	for (int s = 0; s < 2000; ++s)
		CHECK(fingerprint(decide(a, 50, static_cast<uint64_t>(s), &kRoom)) ==
		      fingerprint(decide(b, 50, static_cast<uint64_t>(s), &kRoom)));
}

TEST_CASE("events take the reserved event room from a generated layout")
{
	const std::vector<Row> rows = fiveKinds();
	int fired = 0;
	for (int s = 0; s < 500; ++s) {
		const lay::Layout l = lay::generate(lay::Shape::Winding, lay::Size::Medium, static_cast<uint64_t>(s));
		REQUIRE(l.eventIdx >= 0);
		auto e = decide(rows, 100, l);
		REQUIRE(e);
		CHECK(e->roomIdx == l.eventIdx);
		CHECK(e->room == l.mainPath[static_cast<size_t>(l.eventIdx)]);
		CHECK(l.at(e->room.x, e->room.y).role == lay::Role::Event);
		fired += 1;
		// Same map seed, same event.
		CHECK(fingerprint(e) == fingerprint(decide(rows, 100, l)));
	}
	CHECK(fired == 500);
}

TEST_CASE("golden picks are stable")
{
	const std::vector<Row> rows = fiveKinds();
	const char *expected[] = {
	    "Horde 'Swarm' (quillrat1) x20 at (3,2)",
	    "Shrine 'Old Well' (well1) at (3,2)",
	    "Treasure 'Loot Goblin' (goblin) at (3,2)",
	    "Boss 'The Drowned King' (Bishibosh) minions 6 at (3,2)",
	    "Ambush 'Ambush!' (zombie3) 4 waves of 8 at (3,2)",
	    "Treasure 'Loot Goblin' (goblin) at (3,2)",
	    "Boss 'The Drowned King' (Bishibosh) minions 6 at (3,2)",
	    "Treasure 'Loot Goblin' (goblin) at (3,2)",
	};
	for (int s = 0; s < 8; ++s) {
		const auto e = decide(rows, 100, static_cast<uint64_t>(s), &kRoom);
		REQUIRE(e);
		const std::string got = describe(*e);
		std::printf("golden seed %d: %s\n", s, got.c_str());
		CHECK(got == expected[s]);
	}
	// At the default 20% chance, which of seeds 0..39 fire (so the roll itself is pinned too).
	std::string fires;
	for (int s = 0; s < 40; ++s) fires += decide(rows, 20, static_cast<uint64_t>(s), &kRoom) ? '1' : '0';
	std::printf("fires20: %s\n", fires.c_str());
	CHECK(fires == "0000010010010100000000000110100000100001");
}
