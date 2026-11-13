// Host tests for the Cartographer maze steering core. No game, no SDK: the core is pure C++.
//
// The tileset fixtures hold only numbers derived from vanilla 93847 lvlmaze.txt / lvlprest.txt:
//   Cave:  LvlMaze room 24 x 24, "Act 1 - Cave W" is Def 53 (so base 52, 15 mask pieces 53..67), Prev W/E/S/N 83..86,
//          Down W/E/S/N 91..94.
//   Crypt: room 8 x 8, "Act 1 - Crypt W" is Def 109 (base 108, 109..123), Prev 139..142, Next 143..146.
// A maze has an entrance (Prev) and no exit: the Down / Next numbers are only the calls the slot's code still makes,
// which the core must suppress.
// The level size is the plan's output (up to kMaxLevelTiles). "A 200-tile slot" is a fixture that caps it at vanilla
// levels.txt's 200 x 200 to test the fallback path; it says nothing about what a Cave can hold.
#define _CRT_SECURE_NO_WARNINGS
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "layout.h"
#include "maze.h"
#include "maze_tilesets.h"
#include "rng.h"
#include "steer.h"

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

Tileset caveTileset(int maxLevelTiles = kMaxLevelTiles)
{
	Tileset t;
	t.name = "Cave";
	t.levelType = 3;
	t.roomTiles = 24;
	t.pieceBase = 52;
	t.maxLevelTiles = maxLevelTiles;
	t.prev = {true, {83, 84, 85, 86}}; // Prev W, E, S, N
	return t;
}

Tileset cryptTileset(int maxLevelTiles = kMaxLevelTiles)
{
	Tileset t;
	t.name = "Crypt";
	t.levelType = 4;
	t.roomTiles = 8;
	t.pieceBase = 108;
	t.maxLevelTiles = maxLevelTiles;
	t.prev = {true, {139, 140, 141, 142}}; // Prev W, E, S, N
	return t;
}

// The calls the slot's code still makes and the core must answer with a suppression.
constexpr int kCaveDown[4] = {91, 92, 93, 94};   // Down W, E, S, N
constexpr int kCryptNext[4] = {143, 144, 145, 146}; // Next W, E, S, N
constexpr int kCaveNext[4] = {95, 96, 97, 98};   // a Next group a slot might also call

int popcount(unsigned m) { int n = 0; for (; m; m >>= 1) n += static_cast<int>(m & 1); return n; }

const layout::Cell &cellOf(const Layout &l, Point p) { return l.at(p.x, p.y); }

// Everything below is checked by replaying the plan the way D2R's functions would apply it, without using any of the
// plan's own derived fields (mask, piece) as input: masks come from the adds and links, positions from the add dirs.
// fixPlacedEarly replays the WRONG hook order (the placed entrance fixed right after the walk, before 0x3eb850 runs) and must fail.
std::string verify(const Tileset &ts, const Layout &l, const Plan &p, bool fixPlacedEarly = false)
{
	if (!p.steer) return "plan does not steer";
	// The level size is the plan's output: the formula, the allowed maximum, and room to spare on every side.
	for (int axis = 0; axis < 2; ++axis) {
		const int got = axis == 0 ? p.levelTilesX : p.levelTilesY;
		if (got > ts.maxLevelTiles || got < ts.roomTiles) return "level size outside the allowed range";
		if (got != ts.maxLevelTiles && got % (2 * ts.roomTiles) != ts.roomTiles) return "level size is not (2 x k + 1) rooms";
	}
	const int levelX = p.levelTilesX, levelY = p.levelTilesY;
	const size_t n = p.rooms.size();
	if (static_cast<int>(n) != l.cellCount()) return "room count differs from the layout's cells";
	if (n == 0 || p.rooms[0].dx != 0 || p.rooms[0].dy != 0) return "room 0 is not the first room";

	// Tile positions: the first room is centred in the level.
	const int offX = (levelX - ts.roomTiles) / 2, offY = (levelY - ts.roomTiles) / 2;
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
		if (tx[i] < 0 || ty[i] < 0 || tx[i] + ts.roomTiles > levelX || ty[i] + ts.roomTiles > levelY) return "a room leaves the level";
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
	// The entrance placer: the engine scan is "newest free room whose piece is `from`", and it skips fixed rooms.
	// Rooms the hook fixes after the walk are fixed before the calls; a placed end is fixed by 0x3eb850 itself, after its call.
	std::vector<char> fixed0(n, 0);
	for (size_t i = 0; i < n; ++i)
		if (p.rooms[i].fix == Room::Fix::AfterWalk || (fixPlacedEarly && p.rooms[i].fix == Room::Fix::ByPlacer)) fixed0[i] = 1;
	const Point entranceCell = l.mainPath.front();
	const SpecialCall &sc = p.entrance;
	if (sc.room < 0 || static_cast<size_t>(sc.room) >= n || p.rooms[static_cast<size_t>(sc.room)].cell != entranceCell) return "special room is the wrong cell";
	if (sc.place != (ts.prev.present && popcount(cellOf(l, entranceCell).mask) == 1)) return "place decision wrong";
	// Any Prev group entry reaches the hook and is told which rotation to use. A repeat is suppressed.
	for (int to : ts.prev.piece) {
		const SpecialSteer st = steerSpecial(ts, p, to, 0);
		if ((st.action == SpecialSteer::Action::Redirect) != sc.place) return "steerSpecial disagrees with place";
		if (steerSpecial(ts, p, to, 1).action != SpecialSteer::Action::Suppress) return "a repeated call must be suppressed";
	}
	// The slot's calls: Prev, Down and Next, in every order, each with any of its group entries. The engine scan is
	// "newest free room whose piece is `from`", skipping fixed rooms. Only the Prev call may place anything, and it
	// must hit the entrance room whatever came before or after; Down and Next never move it or place another room.
	const int *down = ts.name == "Crypt" ? kCryptNext : kCaveDown;
	const int *next = ts.name == "Crypt" ? kCaveDown : kCaveNext; // a second suppressed group in either tileset
	const int *groups[3] = {ts.prev.piece.data(), down, next};
	static const int orders[6][3] = {{0, 1, 2}, {0, 2, 1}, {1, 0, 2}, {1, 2, 0}, {2, 0, 1}, {2, 1, 0}};
	for (const auto &order : orders)
		for (int entry = 0; entry < 4; ++entry) {
			std::vector<char> fixed = fixed0;
			std::vector<int> pc = piece;
			int priorPrev = 0, placedRooms = 0;
			for (int call : order) {
				const int to = groups[call][entry];
				const SpecialSteer st = steerSpecial(ts, p, to, call == 0 ? priorPrev : 0);
				if (call == 0) ++priorPrev;
				if (st.action != SpecialSteer::Action::Redirect) {
					if (call != 0 && st.action != SpecialSteer::Action::Suppress) return "a Down / Next call must be suppressed";
					continue;
				}
				if (call != 0) return "a Down / Next call must be suppressed";
				int hit = -1;
				for (int i = static_cast<int>(n) - 1; i >= 0; --i)
					if (!fixed[static_cast<size_t>(i)] && pc[static_cast<size_t>(i)] == st.fromPiece) { hit = i; break; }
				if (hit != sc.room) return "0x3eb850's newest-free-room scan would pick another room";
				// the special piece must be the family piece of the room's open side
				const uint8_t m = mask[static_cast<size_t>(hit)];
				const int idx = m == layout::kWest ? 0 : m == layout::kEast ? 1 : m == layout::kSouth ? 2 : 3;
				if (ts.prev.piece[static_cast<size_t>(idx)] != sc.toPiece) return "special piece for the wrong side";
				pc[static_cast<size_t>(hit)] = sc.toPiece;
				fixed[static_cast<size_t>(hit)] = 1;
				++placedRooms;
			}
			if (placedRooms != (sc.place ? 1 : 0)) return "the entrance was not placed exactly once";
			for (size_t i = 0; i < n; ++i)
				if (pc[i] != (sc.place && static_cast<int>(i) == sc.room ? sc.toPiece : piece[i])) return "a call placed another special room";
		}
	if (sc.place) piece[static_cast<size_t>(sc.room)] = sc.toPiece; // every order ends the same way
	// A piece of no family is never steered.
	if (steerSpecial(ts, p, 1, 0).action != SpecialSteer::Action::Suppress) return "an unknown piece must be suppressed";
	// Reserved rooms: entrance, event and boss, and only those.
	for (size_t i = 0; i < n; ++i) {
		const Room &r = p.rooms[i];
		const bool reserved = r.cell == entranceCell || (l.eventIdx >= 0 && r.cell == l.mainPath[static_cast<size_t>(l.eventIdx)]) ||
		                      (l.bossIdx >= 0 && r.cell == l.mainPath[static_cast<size_t>(l.bossIdx)]);
		if ((r.fix != Room::Fix::None) != reserved) return reserved ? "a reserved room is not fixed" : "a plain room is fixed";
		if ((r.fix == Room::Fix::ByPlacer) != (r.special >= 0)) return "only the placed entrance is left to 0x3eb850 to fix";
		if (piece[i] != r.piece) return "final piece differs from base + mask / special piece";
	}
	if (l.eventIdx >= 0 && p.rooms[static_cast<size_t>(p.eventRoom)].cell != l.mainPath[static_cast<size_t>(l.eventIdx)]) return "event room index";
	if (l.bossIdx >= 0 && p.rooms[static_cast<size_t>(p.bossRoom)].cell != l.mainPath[static_cast<size_t>(l.bossIdx)]) return "boss room index";
	if (p.ring.noOp != ts.callsRingGrower || p.ring.graphInRing != (ts.callsRingGrower && !ts.walkFollowsRing)) return "ring plan";
	if (p.lvlMaze.rooms != static_cast<int>(n) || p.lvlMaze.sizeX != ts.roomTiles || p.lvlMaze.sizeY != ts.roomTiles || p.lvlMaze.merge != 0) return "LvlMaze row";
	return std::string();
}

