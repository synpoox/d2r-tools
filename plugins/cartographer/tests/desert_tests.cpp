// Host tests for the Cartographer Desert (Act 2 sand) tileset: one FrameFillTileset row on the shared frame-plus-fill planner. No game, no SDK.
#define _CRT_SECURE_NO_WARNINGS
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <cstdint>
#include <set>
#include <string>
#include <vector>

#include "frame_fill_plan.h"
#include "outdoor.h"
#include "outdoor_plan.h"
#include "outdoor_tilesets.h"
#include "stamp.h"

using namespace cartographer;
using namespace cartographer::outdoor;
using namespace cartographer::outdoor::framefill;

namespace {

FrameFillRequest request(uint64_t seed, int w, int h)
{
	FrameFillRequest r;
	r.seed = seed;
	r.cellsW = w;
	r.cellsH = h;
	return r;
}

FrameFillRequest arenaRequest(int w, int h, int ex, int ey)
{
	FrameFillRequest r;
	r.cellsW = w;
	r.cellsH = h;
	r.arena = true;
	r.arenaFill = 0;
	r.entryX = ex;
	r.entryY = ey;
	return r;
}

bool isRing(int id) { return id >= 364 && id <= 371; }
bool isFill(int id) { return id >= 398 && id <= 407; }

std::string joined(const std::vector<std::string> &v)
{
	std::string out;
	for (const std::string &s : v) out += s + "; ";
	return out;
}

} // namespace

TEST_CASE("Desert: eight ring pieces of one file, one fill set of ten 8 x 8 pieces, LevelType 16")
{
	CHECK(kDesert.levelType == 16);
	const EdgePieces &f = kDesert.edgeFiles;
	CHECK((f.west == 1 && f.north == 1 && f.east == 1 && f.south == 1 && f.nw == 1 && f.ne == 1 && f.se == 1 && f.sw == 1));
	CHECK(kDesert.setCount == 1);
	const FillSet &set = kDesert.sets[0];
	REQUIRE(set.count == 10);
	std::set<int> ids;
	for (int i = 0; i < set.count; ++i) {
		const Fill &f = set.fills[i];
		CHECK(isFill(f.lvlPrest));
		CHECK((f.w == 1 && f.h == 1));  // all ten are 8 x 8 tiles
		CHECK(f.files >= 1);
		CHECK(f.files <= 5);
		CHECK(f.weight >= 1);
		ids.insert(f.lvlPrest);
	}
	CHECK(ids.size() == 10);
	const std::set<int> ring = {kDesert.edge.west, kDesert.edge.north, kDesert.edge.east, kDesert.edge.south, kDesert.edge.nw, kDesert.edge.ne, kDesert.edge.se, kDesert.edge.sw};
	CHECK(ring == std::set<int>{364, 365, 366, 367, 368, 369, 370, 371});
	CHECK(edgePieceAt(kDesert.edge, -1, 1, 4, 4) == 365);
	CHECK(edgePieceAt(kDesert.edge, 1, -1, 4, 4) == 366);
	CHECK(edgePieceAt(kDesert.edge, 4, 1, 4, 4) == 367);
	CHECK(edgePieceAt(kDesert.edge, 1, 4, 4, 4) == 364);
	CHECK(edgePieceAt(kDesert.edge, -1, -1, 4, 4) == 369);
	CHECK(edgePieceAt(kDesert.edge, 4, -1, 4, 4) == 370);
	CHECK(edgePieceAt(kDesert.edge, 4, 4, 4, 4) == 371);
	CHECK(edgePieceAt(kDesert.edge, -1, 4, 4, 4) == 368);
	CHECK(findFrameFillTileset("DESERT") == &kDesert);
	CHECK(findFillSet(kDesert, "desert") == 0);
	CHECK(fillSetNames(kDesert) == "Desert");
}

