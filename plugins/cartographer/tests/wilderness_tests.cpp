// Host tests for the Cartographer Wilderness (Act 1 fields) plan: a row of the frame-plus-fill table. No game, no SDK: the plan is pure C++.
#define _CRT_SECURE_NO_WARNINGS
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <cstdint>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "frame_fill_plan.h"
#include "outdoor_plan.h"
#include "outdoor_tilesets.h"
#include "stamp.h"

using namespace cartographer;
using namespace cartographer::outdoor;
using namespace cartographer::outdoor::framefill;

namespace {

FrameFillRequest mapRequest(uint64_t seed, int w, int h, bool pin = false)
{
	FrameFillRequest r;
	r.seed = seed;
	r.cellsW = w;
	r.cellsH = h;
	r.options.pinFiles = pin;
	return r;
}

FrameFillRequest arenaRequest(int w, int h, int fill, int ex, int ey)
{
	FrameFillRequest r;
	r.cellsW = w;
	r.cellsH = h;
	r.arena = true;
	r.arenaFill = fill;
	r.entryX = ex;
	r.entryY = ey;
	return r;
}

std::string joined(const std::vector<std::string> &v)
{
	std::string out;
	for (const std::string &s : v) out += s + "; ";
	return out;
}

// The test's own sizes (grid cells): 39 and 40 are 16 x 16 tiles, 38 is 8 x 8, the ring pieces 4..11 are 8 x 8.
int pieceCells(int lvlPrest)
{
	if (lvlPrest >= 4 && lvlPrest <= 11) return 1;
	return lvlPrest == 39 || lvlPrest == 40 ? 2 : (lvlPrest == 38 ? 1 : 0);
}

// Grid cells covered by the stamps, counted here and not by the planner's checker.
void requireCoveredOnce(const StampPlan &sp)
{
	std::vector<int> cover(static_cast<size_t>(sp.cellsW) * sp.cellsH, 0);
	for (const Stamp &s : sp.stamps) {
		const int n = pieceCells(s.lvlPrest);
		REQUIRE(n > 0);
		for (int dy = 0; dy < n; ++dy)
			for (int dx = 0; dx < n; ++dx) {
				REQUIRE(s.cx + dx < sp.cellsW);
				REQUIRE(s.cy + dy < sp.cellsH);
				++cover[static_cast<size_t>(s.cy + dy) * sp.cellsW + s.cx + dx];
			}
	}
	for (int c : cover) REQUIRE(c == 1);
}

uint64_t rectsHash(const StampPlan &sp)
{
	uint64_t h = 1469598103934665603ull;
	for (const TileRect *r : {&sp.entrance, &sp.boss, &sp.event})
		for (const int v : {r->known ? 1 : 0, r->x, r->y, r->w, r->h}) h = (h ^ static_cast<uint64_t>(static_cast<int64_t>(v))) * 1099511628211ull;
	return h;
}

} // namespace

TEST_CASE("the Wilderness row: LevelType 2, ring 4..11, Swamp and Tree fills, found by name")
{
	CHECK(findFrameFillTileset("wilderness") == &kWilderness);
	CHECK(std::string(kWilderness.name) == "Wilderness");
	CHECK(kWilderness.levelType == 2);
	CHECK(frameFillTilesetNames() == "Mesa or Wilderness");
	CHECK(fillSetNames(kWilderness) == "Swamp or Tree");
	CHECK(findFillSet(kWilderness, "TREE") == 1);
	CHECK(findFillSet(kWilderness, "Mesa1") == -1);
	// Bord1 (floor north) rings the south, Bord2 the west, Bord3 the north, Bord4 the east; Bord5..8 are the corners SW, NW, NE, SE.
	CHECK(edgePieceAt(kWilderness.edge, -1, 1, 4, 4) == 5);
	CHECK(edgePieceAt(kWilderness.edge, 1, -1, 4, 4) == 6);
	CHECK(edgePieceAt(kWilderness.edge, 4, 1, 4, 4) == 7);
	CHECK(edgePieceAt(kWilderness.edge, 1, 4, 4, 4) == 4);
	CHECK(edgePieceAt(kWilderness.edge, -1, -1, 4, 4) == 9);
	CHECK(edgePieceAt(kWilderness.edge, 4, -1, 4, 4) == 10);
	CHECK(edgePieceAt(kWilderness.edge, 4, 4, 4, 4) == 11);
	CHECK(edgePieceAt(kWilderness.edge, -1, 4, 4, 4) == 8);
	CHECK(edgePieceAt(kWilderness.edge, 2, 2, 4, 4) == 0);
	// The straight pieces have 3 DS1 files, the corners 1 (lvlprest.txt Files).
	const EdgePieces &f = kWilderness.edgeFiles;
	CHECK((f.west == 3 && f.north == 3 && f.east == 3 && f.south == 3));
	CHECK((f.nw == 1 && f.ne == 1 && f.se == 1 && f.sw == 1));
	REQUIRE(kWilderness.setCount == 2);
	for (int s = 0; s < kWilderness.setCount; ++s) {
		const FillSet &set = kWilderness.sets[s];
		REQUIRE(set.count == 2);
		CHECK((set.fills[0].w == 2 && set.fills[0].h == 2));
		CHECK((set.fills[1].lvlPrest == 38 && set.fills[1].w == 1 && set.fills[1].h == 1));  // the 8 x 8 piece last
		for (int i = 0; i < set.count; ++i) CHECK(set.fills[i].files == 1);
	}
	CHECK(kWilderness.sets[0].fills[0].lvlPrest == 39);
	CHECK(kWilderness.sets[1].fills[0].lvlPrest == 40);
	// The outdoor table row: a frame-plus-fill planner with the same LevelType.
	const OutdoorTileset *row = findOutdoorTileset("Wilderness");
	REQUIRE(row != nullptr);
	CHECK(row->levelType == kWilderness.levelType);
	CHECK(row->planner == Planner::FrameFill);
	CHECK(std::string(row->slot) == "OutdoorWilderness");
	CHECK(std::string(row->levelName) == "Cartographer Wilderness");
}

