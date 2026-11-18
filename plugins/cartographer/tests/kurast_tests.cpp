// Host tests for the Cartographer Kurast (Act 3 Burbs district) plan: a row of the frame-plus-fill table. No game, no SDK: the plan is pure C++.
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

FrameFillRequest request(uint64_t seed, int w, int h, bool pin = false)
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

const char *const kSizes[] = {"Small", "Medium", "Large", "ExtraLarge"};

} // namespace

// --- Kurast (issue #507): one district, the Burbs, no gates, bridge, temple, sewer or waypoint ---------------------------------

namespace {

// The test's own size of a Kurast fill piece (grid cells), and whether the id is one of the ring pieces 619..626.
bool kurastFill(int lvlPrest, int *w, int *h)
{
	if (lvlPrest < 632 || lvlPrest > 635) return false;
	*w = lvlPrest == 634 || lvlPrest == 635 ? 2 : 1;
	*h = lvlPrest == 633 || lvlPrest == 635 ? 2 : 1;
	return true;
}

bool kurastRing(int lvlPrest) { return lvlPrest >= 619 && lvlPrest <= 626; }

std::vector<int> kurastCoverage(const StampPlan &sp)
{
	std::vector<int> cover(static_cast<size_t>(sp.cellsW) * sp.cellsH, 0);
	for (const Stamp &s : sp.stamps) {
		int w = 1, h = 1;
		if (!kurastRing(s.lvlPrest)) REQUIRE(kurastFill(s.lvlPrest, &w, &h));
		for (int dy = 0; dy < h; ++dy)
			for (int dx = 0; dx < w; ++dx) {
				REQUIRE(s.cx + dx < sp.cellsW);
				REQUIRE(s.cy + dy < sp.cellsH);
				++cover[static_cast<size_t>(s.cy + dy) * sp.cellsW + s.cx + dx];
			}
	}
	return cover;
}

uint64_t kurastRects(const StampPlan &sp)
{
	uint64_t h = 1469598103934665603ull;
	for (const TileRect *r : {&sp.entrance, &sp.boss, &sp.event})
		for (const int v : {r->known ? 1 : 0, r->x, r->y, r->w, r->h}) h = (h ^ static_cast<uint64_t>(static_cast<int64_t>(v))) * 1099511628211ull;
	return h;
}

} // namespace

TEST_CASE("the frame-fill table: Kurast is a row of the Burbs district, found by name")
{
	CHECK(findFrameFillTileset("KURAST") == &kKurast);
	CHECK(kKurast.levelType == 22);
	CHECK(frameFillTilesetNames() == "Mesa, Desert, Wilderness or Kurast");
	CHECK(kKurast.setCount == 1);
	CHECK(fillSetNames(kKurast) == "Burbs");
	CHECK(findFillSet(kKurast, "burbs") == 0);
	CHECK(findFillSet(kKurast, "Metro") == -1);
	CHECK(kKurast.sets[0].count == 4);
	CHECK(kKurast.sets[0].fills[3].lvlPrest == 632);  // the 8 x 8 piece is last
	// The ring: Burbs Border W 622, N 619, E 621, S 620, then NW 624, NE 623, SE 625, SW 626 (floor on the inner side, read from the DS1s).
	CHECK(edgePieceAt(kKurast.edge, -1, 3, 6, 6) == 622);
	CHECK(edgePieceAt(kKurast.edge, 3, -1, 6, 6) == 619);
	CHECK(edgePieceAt(kKurast.edge, 6, 3, 6, 6) == 621);
	CHECK(edgePieceAt(kKurast.edge, 3, 6, 6, 6) == 620);
	CHECK(edgePieceAt(kKurast.edge, -1, -1, 6, 6) == 624);
	CHECK(edgePieceAt(kKurast.edge, 6, -1, 6, 6) == 623);
	CHECK(edgePieceAt(kKurast.edge, 6, 6, 6, 6) == 625);
	CHECK(edgePieceAt(kKurast.edge, -1, 6, 6, 6) == 626);
}

TEST_CASE("Kurast: every cell is filled once, with Burbs pieces and the Burbs ring only; every interior size packs")
{
	std::set<int> sizesSeen;
	for (int w = kMinCells; w <= kKurast.maxCellsW; ++w) {
		for (int h = kMinCells; h <= kKurast.maxCellsH; ++h) {
			for (uint64_t seed = 0; seed < 25; ++seed) {
				const FrameFillRequest r = request(seed, w, h);
				const FrameFillPlan p = planFrameFillMap(kKurast, r);
				INFO(w, " x ", h, " seed ", seed, ": ", p.reason);
				REQUIRE(p.ok);
				CHECK(joined(checkFrameFillPlan(kKurast, r, p)).empty());
				CHECK(p.fillSet == 0);
				CHECK(p.stamps.voidCells.empty());
				for (int c : kurastCoverage(p.stamps)) REQUIRE(c == 1);
				for (const Stamp &s : p.stamps.stamps) {
					int fw = 1, fh = 1;
					if (kurastFill(s.lvlPrest, &fw, &fh)) sizesSeen.insert(fw * 10 + fh);
				}
			}
		}
	}
	CHECK(sizesSeen == std::set<int>{11, 12, 21, 22});  // all four fill sizes show up
}