// Independent no-fit check: brute force every used cell as the first room in a level of the largest allowed size, with
// the first room centred, and test every room's tile box against the level. It uses neither capacity() nor planMaze's
// root rule. Only a one-exit entrance that gets a special room (made last, so never first) is excluded as a root.
bool genuinelyDoesNotFit(const Tileset &ts, const Layout &l)
{
	const int room = ts.roomTiles, level = ts.maxLevelTiles;
	if (level < room) return true;
	const int off = (level - room) / 2;
	const Point entrance = l.mainPath.front();
	for (int ry = 0; ry < l.height; ++ry)
		for (int rx = 0; rx < l.width; ++rx) {
			if (l.at(rx, ry).role == Role::Empty) continue;
			const bool special = Point{rx, ry} == entrance && ts.prev.present && popcount(l.at(rx, ry).mask) == 1;
			if (special) continue;
			bool all = true;
			for (int y = 0; y < l.height && all; ++y)
				for (int x = 0; x < l.width && all; ++x) {
					if (l.at(x, y).role == Role::Empty) continue;
					const int tx = off + (x - rx) * room, ty = off + (y - ry) * room;
					if (tx < 0 || ty < 0 || tx + room > level || ty + room > level) all = false;
				}
			if (all) return false;
		}
	return true;
}

struct Totals {
	// fallbacks = noFit (the layout cannot fit the largest level) + unmetRules (the generator could not meet the shape's
	// rules, so there is no layout to judge) + badShape (rejected before any layout: the clamped grid is too small).
	int maxLevelX = 0;
	int noFit = 0, unmetRules = 0, badShape = 0;
	int seeds = 0, steered = 0, fallbacks = 0, refitted = 0, maxRefits = 0, placedEntrance = 0, rooms = 0;
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
			if (sp.plan.levelTilesX > t.maxLevelX) t.maxLevelX = sp.plan.levelTilesX;
			if (sp.plan.entrance.place) ++t.placedEntrance;
			const std::string bad = verify(ts, sp.layout, sp.plan);
			if (!bad.empty()) {
				FAIL_CHECK(ts.name << " " << layout::shapeName(shape) << " " << layout::sizeName(size) << " seed " << seed << ": " << bad << "\n" << layout::toAscii(sp.layout));
				break;
			}
			if (!sp.plan.reason.empty()) { FAIL_CHECK("a steering plan has a reason"); break; }
		} else {
			++t.fallbacks;
			if (sp.plan.reason.empty()) { FAIL_CHECK("a fallback has no reason"); break; }
			if (sp.layout.cells.empty()) { ++t.badShape; continue; } // rules rejected before any layout; reason names what to change
			if (!sp.layout.rulesMet) {
				// The generator's own fallback broke the shape's rules: not a size question, counted on its own.
				++t.unmetRules;
				if (!sp.generatorUnmet) { FAIL_CHECK("an unmet-rules fallback is not flagged"); break; }
				continue;
			}
			// Every other fallback must be a layout that cannot fit even the largest level (brute-force checked).
			++t.noFit;
			if (!genuinelyDoesNotFit(ts, sp.layout)) {
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
	std::printf("%-6s %-9s %-6s seeds %d  steered %d  fallback %d (%.2f%%: no-fit %d, generator-unmet %d, bad-shape %d)  refitted %d  max refits %d  entrance placed %d  avg rooms %.1f\n",
		ts.name.c_str(), layout::shapeName(shape), layout::sizeName(size), t.seeds, t.steered, t.fallbacks, 100.0 * t.fallbacks / t.seeds, t.noFit, t.unmetRules, t.badShape, t.refitted, t.maxRefits,
		t.placedEntrance, t.steered ? static_cast<double>(t.rooms) / t.steered : 0.0);
}

} // namespace

TEST_CASE("capacity: the first room is centred in a level of any size")
{
	// A 200-tile slot (vanilla levels.txt size), not a statement about what a Cave can hold.
	const Capacity slot = capacityAt(24, 200, 200);
	CHECK(slot.left == 3);
	CHECK(slot.right == 3);
	CHECK(slot.across() == 7);
	CHECK(slot.tall() == 7);
	const Capacity roomy = capacityAt(8, 200, 200);
	CHECK(roomy.left == 12);
	CHECK(roomy.right == 12);
	CHECK(roomy.across() == 25);
	CHECK(capacityAt(24, 24, 24).across() == 1); // exactly one room wide
	CHECK(capacityAt(0, 24, 24).across() == 1);
	CHECK(maxCapacity(caveTileset(200)).across() == 7);
	CHECK(maxCapacity(caveTileset()).across() == 15);
}

