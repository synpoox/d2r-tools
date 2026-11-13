// Host tests for the Cartographer Siege (Act 5 Bloody Foothills) plan. No game, no SDK: the plan is pure C++.
#define _CRT_SECURE_NO_WARNINGS
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <set>
#include <string>
#include <vector>

#include "outdoor.h"
#include "outdoor_plan.h"
#include "outdoor_tilesets.h"
#include "siege_plan.h"
#include "stamp.h"

using namespace cartographer;
using namespace cartographer::outdoor;
using namespace cartographer::outdoor::siege;

namespace {

SiegeRequest request(uint64_t seed, int strips)
{
	SiegeRequest r;
	r.seed = seed;
	r.strips = strips;
	return r;
}

std::string joined(const std::vector<std::string> &v)
{
	std::string out;
	for (const std::string &s : v) out += s + "; ";
	return out;
}

bool goldenMode()
{
	return std::getenv("CARTOGRAPHER_GOLDEN") != nullptr;
}

} // namespace

TEST_CASE("the piece table has the 15 Siege strips in order, with the roles and files the plan relies on")
{
	const std::vector<siege::Piece> &all = siege::pieces();
	REQUIRE(all.size() == 15);
	for (size_t i = 0; i < all.size(); ++i) CHECK(all[i].lvlPrest == 865 + static_cast<int>(i));
	CHECK(siege::findPiece(865)->role == siege::Role::EastCap);
	CHECK(siege::findPiece(879)->role == siege::Role::WestCap);
	CHECK(siege::findPiece(878)->role == siege::Role::Fixed);
	CHECK(siege::findPiece(864) == nullptr);
	CHECK(siege::findPiece(880) == nullptr);
	int freeCount = 0;
	for (const siege::Piece &p : all) freeCount += p.role == siege::Role::Free ? 1 : 0;
	CHECK(freeCount == 4);
	CHECK(kFixedStrips + freeCount == kMaxStrips);
	CHECK(funnelBlock().size() == 8);
	CHECK(funnelBlock().front() == 875);
	CHECK(funnelBlock().back() == 868);
	CHECK(std::string(siege::roleName(siege::Role::Free)) == "free strip");
	// The seams of the game's own order (879 west to 865 east, the vanilla chain) all match.
	const std::vector<int> vanilla = {879, 878, 877, 876, 875, 874, 873, 872, 871, 870, 869, 868, 867, 866, 865};
	for (size_t i = 0; i + 1 < vanilla.size(); ++i) {
		CHECK_MESSAGE(siege::findPiece(vanilla[i])->east == siege::findPiece(vanilla[i + 1])->west, "seam after ", vanilla[i]);
	}
	for (const siege::Piece &p : all) CHECK(std::string(p.ds1).find("Expansion/Siege/") == 0);
	// The slot level fits the widest plan: 15 strips of 16 x 48.
	CHECK(kLevelTilesX == kMaxStrips * kStripWidth);
	CHECK(kLevelTilesY == kStripHeight);
}

TEST_CASE("size names map to 12 to 15 strips")
{
	int n = 0;
	CHECK(siegeSizeStrips("", &n));
	CHECK(n == 14);
	CHECK(siegeSizeStrips("Large", &n));
	CHECK(n == 14);
	CHECK(siegeSizeStrips("SMALL", &n));
	CHECK(n == 12);
	CHECK(siegeSizeStrips("medium", &n));
	CHECK(n == 13);
	CHECK(siegeSizeStrips("ExtraLarge", &n));
	CHECK(n == 15);
	n = 99;
	CHECK_FALSE(siegeSizeStrips("Huge", &n));
	CHECK(n == 99);
}

TEST_CASE("every size and many seeds plan cleanly: seams match, rooms are distinct, strips are not repeated")
{
	std::set<std::vector<int>> lines;
	for (int strips = kMinStrips; strips <= kMaxStrips; ++strips) {
		for (uint64_t seed = 0; seed < 200; ++seed) {
			const SiegeRequest r = request(seed, strips);
			const SiegePlan p = planSiegeMap(r);
			INFO("strips ", strips, " seed ", seed, ": ", p.reason);
			REQUIRE(p.ok);
			CHECK(joined(checkSiegePlan(r, p)).empty());
			REQUIRE(p.strips.size() == static_cast<size_t>(strips));
			CHECK(p.strips.front() == 879);
			CHECK(p.strips.back() == 865);
			CHECK(p.strips[1] == 878);
			CHECK(p.stamps.tilesW == strips * 16);
			CHECK(p.stamps.tilesH == 48);
			CHECK(p.stamps.cellsW == strips * 2);
			CHECK(p.stamps.cellsH == 6);
			CHECK(p.stamps.ok());
			CHECK(p.stamps.entrance.known);
			CHECK(p.stamps.boss.known);
			CHECK(p.stamps.event.known);
			CHECK(p.stamps.entrance.x != p.stamps.boss.x);
			CHECK(p.stamps.entrance.x != p.stamps.event.x);
			CHECK(p.stamps.boss.x != p.stamps.event.x);
			// The entrance is next to the town strip.
			CHECK(p.stamps.entrance.x == (strips - 2) * 16);
			CHECK(p.stamps.entrance.w == 16);
			CHECK(p.stamps.entrance.h == 48);
			std::set<int> seen(p.strips.begin(), p.strips.end());
			CHECK(seen.size() == p.strips.size());
			lines.insert(p.strips);
		}
	}
	CHECK(lines.size() > 4);  // the seed really changes the order
}

