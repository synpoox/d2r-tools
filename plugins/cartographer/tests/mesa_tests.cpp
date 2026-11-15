// Host tests for the Cartographer Mesa (Act 4 plains) plan. No game, no SDK: the plan is pure C++.
#define _CRT_SECURE_NO_WARNINGS
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <cstdint>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "outdoor.h"
#include "outdoor_plan.h"
#include "outdoor_tilesets.h"
#include "mesa_plan.h"
#include "stamp.h"

using namespace cartographer;
using namespace cartographer::outdoor;
using namespace cartographer::outdoor::mesa;

namespace {

MesaRequest request(uint64_t seed, int w, int h)
{
	MesaRequest r;
	r.seed = seed;
	r.cellsW = w;
	r.cellsH = h;
	return r;
}

std::string joined(const std::vector<std::string> &v)
{
	std::string out;
	for (const std::string &s : v) out += s + "; ";
	return out;
}

// The test's own size of a fill piece (grid cells): offsets 0..4 in a set are 24 x 24, 16 x 16, 16 x 8, 8 x 16 and 8 x 8 tiles.
bool fillSize(int lvlPrest, int *set, int *w, int *h)
{
	static const int base[3] = {812, 817, 823};
	static const int cw[5] = {3, 2, 2, 1, 1}, ch[5] = {3, 2, 1, 2, 1};
	for (int s = 0; s < 3; ++s)
		if (lvlPrest >= base[s] && lvlPrest < base[s] + 5) {
			*set = s;
			*w = cw[lvlPrest - base[s]];
			*h = ch[lvlPrest - base[s]];
			return true;
		}
	return false;
}

// How many stamps cover each grid cell of the level, worked out here and not by the planner or its checker.
std::vector<int> coverage(const StampPlan &sp)
{
	std::vector<int> cover(static_cast<size_t>(sp.cellsW) * sp.cellsH, 0);
	for (const Stamp &s : sp.stamps) {
		int set = 0, w = 1, h = 1;
		if (s.lvlPrest >= 799 && s.lvlPrest <= 806) w = h = 1;  // a border piece is one 8 x 8 grid cell
		else REQUIRE(fillSize(s.lvlPrest, &set, &w, &h));
		for (int dy = 0; dy < h; ++dy)
			for (int dx = 0; dx < w; ++dx) {
				REQUIRE(s.cx + dx < sp.cellsW);
				REQUIRE(s.cy + dy < sp.cellsH);
				++cover[static_cast<size_t>(s.cy + dy) * sp.cellsW + s.cx + dx];
			}
	}
	return cover;
}

const char *const kSizes[] = {"Small", "Medium", "Large", "ExtraLarge"};

} // namespace

TEST_CASE("the tables: three fill sets of five pieces each, eight ring pieces, and the slot level fits the largest map")
{
	REQUIRE(kFillSets == 3);
	const int base[3] = {812, 817, 823};
	const int wide[5] = {3, 2, 2, 1, 1}, tall[5] = {3, 2, 1, 2, 1};
	for (int s = 0; s < kFillSets; ++s) {
		const FillSet &set = fillSet(s);
		for (int i = 0; i < kFillsPerSet; ++i) {
			CHECK(set.fills[i].lvlPrest == base[s] + i);
			CHECK(set.fills[i].w == wide[i]);
			CHECK(set.fills[i].h == tall[i]);
			CHECK(set.fills[i].files >= 4);
			CHECK(set.fills[i].files <= 5);
			CHECK(set.fills[i].weight >= 1);
		}
		CHECK(set.fills[kFillsPerSet - 1].w == 1);  // the 8 x 8 piece is what makes every cell fillable
		CHECK(set.fills[kFillsPerSet - 1].h == 1);
	}
	// Izual's spot (822), the warp (811) and the Fortress transition (798) are never used.
	for (int s = 0; s < kFillSets; ++s)
		for (const Fill &f : fillSet(s).fills) CHECK((f.lvlPrest != 822 && f.lvlPrest != 811 && f.lvlPrest != 798));
	std::set<int> ring = {kEdgePieces.west, kEdgePieces.north, kEdgePieces.east, kEdgePieces.south, kEdgePieces.nw, kEdgePieces.ne, kEdgePieces.se, kEdgePieces.sw};
	CHECK(ring == std::set<int>{799, 800, 801, 802, 803, 804, 805, 806});
	CHECK(edgePieceAt(0, 0, 4, 4) == 0);
	CHECK(edgePieceAt(3, 3, 4, 4) == 0);
	CHECK(edgePieceAt(-1, 1, 4, 4) == 800);
	CHECK(edgePieceAt(1, -1, 4, 4) == 801);
	CHECK(edgePieceAt(4, 1, 4, 4) == 802);
	CHECK(edgePieceAt(1, 4, 4, 4) == 799);
	CHECK(edgePieceAt(-1, -1, 4, 4) == 804);
	CHECK(edgePieceAt(4, -1, 4, 4) == 805);
	CHECK(edgePieceAt(4, 4, 4, 4) == 806);
	CHECK(edgePieceAt(-1, 4, 4, 4) == 803);
	int w = 0, h = 0;
	CHECK(mesaSizeCells("ExtraLarge", &w, &h));
	CHECK(kLevelTilesX == (w + 2) * 8);
	CHECK(kLevelTilesY == (h + 2) * 8);
}