TEST_CASE("Desert maps: every interior size packs with no gap and the ring closes")
{
	for (int w = kMinCells; w <= kDesert.maxCellsW; ++w) {
		for (int h = kMinCells; h <= kDesert.maxCellsH; ++h) {
			for (uint64_t seed = 0; seed < 8; ++seed) {
				const FrameFillRequest r = request(seed, w, h);
				const FrameFillPlan p = planFrameFillMap(kDesert, r);
				INFO(w, " x ", h, " seed ", seed, ": ", p.reason);
				REQUIRE(p.ok);
				CHECK(joined(checkFrameFillPlan(kDesert, r, p)).empty());
				// Worked out here: every grid cell is covered by exactly one stamp, the outermost cells are ring pieces and the rest are fills.
				std::vector<int> cover(static_cast<size_t>(p.stamps.cellsW) * p.stamps.cellsH, 0);
				for (const Stamp &s : p.stamps.stamps) {
					const bool onRing = s.cx == 0 || s.cy == 0 || s.cx == w + 1 || s.cy == h + 1;
					CHECK(isRing(s.lvlPrest) == onRing);
					CHECK(isFill(s.lvlPrest) == !onRing);
					++cover[static_cast<size_t>(s.cy) * p.stamps.cellsW + s.cx];
				}
				for (int c : cover) REQUIRE(c == 1);
				CHECK(p.stamps.voidCells.empty());
			}
		}
	}
}

TEST_CASE("Desert arena: a closed square of the first fill piece, one entry, no boss or event")
{
	for (int w = 1; w <= 4; ++w) {
		for (int h = 1; h <= 4; ++h) {
			const FrameFillRequest r = arenaRequest(w, h, w / 2, h - 1);
			const FrameFillPlan p = planFrameFillArena(kDesert, r);
			INFO(w, " x ", h, ": ", p.reason);
			REQUIRE(p.ok);
			CHECK(joined(checkFrameFillPlan(kDesert, r, p)).empty());
			int entrances = 0, ring = 0;
			for (const Stamp &s : p.stamps.stamps) {
				entrances += s.role == layout::Role::Entrance ? 1 : 0;
				ring += isRing(s.lvlPrest) ? 1 : 0;
				if (!isRing(s.lvlPrest)) CHECK(s.lvlPrest == 404);
			}
			CHECK(entrances == 1);
			CHECK(ring == 2 * (w + h) + 4);
			CHECK_FALSE(p.stamps.boss.known);
			CHECK_FALSE(p.stamps.event.known);
		}
	}
}

TEST_CASE("planOutdoorMap plans Desert through the tileset table")
{
	MapRequest m;
	m.tileset = "desert";
	m.size = "Large";
	m.seed = 5;
	m.options.maxTilesX = (kDesert.maxCellsW + 2) * kGridTiles;
	m.options.maxTilesY = (kDesert.maxCellsH + 2) * kGridTiles;
	const MapPlan plan = planOutdoorMap(m, builtinJungleCatalogue());
	INFO(plan.reason);
	REQUIRE(plan.ok);
	CHECK(plan.stamps.cellsW == 14);
	CHECK(plan.stamps.cellsH == 12);
	CHECK(plan.hash != 0);
}

TEST_CASE("pinned Desert plans: map and arena stamp hashes never change (issue #505)")
{
	struct Case { uint64_t seed; int w, h; bool pin; uint64_t hash; };
	const Case maps[] = {
		{0, 8, 6, false, 6048522473446374902ull}, {1, 10, 8, false, 5349814731329758023ull}, {7, 12, 10, false, 2634307243107708213ull},
		{12345, 14, 12, false, 8832871089832583154ull}, {3, 8, 6, true, 5752989698000829383ull}, {99, 14, 12, true, 3488774718598288945ull},
	};
	for (const Case &c : maps) {
		FrameFillRequest r = request(c.seed, c.w, c.h);
		r.options.pinFiles = c.pin;
		r.options.originX = 40;
		r.options.originY = 24;
		const FrameFillPlan p = planFrameFillMap(kDesert, r);
		INFO("seed ", c.seed, " ", c.w, "x", c.h, " pin ", c.pin, " hash ", p.hash);
		REQUIRE(p.ok);
		CHECK(p.hash == c.hash);
	}
	struct ArenaCase { int w, h, ex, ey; uint64_t hash; };
	const ArenaCase arenas[] = {{2, 2, 1, 1, 13425675680180535867ull}, {3, 3, 1, 2, 3143828429861464524ull}, {5, 3, 0, 0, 17278567477748844991ull}, {14, 12, 7, 11, 12947504684726549739ull}};
	for (const ArenaCase &c : arenas) {
		FrameFillRequest r = arenaRequest(c.w, c.h, c.ex, c.ey);
		r.options.originX = 6000;
		r.options.originY = 3400;
		const FrameFillPlan p = planFrameFillArena(kDesert, r);
		INFO("arena ", c.w, "x", c.h, " hash ", p.hash);
		REQUIRE(p.ok);
		CHECK(p.hash == c.hash);
	}
}
