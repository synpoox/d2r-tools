// Host tests for the Cartographer maze steering core. No game, no SDK: the core is pure C++.
//
// The tileset fixtures hold only numbers derived from vanilla 93847 lvlmaze.txt / lvlprest.txt / levels.txt:
//   Cave:  LvlMaze room 24 x 24, "Act 1 - Cave W" is Def 53 (so base 52, 15 mask pieces 53..67), Prev W/E/S/N 83..86,
//          Down W/E/S/N 91..94, levels 200 x 200 tiles.
//   Crypt: room 8 x 8, "Act 1 - Crypt W" is Def 109 (base 108, 109..123), Prev 139..142, Next 143..146, levels 200 x 200.
#define _CRT_SECURE_NO_WARNINGS
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "layout.h"
#include "maze.h"
#include "rng.h"

using namespace cartographer;
using namespace cartographer::maze;
using layout::Layout;
using layout::Point;
using layout::Role;

namespace {

constexpr int kDefaultSeeds = 10000;
// CARTOGRAPHER_SEEDS=n shortens a local run; the default is the done-condition's 10k seeds per shape.
int seedCount()
{
	const char *e = std::getenv("CARTOGRAPHER_SEEDS");
	const int n = e ? std::atoi(e) : 0;
	return n > 0 ? n : kDefaultSeeds;
}

Tileset caveTileset()
{
	Tileset t;
	t.name = "Cave";
	t.levelType = 3;
	t.roomTiles = 24;
	t.pieceBase = 52;
	t.levelTilesX = t.levelTilesY = 200;
	t.entrance = {true, {83, 84, 85, 86}}; // Prev W, E, S, N
	t.exit = {true, {91, 92, 93, 94}};     // Down W, E, S, N
	return t;
}

Tileset cryptTileset()
{
	Tileset t;
	t.name = "Crypt";
	t.levelType = 4;
	t.roomTiles = 8;
	t.pieceBase = 108;
	t.levelTilesX = t.levelTilesY = 200;
	t.entrance = {true, {139, 140, 141, 142}}; // Prev W, E, S, N
	t.exit = {true, {143, 144, 145, 146}};     // Next W, E, S, N
	return t;
}

int popcount(unsigned m) { int n = 0; for (; m; m >>= 1) n += static_cast<int>(m & 1); return n; }

const layout::Cell &cellOf(const Layout &l, Point p) { return l.at(p.x, p.y); }

// Everything below is checked by replaying the plan the way D2R's functions would apply it, without using any of the
// plan's own derived fields (mask, piece) as input: masks come from the adds and links, positions from the add dirs.
std::string verify(const Tileset &ts, const Layout &l, const Plan &p)
{
	if (!p.steer) return "plan does not steer";
	const size_t n = p.rooms.size();
	if (static_cast<int>(n) != l.cellCount()) return "room count differs from the layout's cells";
	if (n == 0 || p.rooms[0].dx != 0 || p.rooms[0].dy != 0) return "room 0 is not the first room";

	// Tile positions: the first room is centred in the level.
	const int offX = (ts.levelTilesX - ts.roomTiles) / 2, offY = (ts.levelTilesY - ts.roomTiles) / 2;
	std::vector<int> tx(n, 0), ty(n, 0);
	std::vector<char> made(n, 0);
	std::vector<uint8_t> mask(n, 0);
	tx[0] = offX;
	ty[0] = offY;
	made[0] = 1;
	const int dxOf[4] = {-1, 0, 1, 0}, dyOf[4] = {0, -1, 0, 1};
	const uint8_t bitOf[4] = {layout::kWest, layout::kNorth, layout::kEast, layout::kSouth};
	const uint8_t oppBit[4] = {layout::kEast, layout::kSouth, layout::kWest, layout::kNorth};
	for (const AddStep &a : p.adds) {
		if (a.parent < 0 || a.room < 0 || static_cast<size_t>(a.parent) >= n || static_cast<size_t>(a.room) >= n) return "bad add index";
		if (!made[static_cast<size_t>(a.parent)] || made[static_cast<size_t>(a.room)]) return "add order wrong";
		if (a.dir < 0 || a.dir > 3) return "bad add dir";
		made[static_cast<size_t>(a.room)] = 1;
		tx[static_cast<size_t>(a.room)] = tx[static_cast<size_t>(a.parent)] + dxOf[a.dir] * ts.roomTiles;
		ty[static_cast<size_t>(a.room)] = ty[static_cast<size_t>(a.parent)] + dyOf[a.dir] * ts.roomTiles;
		mask[static_cast<size_t>(a.parent)] = static_cast<uint8_t>(mask[static_cast<size_t>(a.parent)] | bitOf[a.dir]);
		mask[static_cast<size_t>(a.room)] = static_cast<uint8_t>(mask[static_cast<size_t>(a.room)] | oppBit[a.dir]);
	}
	for (size_t i = 0; i < n; ++i)
		if (!made[i]) return "a room is never made";
	// Rooms must not overlap and must stay inside the level.
	for (size_t i = 0; i < n; ++i) {
		if (tx[i] < 0 || ty[i] < 0 || tx[i] + ts.roomTiles > ts.levelTilesX || ty[i] + ts.roomTiles > ts.levelTilesY) return "a room leaves the level";
		for (size_t j = i + 1; j < n; ++j)
			if (tx[i] == tx[j] && ty[i] == ty[j]) return "two rooms overlap";
	}
	// Loop links join rooms that sit side by side.
	for (const LinkStep &k : p.links) {
		if (k.a < 0 || k.b < 0 || static_cast<size_t>(k.a) >= n || static_cast<size_t>(k.b) >= n) return "bad link index";
		if (k.dir < 0 || k.dir > 3) return "bad link dir";
		const size_t a = static_cast<size_t>(k.a), b = static_cast<size_t>(k.b);
		if (tx[a] + dxOf[k.dir] * ts.roomTiles != tx[b] || ty[a] + dyOf[k.dir] * ts.roomTiles != ty[b]) return "link between rooms that do not touch";
		mask[a] = static_cast<uint8_t>(mask[a] | bitOf[k.dir]);
		mask[b] = static_cast<uint8_t>(mask[b] | oppBit[k.dir]);
	}
	// Every room's exits match the layout cell it stands for, and its piece is base + mask.
	std::vector<int> piece(n, 0);
	for (size_t i = 0; i < n; ++i) {
		const Room &r = p.rooms[i];
		if (r.cell.x - p.rooms[0].cell.x != r.dx || r.cell.y - p.rooms[0].cell.y != r.dy) return "room offset differs from its cell";
		if (tx[i] != offX + r.dx * ts.roomTiles || ty[i] != offY + r.dy * ts.roomTiles) return "room position differs from the add chain";
		if (mask[i] != cellOf(l, r.cell).mask) return "the replayed exits differ from the layout's mask";
		if (r.mask != mask[i]) return "room.mask differs from its links";
		if (r.role != cellOf(l, r.cell).role) return "role differs";
		piece[i] = ts.pieceBase + mask[i];
	}
	// The entrance and exit placer: the engine scan is "newest free room whose piece is `from`".
	std::vector<char> fixed(n, 0);
	const Point entranceCell = l.mainPath.front();
	const Point exitCell = l.bossIdx >= 0 ? l.mainPath[static_cast<size_t>(l.bossIdx)] : l.mainPath.back();
	if (p.specials.size() != 2) return "expected two special calls";
	for (int call = 0; call < 2; ++call) {
		const SpecialCall &sc = p.specials[static_cast<size_t>(call)];
		const Point want = call == 0 ? entranceCell : exitCell;
		if (sc.room < 0 || static_cast<size_t>(sc.room) >= n || p.rooms[static_cast<size_t>(sc.room)].cell != want) return "special call targets the wrong cell";
		if ((sc.kind == SpecialKind::Entrance) != (call == 0)) return "special call order";
		const DeadEndFamily &fam = call == 0 ? ts.entrance : ts.exit;
		const bool deadEnd = popcount(cellOf(l, want).mask) == 1;
		if (sc.place != (fam.present && deadEnd)) return "place decision wrong";
		// Any group entry of the family reaches the hook; the hook is told which rotation to use.
		for (int to : {fam.piece[0], fam.piece[1], fam.piece[2], fam.piece[3]}) {
			const SpecialSteer st = steerSpecial(ts, p, call, to);
			if ((st.action == SpecialSteer::Action::Redirect) != sc.place) return "steerSpecial disagrees with place";
		}
		if (!sc.place) {
			if (steerSpecial(ts, p, call, 1).action != SpecialSteer::Action::Suppress) return "an unplaced call must be suppressed";
			continue;
		}
		const int fromPiece = steerSpecial(ts, p, call, sc.toPiece).fromPiece;
		int hit = -1;
		for (int i = static_cast<int>(n) - 1; i >= 0; --i)
			if (!fixed[static_cast<size_t>(i)] && piece[static_cast<size_t>(i)] == fromPiece) { hit = i; break; }
		if (hit != sc.room) return "0x3eb850's newest-free-room scan would pick another room";
		// the special piece must be the family piece of the room's open side
		const uint8_t m = mask[static_cast<size_t>(hit)];
		const int idx = m == layout::kWest ? 0 : m == layout::kEast ? 1 : m == layout::kSouth ? 2 : 3;
		if (fam.piece[static_cast<size_t>(idx)] != sc.toPiece) return "special piece for the wrong side";
		piece[static_cast<size_t>(hit)] = sc.toPiece;
		fixed[static_cast<size_t>(hit)] = 1;
	}
	// An unexpected third call is suppressed.
	if (steerSpecial(ts, p, 2, ts.entrance.piece[0]).action != SpecialSteer::Action::Suppress) return "a third call must be suppressed";
	// Reserved rooms: entrance, exit, event and boss, and only those.
	for (size_t i = 0; i < n; ++i) {
		const Room &r = p.rooms[i];
		const bool reserved = r.cell == entranceCell || r.cell == exitCell || (l.eventIdx >= 0 && r.cell == l.mainPath[static_cast<size_t>(l.eventIdx)]) ||
		                      (l.bossIdx >= 0 && r.cell == l.mainPath[static_cast<size_t>(l.bossIdx)]);
		if (r.fixed != reserved) return reserved ? "a reserved room is not fixed" : "a plain room is fixed";
		if (piece[i] != r.piece) return "final piece differs from base + mask / special piece";
	}
	if (l.eventIdx >= 0 && p.rooms[static_cast<size_t>(p.eventRoom)].cell != l.mainPath[static_cast<size_t>(l.eventIdx)]) return "event room index";
	if (l.bossIdx >= 0 && p.rooms[static_cast<size_t>(p.bossRoom)].cell != l.mainPath[static_cast<size_t>(l.bossIdx)]) return "boss room index";
	if (p.ring.noOp != ts.callsRingGrower || p.ring.graphInRing != (ts.callsRingGrower && !ts.walkFollowsRing)) return "ring plan";
	if (p.lvlMaze.rooms != static_cast<int>(n) || p.lvlMaze.sizeX != ts.roomTiles || p.lvlMaze.sizeY != ts.roomTiles || p.lvlMaze.merge != 0) return "LvlMaze row";
	return std::string();
}

// Independent reason a layout cannot fit: its extent exceeds the level, or no usable first room exists.
bool genuinelyDoesNotFit(const Tileset &ts, const Layout &l)
{
	const Capacity cap = capacity(ts);
	int minX = l.width, maxX = -1, minY = l.height, maxY = -1;
	for (int y = 0; y < l.height; ++y)
		for (int x = 0; x < l.width; ++x)
			if (l.at(x, y).role != Role::Empty) { minX = std::min(minX, x); maxX = std::max(maxX, x); minY = std::min(minY, y); maxY = std::max(maxY, y); }
	if (maxX - minX + 1 > cap.across() || maxY - minY + 1 > cap.tall()) return true;
	// Every cell that is not a special end must be unable to be the first room.
	const Point ends[2] = {l.mainPath.front(), l.bossIdx >= 0 ? l.mainPath[static_cast<size_t>(l.bossIdx)] : l.mainPath.back()};
	for (int y = minY; y <= maxY; ++y)
		for (int x = minX; x <= maxX; ++x) {
			if (l.at(x, y).role == Role::Empty) continue;
			const Point p{x, y};
			const bool special = (p == ends[0] && ts.entrance.present && popcount(l.at(x, y).mask) == 1) || (p == ends[1] && ts.exit.present && popcount(l.at(x, y).mask) == 1);
			if (special) continue;
			if (x - minX <= cap.left && maxX - x <= cap.right && y - minY <= cap.up && maxY - y <= cap.down) return false;
		}
	return true;
}

struct Totals {
	int seeds = 0, steered = 0, fallbacks = 0, refitted = 0, maxRefits = 0, placedEntrance = 0, placedExit = 0, rooms = 0;
};

Totals runSeeds(const Tileset &ts, layout::Shape shape, layout::Size size)
{
	Totals t;
	const layout::Rules rules = layout::presetRules(shape, size);
	const int kSeeds = seedCount();
	for (int i = 0; i < kSeeds; ++i) {
		const uint64_t seed = layout::splitmix64(static_cast<uint64_t>(i) + 0x2000);
		const SeedPlan sp = planForSeed(ts, rules, seed);
		++t.seeds;
		if (sp.refits > 0) ++t.refitted;
		t.maxRefits = std::max(t.maxRefits, sp.refits);
		if (sp.plan.steer) {
			++t.steered;
			t.rooms += static_cast<int>(sp.plan.rooms.size());
			if (sp.plan.specials[0].place) ++t.placedEntrance;
			if (sp.plan.specials[1].place) ++t.placedExit;
			const std::string bad = verify(ts, sp.layout, sp.plan);
			if (!bad.empty()) {
				FAIL_CHECK(ts.name << " " << layout::shapeName(shape) << " " << layout::sizeName(size) << " seed " << seed << ": " << bad << "\n" << layout::toAscii(sp.layout));
				break;
			}
			if (!sp.plan.reason.empty()) { FAIL_CHECK("a steering plan has a reason"); break; }
		} else {
			++t.fallbacks;
			if (sp.plan.reason.empty()) { FAIL_CHECK("a fallback has no reason"); break; }
			// The fallback fires only when the last layout really cannot fit (or the shape cannot fit the tileset at all).
			if (sp.layout.cells.empty()) continue; // rules rejected before any layout; reason names the rules
			if (!genuinelyDoesNotFit(ts, sp.layout) && sp.layout.rulesMet) {
				FAIL_CHECK(ts.name << " " << layout::shapeName(shape) << " seed " << seed << ": fell back on a layout that fits: " << sp.plan.reason << "\n" << layout::toAscii(sp.layout));
				break;
			}
		}
	}
	return t;
}

void printStats(const Tileset &ts, layout::Shape shape, layout::Size size, const Totals &t)
{
	if (!std::getenv("CARTOGRAPHER_STATS")) return;
	std::printf("%-6s %-9s %-6s seeds %d  steered %d  fallback %d (%.2f%%)  refitted %d  max refits %d  entrance placed %d  exit placed %d  avg rooms %.1f\n",
		ts.name.c_str(), layout::shapeName(shape), layout::sizeName(size), t.seeds, t.steered, t.fallbacks, 100.0 * t.fallbacks / t.seeds, t.refitted, t.maxRefits,
		t.placedEntrance, t.placedExit, t.steered ? static_cast<double>(t.rooms) / t.steered : 0.0);
}

} // namespace