TEST_CASE("size names map to 8 x 6, 10 x 8, 12 x 10 and 14 x 12 cells")
{
	int w = 0, h = 0;
	CHECK(mesaSizeCells("", &w, &h));
	CHECK((w == 12 && h == 10));
	CHECK(mesaSizeCells("SMALL", &w, &h));
	CHECK((w == 8 && h == 6));
	CHECK(mesaSizeCells("medium", &w, &h));
	CHECK((w == 10 && h == 8));
	CHECK(mesaSizeCells("Large", &w, &h));
	CHECK((w == 12 && h == 10));
	CHECK(mesaSizeCells("ExtraLarge", &w, &h));
	CHECK((w == 14 && h == 12));
	w = 99;
	CHECK_FALSE(mesaSizeCells("Huge", &w, &h));
	CHECK(w == 99);
}

TEST_CASE("every cell is filled, once: every Size, every fill set, many seeds")
{
	for (const char *name : kSizes) {
		int w = 0, h = 0;
		REQUIRE(mesaSizeCells(name, &w, &h));
		std::set<int> sets;
		std::set<int> sizesSeen;
		for (uint64_t seed = 0; seed < 300; ++seed) {
			const MesaRequest r = request(seed, w, h);
			const MesaPlan p = planMesaMap(r);
			INFO(name, " seed ", seed, ": ", p.reason);
			REQUIRE(p.ok);
			CHECK(joined(checkMesaPlan(r, p)).empty());
			sets.insert(p.fillSet);
			const std::vector<int> cover = coverage(p.stamps);
			bool every = true;
			for (int c : cover) every = every && c == 1;
			CHECK(every);
			CHECK(p.stamps.cellsW == w + 2);
			CHECK(p.stamps.cellsH == h + 2);
			CHECK(p.stamps.tilesW == (w + 2) * 8);
			CHECK(p.stamps.tilesH == (h + 2) * 8);
			CHECK(p.stamps.voidCells.empty());
			// One fill set only, and its own pieces.
			for (const Stamp &s : p.stamps.stamps) {
				int set = -1, fw = 1, fh = 1;
				if (fillSize(s.lvlPrest, &set, &fw, &fh)) {
					CHECK(set == p.fillSet);
					sizesSeen.insert(fw * 10 + fh);
				}
			}
		}
		INFO(name);
		CHECK(sets == std::set<int>{0, 1, 2});  // the seeds reach every fill set
		CHECK(sizesSeen.size() == 5);           // and every one of the five piece sizes shows up
	}
}

TEST_CASE("every interior size from the minimum to the maximum packs completely with every fill set")
{
	for (int w = kMinCells; w <= kMaxCellsW; ++w) {
		for (int h = kMinCells; h <= kMaxCellsH; ++h) {
			std::set<int> sets;
			for (uint64_t seed = 0; seed < 40; ++seed) {
				const MesaRequest r = request(seed, w, h);
				const MesaPlan p = planMesaMap(r);
				INFO(w, " x ", h, " seed ", seed, ": ", p.reason);
				REQUIRE(p.ok);
				sets.insert(p.fillSet);
				for (int c : coverage(p.stamps)) REQUIRE(c == 1);
			}
			CHECK(sets.size() == 3);
		}
	}
}

TEST_CASE("the frame is one border piece per side and corner, round the whole interior")
{
	const MesaRequest r = request(7, 10, 8);
	const MesaPlan p = planMesaMap(r);
	REQUIRE(p.ok);
	// Worked out here: the ring is the level's outermost grid cells, the interior is the rest.
	int ring = 0;
	for (const Stamp &s : p.stamps.stamps) {
		const bool onRing = s.cx == 0 || s.cy == 0 || s.cx == 11 || s.cy == 9;
		const bool isBorder = s.lvlPrest >= 799 && s.lvlPrest <= 806;
		CHECK(onRing == isBorder);
		if (!onRing) continue;
		++ring;
		int want = 0;
		if (s.cy == 0) want = s.cx == 0 ? 804 : (s.cx == 11 ? 805 : 801);
		else if (s.cy == 9) want = s.cx == 0 ? 803 : (s.cx == 11 ? 806 : 799);
		else want = s.cx == 0 ? 800 : 802;
		CHECK(s.lvlPrest == want);
	}
	CHECK(ring == 2 * 12 + 2 * 8);
	// The stamps are in row-major order of their top-left cell.
	for (size_t i = 1; i < p.stamps.stamps.size(); ++i) {
		const Stamp &a = p.stamps.stamps[i - 1], &b = p.stamps.stamps[i];
		CHECK((a.cy < b.cy || (a.cy == b.cy && a.cx < b.cx)));
	}
}

