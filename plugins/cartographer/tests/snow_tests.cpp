// Host tests for the Cartographer Snow (Act 5 Barricade) plan. No game, no SDK: the plan is pure C++.
// The piece table is checked against tools/catalogue.tsv (derived rows, no game files).
#define _CRT_SECURE_NO_WARNINGS
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

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
const uint64_t GOLDEN_HORIZONTAL_HASH = 5564385770962023109ull;

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

TEST_CASE("sizes: Small, Medium, Large, and the game's own Frozen Tundra size")
{
	int w = 0, h = 0;
	CHECK(snowSizeCells("", &w, &h));
	CHECK(w == 8);
	CHECK(h == 5);
	CHECK(w * kCellTiles == 128); // level 117 is 128 x 80 tiles
	CHECK(h * kCellTiles == 80);
	CHECK(snowSizeCells("SMALL", &w, &h));
	CHECK(w == 6);
	CHECK(h == 4);
	CHECK(snowSizeCells("Large", &w, &h));
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
		CHECK(plan.stamps.tilesW == r.cellsW * 16);
		CHECK(plan.stamps.tilesH == r.cellsH * 16);
		CHECK(plan.stamps.voidCells.empty());
		CHECK(plan.stamps.event.known == false);
		CHECK(plan.stamps.boss.known == false);
	}
	CHECK(horizontal > 1500);
	CHECK(vertical > 1500);
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
	CHECK(p.stamps.cellsW == 16);
	CHECK(p.stamps.cellsH == 10);
	CHECK(p.stamps.entrance.known);
	CHECK(p.stamps.entrance.x == 2000 + 16);
	CHECK(p.stamps.entrance.y == 1896 + 2 * 16);
	CHECK(p.stamps.entrance.w == 16);
	CHECK(p.stamps.entrance.h == 16);
	int entrances = 0;
	for (size_t i = 0; i < p.stamps.stamps.size(); ++i) {
		const Stamp &s = p.stamps.stamps[i];
		CHECK(s.cx == (static_cast<int>(i) % 8) * 2);
		CHECK(s.cy == (static_cast<int>(i) / 8) * 2);
		if (s.role == layout::Role::Entrance) {
			++entrances;
			CHECK(findPiece(s.lvlPrest)->kind == Kind::RunH);
		}
	}
	CHECK(entrances == 1);

	SnowRequest v = request(99, 5, 8, Orientation::Vertical);
	const SnowPlan pv = planSnowMap(v);
	REQUIRE(pv.ok);
	CHECK(pv.stamps.entrance.x == 2 * 16);
	CHECK(pv.stamps.entrance.y == 16);
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
	limited.options.maxTilesX = 127;
	const SnowPlan over = planSnowMap(limited);
	CHECK_FALSE(over.ok);
	CHECK(over.reason.find("128 x 80") != std::string::npos);
	CHECK(over.grid.empty());
	CHECK(over.stamps.stamps.empty());
	limited.options.maxTilesX = 128;
	limited.options.maxTilesY = 80;
	CHECK(planSnowMap(limited).ok);
	limited.options.maxTilesY = 79;
	CHECK_FALSE(planSnowMap(limited).ok);
}

TEST_CASE("the independent check catches a broken plan")
{
	const SnowRequest r = request(7, 8, 5, Orientation::Horizontal);
	const SnowPlan good = planSnowMap(r);
	REQUIRE(good.ok);
	REQUIRE(checkSnowPlan(r, good).empty());

	{ // a run swapped for one whose start socket does not match
		SnowPlan p = good;
		const size_t at = 1;
		const snow::Piece *cur = findPiece(p.grid[at]);
		const int wrong = cur->start == 'a' ? 988 : 987; // BB or AA
		p.grid[at] = wrong;
		p.stamps.stamps[at].lvlPrest = wrong;
		p.hash = stampPlanHash(p.stamps);  // so the hash does not hide what the case is about
		CHECK_FALSE(checkSnowPlan(r, p).empty());
	}
	{ // a cap where a run belongs
		SnowPlan p = good;
		p.grid[3] = 995;
		p.stamps.stamps[3].lvlPrest = 995;
		p.hash = stampPlanHash(p.stamps);  // so the hash does not hide what the case is about
		CHECK_FALSE(checkSnowPlan(r, p).empty());
	}
	{ // a jungle piece
		SnowPlan p = good;
		p.grid[2] = 530;
		p.stamps.stamps[2].lvlPrest = 530;
		p.hash = stampPlanHash(p.stamps);  // so the hash does not hide what the case is about
		CHECK_FALSE(checkSnowPlan(r, p).empty());
	}
	{ // a stamp that disagrees with the grid
		SnowPlan p = good;
		p.stamps.stamps[4].lvlPrest = p.stamps.stamps[4].lvlPrest == 987 ? 991 : 987;
		p.hash = stampPlanHash(p.stamps);  // so the hash does not hide what the case is about
		CHECK_FALSE(checkSnowPlan(r, p).empty());
	}
	{ // a stamp moved off the 16-tile grid
		SnowPlan p = good;
		p.stamps.stamps[5].cx += 1;
		p.hash = stampPlanHash(p.stamps);  // so the hash does not hide what the case is about
		CHECK_FALSE(checkSnowPlan(r, p).empty());
	}
	{ // a file index on an unpinned plan, and one out of range
		SnowPlan p = good;
		p.stamps.stamps[5].file = 0;
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
