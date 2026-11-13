// Host tests for the Cartographer event plan core: spawn name resolution, placement, the event room, the spawn plan, the
// entry message and the banner timing. No game, no SDK.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <algorithm>
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
	CHECK(tiny.size() == 9);
	for (const Spot &s : tiny) {
		CHECK(inside(s, {10, 10, 3, 3}));
	}
}

TEST_CASE("placements: an outdoor 8 x 8 tile event room (40 x 40 units) holds 4 at the usual spacing and packs tighter for more") {
	const Rect room{3000, 1500, 40, 40};
	CHECK(placements(room, 4, 3, 5).size() == 4);
	for (int n : {6, 8, 12}) {
		const std::vector<Spot> p = placements(room, n, 3, 5);
		CHECK(static_cast<int>(p.size()) == n);
		std::set<std::pair<int, int>> seen;
		for (const Spot &s : p) {
			CHECK(inside(s, room));
			CHECK(seen.insert({s.x, s.y}).second);
		}
		CHECK(p[0].x == 3020);
		CHECK(p[0].y == 1520);
	}
	const Rect cell{3000, 1500, 160, 160};  // the 32 x 32 tile cell
	CHECK(placements(cell, 6, 3, 5).size() == 6);
	CHECK(placements(room, 100, 3, 5).size() == 36);  // never more than the room holds: 6 x 6 at one tile
}

TEST_CASE("planEvent: a Horde of 6 in an outdoor event room gets 6 spots and records the count asked for") {
	ev::Event e = event(ev::Kind::Horde, "Bishibosh");
	e.hordeSize = 6;
	SpawnPlan p = planEvent(e, resolveSpawn(table(), "Bishibosh"), {3000, 1500, 40, 40}, 1, 5);
	CHECK(p.problem.empty());
	CHECK(p.requested == 6);
	CHECK(p.spawns.size() == 6);
	e.hordeSize = 5000;
	p = planEvent(e, resolveSpawn(table(), "Bishibosh"), {3000, 1500, 40, 40}, 1, 5);
	CHECK(p.requested == 5000);
	CHECK(p.spawns.size() < 5000);
}

TEST_CASE("spotScale: server units per DRLG tile") {
	CHECK(spotScale({0, 0, 100, 80}, {0, 0, 20, 16}) == 5);
	CHECK(spotScale({0, 0, 20, 16}, {0, 0, 20, 16}) == 1);
	CHECK(spotScale({0, 0, 100, 40}, {0, 0, 20, 16}) == 1);  // not one factor in both directions
	CHECK(spotScale({0, 0, 101, 80}, {0, 0, 20, 16}) == 1);
	CHECK(spotScale({0, 0, 10, 10}, {0, 0, 20, 16}) == 1);
	CHECK(spotScale({0, 0, 10, 10}, {0, 0, 0, 0}) == 1);
}