TEST_CASE("the plan is deterministic and the seed moves it")
{
	const SiegePlan a = planSiegeMap(request(12345, 14));
	const SiegePlan b = planSiegeMap(request(12345, 14));
	REQUIRE(a.ok);
	CHECK(a.hash == b.hash);
	CHECK(a.strips == b.strips);
	std::set<uint64_t> hashes;
	for (uint64_t seed = 0; seed < 50; ++seed) hashes.insert(planSiegeMap(request(seed, 14)).hash);
	CHECK(hashes.size() >= 3);
}

TEST_CASE("origin, pinned files and the size limits")
{
	SiegeRequest r = request(7, 13);
	r.options.originX = 6000;
	r.options.originY = 3900;
	r.options.pinFiles = true;
	r.options.maxTilesX = kLevelTilesX;
	r.options.maxTilesY = kLevelTilesY;
	const SiegePlan p = planSiegeMap(r);
	REQUIRE(p.ok);
	CHECK(p.stamps.originX == 6000);
	CHECK(p.stamps.originY == 3900);
	CHECK(p.stamps.entrance.x == 6000 + 11 * 16);
	CHECK(p.stamps.entrance.y == 3900);
	for (const Stamp &s : p.stamps.stamps) CHECK(s.file == 0);
	CHECK(levelRectOf(p.stamps).w == 13 * 16);
	CHECK(levelRectOf(p.stamps).h == 48);

	SiegeRequest tight = request(7, 15);
	tight.options.maxTilesX = 224;
	const SiegePlan refused = planSiegeMap(tight);
	CHECK_FALSE(refused.ok);
	CHECK(refused.reason.find("240 x 48") != std::string::npos);
	SiegeRequest flat = request(7, 12);
	flat.options.maxTilesY = 40;
	CHECK_FALSE(planSiegeMap(flat).ok);

	// Unpinned files are drawn by the engine.
	const SiegePlan loose = planSiegeMap(request(7, 13));
	for (const Stamp &s : loose.stamps.stamps) CHECK(s.file == -1);
}

TEST_CASE("bad sizes are refused with a reason, never thrown")
{
	for (int strips : {-1, 0, 11, 16, 100}) {
		const SiegePlan p = planSiegeMap(request(1, strips));
		CHECK_FALSE(p.ok);
		CHECK(p.reason.find("12 to 15 strips") != std::string::npos);
		CHECK(p.stamps.stamps.empty());
		CHECK(p.hash == 0);
	}
}

TEST_CASE("the checker catches a broken plan")
{
	const SiegeRequest r = request(99, 14);
	const SiegePlan good = planSiegeMap(r);
	REQUIRE(good.ok);
	REQUIRE(checkSiegePlan(r, good).empty());

	{  // two strips swapped: seams and stamps no longer agree
		SiegePlan p = good;
		std::swap(p.strips[2], p.strips[3]);
		CHECK_FALSE(checkSiegePlan(r, p).empty());
	}
	{  // a repeated strip
		SiegePlan p = good;
		p.strips[3] = p.strips[2];
		p.stamps.stamps[3].lvlPrest = p.strips[2];
		p.hash = stampPlanHash(p.stamps);
		CHECK_FALSE(checkSiegePlan(r, p).empty());
	}
	{  // a stamp off the grid
		SiegePlan p = good;
		p.stamps.stamps[4].cx += 1;
		p.hash = stampPlanHash(p.stamps);
		CHECK_FALSE(checkSiegePlan(r, p).empty());
	}
	{  // the entrance moved
		SiegePlan p = good;
		p.stamps.entrance.x += 16;
		p.hash = stampPlanHash(p.stamps);
		CHECK_FALSE(checkSiegePlan(r, p).empty());
	}
	{  // the boss on the entrance
		SiegePlan p = good;
		p.stamps.boss = p.stamps.entrance;
		p.hash = stampPlanHash(p.stamps);
		CHECK_FALSE(checkSiegePlan(r, p).empty());
	}
	{  // a stale hash
		SiegePlan p = good;
		p.hash ^= 1;
		CHECK_FALSE(checkSiegePlan(r, p).empty());
	}
	{  // a strip that is not a Siege piece
		SiegePlan p = good;
		p.strips[5] = 700;
		p.stamps.stamps[5].lvlPrest = 700;
		p.hash = stampPlanHash(p.stamps);
		CHECK_FALSE(checkSiegePlan(r, p).empty());
	}
	{  // a plan for another size
		SiegePlan p = good;
		p.strips.pop_back();
		CHECK_FALSE(checkSiegePlan(r, p).empty());
	}
}