TEST_CASE("level size is an output: (2 x k + 1) x room, clamped to the allowed maximum")
{
	const Tileset cave = caveTileset(), crypt = cryptTileset();
	CHECK(levelTilesFor(cave, 6) == 312);
	CHECK(levelTilesFor(cave, 1) == 72);
	CHECK(levelTilesFor(cave, 8) == kMaxLevelTiles);
	CHECK(levelTilesFor(crypt, 12) == 200);
	CHECK(levelTilesFor(caveTileset(200), 6) == 200);
	CHECK(capacityAt(24, 312, 312).left == 6);
	CHECK(capacityAt(24, 312, 312).right == 6);
	const layout::Rules r = layout::presetRules(layout::Shape::Winding, layout::Size::Large);
	for (uint64_t seed = 1; seed <= 200; ++seed) {
		const SeedPlan sp = planForSeed(cave, r, seed);
		REQUIRE(sp.plan.steer);
		CHECK(sp.plan.levelTilesX >= levelTilesFor(cave, sp.plan.widthRooms / 2));
		CHECK(sp.plan.levelTilesX <= kMaxLevelTiles);
		CHECK(sp.plan.levelTilesY <= kMaxLevelTiles);
		CHECK(verify(cave, sp.layout, sp.plan).empty());
	}
	// A smaller tileset asks for a smaller level.
	const SeedPlan c = planForSeed(crypt, r, 11);
	REQUIRE(c.plan.steer);
	CHECK(c.plan.levelTilesX <= 200);
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

TEST_CASE("10k seeds per shape, every size: pieces, ends, reserved rooms, fallback only when it cannot fit")
{
	// Cave and Crypt at the allowed maximum, and Cave in a 200-tile slot (the fallback path).
	struct Config { Tileset ts; const char *label; bool slot200; };
	const Config configs[3] = {{caveTileset(), "Cave", false}, {cryptTileset(), "Crypt", false}, {caveTileset(200), "Cave in a 200-tile slot", true}};
	for (const Config &cfg : configs)
		for (int s = 0; s < layout::kShapeCount; ++s)
			for (int z = 0; z < layout::kSizeCount; ++z) {
				const Tileset &ts = cfg.ts;
				const layout::Shape shape = static_cast<layout::Shape>(s);
				const layout::Size size = static_cast<layout::Size>(z);
				const Totals t = runSeeds(ts, shape, size);
				if (std::getenv("CARTOGRAPHER_STATS") && cfg.slot200) std::printf("[200-tile slot] ");
				printStats(ts, shape, size, t);
				INFO(cfg.label << " " << layout::shapeName(shape) << " " << layout::sizeName(size));
				CHECK(t.steered + t.fallbacks == seedCount());
				CHECK(t.noFit + t.unmetRules + t.badShape == t.fallbacks);
				CHECK(t.maxLevelX <= ts.maxLevelTiles);
				if (!cfg.slot200) {
					// With the level size chosen by the plan, no layout is ever too big: only the generator may give up.
					CHECK(t.noFit == 0);
					CHECK(t.badShape == 0);
				} else {
					// In a 200-tile slot, Cave holds 7 x 7 rooms: these combinations cannot be built in it at all (Open is not one: its grid shrinks to fit).
					const bool impossible = (shape == layout::Shape::Sprawl && size != layout::Size::Small && size != layout::Size::ExtraSmall) ||
					                        (shape == layout::Shape::Loop && size == layout::Size::Large);
					CHECK((t.steered > 0) != impossible);
				}
				// Crypt rooms are tiny: nothing may fall back, whatever the reason.
				if (cfg.ts.name == "Crypt") CHECK(t.fallbacks == 0);
				// A one-exit entrance gets its special piece in every shape except the rings and Open (its entrance has two or more exits).
				if (shape != layout::Shape::Loop && shape != layout::Shape::Sprawl && shape != layout::Shape::Open) CHECK(t.placedEntrance == t.steered);
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
			// The entrance is a dead end and the last room made. The far end is an ordinary mask room.
			CHECK(p.entrance.place);
			CHECK(p.entrance.room == static_cast<int>(p.rooms.size()) - 1);
			CHECK(p.rooms[static_cast<size_t>(p.entrance.room)].cell == sp.layout.mainPath.front());
			CHECK(p.rooms[static_cast<size_t>(p.entrance.room)].piece == p.entrance.toPiece);
			for (const Room &r : p.rooms)
				if (r.special < 0) CHECK(r.piece == c.ts.pieceBase + r.mask);
			CHECK(!p.ring.noOp);
			CHECK(p.lvlMaze.merge == 0);
			CHECK(p.eventRoom >= 0);
			CHECK(p.bossRoom >= 0);
			CHECK(p.rooms[static_cast<size_t>(p.eventRoom)].fix == Room::Fix::AfterWalk);
			// The boss room is an ordinary mask room, fixed by the hook after the walk.
			CHECK(p.rooms[static_cast<size_t>(p.bossRoom)].fix == Room::Fix::AfterWalk);
			CHECK(p.rooms[static_cast<size_t>(p.bossRoom)].special < 0);
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
	CHECK(planHash(planForSeed(cave, layout::presetRules(layout::Shape::Corridor, layout::Size::Small), 1).plan) == 13524385595187565503ull);
	CHECK(planHash(planForSeed(crypt, layout::presetRules(layout::Shape::Branching, layout::Size::Medium), 1).plan) == 8847548717809474728ull);
}

TEST_CASE("fixed-flag timing: a placed end fixed before 0x3eb850 runs is missed by its scan")
{
	for (const Tileset &ts : {caveTileset(), cryptTileset()})
		for (uint64_t seed = 1; seed <= 50; ++seed) {
			const SeedPlan sp = planForSeed(ts, layout::presetRules(layout::Shape::Corridor, layout::Size::Medium), seed);
			REQUIRE(sp.plan.steer);
			CHECK(verify(ts, sp.layout, sp.plan).empty());
			// The wrong order (fix the entrance right after the walk) must be caught by the replay.
			CHECK(verify(ts, sp.layout, sp.plan, true) == "0x3eb850's newest-free-room scan would pick another room");
			for (const Room &r : sp.plan.rooms) CHECK((r.fix == Room::Fix::ByPlacer) == (r.special >= 0));
		}
}

TEST_CASE("a maze has an entrance and no exit: Down and Next calls never move the entrance or place a room")
{
	// verify() replays the slot's Prev, Down and Next calls in all six orders (and every group entry) and requires that
	// the entrance is the only room ever turned into a special room, wherever Prev comes.
	for (const Tileset &ts : {caveTileset(), cryptTileset()}) {
		const int *down = ts.name == "Crypt" ? kCryptNext : kCaveDown;
		for (uint64_t seed = 1; seed <= 200; ++seed)
			for (layout::Shape shape : {layout::Shape::Corridor, layout::Shape::Winding, layout::Shape::Branching}) {
				const SeedPlan sp = planForSeed(ts, layout::presetRules(shape, layout::Size::Small), seed);
				REQUIRE(sp.plan.steer);
				REQUIRE(sp.plan.entrance.place);
				INFO(verify(ts, sp.layout, sp.plan));
				CHECK(verify(ts, sp.layout, sp.plan).empty());
				for (int e = 0; e < 4; ++e) {
					CHECK(steerSpecial(ts, sp.plan, down[e]).action == SpecialSteer::Action::Suppress);
					CHECK(steerSpecial(ts, sp.plan, kCaveNext[e]).action == SpecialSteer::Action::Suppress);
					CHECK(steerSpecial(ts, sp.plan, ts.prev.piece[static_cast<size_t>(e)]).action == SpecialSteer::Action::Redirect);
					// a second Prev is a stray
					CHECK(steerSpecial(ts, sp.plan, ts.prev.piece[static_cast<size_t>(e)], 1).action == SpecialSteer::Action::Suppress);
				}
				// The far end (boss room) is ordinary: its piece is base + mask.
				const Room &boss = sp.plan.rooms[static_cast<size_t>(sp.plan.bossRoom)];
				CHECK(boss.piece == ts.pieceBase + boss.mask);
				CHECK(boss.special < 0);
			}
	}
}

TEST_CASE("verify is not vacuous: a plan whose entrance names the wrong room is rejected")
{
	const Tileset ts = caveTileset();
	const SeedPlan sp = planForSeed(ts, layout::presetRules(layout::Shape::Corridor, layout::Size::Small), 7);
	REQUIRE(sp.plan.steer);
	Plan bad = sp.plan;
	bad.entrance.room = 0;
	CHECK(!verify(ts, sp.layout, bad).empty());
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
		// A ring's entrance has two exits: the special call is suppressed, with a reason.
		CHECK(!sp.plan.entrance.place);
		CHECK(!sp.plan.entrance.note.empty());
		CHECK(sp.plan.rooms[static_cast<size_t>(sp.plan.entrance.room)].piece == crypt.pieceBase + sp.plan.rooms[static_cast<size_t>(sp.plan.entrance.room)].mask);
	}
	CHECK(withLinks == 200);
}

TEST_CASE("a tileset without an entrance family suppresses the call and keeps the entrance an ordinary fixed room")
{
	Tileset ts = caveTileset();
	ts.prev = DeadEndFamily{};
	for (uint64_t seed = 1; seed <= 200; ++seed) {
		const SeedPlan sp = planForSeed(ts, layout::presetRules(layout::Shape::Corridor, layout::Size::Small), seed);
		REQUIRE(sp.plan.steer);
		CHECK(verify(ts, sp.layout, sp.plan).empty());
		CHECK(!sp.plan.entrance.place);
		CHECK(!sp.plan.entrance.note.empty());
		CHECK(steerSpecial(ts, sp.plan, 83).action == SpecialSteer::Action::Suppress);
		CHECK(sp.plan.rooms[static_cast<size_t>(sp.plan.entrance.room)].fix == Room::Fix::AfterWalk);
	}
}

TEST_CASE("steerSpecial: the hook is told which rotation, and strays are suppressed")
{
	const Tileset ts = caveTileset();
	const SeedPlan sp = planForSeed(ts, layout::presetRules(layout::Shape::Corridor, layout::Size::Small), 7);
	REQUIRE(sp.plan.steer);
	const SpecialCall &en = sp.plan.entrance;
	SpecialSteer s = steerSpecial(ts, sp.plan, en.toPiece);
	CHECK(s.action == SpecialSteer::Action::Redirect);
	CHECK(s.fromPiece == en.fromPiece);
	CHECK(en.fromPiece >= ts.pieceBase + 1);
	CHECK(popcount(static_cast<unsigned>(en.fromPiece - ts.pieceBase)) == 1);
	// Every Down entry is a stray: a maze has no exit.
	for (int to : kCaveDown) CHECK(steerSpecial(ts, sp.plan, to).action == SpecialSteer::Action::Suppress);
	CHECK(steerSpecial(ts, sp.plan, 99).action == SpecialSteer::Action::Suppress);
	Plan off = sp.plan;
	off.steer = false;
	CHECK(steerSpecial(ts, off, en.toPiece).action == SpecialSteer::Action::Suppress);
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
	const Tileset slotCave = caveTileset(200);
	// 2. The layout is wider than the largest allowed level holds (a Cave capped at a 200-tile slot: 7 across).
	{
		bool found = false;
		for (uint64_t seed = 1; seed < 400 && !found; ++seed) {
			const Layout l = layout::generate(layout::presetRules(layout::Shape::Winding, layout::Size::Large), seed);
			int minX = l.width, maxX = -1;
			for (int y = 0; y < l.height; ++y)
				for (int x = 0; x < l.width; ++x)
					if (l.at(x, y).role != Role::Empty) { minX = std::min(minX, x); maxX = std::max(maxX, x); }
			if (maxX - minX + 1 > 7) {
				const Plan p = planMaze(slotCave, l);
				CHECK(!p.steer);
				CHECK(p.reason.find("across") != std::string::npos);
				CHECK(p.reason.find("smaller size") != std::string::npos);
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
		CHECK(p.reason.find("shape") != std::string::npos);
	}
	// 5. A level with room for one room only: nothing fits, and planForSeed says so after its refits.
	{
		Tileset ts = cave;
		ts.maxLevelTiles = 24;
		const SeedPlan sp = planForSeed(ts, layout::presetRules(layout::Shape::Corridor, layout::Size::Small), 1);
		CHECK(!sp.plan.steer);
		CHECK(!sp.plan.reason.empty());
	}
	// 6. Plain English: no code names in the reasons.
	{
		Tileset ts = cave;
		ts.maxLevelTiles = 72; // 1 + 1 + 1 rooms: a corridor of 3 can not hold a 10-room path
		const SeedPlan sp = planForSeed(ts, layout::presetRules(layout::Shape::Corridor, layout::Size::Small), 1);
		CHECK(!sp.plan.steer);
		CHECK(sp.plan.reason.find("0x") == std::string::npos);
		// No preset field names: an author never sees them.
		for (const char *name : {"MaxPath", "MinPath", "Width", "Height", "grid", "Rules", "rules"})
			CHECK(sp.plan.reason.find(name) == std::string::npos);
		CHECK(sp.plan.reason.find("smaller size or a simpler shape") != std::string::npos);
	}
}

TEST_CASE("fitRules clamps the grid to the level and leaves a roomy level alone")
{
	const layout::Rules r = layout::presetRules(layout::Shape::Winding, layout::Size::Large);
	CHECK(r.width == 12);
	CHECK(fitRules(caveTileset(200), r).width == 7);  // a 200-tile slot
	CHECK(fitRules(caveTileset(200), r).height == 7);
	CHECK(fitRules(caveTileset(), r).width == 12);    // the allowed maximum holds the whole Large grid
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
	CHECK(!p.entrance.place); // two exits, so the entrance stays an ordinary room
	CHECK(p.widthRooms == 3);
	CHECK(p.heightRooms == 3);
}

// --- the pure part of the hooks (steer.h) --------------------------------------------------------------------------

namespace {

// The Cave Prev group as the game's table lists it, N E S W: {from, to, file, dir}.
Group caveGroup()
{
	return Group{{{60, 86, -1, 3}, {54, 84, -1, 0}, {56, 85, -1, 1}, {53, 83, -1, 2}}};
}

Plan steeringCavePlan(const Tileset &ts)
{
	const layout::Rules r = layout::presetRules(layout::Shape::Winding, layout::Size::Medium);
	for (uint64_t seed = 1; seed < 200; ++seed) {
		const SeedPlan sp = planForSeed(ts, r, seed);
		if (sp.plan.steer && sp.plan.entrance.place) return sp.plan;
	}
	return Plan{};
}

} // namespace

TEST_CASE("built-in tilesets are Cave and Crypt, found ignoring case")
{
	Tileset t;
	REQUIRE(builtinTileset("cave", t));
	CHECK(t.roomTiles == 24);
	CHECK(t.pieceBase == 52);
	CHECK(t.prev.piece[3] == 86);
	REQUIRE(builtinTileset("CRYPT", t));
	CHECK(t.roomTiles == 8);
	CHECK(!builtinTileset("Lava", t));
	CHECK(!builtinTileset("", t));
}

TEST_CASE("the slot row's declared size clamps the tileset and a row below one room is refused")
{
	Tileset cave;
	REQUIRE(builtinTileset("Cave", cave));
	const SlotFit big = clampToSlot(cave, 1000, 1000);
	REQUIRE(big.ok);
	CHECK(big.tileset.maxLevelTiles == kMaxLevelTiles); // never more than the allowed maximum
	const SlotFit small = clampToSlot(cave, 200, 360);
	REQUIRE(small.ok);
	CHECK(small.tileset.maxLevelTiles == 200); // the smaller declared side
	CHECK(maxCapacity(small.tileset).across() == 7);
	const SlotFit tiny = clampToSlot(cave, 20, 20);
	CHECK(!tiny.ok);
	CHECK(tiny.reason.find("less than one Cave room") != std::string::npos);
	CHECK(!clampToSlot(cave, 0, 0).ok);
	// A plan on the clamped tileset never asks for more than the row declares.
	const layout::Rules r = layout::presetRules(layout::Shape::Winding, layout::Size::Large);
	for (uint64_t seed = 1; seed <= 100; ++seed) {
		const SeedPlan sp = planForSeed(small.tileset, r, seed);
		if (!sp.plan.steer) continue;
		CHECK(sp.plan.levelTilesX <= 200);
		CHECK(sp.plan.levelTilesY <= 200);
	}
}

TEST_CASE("planForMap: a shapes.txt shape plans within the size caps across many seeds, like a built-in one")
{
	const layout::ParseResult parsed = layout::parseShapesTxt(
		"Name\tBase\tMinPath\tMaxPath\tMinBranches\tMaxBranches\n"
		"Snake\tWinding\t12\t16\t0\t1\n"
		"Fat Snake\tSnake\t\t\t2\t3\n");
	REQUIRE(parsed.errors.empty());
	// The slot's declared size is the cap: 360 is the most any slot gets, 200 is a smaller one.
	for (const int slot : {360, 200}) {
		for (const char *tileset : {"Cave", "Crypt"}) {
			for (const char *name : {"Snake", "fat snake"}) {
				for (const char *size : {"Small", "Medium", "Large"}) {  // ExtraSmall (4 x 4) is too tight for these 12+ cell shapes
					int steered = 0;
					for (uint64_t seed = 1; seed <= 150; ++seed) {
						const MapPlan p = planForMap(tileset, parsed.table, name, size, seed, slot, slot);
						INFO(tileset << " " << name << " " << size << " slot " << slot << " seed " << seed << ": " << p.reason);
						if (!p.steer) {
							CHECK_FALSE(p.reason.empty());
							continue;
						}
						++steered;
						CHECK(p.seedPlan.plan.levelTilesX <= slot);
						CHECK(p.seedPlan.plan.levelTilesY <= slot);
						CHECK(p.hash == planForMap(tileset, parsed.table, name, size, seed, slot, slot).hash);
					}
					if (slot == 360) CHECK(steered >= 75);  // a small slot may fall back often; a full one rarely
				}
			}
		}
	}
}

TEST_CASE("planForMap: a shape whose shapes.txt row was bad falls back with a reason")
{
	const layout::ParseResult parsed = layout::parseShapesTxt(
		"Name\tBase\tMinPath\tMaxPath\n"
		"Snake\tWinding\t12\t16\n"
		"Broken\tWinding\tmany\t16\n");
	CHECK_FALSE(parsed.errors.empty());
	CHECK(planForMap("Cave", parsed.table, "Snake", "Medium", 7, 360, 360).steer);
	const MapPlan bad = planForMap("Cave", parsed.table, "Broken", "Medium", 7, 360, 360);
	CHECK_FALSE(bad.steer);
	CHECK(bad.reason.find("Broken") != std::string::npos);
	CHECK(bad.reason.find("cannot be used") != std::string::npos);
}

TEST_CASE("planForMap: same {map, seed, slot size} gives the same plan; the slot size bounds it; bad input gives a reason")
{
	const layout::ShapeTable none;
	const MapPlan a = planForMap("Cave", none, "Winding", "Medium", 12345, 360, 360);
	const MapPlan b = planForMap("cave", none, "winding", "medium", 12345, 360, 360);
	REQUIRE(a.steer);
	REQUIRE(b.steer);
	CHECK(a.hash == b.hash);
	CHECK(a.seedPlan.plan.levelTilesX <= 360);
	const MapPlan c = planForMap("Cave", none, "Winding", "Large", 777, 200, 200);
	if (c.steer) {
		CHECK(c.seedPlan.plan.levelTilesX <= 200);
		CHECK(c.seedPlan.plan.levelTilesY <= 200);
	} else {
		CHECK(!c.reason.empty());
	}
	CHECK(planForMap("Crypt", none, "Loop", "Small", 5, 360, 360).steer);
	CHECK(planForMap("Lava", none, "Winding", "Medium", 1, 360, 360).reason.find("no maze numbers") != std::string::npos);
	CHECK(planForMap("Cave", none, "Zigzag", "Medium", 1, 360, 360).reason.find("cannot be used") != std::string::npos);
	CHECK(planForMap("Cave", none, "Winding", "Huge", 1, 360, 360).reason.find("Small, Medium or Large") != std::string::npos);
	CHECK(planForMap("Cave", none, "Winding", "Medium", 1, 10, 10).reason.find("less than one Cave room") != std::string::npos);
	CHECK(mixSeed(1, 2) != mixSeed(2, 1));
	CHECK(mixSeed(1, 2) == mixSeed(1, 2));
}

TEST_CASE("the rotation counter stays in 0..3 whatever the call did")
{
	CHECK(rotationAfter(0) == 1);
	CHECK(rotationAfter(3) == 0);
	for (uint32_t r = 0; r < 4; ++r) CHECK(rotationAfter(r) < 4);
}

TEST_CASE("the entrance call is redirected to the plan's table entry and every other call is suppressed")
{
	Tileset cave;
	REQUIRE(builtinTileset("Cave", cave));
	const Plan plan = steeringCavePlan(cave);
	REQUIRE(plan.steer);
	REQUIRE(plan.entrance.place);
	const Group group = caveGroup();
	// Whichever rotation the game rolled, the Prev call lands on the entry the plan wants.
	for (uint32_t rot = 0; rot < 4; ++rot) {
		SpecialState st;
		const SpecialAnswer a = answerSpecial(cave, plan, st, &group, rot);
		REQUIRE(a.kind == SpecialAnswer::Kind::Redirect);
		CHECK(group[static_cast<size_t>(a.index)].from == plan.entrance.fromPiece);
		CHECK(group[static_cast<size_t>(a.index)].to == plan.entrance.toPiece);
		CHECK(st.placed);
		CHECK(specialProblem(plan, st).empty());
		// A second Prev call is suppressed: the entrance never moves.
		CHECK(answerSpecial(cave, plan, st, &group, rotationAfter(rot)).kind == SpecialAnswer::Kind::Suppress);
	}
	// A Down group call (to 91..94) is suppressed, and does not count as a Prev call.
	const Group down{{{60, 94, -1, 3}, {54, 92, -1, 0}, {56, 93, -1, 1}, {53, 91, -1, 2}}};
	SpecialState st;
	CHECK(answerSpecial(cave, plan, st, &down, 1).kind == SpecialAnswer::Kind::Suppress);
	CHECK(st.prevCalls == 0);
	CHECK(!st.placed);
	CHECK(!specialProblem(plan, st).empty()); // the entrance was never asked for: a fallback to report
	// Down first, then Prev: the entrance is still placed.
	CHECK(answerSpecial(cave, plan, st, &group, 2).kind == SpecialAnswer::Kind::Redirect);
	CHECK(specialProblem(plan, st).empty());
}

TEST_CASE("a table that cannot be read or lacks the plan's entry places nothing and says why")
{
	Tileset cave;
	REQUIRE(builtinTileset("Cave", cave));
	const Plan plan = steeringCavePlan(cave);
	REQUIRE(plan.entrance.place);
	const Group group = caveGroup();
	SpecialState a;
	CHECK(answerSpecial(cave, plan, a, &group, 4).kind == SpecialAnswer::Kind::Suppress);
	CHECK(!specialProblem(plan, a).empty());
	SpecialState b;
	CHECK(answerSpecial(cave, plan, b, nullptr, 0).kind == SpecialAnswer::Kind::Suppress);
	CHECK(!b.placed);
	Group wrong = group;
	for (TableEntry &e : wrong) e.from = 1; // no entry starts at the piece the plan wants
	SpecialState c;
	CHECK(answerSpecial(cave, plan, c, &wrong, 0).kind == SpecialAnswer::Kind::Suppress);
	CHECK(specialProblem(plan, c).find("no entry from piece") != std::string::npos);
	// A plan that does not steer leaves every call to the game.
	Plan off;
	SpecialState d;
	CHECK(answerSpecial(cave, off, d, &group, 0).kind == SpecialAnswer::Kind::Original);
}

TEST_CASE("the private LvlMaze row and the piece every room has after the walk")
{
	Tileset cave;
	REQUIRE(builtinTileset("Cave", cave));
	const Plan plan = steeringCavePlan(cave);
	REQUIRE(plan.steer);
	const auto row = lvlMazeRow(138, plan);
	CHECK(row[0] == 138);
	CHECK(row[1] == plan.lvlMaze.rooms);
	CHECK(row[2] == row[1]);
	CHECK(row[3] == row[1]);
	CHECK(row[4] == 24);
	CHECK(row[5] == 24);
	CHECK(row[6] == 0);
	for (size_t i = 0; i < plan.rooms.size(); ++i) {
		const int idx = static_cast<int>(i);
		CHECK(pieceAfterWalk(plan, idx) == cave.pieceBase + plan.rooms[i].mask);
		CHECK(fixAfterWalk(plan, idx) == (plan.rooms[i].fix == Room::Fix::AfterWalk));
	}
	CHECK(pieceAfterWalk(plan, -1) == 0);
	CHECK(!fixAfterWalk(plan, 9999));
}

TEST_CASE("declared sizes: the first size seen in a game is the limit, whatever the level record holds later")
{
	DeclaredSizes d;
	CHECK(d.record(200, 360, 360).x == 360);
	// the hooks wrote a small plan's size into the level record; the next plan still sees 360
	const DeclaredSizes::Size again = d.record(200, 96, 96);
	CHECK(again.x == 360);
	CHECK(again.y == 360);
	// another level id has its own record
	CHECK(d.record(201, 120, 130).y == 130);
	// a failed read is not recorded
	CHECK(d.record(202, 0, 0).x == 0);
	CHECK(d.record(202, 80, 80).x == 80);
	// a small map followed by a large one is limited by the row, not by the small map's box
	Tileset ts;
	REQUIRE(builtinTileset("Cave", ts));
	CHECK(clampToSlot(ts, again.x, again.y).tileset.maxLevelTiles == std::min(ts.maxLevelTiles, std::min(360, kMaxLevelTiles)));
	d.clear();
	CHECK(d.record(200, 96, 96).x == 96);
}

TEST_CASE("slot build state: only a build with a map open uses the slot up, and a running build blocks an open")
{
	SlotBuildState st;
	CHECK(!st.blocked());
	st.begin();  // a build with no map open (lazy world lookup)
	CHECK(st.blocked());
	st.end(false);
	CHECK(!st.blocked());
	st.begin();  // a build for an opened map
	st.end(true);
	CHECK(st.blocked());
	st.clear();
	CHECK(!st.blocked());
}

TEST_CASE("slot build state: ranAny sees a build with no map open, which blocked() does not")
{
	SlotBuildState st;
	CHECK(!st.ranAny());
	st.begin();
	CHECK(st.ranAny());  // running
	st.end(false);
	CHECK(st.ranAny());
	CHECK(!st.blocked());
	st.clear();
	CHECK(!st.ranAny());
}

TEST_CASE("a maze slot level serves one tileset: the other is refused in plain English")
{
	Tileset cave, crypt;
	REQUIRE(builtinTileset("Cave", cave));
	REQUIRE(builtinTileset("Crypt", crypt));
	CHECK(slotTypeProblem(cave, 3, "A").empty());
	CHECK(slotTypeProblem(crypt, 4, "B").empty());
	const std::string wrong = slotTypeProblem(crypt, 3, "Deep");
	CHECK(wrong.find("Cave level (LevelType 3)") != std::string::npos);
	CHECK(wrong.find("Crypt map 'Deep'") != std::string::npos);
	CHECK(slotTypeProblem(cave, 4, "C").find("Crypt level") != std::string::npos);
	CHECK(slotTypeProblem(cave, 5, "C").find("none of the maze tilesets") != std::string::npos);
	CHECK(slotTypeProblem(cave, -1, "C").find("could not read the LevelType") != std::string::npos);
	// Each message names the tileset's own slot.
	CHECK(slotTypeProblem(cave, 4, "C").find("the Maze slot level") != std::string::npos);
	CHECK(slotTypeProblem(crypt, 3, "D").find("the MazeCrypt slot level") != std::string::npos);
	CHECK(slotTypeProblem(crypt, -1, "D").find("MazeCrypt row in slots.txt") != std::string::npos);
}

// --- The tileset table (maze_tilesets.h) -------------------------------------------------------------------------------------

TEST_CASE("the maze tileset table holds Cave then Crypt, with the values the hooks and the installer rely on")
{
	// Pinned: Cave keeps the slot row, level name and LevelType of the installs before the table existed; Crypt's slot level is its own.
	REQUIRE(MazeTilesetCount >= 3);  // Cave, Crypt, then Ice (pinned in its own test below)
	CHECK(std::string(MazeTilesets[0].name) == "Cave");
	CHECK(MazeTilesets[0].levelType == 3);
	CHECK(std::string(MazeTilesets[0].slot) == "Maze");
	CHECK(std::string(MazeTilesets[0].levelName) == "Cartographer Maze");
	CHECK(MazeTilesets[0].roomTiles == 24);
	CHECK(MazeTilesets[0].pieceBase == 52);
	CHECK(MazeTilesets[0].prevPiece[0] == 83);
	CHECK(MazeTilesets[0].prevPiece[3] == 86);
	CHECK_FALSE(MazeTilesets[0].callsRingGrower);
	CHECK(std::string(MazeTilesets[1].name) == "Crypt");
	CHECK(MazeTilesets[1].levelType == 4);
	CHECK(std::string(MazeTilesets[1].slot) == "MazeCrypt");
	CHECK(std::string(MazeTilesets[1].levelName) == "Cartographer Crypt");
	CHECK(MazeTilesets[1].roomTiles == 8);
	CHECK(MazeTilesets[1].pieceBase == 108);
	CHECK(MazeTilesets[1].prevPiece[0] == 139);
	CHECK(MazeTilesets[1].prevPiece[3] == 142);
	CHECK_FALSE(MazeTilesets[1].callsRingGrower);
	CHECK(findMazeTileset("cRyPt") == &MazeTilesets[1]);
	CHECK(findMazeTileset("Lava") == nullptr);
	CHECK(findMazeTilesetBySlot("mazecrypt") == &MazeTilesets[1]);
	CHECK(findMazeTilesetBySlot("Maze") == &MazeTilesets[0]);
	CHECK(findMazeTilesetBySlot("Outdoor") == nullptr);
	CHECK(mazeTilesetIndex("crypt") == 1);
	CHECK(mazeTilesetIndex("Lava") == MazeTilesetCount);
	CHECK(mazeTilesetNames().rfind("Cave, Crypt", 0) == 0);  // the table order, last name after "or"
	// The slots are distinct and every LevelType is used once: one slot level, one tileset.
	for (size_t i = 0; i < MazeTilesetCount; ++i)
		for (size_t j = i + 1; j < MazeTilesetCount; ++j) {
			CHECK(std::string(MazeTilesets[i].slot) != MazeTilesets[j].slot);
			CHECK(std::string(MazeTilesets[i].levelName) != MazeTilesets[j].levelName);
			CHECK(MazeTilesets[i].levelType != MazeTilesets[j].levelType);
		}
	// The planner's Tileset is made from the row.
	for (const MazeTileset &row : MazeTilesets) {
		Tileset t;
		REQUIRE(builtinTileset(row.name, t));
		CHECK(t.name == row.name);
		CHECK(t.levelType == row.levelType);
		CHECK(t.roomTiles == row.roomTiles);
		CHECK(t.pieceBase == row.pieceBase);
		CHECK(t.callsRingGrower == row.callsRingGrower);
		CHECK(t.prev.present);
		for (int i = 0; i < 4; ++i) CHECK(t.prev.piece[i] == row.prevPiece[i]);
	}
	const MapPlan refused = planForMap("Lava", layout::ShapeTable{}, "Loop", "Medium", 1, 360, 360);
	CHECK_FALSE(refused.steer);
	CHECK(refused.reason == "Cartographer has no maze numbers for the tileset 'Lava'. Use " + mazeTilesetNames() + ".");
}

TEST_CASE("tools/install.ps1 lists the same maze tilesets as the table, in the same order")
{
	std::string here = __FILE__;
	std::ifstream in(here.substr(0, here.find_last_of("/\\") + 1) + "../tools/install.ps1", std::ios::binary);
	REQUIRE(in.good());
	std::stringstream all;
	all << in.rdbuf();
	const std::string text = all.str();
	const size_t start = text.find("$MazeTilesets = @(");
	REQUIRE(start != std::string::npos);
	const std::string block = text.substr(start, text.find("\n)", start) - start);
	// Each row reads  @{ Tileset = 'Cave'; Slot = 'Maze'; LevelName = '...'; LevelType = '3'; ...
	const auto field = [](const std::string &row, const char *key) {
		const size_t at = row.find(std::string(key) + " = '");
		REQUIRE(at != std::string::npos);
		const size_t from = at + std::string(key).size() + 4;
		return row.substr(from, row.find('\'', from) - from);
	};
	size_t row = 0, at = 0;
	while ((at = block.find("@{", at)) != std::string::npos) {
		const std::string body = block.substr(at, block.find('}', at) - at);
		REQUIRE(row < MazeTilesetCount);
		CHECK(field(body, "Tileset") == MazeTilesets[row].name);
		CHECK(field(body, "Slot") == MazeTilesets[row].slot);
		CHECK(field(body, "LevelName") == MazeTilesets[row].levelName);
		CHECK(field(body, "LevelType") == std::to_string(MazeTilesets[row].levelType));
		CHECK(field(body, "Size") == std::to_string(levelTiles));
		// The installer's -<Param>OffsetX / -<Param>OffsetY defaults are the row's offsets.
		const std::string param = field(body, "Param");
		for (const char *axis : {"X", "Y"}) {
			const std::string key = "[int]$" + param + "Offset" + axis + " = ";
			const size_t k = text.find(key);
			REQUIRE(k != std::string::npos);
			CHECK(std::stoi(text.substr(k + key.size())) == (axis[0] == 'X' ? MazeTilesets[row].offsetX : MazeTilesets[row].offsetY));
		}
		++row;
		at += 2;
	}
	CHECK(row == MazeTilesetCount);
}

TEST_CASE("every maze slot level keeps its revealed map: offsets are in the 16-bit automap range and overlap nothing")
{
	// Range: the corners of each level, and a rule check of the function itself against the cell arithmetic.
	for (const MazeTileset &row : MazeTilesets) {
		CHECK_MESSAGE(automapInRange(row.offsetX, row.offsetY, levelTiles), row.name);
		for (const int tx : {row.offsetX, row.offsetX + levelTiles})
			for (const int ty : {row.offsetY, row.offsetY + levelTiles}) {
				CHECK((tx - ty) * 8 >= -32768);
				CHECK((tx - ty) * 8 <= 32767);
				CHECK((tx + ty) * 4 <= 32767);
				CHECK(tx >= 0);
				CHECK(ty >= 0);
			}
		// Above the first Act 5 level of the game (Harrogath and the others start at y 1000), so no vanilla level is hit.
		CHECK(row.offsetY + levelTiles <= 1000);
	}
	// The old spots were out of range: Cave at 6000,0.
	CHECK_FALSE(automapInRange(6000, 0, levelTiles));
	CHECK(automapInRange(3500, 0, levelTiles));
	// The planned spots of the rows still to come (Lava) and Snow (192 wide, installer default 1500,0) are in range and apart from every row
	// and from each other: the rule is x 3500, 3000, 2500, 2000 for the maze rows, y 0.
	struct Spot { int x, y, size; };
	constexpr Spot planned[] = {{2500, 0, levelTiles}, {1500, 0, 192}};
	for (const Spot &a : planned) {
		CHECK(automapInRange(a.x, a.y, a.size));
		CHECK(a.y + a.size <= 1000);
		for (const MazeTileset &row : MazeTilesets)
			CHECK((a.x + a.size <= row.offsetX || row.offsetX + levelTiles <= a.x));
		for (const Spot &b : planned)
			if (&a != &b) CHECK((a.x + a.size <= b.x || b.x + b.size <= a.x));
	}
	// No two levels overlap (rectangles of levelTiles x levelTiles).
	for (size_t i = 0; i < MazeTilesetCount; ++i)
		for (size_t j = i + 1; j < MazeTilesetCount; ++j) {
			const bool apart = MazeTilesets[i].offsetX + levelTiles <= MazeTilesets[j].offsetX || MazeTilesets[j].offsetX + levelTiles <= MazeTilesets[i].offsetX ||
			                   MazeTilesets[i].offsetY + levelTiles <= MazeTilesets[j].offsetY || MazeTilesets[j].offsetY + levelTiles <= MazeTilesets[i].offsetY;
			CHECK_MESSAGE(apart, MazeTilesets[i].name << " overlaps " << MazeTilesets[j].name);
		}
}

TEST_CASE("the Ice row: 16-tile rooms, mask pieces from 1003, the one-exit Prev family 1018-1021, the ring grower called first")
{
	const MazeTileset *row = findMazeTileset("ice");
	REQUIRE(row != nullptr);
	CHECK(row == &MazeTilesets[mazeTilesetIndex("Ice")]);
	CHECK(row->levelType == 33);
	CHECK(std::string(row->slot) == "MazeIce");
	CHECK(std::string(row->levelName) == "Cartographer Ice Caves");
	CHECK(row->roomTiles == 16);
	CHECK(row->pieceBase == 1002);  // base + mask: W 1003, E 1004, S 1006, N 1010 ... SEW+N 1017
	CHECK(row->pieceBase + 15 == 1017);
	CHECK(row->prevPiece[0] == 1018);
	CHECK(row->prevPiece[1] == 1019);
	CHECK(row->prevPiece[2] == 1020);
	CHECK(row->prevPiece[3] == 1021);
	CHECK(row->callsRingGrower);  // docs/reverse-engineering/cartographer/24-ice-caves-builder.md
	CHECK(findMazeTilesetBySlot("mazeice") == row);
	Tileset t;
	REQUIRE(builtinTileset("Ice", t));
	CHECK(t.walkFollowsRing);  // the walk 0x3e96d0 runs after the ring grower in the case, so the graph is built in the walk
}

TEST_CASE("Ice plans: every shape and size builds, the entrance is a Prev piece, Next and Down are suppressed, the ring grower is skipped")
{
	Tileset ts;
	REQUIRE(builtinTileset("Ice", ts));
	const layout::Shape shapes[] = {layout::Shape::Corridor, layout::Shape::Winding, layout::Shape::Branching, layout::Shape::Loop};
	const layout::Size sizes[] = {layout::Size::ExtraSmall, layout::Size::Small, layout::Size::Medium, layout::Size::Large};
	int steered = 0;
	for (layout::Shape shape : shapes)
		for (layout::Size size : sizes)
			for (uint64_t seed = 1; seed <= 25; ++seed) {
				const SeedPlan sp = planForSeed(ts, layout::presetRules(shape, size), seed);
				if (!sp.plan.steer) continue;
				++steered;
				INFO("shape " << static_cast<int>(shape) << " size " << static_cast<int>(size) << " seed " << seed << ": " << verify(ts, sp.layout, sp.plan));
				CHECK(verify(ts, sp.layout, sp.plan).empty());
				CHECK(sp.plan.ring.noOp);
				CHECK_FALSE(sp.plan.ring.graphInRing);
				CHECK(sp.plan.lvlMaze.sizeX == 16);
				CHECK(sp.plan.levelTilesX <= 360);
				if (sp.plan.entrance.place) {
					CHECK(sp.plan.entrance.toPiece >= 1018);
					CHECK(sp.plan.entrance.toPiece <= 1021);
					CHECK(steerSpecial(ts, sp.plan, sp.plan.entrance.toPiece).action == SpecialSteer::Action::Redirect);
				}
			}
	CHECK(steered > 200);
	const SeedPlan sp = planForSeed(ts, layout::presetRules(layout::Shape::Winding, layout::Size::Medium), 4);
	REQUIRE(sp.plan.steer);
	for (int to = 1022; to <= 1033; ++to) CHECK(steerSpecial(ts, sp.plan, to).action == SpecialSteer::Action::Suppress);  // Next, Down, Theme: never placed
	// A 21 by 21 rooms level fits the 360-tile slot (22 rooms of 16 tiles across with the first room centred).
	CHECK(maxCapacity(ts).across() >= 21);
}

TEST_CASE("Ice arrival: a 16-tile cell the engine splits into 2 x 2 rooms of 8 gives the top-left room of the entrance cell")
{
	Tileset ts;
	REQUIRE(builtinTileset("Ice", ts));
	for (uint64_t seed = 1; seed <= 40; ++seed) {
		const SeedPlan sp = planForSeed(ts, layout::presetRules(layout::Shape::Branching, layout::Size::Medium), seed);
		if (!sp.plan.steer) continue;
		int minX = sp.plan.rooms[0].dx, minY = sp.plan.rooms[0].dy;
		for (const Room &r : sp.plan.rooms) {
			minX = std::min(minX, r.dx);
			minY = std::min(minY, r.dy);
		}
		std::vector<RoomRect> rects;
		for (const Room &r : sp.plan.rooms)
			for (int j = 0; j < 2; ++j)
				for (int i = 0; i < 2; ++i) rects.push_back({500 + (r.dx - minX) * 16 + i * 8, 700 + (r.dy - minY) * 16 + j * 8, 8, 8, 0});
		const ArrivalMark m = planArrival(sp.plan, 16, rects);
		INFO("seed " << seed << ": " << m.why);
		REQUIRE(m.mark);
		const Room &e = sp.plan.rooms[static_cast<size_t>(sp.plan.entrance.room)];
		CHECK(m.originX == 500 + (e.dx - minX) * 16);
		CHECK(m.originY == 700 + (e.dy - minY) * 16);
		// The four rooms of the cell tie on the distance to its centre: the smaller y, then the smaller x, wins.
		CHECK(rects[static_cast<size_t>(m.room)].x == m.originX);
		CHECK(rects[static_cast<size_t>(m.room)].y == m.originY);
		const CellRoom boss = planCellRoom(sp.plan, 16, sp.plan.bossRoom, rects);
		CHECK(boss.room >= 0);
		const CellRoom event = planCellRoom(sp.plan, 16, sp.plan.eventRoom, rects);
		CHECK(event.room >= 0);
	}
}

// --- the arrival room ----------------------------------------------------------------------------------------------

namespace {

// The rooms the game builds from a plan: every Cave cell is 3 x 3 engine rooms of unit / 3 tiles, every Crypt cell one room, listed in reverse so the first
// room of the list is nowhere near the entrance. `flagged` gets kArrivalFlags the way the DS1 loader's pieces would.
std::vector<RoomRect> builtRects(const Plan &plan, int unit, int baseX, int baseY, int flagged = -1)
{
	int minX = plan.rooms[0].dx, minY = plan.rooms[0].dy;
	for (const Room &r : plan.rooms) {
		minX = std::min(minX, r.dx);
		minY = std::min(minY, r.dy);
	}
	std::vector<RoomRect> out;
	const int per = unit >= 24 ? 3 : 1;  // a Cave cell is 3 x 3 engine rooms, a Crypt cell is one room
	for (const Room &r : plan.rooms) {
		for (int j = 0; j < per; ++j) {
			for (int i = 0; i < per; ++i) {
				out.push_back({baseX + (r.dx - minX) * unit + i * unit / per, baseY + (r.dy - minY) * unit + j * unit / per, unit / per, unit / per, 0});
			}
		}
	}
	std::reverse(out.begin(), out.end());
	if (flagged >= 0) out[static_cast<size_t>(flagged)].flags = kArrivalFlags;
	return out;
}

} // namespace

TEST_CASE("arrival: the room in the middle of the entrance cell is marked, for every shape including loops")
{
	const Tileset tiles[] = {caveTileset(), cryptTileset()};
	const layout::Shape shapes[] = {layout::Shape::Corridor, layout::Shape::Winding, layout::Shape::Branching, layout::Shape::Loop};
	int marked = 0;
	int perShape[4] = {0, 0, 0, 0};
	for (const Tileset &ts : tiles) {
		for (size_t si = 0; si < 4; ++si) {
			const layout::Shape shape = shapes[si];
			for (int seed = 1; seed <= 60; ++seed) {
				const SeedPlan sp = planForSeed(ts, layout::presetRules(shape, layout::Size::Medium), static_cast<uint64_t>(seed));
				if (!sp.plan.steer) continue;
				const std::vector<RoomRect> rects = builtRects(sp.plan, ts.roomTiles, 1000, 2000, seed % 7);
				const ArrivalMark m = planArrival(sp.plan, ts.roomTiles, rects);
				INFO(ts.name << " seed " << seed);
				REQUIRE(m.mark);
				++marked;
				++perShape[si];
				// The pre-flagged room (seed % 7) loses its mark unless it is the chosen room.
				const int flagged = seed % 7;
				if (flagged == m.room) {
					CHECK(m.clear.empty());
				} else {
					REQUIRE(m.clear.size() == 1);
					CHECK(m.clear[0] == flagged);
				}
				const RoomRect &r = rects[static_cast<size_t>(m.room)];
				const Room &e = sp.plan.rooms[static_cast<size_t>(sp.plan.entrance.room)];
				int minX = sp.plan.rooms[0].dx, minY = sp.plan.rooms[0].dy;
				for (const Room &o : sp.plan.rooms) {
					minX = std::min(minX, o.dx);
					minY = std::min(minY, o.dy);
				}
				const int third = ts.roomTiles >= 24 ? ts.roomTiles / 3 : 0;  // the middle room of a Cave cell; a Crypt cell is one room
				CHECK(r.x == 1000 + (e.dx - minX) * ts.roomTiles + third);  // the middle room of the 3 x 3
				CHECK(r.y == 2000 + (e.dy - minY) * ts.roomTiles + third);
				CHECK(m.originX == 1000 + (e.dx - minX) * ts.roomTiles);
				CHECK(m.originY == 2000 + (e.dy - minY) * ts.roomTiles);
				for (const int c : m.clear) {
					CHECK(c != m.room);
					CHECK((rects[static_cast<size_t>(c)].flags & kArrivalFlags) != 0);
				}
			}
		}
	}
	CHECK(marked > 200);
	for (const int n : perShape) CHECK(n > 50);  // Corridor, Winding, Branching and Loop all reach planArrival
}

TEST_CASE("arrival: a room that already carries the mark is cleared, and the marked room is never in the clear list")
{
	const Tileset cave = caveTileset();
	const SeedPlan sp = planForSeed(cave, layout::presetRules(layout::Shape::Corridor, layout::Size::Small), 3);
	REQUIRE(sp.plan.steer);
	std::vector<RoomRect> rects = builtRects(sp.plan, 24, 0, 0);
	const ArrivalMark first = planArrival(sp.plan, 24, rects);
	REQUIRE(first.mark);
	CHECK(first.clear.empty());
	CHECK(first.why.empty());
	rects[0].flags = kArrivalFlags;
	rects[1].flags = 0x10000;
	rects[static_cast<size_t>(first.room)].flags = 0x20000 | 0x4;
	const ArrivalMark again = planArrival(sp.plan, 24, rects);
	REQUIRE(again.mark);
	CHECK(again.room == first.room);
	REQUIRE(again.clear.size() == 2);
	CHECK(again.clear[0] == 0);
	CHECK(again.clear[1] == 1);
}

TEST_CASE("arrival: the level is left to the game, with a reason, when nothing can be proven")
{
	const Tileset cave = caveTileset();
	const SeedPlan sp = planForSeed(cave, layout::presetRules(layout::Shape::Winding, layout::Size::Small), 5);
	REQUIRE(sp.plan.steer);
	const std::vector<RoomRect> good = builtRects(sp.plan, 24, 0, 0);
	REQUIRE(planArrival(sp.plan, 24, good).mark);

	Plan notSteered = sp.plan;
	notSteered.steer = false;
	CHECK(!planArrival(notSteered, 24, good).mark);
	CHECK(!planArrival(sp.plan, 24, {}).mark);
	CHECK(!planArrival(sp.plan, 0, good).mark);
	Plan noEntrance = sp.plan;
	noEntrance.entrance.room = -1;
	CHECK(!planArrival(noEntrance, 24, good).mark);

	// A built box that is not the plan's box: a wing of the maze is missing, so the cells do not line up.
	std::vector<RoomRect> narrow = good;
	narrow.erase(std::remove_if(narrow.begin(), narrow.end(), [&](const RoomRect &r) { return r.x >= (sp.plan.widthRooms - 1) * 24; }), narrow.end());
	const ArrivalMark m = planArrival(sp.plan, 24, narrow);
	CHECK(!m.mark);
	CHECK(m.why.find("span the plan's box") != std::string::npos);

	// The entrance cell holds no room. The cell must be inside the box, or taking its rooms away changes the box instead.
	SeedPlan inner;
	for (const layout::Shape shape : {layout::Shape::Loop, layout::Shape::Branching, layout::Shape::Winding}) {
		for (int seed = 1; seed <= 200 && !inner.plan.steer; ++seed) {
			const SeedPlan c = planForSeed(cave, layout::presetRules(shape, layout::Size::Large), static_cast<uint64_t>(seed));
			if (!c.plan.steer) continue;
			const ArrivalMark m0 = planArrival(c.plan, 24, builtRects(c.plan, 24, 0, 0));
			if (m0.mark && m0.cellX > 0 && m0.cellY > 0 && m0.cellX < c.plan.widthRooms - 1 && m0.cellY < c.plan.heightRooms - 1) inner = c;
		}
	}
	REQUIRE(inner.plan.steer);
	const std::vector<RoomRect> full = builtRects(inner.plan, 24, 0, 0);
	std::vector<RoomRect> hole;
	const ArrivalMark ok = planArrival(inner.plan, 24, full);
	for (const RoomRect &r : full) {
		if (r.x < ok.originX || r.x >= ok.originX + 24 || r.y < ok.originY || r.y >= ok.originY + 24) hole.push_back(r);
	}
	const ArrivalMark none = planArrival(inner.plan, 24, hole);
	CHECK(!none.mark);
	CHECK(none.why.find("holds no room") != std::string::npos);
}

// --- the event and boss rooms (issue #184) --------------------------------------------------------------------------------

TEST_CASE("cell room: the event and boss rooms are found by rect in their plan cell, whatever stale records the level list holds")
{
	const Tileset tiles[] = {caveTileset(), cryptTileset()};
	const layout::Shape shapes[] = {layout::Shape::Corridor, layout::Shape::Winding, layout::Shape::Branching, layout::Shape::Loop};
	int found = 0, notCorner = 0;
	for (const Tileset &ts : tiles) {
		for (const layout::Shape shape : shapes) {
			for (int seed = 1; seed <= 40; ++seed) {
				const SeedPlan sp = planForSeed(ts, layout::presetRules(shape, layout::Size::Medium), static_cast<uint64_t>(seed));
				if (!sp.plan.steer) continue;
				std::vector<RoomRect> rects = builtRects(sp.plan, ts.roomTiles, 1000, 2000);
				// The real failure (issue #184): the walk's record of a plan room is stale after the build. It reads as a 0 x 0 room at
				// (1,0), far outside the box, or as the cell's top-left 8 x 8 room. Neither may be chosen or break the box check.
				rects.insert(rects.begin(), RoomRect{1, 0, 0, 0, 0});
				rects.insert(rects.begin(), RoomRect{1001, 2001, 0, 0, 0});
				int minX = sp.plan.rooms[0].dx, minY = sp.plan.rooms[0].dy;
				for (const Room &o : sp.plan.rooms) {
					minX = std::min(minX, o.dx);
					minY = std::min(minY, o.dy);
				}
				const int third = ts.roomTiles >= 24 ? ts.roomTiles / 3 : 0;
				for (const int planRoom : {sp.plan.eventRoom, sp.plan.bossRoom}) {
					if (planRoom < 0) continue;
					INFO(ts.name << " seed " << seed << " plan room " << planRoom);
					const CellRoom c = planCellRoom(sp.plan, ts.roomTiles, planRoom, rects);
					REQUIRE(c.room >= 0);
					const Room &pr = sp.plan.rooms[static_cast<size_t>(planRoom)];
					const RoomRect &r = rects[static_cast<size_t>(c.room)];
					CHECK(r.w > 0);
					CHECK(r.x == 1000 + (pr.dx - minX) * ts.roomTiles + third);
					CHECK(r.y == 2000 + (pr.dy - minY) * ts.roomTiles + third);
					if (ts.roomTiles >= 24) {  // a Cave cell: the centre room of its 3 x 3, never the cell's top-left one
						CHECK(r.x != c.originX);
						CHECK(r.y != c.originY);
						++notCorner;
					}
					++found;
				}
			}
		}
	}
	CHECK(found > 100);
	CHECK(notCorner > 50);
}

TEST_CASE("cell room: refused with a reason when there is nothing to match")
{
	const Tileset ts = caveTileset();
	const SeedPlan sp = planForSeed(ts, layout::presetRules(layout::Shape::Corridor, layout::Size::Medium), 3);
	REQUIRE(sp.plan.steer);
	const std::vector<RoomRect> rects = builtRects(sp.plan, ts.roomTiles, 1000, 2000);
	CHECK(planCellRoom(sp.plan, ts.roomTiles, -1, rects).room < 0);
	CHECK(planCellRoom(sp.plan, ts.roomTiles, 9999, rects).room < 0);
	CHECK(planCellRoom(sp.plan, 0, 0, rects).room < 0);
	CHECK(planCellRoom(sp.plan, ts.roomTiles, 0, {}).why == "the level has no rooms");
	// the edge rooms missing: the built box is not the plan's
	std::vector<RoomRect> cut = rects;
	int right = 0;
	for (const RoomRect &r : cut) right = std::max(right, r.x + r.w);
	cut.erase(std::remove_if(cut.begin(), cut.end(), [&](const RoomRect &r) { return r.x + r.w == right; }), cut.end());
	const CellRoom c = planCellRoom(sp.plan, ts.roomTiles, 0, cut);
	CHECK(c.room < 0);
	CHECK_FALSE(c.why.empty());
}