TEST_CASE("capacity: the first room is centred in the level")
{
	const Capacity cave = capacity(caveTileset());
	CHECK(cave.left == 3);
	CHECK(cave.right == 3);
	CHECK(cave.across() == 7);
	CHECK(cave.tall() == 7);
	const Capacity crypt = capacity(cryptTileset());
	CHECK(crypt.left == 12);
	CHECK(crypt.right == 12);
	CHECK(crypt.across() == 25);
	Tileset tiny = caveTileset();
	tiny.levelTilesX = 24; // exactly one room wide
	CHECK(capacity(tiny).across() == 1);
	tiny.roomTiles = 0;
	CHECK(capacity(tiny).across() == 1);
}

TEST_CASE("engine directions")
{
	CHECK(engineDir(layout::kWest) == kEngineW);
	CHECK(engineDir(layout::kNorth) == kEngineN);
	CHECK(engineDir(layout::kEast) == kEngineE);
	CHECK(engineDir(layout::kSouth) == kEngineS);
	CHECK(engineDir(3) == -1);
	for (int d = 0; d < 4; ++d) CHECK(engineDir(maskBitOfEngineDir(d)) == d);
	CHECK(maskBitOfEngineDir(7) == 0);
}

TEST_CASE("10k seeds per shape, Cave and Crypt, every size: pieces, ends, reserved rooms, fallback only when it cannot fit")
{
	const Tileset tilesets[2] = {caveTileset(), cryptTileset()};
	for (const Tileset &ts : tilesets)
		for (int s = 0; s < layout::kShapeCount; ++s)
			for (int z = 0; z < layout::kSizeCount; ++z) {
				const layout::Shape shape = static_cast<layout::Shape>(s);
				const layout::Size size = static_cast<layout::Size>(z);
				const Totals t = runSeeds(ts, shape, size);
				printStats(ts, shape, size, t);
				INFO(ts.name << " " << layout::shapeName(shape) << " " << layout::sizeName(size));
				CHECK(t.steered + t.fallbacks == seedCount());
				// Cave holds 7 x 7 rooms: these combinations cannot be built in it at all (every seed falls back, with a reason).
				const bool impossible = ts.name == "Cave" &&
					((shape == layout::Shape::Sprawl && size != layout::Size::Small) ||
					 (shape == layout::Shape::Loop && size == layout::Size::Large));
				CHECK((t.steered > 0) != impossible);
				// Crypt rooms are tiny: its level holds every layout, so nothing may fall back.
				if (ts.name == "Crypt") CHECK(t.fallbacks == 0);
				// A one-exit end gets its special piece in every shape except the rings.
				if (shape != layout::Shape::Loop && shape != layout::Shape::Sprawl) {
					CHECK(t.placedEntrance == t.steered);
					CHECK(t.placedExit == t.steered);
				}
			}
}

