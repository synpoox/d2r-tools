// Host tests for the Cartographer event plan core: spawn name resolution, placement, the event room, the spawn plan, the
// entry message and the banner timing. No game, no SDK.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <set>
#include <string>
#include <utility>
#include <vector>

#include "eventplan.h"
#include "events.h"

using namespace cartographer::eventplan;
namespace ev = cartographer::events;

namespace {

SpawnTable table() {
	SpawnTable t;
	t.monsters = {"skeleton1", "", "zombie1", "andariel", "bishibosh_base"};
	t.supers = {{"Bishibosh", "bishibosh_base"}, {"Corpsefire", "zombie1"}, {"Lost", "nosuchclass"}};
	return t;
}

ev::Event event(ev::Kind kind, const char *spawn) {
	ev::Event e;
	e.kind = kind;
	e.name = "The Drowned King";
	e.message = "Something stirs.";
	e.spawn = spawn;
	e.minions = 3;
	e.hordeSize = 5;
	return e;
}

bool inside(const Spot &s, const Rect &r) { return s.x >= r.x && s.y >= r.y && s.x < r.x + r.w && s.y < r.y + r.h; }

} // namespace

TEST_CASE("resolveSpawn: monstats, then superuniques, exact then any case") {
	const SpawnTable t = table();
	Resolved r = resolveSpawn(t, "zombie1");
	CHECK(r.kind == ev::NameKind::Monster);
	CHECK(r.row == 2);
	CHECK(r.classRow == 2);
	r = resolveSpawn(t, "Bishibosh");
	CHECK(r.kind == ev::NameKind::Unique);
	CHECK(r.row == 0);
	CHECK(r.classRow == 4);
	r = resolveSpawn(t, "ANDARIEL");
	CHECK(r.kind == ev::NameKind::Monster);
	CHECK(r.row == 3);
	r = resolveSpawn(t, "corpsefire");
	CHECK(r.kind == ev::NameKind::Unique);
	CHECK(r.classRow == 2);
	CHECK(resolveSpawn(t, "nothing").kind == ev::NameKind::Unresolved);
	CHECK(resolveSpawn(t, "").kind == ev::NameKind::Unresolved);
	CHECK(resolveSpawn(t, "Lost").classRow == -1);
}

TEST_CASE("resolveSpawn: an empty monstats cell keeps its row and never matches") {
	const SpawnTable t = table();
	CHECK(resolveSpawn(t, "bishibosh_base").row == 4);
	SpawnTable e;
	e.monsters = {"", "a"};
	CHECK(resolveSpawn(e, "").kind == ev::NameKind::Unresolved);
	CHECK(resolveSpawn(e, "a").row == 1);
}

TEST_CASE("placements: centre first, distinct, inside, deterministic, capped by capacity") {
	const Rect room{100, 200, 20, 16};
	for (int n : {1, 2, 5, 12, 40}) {
		const std::vector<Spot> a = placements(room, n, 7), b = placements(room, n, 7);
		CHECK(!a.empty());
		CHECK(static_cast<int>(a.size()) <= n);
		CHECK(a.size() == b.size());
		std::set<std::pair<int, int>> seen;
		for (size_t i = 0; i < a.size(); ++i) {
			CHECK(a[i].x == b[i].x);
			CHECK(a[i].y == b[i].y);
			CHECK(inside(a[i], room));
			CHECK(seen.insert({a[i].x, a[i].y}).second);
		}
		CHECK(a[0].x == 110);
		CHECK(a[0].y == 208);
	}
	CHECK(placements(room, 12, 1).size() == 12);
	CHECK(placements(room, 0, 1).empty());
	CHECK(placements({0, 0, 0, 5}, 3, 1).empty());
	const std::vector<Spot> tiny = placements({10, 10, 3, 3}, 50, 1);
	CHECK(tiny.size() <= 4);
	for (const Spot &s : tiny) {
		CHECK(inside(s, {10, 10, 3, 3}));
	}
}

TEST_CASE("EventRoomTracker: a planned room fires only on that rect, once") {
	EventRoomTracker t(Rect{10, 20, 8, 8});
	CHECK(t.planned());
	CHECK_FALSE(t.onPopulated({0, 0, 8, 8}));
	CHECK_FALSE(t.onPopulated({10, 20, 8, 9}));
	CHECK(t.onPopulated({10, 20, 8, 8}));
	CHECK_FALSE(t.onPopulated({10, 20, 8, 8}));
	CHECK(t.fired());
	CHECK(t.populated() == 4);
}

TEST_CASE("EventRoomTracker: with no plan the first room is the entry and a far enough later room fires") {
	EventRoomTracker t(std::nullopt, 24);
	CHECK_FALSE(t.planned());
	CHECK_FALSE(t.onPopulated({0, 0, 8, 8}));    // entry
	CHECK_FALSE(t.onPopulated({8, 0, 8, 8}));    // 8 away
	CHECK_FALSE(t.onPopulated({0, 16, 8, 8}));   // 16 away
	CHECK(t.onPopulated({24, 0, 8, 8}));         // 24 away
	CHECK_FALSE(t.onPopulated({48, 0, 8, 8}));   // already fired
}

TEST_CASE("EventRoomTracker: the distance is between centres, either axis") {
	EventRoomTracker t(std::nullopt, 10);
	CHECK_FALSE(t.onPopulated({0, 0, 4, 4}));
	CHECK_FALSE(t.onPopulated({0, 8, 4, 4}));
	CHECK(t.onPopulated({-10, 0, 4, 4}));
}