TEST_CASE("Kurast: the ring closes round Small to ExtraLarge, one piece per side and corner")
{
	for (const char *name : kSizes) {
		int w = 0, h = 0;
		REQUIRE(frameFillSizeCells(name, &w, &h));
		const FrameFillPlan p = planFrameFillMap(kKurast, request(5, w, h));
		REQUIRE(p.ok);
		REQUIRE(p.ringCells == 2);  // no size limit: the outer ring always fits
		int ring = 0;
		for (const Stamp &s : p.stamps.stamps) {
			const int ix = s.cx - 2, iy = s.cy - 2;
			const bool onRing = ix < 0 || iy < 0 || ix >= w || iy >= h;
			CHECK(kurastRing(s.lvlPrest) == onRing);
			if (onRing && ix >= -1 && iy >= -1 && ix <= w && iy <= h) {
				CHECK(s.lvlPrest == edgePieceAt(kKurast.edge, ix, iy, w, h));
				++ring;
			}
		}
		CHECK(ring == 2 * (w + h) + 4);
	}
}

// The test's own reading of the Burbs ring DS1 walk flags (tools/d2r_readers.py, build 93847): 619 N and 621 E end in a low wall, 620 S and 622 W in open
// floor on their outer side, so none of them may sit on the level's edge, where the black past the level shows (#550). Each side's outer ring piece is the
// opposite side's piece, back to back: west 621, north 620, east 622, south 619; corners NW 625, NE 626, SE 624, SW 623.
TEST_CASE("Kurast: no Burbs frame piece is on the level's edge; the outer ring closes it in the 128 x 112 slot (#550)")
{
	const int box[3][3] = {{625, 620, 626}, {621, 0, 622}, {623, 619, 624}};  // rows north, middle, south; columns west, middle, east
	for (const char *name : {"Small", "Medium", "Large"}) {
		int w = 0, h = 0;
		REQUIRE(frameFillSizeCells(name, &w, &h));
		FrameFillRequest r = request(9, w, h);
		r.options.maxTilesX = 128;
		r.options.maxTilesY = 112;
		const FrameFillPlan p = planFrameFillMap(kKurast, r);
		INFO(name, ": ", p.reason);
		REQUIRE(p.ok);
		CHECK(joined(checkFrameFillPlan(kKurast, r, p)).empty());
		const StampPlan &sp = p.stamps;
		CHECK(sp.cellsW == w + 4);
		CHECK(sp.cellsH == h + 4);
		int edge = 0;
		for (const Stamp &s : sp.stamps) {
			if (s.cx != 0 && s.cy != 0 && s.cx != sp.cellsW - 1 && s.cy != sp.cellsH - 1) continue;
			const int row = s.cy == 0 ? 0 : (s.cy == sp.cellsH - 1 ? 2 : 1), col = s.cx == 0 ? 0 : (s.cx == sp.cellsW - 1 ? 2 : 1);
			CHECK(s.lvlPrest == box[row][col]);
			++edge;
		}
		CHECK(edge == 2 * (sp.cellsW + sp.cellsH) - 4);
		CHECK(sp.entrance.x >= 2 * kGridTiles);  // the rooms stay inside both rings
		CHECK(sp.entrance.y >= 2 * kGridTiles);
	}
	// ExtraLarge needs 144 x 128 tiles with the outer ring: a 128 x 112 slot keeps the one ring, a 144 x 128 one gets both.
	FrameFillRequest xl = request(9, 14, 12);
	xl.options.maxTilesX = 128;
	xl.options.maxTilesY = 112;
	const FrameFillPlan one = planFrameFillMap(kKurast, xl);
	REQUIRE(one.ok);
	CHECK(one.ringCells == 1);
	CHECK(joined(checkFrameFillPlan(kKurast, xl, one)).empty());
	xl.options.maxTilesX = 144;
	xl.options.maxTilesY = 128;
	const FrameFillPlan two = planFrameFillMap(kKurast, xl);
	REQUIRE(two.ok);
	CHECK(two.ringCells == 2);
	CHECK(joined(checkFrameFillPlan(kKurast, xl, two)).empty());
	// Arenas and the other frame-plus-fill tilesets keep one ring.
	CHECK(planFrameFillArena(kKurast, arenaRequest(3, 3, 0, 0, 0)).ringCells == 1);
	CHECK(planFrameFillMap(kMesa, request(9, 8, 6)).ringCells == 1);
}