TEST_CASE("every interior size from the minimum to the maximum packs completely with both fill sets, many seeds")
{
	std::set<int> seen;
	for (int w = kMinCells; w <= kWilderness.maxCellsW; ++w) {
		for (int h = kMinCells; h <= kWilderness.maxCellsH; ++h) {
			for (uint64_t seed = 0; seed < 12; ++seed) {
				const FrameFillRequest r = mapRequest(seed, w, h, seed % 2 == 1);
				const FrameFillPlan p = planFrameFillMap(kWilderness, r);
				INFO(w, " x ", h, " seed ", seed, ": ", p.reason);
				REQUIRE(p.ok);
				CHECK(joined(checkFrameFillPlan(kWilderness, r, p)).empty());
				requireCoveredOnce(p.stamps);
				CHECK(p.stamps.voidCells.empty());
				seen.insert(p.fillSet);
				for (const Stamp &s : p.stamps.stamps) {
					const bool ring = s.lvlPrest >= 4 && s.lvlPrest <= 11;
					if (!ring) CHECK((s.lvlPrest == 38 || s.lvlPrest == (p.fillSet == 0 ? 39 : 40)));
					if (r.options.pinFiles) {
						CHECK(s.file >= 0);
						CHECK(s.file < (s.lvlPrest >= 4 && s.lvlPrest <= 7 ? 3 : 1));
					} else {
						CHECK(s.file == -1);
					}
				}
			}
		}
	}
	CHECK(seen == std::set<int>{0, 1});  // the seeds reach both fill sets
}

TEST_CASE("the ring is closed: one piece per side and corner round the whole interior, at every Size")
{
	for (const std::pair<int, int> &wh : {std::pair<int, int>{8, 6}, {10, 8}, {12, 10}, {14, 12}}) {
		const FrameFillPlan p = planFrameFillMap(kWilderness, mapRequest(5, wh.first, wh.second));
		REQUIRE(p.ok);
		const int lw = wh.first + 2, lh = wh.second + 2;
		int ring = 0;
		for (const Stamp &s : p.stamps.stamps) {
			const bool onRing = s.cx == 0 || s.cy == 0 || s.cx == lw - 1 || s.cy == lh - 1;
			if (!onRing) continue;
			++ring;
			CHECK(s.lvlPrest == edgePieceAt(kWilderness.edge, s.cx - 1, s.cy - 1, wh.first, wh.second));
		}
		CHECK(ring == 2 * (lw + lh) - 4);
	}
}

TEST_CASE("a Wilderness map is planned through the outdoor tileset table")
{
	MapRequest r;
	r.tileset = "Wilderness";
	r.seed = 9;
	r.size = "Medium";
	const MapPlan plan = planOutdoorMap(r, builtinJungleCatalogue());
	INFO(plan.reason);
	REQUIRE(plan.ok);
	CHECK(plan.hash != 0);
	CHECK(plan.stamps.tilesW == (10 + 2) * 8);
	CHECK(plan.stamps.tilesH == (8 + 2) * 8);
}