TEST_CASE("placements: a scaled (server) rect keeps the same spacing on the ground, in its own units") {
	const Rect tiles{100, 200, 20, 16}, sub{500, 1000, 100, 80};
	const std::vector<Spot> a = placements(tiles, 12, 7, 1), b = placements(sub, 12, 7, 5);
	REQUIRE(a.size() == b.size());
	CHECK(b[0].x == 550);
	CHECK(b[0].y == 1040);
	for (const Spot &s : b) {
		CHECK(inside(s, sub));
	}
	for (size_t i = 0; i < b.size(); ++i) {
		for (size_t j = i + 1; j < b.size(); ++j) {
			CHECK(std::max(std::abs(b[i].x - b[j].x), std::abs(b[i].y - b[j].y)) >= kSpotGap * 5);
		}
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

TEST_CASE("minEventDistance: a third of the larger side, 1 to 24, 24 when the size is unknown") {
	CHECK(minEventDistance(20, 28) == 9);
	CHECK(minEventDistance(28, 20) == 9);
	CHECK(minEventDistance(60, 40) == 20);
	CHECK(minEventDistance(200, 200) == kMinEventDistance);
	CHECK(minEventDistance(2, 1) == 1);
	CHECK(minEventDistance(0, 0) == kMinEventDistance);
	CHECK(minEventDistance(-5, 0) == kMinEventDistance);
}

TEST_CASE("EventRoomTracker: a 20 x 28 map fires at the scaled distance, not at 24") {
	CHECK_FALSE(EventRoomTracker(std::nullopt, kMinEventDistance).onPopulated({0, 0, 8, 4}));
	EventRoomTracker t(std::nullopt, minEventDistance(20, 28));
	CHECK_FALSE(t.onPopulated({0, 0, 8, 4}));
	CHECK_FALSE(t.onPopulated({4, 0, 8, 4}));  // 4 away
	CHECK(t.onPopulated({12, 0, 8, 4}));       // 12 away
}

TEST_CASE("farthestRoom: the room farthest from the entry room, so a small map always has one") {
	const Rect entry{8, 8, 8, 4};
	const std::vector<Rect> rooms{{8, 8, 8, 4}, {0, 8, 8, 4}, {16, 8, 8, 4}, {8, 24, 8, 4}, {8, 16, 8, 4}};
	const auto farthest = farthestRoom(entry, rooms);
	REQUIRE(farthest.has_value());
	CHECK(farthest->y == 24);
	CHECK_FALSE(farthestRoom(entry, {}).has_value());
	CHECK_FALSE(farthestRoom(entry, {entry}).has_value());
	CHECK_FALSE(farthestRoom(entry, {{40, 40, 0, 4}}).has_value());  // no size
	// a tie on the Chebyshev distance goes to the longer straight line, then to the earlier room
	const auto t = farthestRoom({0, 0, 2, 2}, {{10, 0, 2, 2}, {10, 6, 2, 2}});
	REQUIRE(t.has_value());
	CHECK(t->y == 6);
	// equal on both: the earlier room in the list wins, whichever way round
	const auto first = farthestRoom({0, 0, 2, 2}, {{10, 6, 2, 2}, {6, 10, 2, 2}});
	REQUIRE(first.has_value());
	CHECK(first->x == 10);
	const auto second = farthestRoom({0, 0, 2, 2}, {{6, 10, 2, 2}, {10, 6, 2, 2}});
	REQUIRE(second.has_value());
	CHECK(second->x == 6);
}

TEST_CASE("presetEventDistance: capped at the farthest room, so a small level still has a qualifying room") {
	const Rect entry{8, 8, 8, 4};
	const std::vector<Rect> rooms{{8, 8, 8, 4}, {0, 8, 8, 4}, {8, 24, 8, 4}};
	// small level: the farthest room is 16 away, below minEventDistance(90, 90) = 24, so capped at 16
	CHECK(minEventDistance(90, 90) == kMinEventDistance);
	CHECK(presetEventDistance(90, 90, entry, rooms) == 16);
	EventRoomTracker t(std::nullopt, presetEventDistance(90, 90, entry, rooms));
	CHECK_FALSE(t.onPopulated(entry));
	CHECK_FALSE(t.onPopulated({0, 8, 8, 4}));  // 8 away: too near
	CHECK(t.onPopulated({8, 24, 8, 4}));       // the farthest room qualifies
	// big level: the normal threshold applies (9 for 20 x 28) when the farthest room is beyond it, and a nearer room fires first
	const std::vector<Rect> wide{{8, 8, 8, 4}, {8, 20, 8, 4}, {8, 60, 8, 4}};
	CHECK(presetEventDistance(20, 28, entry, wide) == 9);
	EventRoomTracker u(std::nullopt, presetEventDistance(20, 28, entry, wide));
	CHECK_FALSE(u.onPopulated(entry));
	CHECK(u.onPopulated({8, 20, 8, 4}));  // 12 away, not the farthest room
	// the room list could not be read (or holds only the entry): the plain distance
	CHECK(presetEventDistance(20, 28, entry, {}) == 9);
	CHECK(presetEventDistance(20, 28, entry, {entry}) == 9);
	CHECK(presetEventDistance(0, 0, entry, {}) == kMinEventDistance);
}

TEST_CASE("EventRoomTracker: the map Boss's tracker, made from the plan's boss room, fires only there, once") {
	EventRoomTracker t(Rect{40, 40, 8, 8});
	CHECK_FALSE(t.onPopulated({8, 24, 8, 4}));
	CHECK(t.onPopulated({40, 40, 8, 8}));
	CHECK_FALSE(t.onPopulated({40, 40, 8, 8}));
}

TEST_CASE("planMapBoss: the centre first and then spots around it, a superunique as itself, no minions") {
	const Rect room{0, 0, 20, 20};
	SpawnPlan p = planMapBoss("Bishibosh", resolveSpawn(table(), "Bishibosh"), room);
	REQUIRE(p.problem.empty());
	REQUIRE(p.spawns.size() > 1);  // the centre first, then spots around it (issue #183)
	CHECK(p.spawns[0].superUnique);
	CHECK(p.spawns[0].row == 0);
	CHECK(p.spawns[0].classRow == 4);
	CHECK(p.spawns[0].spot.x == 10);
	CHECK(p.spawns[0].spot.y == 10);
	p = planMapBoss("andariel", resolveSpawn(table(), "andariel"), room);
	REQUIRE(p.spawns.size() > 1);
	CHECK_FALSE(p.spawns[0].superUnique);
	CHECK(p.spawns[0].row == 3);
	// a server room rect: the same spacing rule as the events (the centre of a 40 x 20 rect)
	p = planMapBoss("andariel", resolveSpawn(table(), "andariel"), Rect{100, 50, 40, 20}, 5);
	REQUIRE(p.spawns.size() > 1);
	CHECK(p.spawns[0].spot.x == 120);
	CHECK(p.spawns[0].spot.y == 60);
	// problems are said
	CHECK_FALSE(planMapBoss("Nobody", resolveSpawn(table(), "Nobody"), room).problem.empty());
	Resolved noClass;
	noClass.kind = ev::NameKind::Unique;
	noClass.row = 2;
	CHECK_FALSE(planMapBoss("X", noClass, room).problem.empty());
	CHECK_FALSE(planMapBoss("andariel", resolveSpawn(table(), "andariel"), Rect{0, 0, 0, 0}).problem.empty());
}

TEST_CASE("bossSpots: the centre first, then distinct spots inside the room nearest the centre first (issue #183)") {
	const Rect room{30000, 120, 40, 40};  // a server rect: 8 x 8 tiles at 5 units a tile
	const std::vector<Spot> s = bossSpots(room, 5);
	REQUIRE(s.size() == static_cast<size_t>(kBossSpots));
	CHECK(s[0].x == 30020);
	CHECK(s[0].y == 140);
	long last = 0;
	for (size_t i = 0; i < s.size(); ++i) {
		CHECK(s[i].x >= room.x + 5);
		CHECK(s[i].x < room.x + room.w - 5 + 1);
		CHECK(s[i].y >= room.y + 5);
		CHECK(s[i].y < room.y + room.h - 5 + 1);
		const long d = static_cast<long>(s[i].x - s[0].x) * (s[i].x - s[0].x) + static_cast<long>(s[i].y - s[0].y) * (s[i].y - s[0].y);
		if (i > 1) CHECK(d >= last);
		if (i >= 1) last = d;
		for (size_t j = 0; j < i; ++j) CHECK((s[i].x != s[j].x || s[i].y != s[j].y));
	}
	CHECK(bossSpots(Rect{0, 0, 0, 0}, 1).empty());
	CHECK(bossSpots(Rect{0, 0, 2, 2}, 1).size() >= 1);  // a tiny room still has its centre
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

TEST_CASE("planEvent: an Ambush plans one wave, a Treasure monster plans the one monster") {
	const Rect room{0, 0, 30, 30};
	ev::Event e = event(ev::Kind::Ambush, "andariel");
	e.waveSize = 4;
	e.waves = 3;
	SpawnPlan p = planEvent(e, resolveSpawn(table(), "andariel"), room, 5);
	REQUIRE(p.problem.empty());
	CHECK(p.spawns.size() == 4);  // one wave, not waves x waveSize
	for (const Spawn &s : p.spawns) {
		CHECK_FALSE(s.superUnique);
		CHECK(s.row == 3);
	}
	// A superunique in an Ambush spawns as its class (it can only exist once per game).
	p = planEvent(e, resolveSpawn(table(), "Bishibosh"), room, 5);
	REQUIRE(p.problem.empty());
	for (const Spawn &s : p.spawns) {
		CHECK_FALSE(s.superUnique);
		CHECK(s.row == 4);
	}
	e.kind = ev::Kind::Treasure;
	p = planEvent(e, resolveSpawn(table(), "andariel"), room, 5);
	REQUIRE(p.problem.empty());
	CHECK(p.spawns.size() == 1);
	CHECK_FALSE(p.spawns[0].superUnique);
	p = planEvent(e, resolveSpawn(table(), "Bishibosh"), room, 5);
	REQUIRE(p.spawns.size() == 1);
	CHECK(p.spawns[0].superUnique);  // a named superunique is spawned as one, like a Boss
	CHECK(p.spawns[0].classRow == 4);
	// A Treasure whose Spawn is not a monster (a chest) is not planned here.
	CHECK(planEvent(e, resolveSpawn(table(), "Casket"), room, 5).problem.find("not a monstats Id") != std::string::npos);
	e.kind = ev::Kind::Ambush;
	e.waveSize = 0;
	CHECK(!planEvent(e, resolveSpawn(table(), "andariel"), room, 5).problem.empty());
}

TEST_CASE("planEvent: problems are said, not guessed") {
	const Rect room{0, 0, 20, 20};
	CHECK(planEvent(event(ev::Kind::Boss, "nothing"), resolveSpawn(table(), "nothing"), room, 1).problem.find("not a monstats Id") != std::string::npos);
	CHECK(planEvent(event(ev::Kind::Boss, "Lost"), resolveSpawn(table(), "Lost"), room, 1).problem.find("Class") != std::string::npos);
	CHECK(planEvent(event(ev::Kind::Shrine, "andariel"), resolveSpawn(table(), "andariel"), room, 1).problem.find("no monster spawn plan") != std::string::npos);
	CHECK(planEvent(event(ev::Kind::Invalid, "andariel"), resolveSpawn(table(), "andariel"), room, 1).problem.find("unknown kind") != std::string::npos);
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

TEST_CASE("bannerLines: the event only once announced, no map name in a late banner") {
	ev::Event e = event(ev::Kind::Boss, "andariel");
	CHECK(bannerLines("Crypt", &e, false, false) == std::vector<std::string>{"Crypt"});
	CHECK(bannerLines("Crypt", &e, true, false) == std::vector<std::string>{"Crypt", "The Drowned King", "Something stirs."});
	CHECK(bannerLines("Crypt", &e, true, true) == std::vector<std::string>{"The Drowned King", "Something stirs."});
	CHECK(bannerLines("Crypt", &e, false, true).empty());
	CHECK(bannerLines("Crypt", nullptr, true, true).empty());
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

TEST_CASE("bannerVisible is true from the start until the banner's total time") {
	CHECK_FALSE(bannerVisible(-1));
	CHECK(bannerVisible(0));
	CHECK(bannerVisible(kBannerTotalMs - 1));
	CHECK_FALSE(bannerVisible(kBannerTotalMs));
	CHECK_FALSE(bannerVisible(1'000'000));
}

TEST_CASE("eventSeed is stable and differs from the map seed") {
	CHECK(eventSeed(1) == eventSeed(1));
	CHECK(eventSeed(1) != eventSeed(2));
	CHECK(eventSeed(12345) != 12345u);
}

// --- Spawn names from the game's tables --------------------------------------------------------------------------------

namespace {

// A fake compiled monstats table: rows of kMonStatsRowSize bytes with the Id inline at +2 (a row index u16 at +0).
std::vector<uint8_t> fakeMonstats(const std::vector<std::string> &ids) {
	std::vector<uint8_t> rows(ids.size() * kMonStatsRowSize, 0xCC);  // junk after the Id, as a real row has
	for (size_t i = 0; i < ids.size(); ++i) {
		uint8_t *row = rows.data() + i * kMonStatsRowSize;
		std::fill(row, row + kMonStatsIdOffset + kMonStatsIdSize, uint8_t{0});
		std::copy(ids[i].begin(), ids[i].end(), row + kMonStatsIdOffset);
	}
	return rows;
}

TableFacts goodFacts(const void *rows, uint32_t count, uint32_t rowSize) {
	TableFacts f;
	f.viewRows = rows;
	f.viewCount = count;
	f.viewRowSize = rowSize;
	f.dtRows = rows;
	f.dtCount = count;
	return f;
}

} // namespace

TEST_CASE("findMonsterRow: the inline Id, any case, exact length") {
	const std::vector<uint8_t> rows = fakeMonstats({"skeleton1", "", "Zombie1", "andariel"});
	CHECK(findMonsterRow(rows.data(), 4, kMonStatsRowSize, "skeleton1") == 0);
	CHECK(findMonsterRow(rows.data(), 4, kMonStatsRowSize, "ZOMBIE1") == 2);
	CHECK(findMonsterRow(rows.data(), 4, kMonStatsRowSize, "Andariel") == 3);
	CHECK(findMonsterRow(rows.data(), 4, kMonStatsRowSize, "skeleton") == -1);   // a prefix is not the Id
	CHECK(findMonsterRow(rows.data(), 4, kMonStatsRowSize, "skeleton12") == -1);
	CHECK(findMonsterRow(rows.data(), 4, kMonStatsRowSize, "") == -1);           // an empty cell never matches an empty name
	CHECK(findMonsterRow(rows.data(), 3, kMonStatsRowSize, "andariel") == -1);   // past the count
	CHECK(findMonsterRow(nullptr, 4, kMonStatsRowSize, "andariel") == -1);
	CHECK(findMonsterRow(rows.data(), 4, 16, "andariel") == -1);                 // a row too small to hold an Id
}

TEST_CASE("idMatches: a field with no NUL is not text") {
	char full[kMonStatsIdSize];
	std::fill(full, full + kMonStatsIdSize, 'a');
	CHECK_FALSE(idMatches(full, kMonStatsIdSize, std::string(kMonStatsIdSize, 'a')));
	CHECK_FALSE(idText(full, kMonStatsIdSize).has_value());
	char idBuf[kMonStatsIdSize] = {};
	std::fill(idBuf, idBuf + 46, 'b');
	CHECK(idMatches(idBuf, kMonStatsIdSize, std::string(46, 'B')));  // the longest Id: 46 characters and the terminator
	CHECK_FALSE(idMatches(nullptr, kMonStatsIdSize, "b"));
}

TEST_CASE("checkTable: the SDK view must be the game's table for the data flag") {
	const std::vector<uint8_t> rows = fakeMonstats({"a", "b"});
	CHECK(checkTable(goodFacts(rows.data(), 2, kMonStatsRowSize), kMonStatsRowSize).empty());
	CHECK_FALSE(checkTable(goodFacts(rows.data(), 2, kMonStatsRowSize + 4), kMonStatsRowSize).empty());  // another row layout
	TableFacts other = goodFacts(rows.data(), 2, kMonStatsRowSize);
	other.dtRows = rows.data() + 1;  // another bank's table
	CHECK_FALSE(checkTable(other, kMonStatsRowSize).empty());
	TableFacts count = goodFacts(rows.data(), 2, kMonStatsRowSize);
	count.dtCount = 3;
	CHECK_FALSE(checkTable(count, kMonStatsRowSize).empty());
	count.dtCount = (1ull << 32) | 2;  // the low 32 bits agree, the 64-bit count does not
	CHECK_FALSE(checkTable(count, kMonStatsRowSize).empty());
	CHECK_FALSE(checkTable(goodFacts(nullptr, 2, kMonStatsRowSize), kMonStatsRowSize).empty());
	CHECK_FALSE(checkTable(goodFacts(rows.data(), 0, kMonStatsRowSize), kMonStatsRowSize).empty());
	CHECK(checkTable(goodFacts(rows.data(), 2, kSuperRowSize), kSuperRowSize).empty());
}

TEST_CASE("checkRoundTrip: the name map is trusted only when every Id came back as its own row") {
	RoundTrip t;
	t.checked = 800;
	CHECK(checkRoundTrip(t).empty());
	t.wrong = 1;
	t.firstWrongRow = 7;
	t.firstWrongGot = -1;
	const std::string why = checkRoundTrip(t);
	CHECK(why.find("1 of 800") != std::string::npos);
	CHECK(why.find("row 7 gave -1") != std::string::npos);
	RoundTrip none;
	CHECK_FALSE(checkRoundTrip(none).empty());  // nothing was looked up
	RoundTrip nullMap;
	nullMap.mapNull = true;
	nullMap.checked = 5;
	CHECK_FALSE(checkRoundTrip(nullMap).empty());
}

TEST_CASE("superRowFromLookup: only a row inside the table counts; -1 is the miss") {
	CHECK(superRowFromLookup(0, 66) == 0);
	CHECK(superRowFromLookup(65, 66) == 65);
	CHECK(superRowFromLookup(-1, 66) == -1);
	CHECK(superRowFromLookup(-2, 66) == -1);
	CHECK(superRowFromLookup(66, 66) == -1);
	CHECK(superRowFromLookup(0, 0) == -1);
}

TEST_CASE("RouteCache: keyed by revision and data flag; a newer revision drops the rest") {
	RouteCache cache;
	RouteVerdict classic;
	classic.monsters = true;
	classic.bank = 2;
	RouteVerdict rotw;
	rotw.monsters = true;
	rotw.supers = true;
	rotw.bank = 3;
	cache.store({5, 0}, classic);
	cache.store({5, 3}, rotw);
	REQUIRE(cache.find({5, 0}) != nullptr);
	CHECK(cache.find({5, 0})->bank == 2);
	CHECK(cache.find({5, 3})->supers);          // the same revision pairs a different data flag with a different verdict
	CHECK(cache.find({5, 1}) == nullptr);
	CHECK(cache.find({4, 0}) == nullptr);
	CHECK(cache.size() == 2);
	RouteVerdict off;
	off.why = "no bank matches";
	cache.store({6, 0}, off);                   // a table load: everything from revision 5 is stale
	CHECK(cache.size() == 1);
	CHECK(cache.find({5, 0}) == nullptr);
	REQUIRE(cache.find({6, 0}) != nullptr);
	CHECK(cache.find({6, 0})->why == "no bank matches");
	cache.store({6, 0}, classic);               // the same key replaces
	CHECK(cache.size() == 1);
	CHECK(cache.find({6, 0})->monsters);
	CHECK(RouteKey{1, 2} == RouteKey{1, 2});
	CHECK_FALSE(RouteKey{1, 2} == RouteKey{2, 2});
}

TEST_CASE("idRowAccepted: a repeated Id may come back as either copy's row") {
	CHECK(idRowAccepted({4}, 4));
	CHECK_FALSE(idRowAccepted({4}, 5));
	CHECK_FALSE(idRowAccepted({4}, -1));
	CHECK(idRowAccepted({4, 90}, 4));   // the game kept the first copy
	CHECK(idRowAccepted({4, 90}, 90));  // or the last
	CHECK_FALSE(idRowAccepted({4, 90}, 50));
	CHECK_FALSE(idRowAccepted({}, 0));
}

TEST_CASE("superClassOf and checkSuperClass: the game's Class is compared with the txt where the txt has the name") {
	SpawnTable t;
	t.monsters = {"zombie", "", "fallen", "Skeleton"};
	t.supers = {{"Bishibosh", "fallen"}, {"Rakanishu", "nothere"}, {"", "zombie"}};
	CHECK(superClassOf(t, 0) == "fallen");
	CHECK(superClassOf(t, -1).empty());
	CHECK(superClassOf(t, 3).empty());
	CHECK(checkSuperClass(t, "Bishibosh", "fallen") == ClassCheck::Agrees);
	CHECK(checkSuperClass(t, "bishibosh", "FALLEN") == ClassCheck::Agrees);  // case-insensitive, both sides
	CHECK(checkSuperClass(t, "Bishibosh", "zombie") == ClassCheck::Differs);
	CHECK(checkSuperClass(t, "Rakanishu", "fallen") == ClassCheck::Differs);  // names, not rows: the txt's Class is not the game's
	CHECK(checkSuperClass(t, "Corpsefire", "fallen") == ClassCheck::NoReference);  // the txt does not have the name
	CHECK(checkSuperClass(t, "Bishibosh", "") == ClassCheck::NoReference);         // no game Id to compare
	CHECK(checkSuperClass(t, "", "fallen") == ClassCheck::NoReference);
	CHECK(checkSuperClass(SpawnTable{}, "Bishibosh", "fallen") == ClassCheck::NoReference);
}

TEST_CASE("scaleKind says why the spot scale is what it is") {
	CHECK(scaleKind({0, 0, 20, 16}, {0, 0, 20, 16}) == ScaleKind::Same);
	CHECK(scaleKind({0, 0, 100, 80}, {0, 0, 20, 16}) == ScaleKind::Multiple);
	CHECK(scaleKind({0, 0, 101, 80}, {0, 0, 20, 16}) == ScaleKind::NotAMultiple);
	CHECK(scaleKind({0, 0, 100, 40}, {0, 0, 20, 16}) == ScaleKind::NotAMultiple);
	CHECK(std::string(scaleKindName(ScaleKind::NotAMultiple)).find("fallback") != std::string::npos);
}