TEST_CASE("the entrance, boss and event rectangles are separate and inside the interior, with a free cell round each")
{
	for (const char *name : kSizes) {
		int w = 0, h = 0;
		REQUIRE(mesaSizeCells(name, &w, &h));
		std::set<std::pair<int, int>> entrances;
		for (uint64_t seed = 0; seed < 200; ++seed) {
			MesaRequest r = request(seed, w, h);
			r.options.originX = 6000;
			r.options.originY = 4200;
			const MesaPlan p = planMesaMap(r);
			REQUIRE(p.ok);
			const TileRect *rects[3] = {&p.stamps.entrance, &p.stamps.boss, &p.stamps.event};
			for (int i = 0; i < 3; ++i) {
				REQUIRE(rects[i]->known);
				CHECK(rects[i]->w == 16);
				CHECK(rects[i]->h == 16);
				CHECK((rects[i]->x - 6000) % 8 == 0);
				CHECK(rects[i]->x >= 6000 + 16);  // one interior cell in from the frame
				CHECK(rects[i]->y >= 4200 + 16);
				CHECK(rects[i]->x + 16 <= 6000 + (w + 1) * 8);
				CHECK(rects[i]->y + 16 <= 4200 + (h + 1) * 8);
				for (int j = i + 1; j < 3; ++j) {
					const TileRect &a = *rects[i], &b = *rects[j];
					CHECK_FALSE((a.x < b.x + b.w && b.x < a.x + a.w && a.y < b.y + b.h && b.y < a.y + a.h));
				}
			}
			entrances.insert({p.stamps.entrance.x, p.stamps.entrance.y});
			int roleCount = 0;
			for (const Stamp &s : p.stamps.stamps) roleCount += s.role == layout::Role::Entrance ? 1 : 0;
			CHECK(roleCount == 1);
		}
		CHECK(entrances.size() == 4);  // the seed picks among the four inner corners
	}
}

TEST_CASE("pinned files stay in range and do not move the layout")
{
	const MesaRequest free = request(11, 12, 10);
	MesaRequest pinned = free;
	pinned.options.pinFiles = true;
	const MesaPlan a = planMesaMap(free), b = planMesaMap(pinned);
	REQUIRE(a.ok);
	REQUIRE(b.ok);
	REQUIRE(a.stamps.stamps.size() == b.stamps.stamps.size());
	bool anyFile = false;
	for (size_t i = 0; i < a.stamps.stamps.size(); ++i) {
		CHECK(a.stamps.stamps[i].lvlPrest == b.stamps.stamps[i].lvlPrest);
		CHECK(a.stamps.stamps[i].cx == b.stamps.stamps[i].cx);
		CHECK(a.stamps.stamps[i].cy == b.stamps.stamps[i].cy);
		CHECK(a.stamps.stamps[i].file == -1);
		const int file = b.stamps.stamps[i].file;
		int set = 0, w = 1, h = 1;
		const int limit = fillSize(b.stamps.stamps[i].lvlPrest, &set, &w, &h) ? fillSet(set).fills[b.stamps.stamps[i].lvlPrest - (set == 0 ? 812 : (set == 1 ? 817 : 823))].files : 3;
		CHECK(file >= 0);
		CHECK(file < limit);
		anyFile = anyFile || file > 0;
	}
	CHECK(anyFile);
	CHECK(joined(checkMesaPlan(pinned, b)).empty());
}

TEST_CASE("a plan is the same for the same seed and differs between seeds")
{
	const MesaPlan a = planMesaMap(request(5, 12, 10)), b = planMesaMap(request(5, 12, 10));
	REQUIRE(a.ok);
	CHECK(a.hash == b.hash);
	CHECK(mesaPlanAscii(a) == mesaPlanAscii(b));
	std::set<uint64_t> hashes;
	for (uint64_t seed = 0; seed < 50; ++seed) hashes.insert(planMesaMap(request(seed, 12, 10)).hash);
	CHECK(hashes.size() > 40);
}