TEST_CASE("Kurast: planOutdoorMap plans a Kurast map through the tileset table")
{
	MapRequest m;
	m.tileset = "kurast";
	m.size = "Large";
	m.seed = 42;
	m.options.originX = 6000;
	m.options.originY = 5100;
	m.options.maxTilesX = (kKurast.maxCellsW + 2) * kGridTiles;
	m.options.maxTilesY = (kKurast.maxCellsH + 2) * kGridTiles;
	const MapPlan plan = planOutdoorMap(m, builtinJungleCatalogue());
	INFO(plan.reason);
	REQUIRE(plan.ok);
	CHECK(plan.stamps.cellsW == 16);  // Large, 12 x 10, in two rings
	CHECK(plan.stamps.cellsH == 14);
	CHECK(plan.stamps.entrance.known);
	CHECK(plan.stamps.boss.known);
	CHECK(plan.stamps.event.known);
	MapRequest mesa = m;
	mesa.tileset = "mesa";
	CHECK(plan.hash != planOutdoorMap(mesa, builtinJungleCatalogue()).hash);
}

TEST_CASE("Kurast: an arena is a closed square ring round a Burbs fill with one entry")
{
	for (int w = 1; w <= 4; ++w) {
		for (int h = 1; h <= 4; ++h) {
			const FrameFillRequest r = arenaRequest(w, h, 0, w / 2, h - 1);
			const FrameFillPlan plan = planFrameFillArena(kKurast, r);
			INFO(w, " x ", h, ": ", plan.reason);
			REQUIRE(plan.ok);
			CHECK(joined(checkFrameFillPlan(kKurast, r, plan)).empty());
			for (int c : kurastCoverage(plan.stamps)) CHECK(c == 1);
			int entrances = 0;
			for (const Stamp &s : plan.stamps.stamps) {
				entrances += s.role == layout::Role::Entrance ? 1 : 0;
				CHECK(s.file == -1);
			}
			CHECK(entrances == 1);
			CHECK_FALSE(plan.stamps.boss.known);
		}
	}
	// 2 x 2 is one 16 x 16 piece (635); sizes beyond the tileset's largest and a second fill set are refused.
	const FrameFillPlan big = planFrameFillArena(kKurast, arenaRequest(2, 2, 0, 1, 1));
	REQUIRE(big.ok);
	CHECK(big.stamps.stamps.size() == 13);
	CHECK(big.stamps.entrance.w == 16);
	CHECK_FALSE(planFrameFillArena(kKurast, arenaRequest(15, 3, 0, 0, 0)).ok);
	CHECK_FALSE(planFrameFillArena(kKurast, arenaRequest(3, 3, 1, 0, 0)).ok);
}

TEST_CASE("pinned Kurast plans: map and arena stamp hashes never change (issue #507; maps re-pinned for the outer ring, #550)")
{
	struct Case { uint64_t seed; int w, h; bool pin; uint64_t hash; uint64_t rects; };
	const Case maps[] = {
		{0, 8, 6, false, 3403018239417076907ull, 15537144075753436292ull}, {7, 12, 10, false, 7035451511782522895ull, 1680055067567658836ull},
		{12345, 14, 12, false, 7114334581385018969ull, 17810577086248832308ull}, {3, 10, 8, true, 674245821697183831ull, 12593357563485428564ull},
	};
	for (const Case &c : maps) {
		FrameFillRequest r = request(c.seed, c.w, c.h);
		r.options.pinFiles = c.pin;
		r.options.originX = 40;
		r.options.originY = 24;
		const FrameFillPlan p = planFrameFillMap(kKurast, r);
		INFO("seed ", c.seed, " ", c.w, "x", c.h, " pin ", c.pin, " hash ", p.hash, " rects ", kurastRects(p.stamps));
		REQUIRE(p.ok);
		CHECK(p.hash == c.hash);
		CHECK(kurastRects(p.stamps) == c.rects);
	}
	struct ArenaCase { int w, h, ex, ey; uint64_t hash, mapHash, rects; };
	const ArenaCase arenas[] = {{2, 2, 1, 1, 1871739203660872654ull, 15313537586158998761ull, 15255628362437481182ull},
	                         {5, 3, 0, 0, 17620717595060370682ull, 12718944157695743853ull, 15255628362437481182ull},
	                         {14, 12, 7, 11, 14969474333758011373ull, 13727908100616622655ull, 10649611213771389278ull}};
	for (const ArenaCase &c : arenas) {
		FrameFillRequest r = arenaRequest(c.w, c.h, 0, c.ex, c.ey);
		r.options.originX = 6000;
		r.options.originY = 5000;
		const FrameFillPlan p = planFrameFillArena(kKurast, r);
		const MapPlan m = planArenaMap(kKurast, r);
		INFO("arena ", c.w, "x", c.h, " hash ", p.hash, " map ", m.hash, " rects ", kurastRects(p.stamps));
		REQUIRE(p.ok);
		CHECK(p.hash == c.hash);
		CHECK(m.hash == c.mapHash);
		CHECK(kurastRects(p.stamps) == c.rects);
	}
}