TEST_CASE("Cave / Corridor and Crypt / Branching: the plan, step by step")
{
	struct Case { Tileset ts; layout::Shape shape; };
	const Case cases[2] = {{caveTileset(), layout::Shape::Corridor}, {cryptTileset(), layout::Shape::Branching}};
	for (const Case &c : cases) {
		INFO(c.ts.name << " " << layout::shapeName(c.shape));
		for (layout::Size size : {layout::Size::Small}) {
			const SeedPlan sp = planForSeed(c.ts, layout::presetRules(c.shape, size), 1);
			REQUIRE(sp.plan.steer);
			const Plan &p = sp.plan;
			CHECK(verify(c.ts, sp.layout, p).empty());
			// A tree edge per room but the first; Corridor and Branching have no loops.
			CHECK(p.adds.size() + 1 == p.rooms.size());
			CHECK(p.links.empty());
			// The entrance and the boss are dead ends and the last two rooms made: exit, then entrance.
			CHECK(p.specials[0].place);
			CHECK(p.specials[1].place);
			CHECK(p.specials[0].room == static_cast<int>(p.rooms.size()) - 1);
			CHECK(p.specials[1].room == static_cast<int>(p.rooms.size()) - 2);
			CHECK(p.rooms[static_cast<size_t>(p.specials[0].room)].cell == sp.layout.mainPath.front());
			CHECK(p.rooms[static_cast<size_t>(p.specials[1].room)].cell == sp.layout.mainPath[static_cast<size_t>(sp.layout.bossIdx)]);
			CHECK(p.rooms[static_cast<size_t>(p.specials[0].room)].piece == p.specials[0].toPiece);
			CHECK(p.rooms[static_cast<size_t>(p.specials[1].room)].piece == p.specials[1].toPiece);
			CHECK(!p.ring.noOp);
			CHECK(p.lvlMaze.merge == 0);
			CHECK(p.eventRoom >= 0);
			CHECK(p.bossRoom >= 0);
			CHECK(p.rooms[static_cast<size_t>(p.eventRoom)].fixed);
			CHECK(p.rooms[static_cast<size_t>(p.bossRoom)].fixed);
			CHECK(p.rooms[0].dx == 0);
		}
	}
}

