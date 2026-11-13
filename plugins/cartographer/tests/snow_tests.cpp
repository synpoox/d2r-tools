// Host tests for the Cartographer Snow (Act 5 Barricade) plan. No game, no SDK: the plan is pure C++.
// The piece table is checked against tools/catalogue.tsv (derived rows, no game files).
#define _CRT_SECURE_NO_WARNINGS
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <set>
#include <string>
#include <vector>

#include "outdoor.h"
#include "snow_plan.h"
#include "stamp.h"

using namespace cartographer;
using namespace cartographer::outdoor;
using namespace cartographer::outdoor::snow;

namespace {

std::string toolsPath(const char *name)
{
	std::string here = __FILE__;
	return here.substr(0, here.find_last_of("/\\") + 1) + "../tools/" + name;
}

std::string lowered(std::string s)
{
	for (char &c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
	return s;
}

SnowRequest request(uint64_t seed, int w, int h, Orientation o)
{
	SnowRequest r;
	r.seed = seed;
	r.cellsW = w;
	r.cellsH = h;
	r.autoOrientation = false;
	r.orientation = o;
	return r;
}

// Goldens: run with CARTOGRAPHER_GOLDEN=1 to print the current values when a change is meant to move them.
bool goldenMode()
{
	return std::getenv("CARTOGRAPHER_GOLDEN") != nullptr;
}

const std::string GOLDEN_HORIZONTAL =
	"Fa aa ab bb ba aa ab bR\n"
	"Fb ba ab bb bb bb bb bR\n"
	"Fb ba aa ab ba aa ab bR\n"
	"Fb bb ba aa ab bb ba aR\n"
	"Fa ab ba aa ab bb ba aR\n";
const uint64_t GOLDEN_HORIZONTAL_HASH = 10896496030343949601ull;

} // namespace

TEST_CASE("the piece table is the sixteen Snow rows, in lvlprest order")
{
	const std::vector<snow::Piece> &all = pieces();
	REQUIRE(all.size() == 16);
	int kinds[6] = {0, 0, 0, 0, 0, 0};
	for (size_t i = 0; i < all.size(); ++i) {
		CHECK(all[i].lvlPrest == kFirstLvlPrest + static_cast<int>(i));
		CHECK(all[i].dirtLvlPrest() == 915 + static_cast<int>(i));
		CHECK(std::string(all[i].name).find("Snow") != std::string::npos);
		CHECK(findPiece(all[i].lvlPrest) == &all[i]);
		++kinds[static_cast<int>(all[i].kind)];
		CHECK(all[i].files >= 1);
	}
	CHECK(all.back().lvlPrest == kLastLvlPrest);
	for (int n : kinds) CHECK((n == 4 || n == 2));
	CHECK(kinds[static_cast<int>(Kind::RunH)] == 4);
	CHECK(kinds[static_cast<int>(Kind::RunV)] == 4);
	CHECK(findPiece(986) == nullptr);
	CHECK(findPiece(1003) == nullptr);
	CHECK(findPiece(529) == nullptr);
}

TEST_CASE("every run has both sockets of its axis and every cap one")
{
	for (const snow::Piece &p : pieces()) {
		const bool h = p.kind == Kind::RunH || p.kind == Kind::CapEast || p.kind == Kind::CapWest;
		const std::string letters = h ? "ab" : "cd";
		const bool run = p.kind == Kind::RunH || p.kind == Kind::RunV;
		const bool startCap = p.kind == Kind::CapWest || p.kind == Kind::CapNorth;
		if (run) {
			CHECK(letters.find(p.start) != std::string::npos);
			CHECK(letters.find(p.end) != std::string::npos);
		} else if (startCap) {
			CHECK(p.start == '\0');
			CHECK(letters.find(p.end) != std::string::npos);
		} else {
			CHECK(letters.find(p.start) != std::string::npos);
			CHECK(p.end == '\0');
		}
	}
	// Each (kind, socket) a line needs exists: a start cap per letter, a run per start letter (two), an end cap per letter.
	for (const char *letters : {"ab", "cd"}) {
		for (const char *l = letters; *l; ++l) {
			int startCaps = 0, runs = 0, endCaps = 0;
			for (const snow::Piece &p : pieces()) {
				const bool inAxis = std::string(letters).find(p.start ? p.start : p.end) != std::string::npos;
				if (!inAxis) continue;
				if ((p.kind == Kind::CapWest || p.kind == Kind::CapNorth) && p.end == *l) ++startCaps;
				if ((p.kind == Kind::RunH || p.kind == Kind::RunV) && p.start == *l) ++runs;
				if ((p.kind == Kind::CapEast || p.kind == Kind::CapSouth) && p.start == *l) ++endCaps;
			}
			CHECK(startCaps == 1);
			CHECK(runs == 2);
			CHECK(endCaps == 1);
		}
	}
}

TEST_CASE("the piece table matches the Snow rows of tools/catalogue.tsv")
{
	const LoadResult loaded = loadCatalogueFile(toolsPath("catalogue.tsv"));
	REQUIRE(loaded.ok());
	int found = 0;
	for (const snow::Piece &p : pieces()) {
		const outdoor::Piece *row = nullptr;
		for (const outdoor::Piece &c : loaded.catalogue.pieces)
			if (lowered(c.tileset) == "snow" && c.lvlPrest == p.lvlPrest) row = &c;
		REQUIRE_MESSAGE(row != nullptr, "no catalogue row for " << p.lvlPrest);
		++found;
		CHECK(row->name == p.name);
		CHECK(row->levelType == kLevelType);
		CHECK(row->w == 1);
		CHECK(row->h == 1);
		REQUIRE(!row->ds1.empty());
		CHECK(lowered(row->ds1.front()) == lowered(p.ds1));
		CHECK(static_cast<int>(row->ds1.size()) >= p.files);
	}
	CHECK(found == 16);
}

TEST_CASE("sizes: Small, Medium, Large, ExtraLarge, and the game's own Frozen Tundra size")
{
	int w = 0, h = 0;
	CHECK(snowSizeCells("", &w, &h));
	CHECK(w == 8);
	CHECK(h == 5);
	CHECK(w * kCellTiles == 128); // level 117 is 128 x 80 tiles
	CHECK(h * kCellTiles == 80);
	CHECK(snowSizeCells("Small", &w, &h));
	CHECK(w == 4);
	CHECK(h == 3);
	CHECK(snowSizeCells("MEDIUM", &w, &h));
	CHECK(w == 6);
	CHECK(h == 4);
	CHECK(snowSizeCells("ExtraLarge", &w, &h));
	CHECK(w == 10);
	CHECK(h == 7);
	CHECK_FALSE(snowSizeCells("Huge", &w, &h));
}

TEST_CASE("5000 seeds in both orientations plan, pass the check and chain their fence sockets")
{
	int horizontal = 0, vertical = 0;
	for (uint64_t seed = 0; seed < 5000; ++seed) {
		SnowRequest r;
		r.seed = seed * 7919 + 1;
		r.cellsW = 3 + static_cast<int>(seed % 10);
		r.cellsH = 3 + static_cast<int>((seed / 10) % 10);
		r.options.pinFiles = (seed % 2) == 0;
		const SnowPlan plan = planSnowMap(r);
		REQUIRE_MESSAGE(plan.ok, plan.reason);
		CHECK(checkSnowPlan(r, plan).empty());
		(plan.orientation == Orientation::Horizontal ? horizontal : vertical)++;
		CHECK(plan.hash == stampPlanHash(plan.stamps));
		CHECK(plan.stamps.tilesW == (r.cellsW + 2 * kEdgeCells) * 16);
		CHECK(plan.stamps.tilesH == (r.cellsH + 2 * kEdgeCells) * 16);
		CHECK(plan.stamps.voidCells.empty());
	}
	CHECK(horizontal > 1500);
	CHECK(vertical > 1500);
}

TEST_CASE("entrance, boss and event rooms: three different run cells inside the layout, the same for a seed")
{
	const auto cellOf = [](const TileRect &r, const SnowRequest &q, int *cx, int *cy) {
		*cx = (r.x - q.options.originX) / 16 - kEdgeCells;  // the rectangle's own cell, past the edge ring
		*cy = (r.y - q.options.originY) / 16 - kEdgeCells;
		return (r.x - q.options.originX) % 16 == 0 && (r.y - q.options.originY) % 16 == 0 && r.w == 16 && r.h == 16;
	};
	for (uint64_t seed = 0; seed < 3000; ++seed) {
		SnowRequest r;
		r.seed = seed * 104729 + 3;
		r.cellsW = 3 + static_cast<int>(seed % 10);
		r.cellsH = 3 + static_cast<int>((seed / 10) % 10);
		r.options.originX = 6000;
		r.options.originY = 3600;
		const SnowPlan a = planSnowMap(r);
		REQUIRE_MESSAGE(a.ok, a.reason);
		const SnowPlan b = planSnowMap(r);
		const StampPlan &s = a.stamps;
		CHECK(s.boss.x == b.stamps.boss.x);
		CHECK(s.boss.y == b.stamps.boss.y);
		CHECK(s.event.x == b.stamps.event.x);
		CHECK(s.event.y == b.stamps.event.y);
		const bool horizontal = a.orientation == Orientation::Horizontal;
		const int along = horizontal ? r.cellsW : r.cellsH;
		const int runCells = (along - 2) * (horizontal ? r.cellsH : r.cellsW);
		if (runCells < 3) {
			CHECK_FALSE(s.boss.known);
			CHECK_FALSE(s.event.known);
			continue;
		}
		REQUIRE(s.entrance.known);
		REQUIRE(s.boss.known);
		REQUIRE(s.event.known);
		int cells[3][2];
		const TileRect *rects[3] = {&s.entrance, &s.boss, &s.event};
		for (int i = 0; i < 3; ++i) {
			CHECK(cellOf(*rects[i], r, &cells[i][0], &cells[i][1]));
			CHECK(cells[i][0] >= 0);
			CHECK(cells[i][1] >= 0);
			CHECK(cells[i][0] < r.cellsW);
			CHECK(cells[i][1] < r.cellsH);
			const int index = horizontal ? cells[i][0] : cells[i][1];
			CHECK(index > 0);  // never a cap
			CHECK(index < along - 1);
		}
		for (int i = 0; i < 3; ++i)
			for (int j = i + 1; j < 3; ++j) CHECK((cells[i][0] != cells[j][0] || cells[i][1] != cells[j][1]));
	}
}

TEST_CASE("a Snow plan with a moved boss or event rectangle fails the check")
{
	const SnowRequest r = request(7, 8, 5, Orientation::Horizontal);
	const SnowPlan good = planSnowMap(r);
	REQUIRE(good.ok);
	{
		SnowPlan p = good;
		p.stamps.boss.x += 16;
		CHECK_FALSE(checkSnowPlan(r, p).empty());
	}
	{
		SnowPlan p = good;
		p.stamps.event = p.stamps.entrance;
		CHECK_FALSE(checkSnowPlan(r, p).empty());
	}
	{
		SnowPlan p = good;
		p.stamps.event = TileRect{};
		CHECK_FALSE(checkSnowPlan(r, p).empty());
	}
}

TEST_CASE("a fixed orientation is kept and every line starts and ends with a cap")
{
	for (uint64_t seed = 1; seed <= 200; ++seed) {
		const SnowRequest rh = request(seed, 8, 5, Orientation::Horizontal);
		const SnowPlan ph = planSnowMap(rh);
		REQUIRE(ph.ok);
		CHECK(ph.orientation == Orientation::Horizontal);
		for (int y = 0; y < 5; ++y) {
			CHECK(findPiece(ph.grid[static_cast<size_t>(y) * 8])->kind == Kind::CapWest);
			CHECK(findPiece(ph.grid[static_cast<size_t>(y) * 8 + 7])->kind == Kind::CapEast);
			for (int x = 1; x < 7; ++x) CHECK(findPiece(ph.grid[static_cast<size_t>(y) * 8 + x])->kind == Kind::RunH);
		}
		const SnowRequest rv = request(seed, 5, 8, Orientation::Vertical);
		const SnowPlan pv = planSnowMap(rv);
		REQUIRE(pv.ok);
		CHECK(pv.orientation == Orientation::Vertical);
		for (int x = 0; x < 5; ++x) {
			CHECK(findPiece(pv.grid[static_cast<size_t>(x)])->kind == Kind::CapNorth);
			CHECK(findPiece(pv.grid[static_cast<size_t>(7) * 5 + x])->kind == Kind::CapSouth);
			for (int y = 1; y < 7; ++y) CHECK(findPiece(pv.grid[static_cast<size_t>(y) * 5 + x])->kind == Kind::RunV);
		}
	}
}

TEST_CASE("the same request gives the same plan; seeds give different ones")
{
	const SnowRequest r = request(12345, 8, 5, Orientation::Horizontal);
	const SnowPlan a = planSnowMap(r), b = planSnowMap(r);
	REQUIRE(a.ok);
	CHECK(a.hash == b.hash);
	CHECK(a.grid == b.grid);
	std::set<uint64_t> hashes;
	std::set<int> used;
	for (uint64_t seed = 0; seed < 300; ++seed) {
		const SnowPlan p = planSnowMap(request(seed, 8, 5, Orientation::Horizontal));
		REQUIRE(p.ok);
		hashes.insert(p.hash);
		for (int id : p.grid) used.insert(id);
	}
	CHECK(hashes.size() > 290);
	// All eight horizontal pieces show up: both caps pairs and all four runs.
	CHECK(used.size() == 8);
}

TEST_CASE("pinned files stay inside each piece's file count, unpinned plans leave -1")
{
	bool sawSecond = false;
	for (uint64_t seed = 0; seed < 500; ++seed) {
		SnowRequest r = request(seed, 8, 5, Orientation::Horizontal);
		r.options.pinFiles = true;
		const SnowPlan pinned = planSnowMap(r);
		REQUIRE(pinned.ok);
		for (const Stamp &s : pinned.stamps.stamps) {
			CHECK(s.file >= 0);
			if (s.lvlPrest < kFirstLvlPrest) {  // the edge ring: one file
				CHECK(s.file == 0);
				continue;
			}
			CHECK(s.file < findPiece(s.lvlPrest)->files);
			if (s.file == 1) {
				sawSecond = true;
				CHECK(s.lvlPrest == 987); // only Snow 1 has a second file
			}
		}
		r.options.pinFiles = false;
		const SnowPlan free = planSnowMap(r);
		REQUIRE(free.ok);
		for (const Stamp &s : free.stamps.stamps) CHECK(s.file == -1);
		CHECK(free.grid == pinned.grid); // the file draw does not move the layout of later cells
	}
	CHECK(sawSecond);
}

TEST_CASE("stamps sit on the 8-tile grid, row-major, and the entrance is a run cell")
{
	SnowRequest r = request(99, 8, 5, Orientation::Horizontal);
	r.options.originX = 2000;
	r.options.originY = 1896;
	const SnowPlan p = planSnowMap(r);
	REQUIRE(p.ok);
	CHECK(p.stamps.cellsW == 20);  // (8 + 2) cells of 2 grid cells
	CHECK(p.stamps.cellsH == 14);  // (5 + 2)
	CHECK(p.stamps.entrance.known);
	CHECK(p.stamps.entrance.x == 2000 + 2 * 16);  // cell 1 of the rectangle, one edge cell in
	CHECK(p.stamps.entrance.y == 1896 + 3 * 16);
	CHECK(p.stamps.entrance.w == 16);
	CHECK(p.stamps.entrance.h == 16);
	int entrances = 0;
	for (size_t i = 0; i < p.stamps.stamps.size(); ++i) {
		const Stamp &s = p.stamps.stamps[i];
		CHECK(s.cx == (static_cast<int>(i) % 10) * 2);
		CHECK(s.cy == (static_cast<int>(i) / 10) * 2);
		if (s.role == layout::Role::Entrance) {
			++entrances;
			CHECK(findPiece(s.lvlPrest)->kind == Kind::RunH);
		}
	}
	CHECK(entrances == 1);

	SnowRequest v = request(99, 5, 8, Orientation::Vertical);
	const SnowPlan pv = planSnowMap(v);
	REQUIRE(pv.ok);
	CHECK(pv.stamps.entrance.x == 3 * 16);
	CHECK(pv.stamps.entrance.y == 2 * 16);
}

TEST_CASE("the edge ring: every side of the rectangle has kEdgeCells cells of the edge piece, at every size and orientation (issue #395)")
{
	CHECK(kEdgeCells >= 1);
	// Not fence pieces: the fence grammar never picks them.
	for (const int id : {kEdgePieces.west, kEdgePieces.north, kEdgePieces.east, kEdgePieces.south, kEdgePieces.nw, kEdgePieces.ne, kEdgePieces.se, kEdgePieces.sw}) CHECK(findPiece(id) == nullptr);
	const int sizes[4][2] = {{4, 3}, {6, 4}, {8, 5}, {10, 7}};  // Small, Medium, Large, ExtraLarge
	for (const auto &size : sizes) {
		for (const Orientation o : {Orientation::Horizontal, Orientation::Vertical}) {
			const SnowRequest r = request(31, size[0], size[1], o);
			const SnowPlan p = planSnowMap(r);
			REQUIRE_MESSAGE(p.ok, p.reason);
			const int TW = size[0] + 2 * kEdgeCells, TH = size[1] + 2 * kEdgeCells;
			CHECK(p.stamps.tilesW == TW * kCellTiles);
			CHECK(p.stamps.tilesH == TH * kCellTiles);
			CHECK(p.stamps.macroMinX == -kEdgeCells);
			CHECK(p.stamps.macroMinY == -kEdgeCells);
			REQUIRE(p.stamps.stamps.size() == static_cast<size_t>(TW) * TH);
			// Every stamp: outside the rectangle the edge piece, inside a Snow fence piece; the outermost cells of the level are always ring.
			int ring = 0, inside = 0;
			for (int y = 0; y < TH; ++y)
				for (int x = 0; x < TW; ++x) {
					const Stamp &s = p.stamps.stamps[static_cast<size_t>(y) * TW + x];
					const bool rect = x >= kEdgeCells && y >= kEdgeCells && x < TW - kEdgeCells && y < TH - kEdgeCells;
					if (rect) {
						CHECK(s.lvlPrest >= 987);
						CHECK(s.lvlPrest <= 1002);
						++inside;
					} else {
						CHECK(s.lvlPrest == edgePieceAt(x - kEdgeCells, y - kEdgeCells, size[0], size[1]));
						CHECK(s.file == -1);
						++ring;
					}
				}
			CHECK(inside == size[0] * size[1]);
			CHECK(ring == TW * TH - size[0] * size[1]);
			// The rectangle's own rooms are untouched: entrance, boss and event stay inside the rectangle, never on the ring.
			const TileRect *rects[3] = {&p.stamps.entrance, &p.stamps.boss, &p.stamps.event};
			for (const TileRect *t : rects) {
				if (!t->known) continue;
				CHECK(t->x >= kEdgeCells * kCellTiles);
				CHECK(t->y >= kEdgeCells * kCellTiles);
				CHECK(t->x + t->w <= (TW - kEdgeCells) * kCellTiles);
				CHECK(t->y + t->h <= (TH - kEdgeCells) * kCellTiles);
			}
			// The ring is on the engine's 8-tile grid like every other stamp, so the level has no cell without a piece.
			CHECK(p.stamps.cellsW == TW * 2);
			CHECK(p.stamps.cellsH == TH * 2);
			CHECK(p.stamps.voidCells.empty());
		}
	}
}

TEST_CASE("the edge ring has its own border piece on each side and each corner (issue #395)")
{
	// The ring cell west, north, east and south of a 6 x 4 rectangle, then its four corners.
	CHECK(edgePieceAt(-1, 2, 6, 4) == kEdgePieces.west);
	CHECK(edgePieceAt(3, -1, 6, 4) == kEdgePieces.north);
	CHECK(edgePieceAt(6, 2, 6, 4) == kEdgePieces.east);
	CHECK(edgePieceAt(3, 4, 6, 4) == kEdgePieces.south);
	CHECK(edgePieceAt(-1, -1, 6, 4) == kEdgePieces.nw);
	CHECK(edgePieceAt(6, -1, 6, 4) == kEdgePieces.ne);
	CHECK(edgePieceAt(6, 4, 6, 4) == kEdgePieces.se);
	CHECK(edgePieceAt(-1, 4, 6, 4) == kEdgePieces.sw);
	CHECK(edgePieceAt(0, 0, 6, 4) == 0);
	CHECK(edgePieceAt(5, 3, 6, 4) == 0);
	// Eight different pieces, all rows of the game's own Act 5 Barricade border table (Cliff and Ravine Border 1 to 12 Snow, 957..982).
	std::set<int> seen;
	for (const int id : {kEdgePieces.west, kEdgePieces.north, kEdgePieces.east, kEdgePieces.south, kEdgePieces.nw, kEdgePieces.ne, kEdgePieces.se, kEdgePieces.sw}) {
		CHECK(id >= 957);
		CHECK(id <= 982);
		seen.insert(id);
	}
	CHECK(seen.size() == 8);
}

TEST_CASE("the edge ring is checked: a missing or wrong ring cell fails the plan check")
{
	const SnowRequest r = request(7, 8, 5, Orientation::Horizontal);
	const SnowPlan good = planSnowMap(r);
	REQUIRE(good.ok);
	const auto fails = [&](SnowPlan p) {
		p.hash = stampPlanHash(p.stamps);
		return !checkSnowPlan(r, p).empty();
	};
	{ // a corner of the ring holding a fence piece
		SnowPlan p = good;
		p.stamps.stamps.front().lvlPrest = 987;
		CHECK(fails(p));
	}
	{ // the last ring cell holding nothing
		SnowPlan p = good;
		p.stamps.stamps.back().lvlPrest = 0;
		CHECK(fails(p));
	}
	{ // a ring cell moved off its place
		SnowPlan p = good;
		p.stamps.stamps[3].cx += 2;
		CHECK(fails(p));
	}
	{ // a level box without the ring
		SnowPlan p = good;
		p.stamps.tilesW -= 2 * kEdgeCells * kCellTiles;
		CHECK(fails(p));
	}
	{ // the north side holding the piece of the west side
		SnowPlan p = good;
		p.stamps.stamps[1].lvlPrest = kEdgePieces.west;
		CHECK(fails(p));
	}
	{ // a pinned file on the ring of an unpinned plan
		SnowPlan p = good;
		p.stamps.stamps[1].file = 0;
		CHECK(fails(p));
	}
	// A level limit counts the ring: 10 x 7 cells need 12 x 9 with it.
	SnowRequest big = request(1, 10, 7, Orientation::Horizontal);
	big.options.maxTilesX = 192;
	big.options.maxTilesY = 144;
	CHECK(planSnowMap(big).ok);
	big.options.maxTilesY = 143;
	CHECK_FALSE(planSnowMap(big).ok);
}

TEST_CASE("bad requests are refused with a plain reason")
{
	CHECK_FALSE(planSnowMap(request(1, 2, 5, Orientation::Horizontal)).ok);  // needs 3 along the line
	CHECK_FALSE(planSnowMap(request(1, 5, 2, Orientation::Vertical)).ok);
	CHECK(planSnowMap(request(1, 2, 5, Orientation::Vertical)).ok);          // 5 along a column is fine, the width is free
	CHECK(planSnowMap(request(1, 1, 5, Orientation::Vertical)).ok);
	CHECK_FALSE(planSnowMap(request(1, 0, 5, Orientation::Horizontal)).ok);
	CHECK_FALSE(planSnowMap(request(1, 13, 5, Orientation::Horizontal)).ok);
	CHECK_FALSE(planSnowMap(request(1, 5, 13, Orientation::Vertical)).ok);
	CHECK(planSnowMap(request(1, 12, 12, Orientation::Horizontal)).ok);

	SnowRequest limited = request(1, 8, 5, Orientation::Horizontal);
	limited.options.maxTilesX = 159;  // 8 x 5 cells and one edge cell around them: 160 x 112 tiles
	const SnowPlan over = planSnowMap(limited);
	CHECK_FALSE(over.ok);
	CHECK(over.reason.find("160 x 112") != std::string::npos);
	CHECK(over.grid.empty());
	CHECK(over.stamps.stamps.empty());
	limited.options.maxTilesX = 160;
	limited.options.maxTilesY = 112;
	CHECK(planSnowMap(limited).ok);
	limited.options.maxTilesY = 111;
	CHECK_FALSE(planSnowMap(limited).ok);
}

TEST_CASE("the independent check catches a broken plan")
{
	const SnowRequest r = request(7, 8, 5, Orientation::Horizontal);
	const SnowPlan good = planSnowMap(r);
	REQUIRE(good.ok);
	REQUIRE(checkSnowPlan(r, good).empty());
	// The stamp of the rectangle's cell (x, y): the stamps start at the edge ring.
	const auto ix = [](int x, int y) { return static_cast<size_t>((y + kEdgeCells) * (8 + 2 * kEdgeCells) + x + kEdgeCells); };

	{ // a run swapped for one whose start socket does not match
		SnowPlan p = good;
		const size_t at = 1;
		const snow::Piece *cur = findPiece(p.grid[at]);
		const int wrong = cur->start == 'a' ? 988 : 987; // BB or AA
		p.grid[at] = wrong;
		p.stamps.stamps[ix(1, 0)].lvlPrest = wrong;
		p.hash = stampPlanHash(p.stamps);  // so the hash does not hide what the case is about
		CHECK_FALSE(checkSnowPlan(r, p).empty());
	}
	{ // a cap where a run belongs
		SnowPlan p = good;
		p.grid[3] = 995;
		p.stamps.stamps[ix(3, 0)].lvlPrest = 995;
		p.hash = stampPlanHash(p.stamps);  // so the hash does not hide what the case is about
		CHECK_FALSE(checkSnowPlan(r, p).empty());
	}
	{ // a jungle piece
		SnowPlan p = good;
		p.grid[2] = 530;
		p.stamps.stamps[ix(2, 0)].lvlPrest = 530;
		p.hash = stampPlanHash(p.stamps);  // so the hash does not hide what the case is about
		CHECK_FALSE(checkSnowPlan(r, p).empty());
	}
	{ // a stamp that disagrees with the grid
		SnowPlan p = good;
		p.stamps.stamps[ix(4, 0)].lvlPrest = p.stamps.stamps[ix(4, 0)].lvlPrest == 987 ? 991 : 987;
		p.hash = stampPlanHash(p.stamps);  // so the hash does not hide what the case is about
		CHECK_FALSE(checkSnowPlan(r, p).empty());
	}
	{ // a stamp moved off the 16-tile grid
		SnowPlan p = good;
		p.stamps.stamps[ix(5, 0)].cx += 1;
		p.hash = stampPlanHash(p.stamps);  // so the hash does not hide what the case is about
		CHECK_FALSE(checkSnowPlan(r, p).empty());
	}
	{ // a file index on an unpinned plan, and one out of range
		SnowPlan p = good;
		p.stamps.stamps[ix(5, 0)].file = 0;
		p.hash = stampPlanHash(p.stamps);  // so the hash does not hide what the case is about
		CHECK_FALSE(checkSnowPlan(r, p).empty());
	}
	{ // a missing stamp
		SnowPlan p = good;
		p.stamps.stamps.pop_back();
		p.hash = stampPlanHash(p.stamps);  // so the hash does not hide what the case is about
		CHECK_FALSE(checkSnowPlan(r, p).empty());
	}
	{ // the entrance rectangle on the wrong cell
		SnowPlan p = good;
		p.stamps.entrance.x += 16;
		p.hash = stampPlanHash(p.stamps);  // so the hash does not hide what the case is about
		CHECK_FALSE(checkSnowPlan(r, p).empty());
	}
	{ // a void list on a full rectangle
		SnowPlan p = good;
		p.stamps.voidCells.push_back({0, 0});
		p.hash = stampPlanHash(p.stamps);  // so the hash does not hide what the case is about
		CHECK_FALSE(checkSnowPlan(r, p).empty());
	}
	{ // checked against a different request
		SnowRequest other = r;
		other.cellsW = 9;
		CHECK_FALSE(checkSnowPlan(other, good).empty());
	}
}

TEST_CASE("golden plan")
{
	const SnowPlan p = planSnowMap(request(2026, 8, 5, Orientation::Horizontal));
	REQUIRE(p.ok);
	if (goldenMode()) std::printf("GOLDEN_HORIZONTAL:\n%s\nhash %llu\n", snowPlanAscii(p).c_str(), static_cast<unsigned long long>(p.hash));
	CHECK(snowPlanAscii(p) == GOLDEN_HORIZONTAL);
	CHECK(p.hash == GOLDEN_HORIZONTAL_HASH);
}

TEST_CASE("the check re-derives the orientation, the macro origin and the hash")
{
	SnowRequest r;
	r.seed = 11;
	r.cellsW = 8;
	r.cellsH = 5;
	const SnowPlan good = planSnowMap(r);
	REQUIRE(good.ok);
	REQUIRE(checkSnowPlan(r, good).empty());
	{ // an internally consistent plan of the other orientation, when the seed decides
		SnowRequest fixed = r;
		fixed.autoOrientation = false;
		fixed.orientation = good.orientation == Orientation::Horizontal ? Orientation::Vertical : Orientation::Horizontal;
		const SnowPlan other = planSnowMap(fixed);
		REQUIRE(other.ok);
		CHECK(checkSnowPlan(fixed, other).empty());
		CHECK_FALSE(checkSnowPlan(r, other).empty());
	}
	{
		SnowPlan p = good;
		p.stamps.macroMinX = 1;
		p.hash = stampPlanHash(p.stamps);
		CHECK_FALSE(checkSnowPlan(r, p).empty());
	}
	{
		SnowPlan p = good;
		p.hash ^= 1;
		CHECK_FALSE(checkSnowPlan(r, p).empty());
	}
}

TEST_CASE("an automatic orientation takes the one that fits the size")
{
	// 2 x 5 cells has no horizontal line of 3 cells but a vertical line of 5; 5 x 2 is the reverse. Whatever the seed picks, both plan.
	for (uint64_t seed = 0; seed < 64; ++seed) {
		SnowRequest tall;
		tall.seed = seed;
		tall.cellsW = 2;
		tall.cellsH = 5;
		const SnowPlan a = planSnowMap(tall);
		REQUIRE_MESSAGE(a.ok, a.reason);
		CHECK(a.orientation == Orientation::Vertical);
		SnowRequest wide = tall;
		wide.cellsW = 5;
		wide.cellsH = 2;
		const SnowPlan b = planSnowMap(wide);
		REQUIRE_MESSAGE(b.ok, b.reason);
		CHECK(b.orientation == Orientation::Horizontal);
		CHECK(checkSnowPlan(tall, a).empty());
	}
	SnowRequest neither;
	neither.cellsW = 2;
	neither.cellsH = 2;
	CHECK_FALSE(planSnowMap(neither).ok);
}

TEST_CASE("a 3 x 1 or 3 x 2 map has fewer than three run cells: it plans, with no boss or event rectangle")
{
	for (int h = 1; h <= 2; ++h) {
		for (uint64_t seed = 0; seed < 50; ++seed) {
			SnowRequest fixed = request(seed, 3, h, Orientation::Horizontal);
			fixed.options.originX = 6000;
			fixed.options.originY = 3600;
			const SnowPlan p = planSnowMap(fixed);
			REQUIRE_MESSAGE(p.ok, p.reason);
			CHECK(p.stamps.entrance.known);
			CHECK_FALSE(p.stamps.boss.known);
			CHECK_FALSE(p.stamps.event.known);
			CHECK(checkSnowPlan(fixed, p).empty());
			// Three cells across and one or two down has no vertical line of three.
			CHECK_FALSE(planSnowMap(request(seed, 3, h, Orientation::Vertical)).ok);
			SnowRequest autoPick;
			autoPick.seed = seed;
			autoPick.cellsW = 3;
			autoPick.cellsH = h;
			const SnowPlan a = planSnowMap(autoPick);
			REQUIRE_MESSAGE(a.ok, a.reason);
			CHECK(a.orientation == Orientation::Horizontal);
			CHECK_FALSE(a.stamps.boss.known);
			CHECK_FALSE(a.stamps.event.known);
		}
	}
}

TEST_CASE("the boss cell is a farthest run cell from the entrance and the event cell maximises its distance to the nearer of the two")
{
	for (uint64_t seed = 0; seed < 2000; ++seed) {
		const SnowRequest r = request(seed * 7919 + 1, 4 + static_cast<int>(seed % 9), 3 + static_cast<int>((seed / 9) % 8), seed % 2 ? Orientation::Horizontal : Orientation::Vertical);
		const SnowPlan p = planSnowMap(r);
		if (!p.ok || !p.stamps.boss.known) continue;
		const bool horizontal = p.orientation == Orientation::Horizontal;
		const int along = horizontal ? r.cellsW : r.cellsH;
		const auto cell = [](const TileRect &t, int *x, int *y) { *x = t.x / 16 - kEdgeCells; *y = t.y / 16 - kEdgeCells; };
		int ex, ey, bx, by, vx, vy;
		cell(p.stamps.entrance, &ex, &ey);
		cell(p.stamps.boss, &bx, &by);
		cell(p.stamps.event, &vx, &vy);
		const auto d2 = [](int ax, int ay, int cx, int cy) { return (ax - cx) * (ax - cx) + (ay - cy) * (ay - cy); };
		int bestBoss = 0, bestEvent = 0;
		for (int y = 0; y < r.cellsH; ++y)
			for (int x = 0; x < r.cellsW; ++x) {
				const int idx = horizontal ? x : y;
				if (idx == 0 || idx == along - 1 || (x == ex && y == ey)) continue;
				bestBoss = std::max(bestBoss, d2(x, y, ex, ey));
				if (x != bx || y != by) bestEvent = std::max(bestEvent, std::min(d2(x, y, ex, ey), d2(x, y, bx, by)));
			}
		CHECK(d2(bx, by, ex, ey) == bestBoss);
		CHECK(std::min(d2(vx, vy, ex, ey), d2(vx, vy, bx, by)) == bestEvent);
	}
}