TEST_CASE("the boss and the event stand on wide strips away from the entrance")
{
	for (uint64_t seed = 0; seed < 100; ++seed) {
		const SiegePlan p = planSiegeMap(request(seed, 14));
		REQUIRE(p.ok);
		const auto stripAt = [&](const TileRect &rect) { return siege::findPiece(p.strips[static_cast<size_t>(rect.x / 16)]); };
		for (const TileRect *rect : {&p.stamps.boss, &p.stamps.event, &p.stamps.entrance}) {
			const siege::Piece *piece = stripAt(*rect);
			REQUIRE(piece != nullptr);
			CHECK(piece->role != siege::Role::WestCap);
			CHECK(piece->role != siege::Role::EastCap);
			CHECK(piece->west >= 'A');
			CHECK(piece->west <= 'E');
			CHECK(piece->east >= 'A');
			CHECK(piece->east <= 'E');  // never a funnel strip
		}
		// The boss is not next to the entrance.
		CHECK(std::abs(p.stamps.boss.x - p.stamps.entrance.x) >= 2 * 16);
	}
}

TEST_CASE("the ascii dump shows the socket letters")
{
	const SiegePlan p = planSiegeMap(request(5, 12));
	REQUIRE(p.ok);
	const std::string ascii = siegePlanAscii(p);
	CHECK(ascii.rfind("-E ", 0) == 0);  // the barricade strip: closed west, socket E east
	CHECK(ascii.find("A-\n") != std::string::npos);  // the town strip: socket A west, closed east
	if (goldenMode()) std::printf("%s\n", ascii.c_str());
}

TEST_CASE("planOutdoorMap plans Siege through the tileset table and the grid comparison agrees with a 2 x 6 strip grid")
{
	MapRequest m;
	m.tileset = "siege";
	m.size = "Medium";
	m.seed = 42;
	m.options.originX = 6000;
	m.options.originY = 3900;
	m.options.maxTilesX = 240;
	m.options.maxTilesY = 48;
	const MapPlan plan = planOutdoorMap(m, builtinJungleCatalogue());
	INFO(plan.reason);
	REQUIRE(plan.ok);
	CHECK(plan.hash != 0);
	CHECK(plan.layout.cells.empty());
	CHECK(plan.stamps.stamps.size() == 13);
	CHECK(plan.stamps.cellsW == 26);
	CHECK(plan.stamps.cellsH == 6);
	CHECK(planCacheKey(m).find("siege") != std::string::npos);

	// A grid sample as the engine would hold it: a piece in the first cell of every strip, the rest covered, nothing void.
	GridSample grid;
	grid.piece.assign(static_cast<size_t>(plan.stamps.cellsW) * plan.stamps.cellsH, 0);
	grid.flags.assign(grid.piece.size(), 0x200);
	for (const Stamp &s : plan.stamps.stamps) {
		for (int dy = 0; dy < 6; ++dy)
			for (int dx = 0; dx < 2; ++dx) grid.flags[static_cast<size_t>(s.cy + dy) * plan.stamps.cellsW + s.cx + dx] = 0x200;
		grid.piece[static_cast<size_t>(s.cy) * plan.stamps.cellsW + s.cx] = static_cast<uint64_t>(s.lvlPrest);
	}
	GridCounts counts;
	CHECK(joined(compareGrid(plan.stamps, grid, &counts)).empty());
	CHECK(counts.pieces == 13);
	// A void cell inside a strip's 2 x 6 block, not only in its first cell, is caught.
	grid.flags[static_cast<size_t>(5) * plan.stamps.cellsW + plan.stamps.stamps[3].cx + 1] = 0x100;
	CHECK_FALSE(compareGrid(plan.stamps, grid).empty());

	// The same request for another size, and a bad size.
	m.size = "Huge";
	CHECK_FALSE(planOutdoorMap(m, builtinJungleCatalogue()).ok);
	m.size = "ExtraLarge";
	m.options.maxTilesX = 224;
	const MapPlan tight = planOutdoorMap(m, builtinJungleCatalogue());
	CHECK_FALSE(tight.ok);
	CHECK(tight.reason.find("Siege") != std::string::npos);
}

TEST_CASE("golden: a fixed seed keeps its line")
{
	const SiegePlan p = planSiegeMap(request(2026, 14));
	REQUIRE(p.ok);
	if (goldenMode()) {
		std::printf("GOLDEN line:");
		for (int id : p.strips) std::printf(" %d", id);
		std::printf("\nGOLDEN hash: %llu\n", static_cast<unsigned long long>(p.hash));
	}
	// Pinned: a change here moves every Siege map of a shipped seed. Run with CARTOGRAPHER_GOLDEN=1 to print the new values when that is meant.
	const std::vector<int> want = {879, 878, 877, 867, 866, 875, 874, 873, 872, 871, 870, 869, 868, 865};
	CHECK(p.strips == want);
	CHECK(p.hash == 1571578296766984929ull);
}