TEST_CASE("bad sizes and tight limits fail with a reason, never a half plan")
{
	for (const MesaRequest &r : {request(1, kMinCells - 1, 8), request(1, 8, kMinCells - 1), request(1, kMaxCellsW + 1, 8), request(1, 8, kMaxCellsH + 1)}) {
		const MesaPlan p = planMesaMap(r);
		CHECK_FALSE(p.ok);
		CHECK_FALSE(p.reason.empty());
		CHECK(p.stamps.stamps.empty());
		CHECK_FALSE(checkMesaPlan(r, p).empty());
	}
	MesaRequest tight = request(1, 14, 12);
	tight.options.maxTilesX = 120;
	const MesaPlan p = planMesaMap(tight);
	CHECK_FALSE(p.ok);
	CHECK(p.reason.find("128 x 112") != std::string::npos);
	tight.options.maxTilesX = kLevelTilesX;
	tight.options.maxTilesY = kLevelTilesY;
	CHECK(planMesaMap(tight).ok);
}

TEST_CASE("the checker catches a hole, a double cover, a wrong ring piece and a moved room")
{
	const MesaRequest r = request(3, 10, 8);
	const MesaPlan good = planMesaMap(r);
	REQUIRE(good.ok);
	REQUIRE(joined(checkMesaPlan(r, good)).empty());

	MesaPlan hole = good;
	for (size_t i = 0; i < hole.stamps.stamps.size(); ++i)
		if (hole.stamps.stamps[i].cx > 0 && hole.stamps.stamps[i].cy > 0 && hole.stamps.stamps[i].cx < 11 && hole.stamps.stamps[i].cy < 9) {
			hole.stamps.stamps.erase(hole.stamps.stamps.begin() + static_cast<std::ptrdiff_t>(i));
			break;
		}
	hole.hash = stampPlanHash(hole.stamps);
	CHECK_FALSE(checkMesaPlan(r, hole).empty());

	MesaPlan twice = good;
	for (Stamp &s : twice.stamps.stamps)
		if (s.cx == 1 && s.cy == 1) {
			s.lvlPrest = fillSet(twice.fillSet).fills[0].lvlPrest;  // a 24 x 24 piece over its neighbours
			break;
		}
	twice.hash = stampPlanHash(twice.stamps);
	CHECK_FALSE(checkMesaPlan(r, twice).empty());

	MesaPlan swapped = good;
	for (Stamp &s : swapped.stamps.stamps)
		if (s.cx == 0 && s.cy == 4) s.lvlPrest = 802;  // the east piece on the west side
	swapped.hash = stampPlanHash(swapped.stamps);
	CHECK_FALSE(checkMesaPlan(r, swapped).empty());

	MesaPlan moved = good;
	moved.stamps.boss.x += 8;
	moved.hash = stampPlanHash(moved.stamps);
	CHECK_FALSE(checkMesaPlan(r, moved).empty());
}

TEST_CASE("planOutdoorMap plans Mesa through the tileset table and the grid comparison agrees")
{
	MapRequest m;
	m.tileset = "mesa";
	m.size = "Medium";
	m.seed = 42;
	m.options.originX = 6000;
	m.options.originY = 4200;
	m.options.maxTilesX = kLevelTilesX;
	m.options.maxTilesY = kLevelTilesY;
	const MapPlan plan = planOutdoorMap(m, builtinJungleCatalogue());
	INFO(plan.reason);
	REQUIRE(plan.ok);
	CHECK(plan.hash != 0);
	CHECK(plan.layout.cells.empty());
	CHECK(plan.stamps.cellsW == 12);
	CHECK(plan.stamps.cellsH == 10);
	CHECK(planCacheKey(m).find("mesa") != std::string::npos);

	// A grid sample as the engine would hold it: a piece in the first cell of every stamp, nothing void.
	GridSample grid;
	grid.piece.assign(static_cast<size_t>(plan.stamps.cellsW) * plan.stamps.cellsH, 0);
	grid.flags.assign(grid.piece.size(), 0x200);
	for (const Stamp &s : plan.stamps.stamps) grid.piece[static_cast<size_t>(s.cy) * plan.stamps.cellsW + s.cx] = static_cast<uint64_t>(s.lvlPrest);
	CHECK(joined(compareGrid(plan.stamps, grid)).empty());
	grid.flags[0] = 0x100;
	CHECK_FALSE(compareGrid(plan.stamps, grid).empty());

	m.size = "Huge";
	CHECK_FALSE(planOutdoorMap(m, builtinJungleCatalogue()).ok);
	m.size = "ExtraLarge";
	m.options.maxTilesX = 100;
	const MapPlan tight = planOutdoorMap(m, builtinJungleCatalogue());
	CHECK_FALSE(tight.ok);
	CHECK(tight.reason.find("Mesa") != std::string::npos);
}