TEST_CASE("an arena per fill set is a closed square with one entry piece and no RNG")
{
	for (int fill = 0; fill < kWilderness.setCount; ++fill) {
		for (int w = 1; w <= 5; ++w) {
			for (int h = 1; h <= 5; ++h) {
				const FrameFillRequest r = arenaRequest(w, h, fill, w / 2, h - 1);
				const FrameFillPlan p = planFrameFillArena(kWilderness, r);
				INFO("fill ", fill, " size ", w, " x ", h, ": ", p.reason);
				REQUIRE(p.ok);
				CHECK(joined(checkFrameFillPlan(kWilderness, r, p)).empty());
				requireCoveredOnce(p.stamps);
				int entrances = 0;
				for (const Stamp &s : p.stamps.stamps) entrances += s.role == layout::Role::Entrance ? 1 : 0;
				CHECK(entrances == 1);
				CHECK_FALSE(p.stamps.boss.known);
				CHECK_FALSE(p.stamps.event.known);
			}
		}
	}
	// 2 x 2 is the ring (12 pieces) and exactly one 16 x 16 fill: swamp for set 0, trees for set 1.
	const FrameFillPlan swamp = planFrameFillArena(kWilderness, arenaRequest(2, 2, 0, 1, 1));
	const FrameFillPlan trees = planFrameFillArena(kWilderness, arenaRequest(2, 2, 1, 1, 1));
	REQUIRE(swamp.ok);
	REQUIRE(trees.ok);
	CHECK(swamp.stamps.stamps.size() == 13);
	CHECK(trees.stamps.stamps.size() == 13);
	CHECK(swamp.stamps.stamps[5].lvlPrest == 39);  // row-major: 4 ring cells above, then the ring cell west of the fill
	CHECK(trees.stamps.stamps[5].lvlPrest == 40);
	CHECK(swamp.hash != trees.hash);
	CHECK_FALSE(planFrameFillArena(kWilderness, arenaRequest(2, 2, 2, 0, 0)).ok);  // only two fill sets
	CHECK_FALSE(planFrameFillArena(kWilderness, arenaRequest(15, 2, 0, 0, 0)).ok);
}

TEST_CASE("pinned Wilderness plans: map and arena hashes never change (issue #506)")
{
	struct Case { uint64_t seed; int w, h; bool pin; uint64_t hash; uint64_t rects; };
	const Case maps[] = {
		{0, 8, 6, false, 5315297695837720023ull, 2972327143189534596ull}, {1, 10, 8, false, 17721928737335949923ull, 11157239327043583572ull}, {7, 12, 10, false, 2855673329174517925ull, 10701298169011675732ull}, {12345, 14, 12, false, 7338266188583150978ull, 4133875070216860340ull}, {3, 8, 6, true, 11822290938128592392ull, 18244796806012473460ull}, {99, 14, 12, true, 11151126320477955720ull, 15510784724848022196ull},
	};
	for (const Case &c : maps) {
		FrameFillRequest r = mapRequest(c.seed, c.w, c.h, c.pin);
		r.options.originX = 40;
		r.options.originY = 24;
		const FrameFillPlan p = planFrameFillMap(kWilderness, r);
		INFO("seed ", c.seed, " ", c.w, "x", c.h, " pin ", c.pin, " set ", p.fillSet, " rects ", rectsHash(p.stamps));
		REQUIRE(p.ok);
		CHECK(p.hash == c.hash);
		CHECK(rectsHash(p.stamps) == c.rects);
	}
	struct ArenaCase { int w, h, fill, ex, ey; uint64_t hash; uint64_t rects; };
	const ArenaCase arenas[] = {
		{2, 2, 0, 1, 1, 9549923419358924052ull, 11803801630925321014ull}, {2, 2, 1, 1, 1, 4002142333883750667ull, 11803801630925321014ull}, {5, 3, 0, 0, 0, 13707480755911955012ull, 11803801630925321014ull}, {5, 3, 1, 2, 2, 1487570122610687060ull, 13099807372302649510ull}, {14, 12, 1, 7, 11, 5768179336321506297ull, 2384668988530975254ull},
	};
	for (const ArenaCase &c : arenas) {
		FrameFillRequest r = arenaRequest(c.w, c.h, c.fill, c.ex, c.ey);
		r.options.originX = 6000;
		r.options.originY = 4000;
		const FrameFillPlan p = planFrameFillArena(kWilderness, r);
		INFO("arena ", c.w, "x", c.h, " fill ", c.fill, " rects ", rectsHash(p.stamps));
		REQUIRE(p.ok);
		CHECK(p.hash == c.hash);
		CHECK(rectsHash(p.stamps) == c.rects);
	}
}