TEST_CASE("same seed gives the same plan; the plan hash is stable")
{
	const Tileset cave = caveTileset();
	const Tileset crypt = cryptTileset();
	for (uint64_t seed = 1; seed <= 20; ++seed) {
		const SeedPlan a = planForSeed(cave, layout::presetRules(layout::Shape::Corridor, layout::Size::Small), seed);
		const SeedPlan b = planForSeed(cave, layout::presetRules(layout::Shape::Corridor, layout::Size::Small), seed);
		CHECK(planHash(a.plan) == planHash(b.plan));
	}
	CHECK(planHash(planForSeed(cave, layout::presetRules(layout::Shape::Corridor, layout::Size::Small), 1).plan) !=
	      planHash(planForSeed(cave, layout::presetRules(layout::Shape::Corridor, layout::Size::Small), 2).plan));
	// Golden values (FNV-1a over the plan): they pin the plan across compilers and platforms.
	CHECK(planHash(planForSeed(cave, layout::presetRules(layout::Shape::Corridor, layout::Size::Small), 1).plan) == 2575042952238616093ull);
	CHECK(planHash(planForSeed(crypt, layout::presetRules(layout::Shape::Branching, layout::Size::Medium), 1).plan) == 10926222372997424906ull);
}

TEST_CASE("loops: link steps close the rings, and the ends of a ring are ordinary rooms")
{
	const Tileset crypt = cryptTileset();
	int withLinks = 0;
	for (uint64_t seed = 1; seed <= 200; ++seed) {
		const SeedPlan sp = planForSeed(crypt, layout::presetRules(layout::Shape::Loop, layout::Size::Medium), seed);
		REQUIRE(sp.plan.steer);
		CHECK(verify(crypt, sp.layout, sp.plan).empty());
		CHECK(!sp.plan.links.empty()); // the ring has one non-tree edge at least
		withLinks += sp.plan.links.empty() ? 0 : 1;
		// A ring's entrance and boss have two exits: the special calls are suppressed, with a reason.
		CHECK(!sp.plan.specials[0].place);
		CHECK(!sp.plan.specials[0].note.empty());
		CHECK(!sp.plan.specials[1].place);
		CHECK(sp.plan.rooms[static_cast<size_t>(sp.plan.specials[0].room)].piece == crypt.pieceBase + sp.plan.rooms[static_cast<size_t>(sp.plan.specials[0].room)].mask);
	}
	CHECK(withLinks == 200);
}