TEST_CASE("planEvent: a Boss is the lead plus minions, centre first") {
	const Rect room{0, 0, 20, 20};
	ev::Event e = event(ev::Kind::Boss, "Bishibosh");
	SpawnPlan p = planEvent(e, resolveSpawn(table(), "Bishibosh"), room, 1);
	REQUIRE(p.problem.empty());
	REQUIRE(p.spawns.size() == 4);
	CHECK(p.spawns[0].superUnique);
	CHECK(p.spawns[0].row == 0);
	CHECK(p.spawns[0].classRow == 4);
	CHECK(p.spawns[0].spot.x == 10);
	CHECK(p.spawns[0].spot.y == 10);
	for (size_t i = 1; i < p.spawns.size(); ++i) {
		CHECK_FALSE(p.spawns[i].superUnique);
		CHECK(p.spawns[i].row == 4);
	}
	e.spawn = "andariel";
	p = planEvent(e, resolveSpawn(table(), "andariel"), room, 1);
	REQUIRE(p.spawns.size() == 4);
	CHECK_FALSE(p.spawns[0].superUnique);
	CHECK(p.spawns[0].row == 3);
}

TEST_CASE("planEvent: a Horde is hordeSize of the monster, a superunique spawns as its class") {
	const Rect room{0, 0, 30, 30};
	ev::Event e = event(ev::Kind::Horde, "Bishibosh");
	SpawnPlan p = planEvent(e, resolveSpawn(table(), "Bishibosh"), room, 5);
	REQUIRE(p.problem.empty());
	CHECK(p.spawns.size() == 5);
	for (const Spawn &s : p.spawns) {
		CHECK_FALSE(s.superUnique);
		CHECK(s.row == 4);
	}
	e.hordeSize = 0;
	CHECK(!planEvent(e, resolveSpawn(table(), "Bishibosh"), room, 5).problem.empty());
}

TEST_CASE("planEvent: problems are said, not guessed") {
	const Rect room{0, 0, 20, 20};
	CHECK(planEvent(event(ev::Kind::Boss, "nothing"), resolveSpawn(table(), "nothing"), room, 1).problem.find("not a monstats Id") != std::string::npos);
	CHECK(planEvent(event(ev::Kind::Boss, "Lost"), resolveSpawn(table(), "Lost"), room, 1).problem.find("Class") != std::string::npos);
	CHECK(planEvent(event(ev::Kind::Ambush, "andariel"), resolveSpawn(table(), "andariel"), room, 1).problem.find("not applied yet") != std::string::npos);
	CHECK(planEvent(event(ev::Kind::Shrine, "andariel"), resolveSpawn(table(), "andariel"), room, 1).problem.find("not applied yet") != std::string::npos);
	CHECK(!planEvent(event(ev::Kind::Boss, "andariel"), resolveSpawn(table(), "andariel"), {0, 0, 0, 0}, 1).problem.empty());
}

TEST_CASE("superSeen reads bit (row & 7) of byte (row >> 3)") {
	uint8_t map[4] = {0x01, 0x00, 0x80, 0x00};
	CHECK(superSeen(map, 0));
	CHECK_FALSE(superSeen(map, 1));
	CHECK_FALSE(superSeen(map, 8));
	CHECK(superSeen(map, 23));
	CHECK_FALSE(superSeen(map, 22));
	CHECK_FALSE(superSeen(nullptr, 0));
	CHECK_FALSE(superSeen(map, -1));
}

TEST_CASE("entryLines: the map name, then the event name and message") {
	ev::Event e = event(ev::Kind::Boss, "andariel");
	CHECK(entryLines("Crypt of Bones", nullptr) == std::vector<std::string>{"Crypt of Bones"});
	CHECK(entryLines("Crypt of Bones", &e) == std::vector<std::string>{"Crypt of Bones", "The Drowned King", "Something stirs."});
	e.message.clear();
	CHECK(entryLines("Crypt of Bones", &e).size() == 2);
	CHECK(entryLines("", nullptr).empty());
}

TEST_CASE("EntryTracker fires once per arrival") {
	EntryTracker t;
	CHECK_FALSE(t.update(false));
	CHECK(t.update(true));
	CHECK_FALSE(t.update(true));
	CHECK_FALSE(t.update(false));
	CHECK(t.update(true));
	t.reset();
	CHECK(t.update(true));
}

TEST_CASE("bannerAlpha fades in, holds, fades out, then is 0") {
	CHECK(bannerAlpha(-1) == 0);
	CHECK(bannerAlpha(0) == 0);
	CHECK(bannerAlpha(kBannerFadeInMs / 2) > 100);
	CHECK(bannerAlpha(kBannerFadeInMs / 2) < 160);
	CHECK(bannerAlpha(kBannerFadeInMs) == 255);
	CHECK(bannerAlpha(kBannerFadeInMs + kBannerHoldMs - 1) == 255);
	CHECK(bannerAlpha(kBannerTotalMs - 1) < 5);
	CHECK(bannerAlpha(kBannerTotalMs) == 0);
	int last = 0;
	for (int ms = 0; ms < kBannerFadeInMs; ++ms) {
		CHECK(bannerAlpha(ms) >= last);
		last = bannerAlpha(ms);
	}
}

TEST_CASE("eventSeed is stable and differs from the map seed") {
	CHECK(eventSeed(1) == eventSeed(1));
	CHECK(eventSeed(1) != eventSeed(2));
	CHECK(eventSeed(12345) != 12345u);
}
