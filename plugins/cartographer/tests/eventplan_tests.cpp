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
	CHECK(tiny.size() <= 4);
	for (const Spot &s : tiny) {
		CHECK(inside(s, {10, 10, 3, 3}));
	}
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