TEST_CASE("a tileset without special rooms suppresses both calls and keeps every end an ordinary fixed room")
{
	Tileset ts = caveTileset();
	ts.entrance = DeadEndFamily{};
	ts.exit = DeadEndFamily{};
	for (uint64_t seed = 1; seed <= 200; ++seed) {
		const SeedPlan sp = planForSeed(ts, layout::presetRules(layout::Shape::Corridor, layout::Size::Small), seed);
		REQUIRE(sp.plan.steer);
		CHECK(verify(ts, sp.layout, sp.plan).empty());
		for (const SpecialCall &sc : sp.plan.specials) {
			CHECK(!sc.place);
			CHECK(!sc.note.empty());
			CHECK(steerSpecial(ts, sp.plan, 0, 83).action == SpecialSteer::Action::Suppress);
		}
		CHECK(sp.plan.rooms[static_cast<size_t>(sp.plan.specials[0].room)].fixed);
		CHECK(sp.plan.rooms[static_cast<size_t>(sp.plan.specials[1].room)].fixed);
	}
}

TEST_CASE("steerSpecial: the hook is told which rotation, and strays are suppressed")
{
	const Tileset ts = caveTileset();
	const SeedPlan sp = planForSeed(ts, layout::presetRules(layout::Shape::Corridor, layout::Size::Small), 7);
	REQUIRE(sp.plan.steer);
	const SpecialCall &en = sp.plan.specials[0];
	const SpecialCall &ex = sp.plan.specials[1];
	SpecialSteer s = steerSpecial(ts, sp.plan, 0, en.toPiece);
	CHECK(s.action == SpecialSteer::Action::Redirect);
	CHECK(s.fromPiece == en.fromPiece);
	CHECK(en.fromPiece >= ts.pieceBase + 1);
	CHECK(popcount(static_cast<unsigned>(en.fromPiece - ts.pieceBase)) == 1);
	// An entry of the wrong group for the call index is not steered.
	CHECK(steerSpecial(ts, sp.plan, 0, ex.toPiece).action == SpecialSteer::Action::Suppress);
	CHECK(steerSpecial(ts, sp.plan, 1, 99).action == SpecialSteer::Action::Suppress);
	CHECK(steerSpecial(ts, sp.plan, -1, en.toPiece).action == SpecialSteer::Action::Suppress);
	CHECK(steerSpecial(ts, sp.plan, 5, en.toPiece).action == SpecialSteer::Action::Suppress);
	Plan off = sp.plan;
	off.steer = false;
	CHECK(steerSpecial(ts, off, 0, en.toPiece).action == SpecialSteer::Action::Suppress);
}

TEST_CASE("ring grower: no-op only for tilesets that call it, and the graph moves inside it when no walk follows")
{
	Tileset ts = caveTileset();
	ts.name = "Jail";
	ts.callsRingGrower = true;
	SeedPlan sp = planForSeed(ts, layout::presetRules(layout::Shape::Winding, layout::Size::Small), 3);
	REQUIRE(sp.plan.steer);
	CHECK(sp.plan.ring.noOp);
	CHECK(!sp.plan.ring.graphInRing);
	ts.walkFollowsRing = false;
	sp = planForSeed(ts, layout::presetRules(layout::Shape::Winding, layout::Size::Small), 3);
	CHECK(sp.plan.ring.noOp);
	CHECK(sp.plan.ring.graphInRing);
	INFO(verify(ts, sp.layout, sp.plan));
	CHECK(verify(ts, sp.layout, sp.plan).empty());
	CHECK(!planForSeed(caveTileset(), layout::presetRules(layout::Shape::Winding, layout::Size::Small), 3).plan.ring.noOp);
}

TEST_CASE("fallback: each way a layout cannot fit gives a plain-English reason")
{
	const Tileset cave = caveTileset();
	const Layout good = planForSeed(cave, layout::presetRules(layout::Shape::Corridor, layout::Size::Small), 5).layout;
	INFO(planMaze(cave, good).reason << "\n" << layout::toAscii(good));
	REQUIRE(planMaze(cave, good).steer);

	// 1. The tileset cannot be steered.
	{
		Tileset ts = cave;
		ts.name = "Arcane";
		ts.steerable = false;
		const Plan p = planMaze(ts, good);
		CHECK(!p.steer);
		CHECK(p.reason.find("Arcane") != std::string::npos);
		ts.notSteerableReason = "Arcane runs its own room grower.";
		CHECK(planMaze(ts, good).reason == "Arcane runs its own room grower.");
	}
	// 2. The layout is wider than the level holds: a 9-wide corridor on a Cave (7 across).
	{
		bool found = false;
		for (uint64_t seed = 1; seed < 400 && !found; ++seed) {
			const Layout l = layout::generate(layout::presetRules(layout::Shape::Winding, layout::Size::Large), seed);
			int minX = l.width, maxX = -1;
			for (int y = 0; y < l.height; ++y)
				for (int x = 0; x < l.width; ++x)
					if (l.at(x, y).role != Role::Empty) { minX = std::min(minX, x); maxX = std::max(maxX, x); }
			if (maxX - minX + 1 > 7) {
				const Plan p = planMaze(cave, l);
				CHECK(!p.steer);
				CHECK(p.reason.find("across") != std::string::npos);
				CHECK(p.reason.find("Cave") != std::string::npos);
				found = true;
			}
		}
		CHECK(found);
	}
	// 3. The tileset lacks a piece the layout needs.
	{
		Tileset ts = cave;
		ts.maskPieces = static_cast<uint16_t>(0xFFFE & ~(1u << 3)); // no "EW" piece
		const Layout l = layout::generate(layout::Shape::Corridor, layout::Size::Small, 5);
		bool needs = false;
		for (const layout::Cell &c : l.cells) needs = needs || c.mask == 3;
		const Plan p = planMaze(ts, l);
		if (needs) {
			CHECK(!p.steer);
			CHECK(p.reason.find("EW") != std::string::npos);
		}
		ts.maskPieces = 0; // no pieces at all
		CHECK(!planMaze(ts, l).steer);
	}
	// 4. Unusable tileset sizes, empty and unmet layouts.
	{
		Tileset ts = cave;
		ts.roomTiles = 0;
		CHECK(!planMaze(ts, good).steer);
		CHECK(!planMaze(cave, Layout{}).steer);
		CHECK(!planMaze(cave, Layout{}).reason.empty());
		Layout bad = good;
		bad.rulesMet = false;
		const Plan p = planMaze(cave, bad);
		CHECK(!p.steer);
		CHECK(p.reason.find("rules") != std::string::npos);
	}
	// 5. A level with room for one room only: nothing fits, and planForSeed says so after its refits.
	{
		Tileset ts = cave;
		ts.levelTilesX = ts.levelTilesY = 24;
		const SeedPlan sp = planForSeed(ts, layout::presetRules(layout::Shape::Corridor, layout::Size::Small), 1);
		CHECK(!sp.plan.steer);
		CHECK(!sp.plan.reason.empty());
	}
	// 6. Plain English: no code names in the reasons.
	{
		Tileset ts = cave;
		ts.levelTilesX = ts.levelTilesY = 72; // 1 + 1 + 1 rooms: a corridor of 3 can not hold a 10-room path
		const SeedPlan sp = planForSeed(ts, layout::presetRules(layout::Shape::Corridor, layout::Size::Small), 1);
		CHECK(!sp.plan.steer);
		CHECK(sp.plan.reason.find("0x") == std::string::npos);
	}
}

TEST_CASE("fitRules clamps the grid to the level and leaves a roomy level alone")
{
	const layout::Rules r = layout::presetRules(layout::Shape::Winding, layout::Size::Large);
	CHECK(r.width == 12);
	CHECK(fitRules(caveTileset(), r).width == 7);
	CHECK(fitRules(caveTileset(), r).height == 7);
	CHECK(fitRules(cryptTileset(), r).width == 12);
}

TEST_CASE("a plan is built from a hand-made layout: a Loop closes with a link step")
{
	// A 3x3 ring of eight rooms (centre empty), entrance at (0,0), boss at (2,2).
	Layout l;
	l.width = l.height = 3;
	l.cells.assign(9, layout::Cell{});
	auto set = [&](int x, int y, uint8_t m, Role r) { l.at(x, y).mask = m; l.at(x, y).role = r; };
	set(0, 0, layout::kEast | layout::kSouth, Role::Entrance);
	set(1, 0, layout::kWest | layout::kEast, Role::Path);
	set(2, 0, layout::kWest | layout::kSouth, Role::Event);
	set(2, 1, layout::kNorth | layout::kSouth, Role::Path);
	set(2, 2, layout::kNorth | layout::kWest, Role::Boss);
	set(1, 2, layout::kEast | layout::kWest, Role::Path);
	set(0, 2, layout::kEast | layout::kNorth, Role::Path);
	set(0, 1, layout::kNorth | layout::kSouth, Role::Path);
	l.mainPath = {Point{0, 0}, Point{1, 0}, Point{2, 0}, Point{2, 1}, Point{2, 2}, Point{1, 2}, Point{0, 2}, Point{0, 1}};
	l.eventIdx = 2;
	l.bossIdx = 4;
	l.ring = true;
	l.rulesMet = true;
	const Tileset ts = cryptTileset();
	const Plan p = planMaze(ts, l);
	REQUIRE(p.steer);
	CHECK(p.rooms.size() == 8);
	CHECK(p.adds.size() == 7);
	REQUIRE(p.links.size() == 1);
	CHECK(verify(ts, l, p).empty());
	CHECK(!p.specials[0].place); // two exits
	CHECK(p.widthRooms == 3);
	CHECK(p.heightRooms == 3);
}
