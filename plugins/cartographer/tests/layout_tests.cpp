// Host tests for the Cartographer layout generator. No game, no SDK: the generator is pure C++.
#define _CRT_SECURE_NO_WARNINGS
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "layout.h"
#include "rng.h"

using namespace cartographer::layout;

namespace {

constexpr int kSeeds = 10000;

int bitsIn(uint8_t m)
{
	return (m & 1) + ((m >> 1) & 1) + ((m >> 2) & 1) + ((m >> 3) & 1);
}

// Rule checks written here, apart from checkLayout(), so the generator's own checker is not the only judge.
std::string independentCheck(const Layout &l, const Rules &r)
{
	auto bad = [](const std::string &s) { return s; };
	if (l.width != r.width || l.height != r.height) return bad("grid size");
	int used = 0;
	for (int y = 0; y < l.height; ++y)
		for (int x = 0; x < l.width; ++x) {
			const Cell &c = l.at(x, y);
			if (c.role == Role::Empty) {
				if (c.mask != 0) return bad("empty cell with exits");
				continue;
			}
			++used;
			const int dx[4] = {-1, 1, 0, 0}, dy[4] = {0, 0, 1, -1};
			const uint8_t bit[4] = {kWest, kEast, kSouth, kNorth}, opp[4] = {kEast, kWest, kNorth, kSouth};
			for (int d = 0; d < 4; ++d) {
				const int nx = x + dx[d], ny = y + dy[d];
				const bool has = (c.mask & bit[d]) != 0;
				const bool nUsed = l.inside(nx, ny) && l.at(nx, ny).role != Role::Empty;
				if (has && !nUsed) return bad("exit to nothing");
				if (has && !(l.at(nx, ny).mask & opp[d])) return bad("facing bits differ between neighbours");
				if (!has && nUsed) return bad("unlinked cells touch side to side");
			}
		}
	const int n = static_cast<int>(l.mainPath.size());
	if (n < r.pathMin || n > r.pathMax) return bad("path length");
	if (l.turns() < r.turnsMin || l.turns() > r.turnsMax) return bad("turns");
	if (l.ring != r.loop) return bad("ring flag");
	if (static_cast<int>(l.branches.size()) < r.branchesMin || static_cast<int>(l.branches.size()) > r.branchesMax) return bad("branch count");
	for (const Branch &b : l.branches) {
		const int len = static_cast<int>(b.cells.size());
		if (len < r.branchLenMin || len > r.branchLenMax) return bad("branch length");
	}
	if (static_cast<int>(l.loops.size()) < r.loopsMin || static_cast<int>(l.loops.size()) > r.loopsMax) return bad("loop count");
	if (l.eventIdx < 1 || l.eventIdx >= n) return bad("event room");
	if (r.bossRoom) {
		if (l.bossIdx < 1 || l.bossIdx >= n || l.bossIdx == l.eventIdx) return bad("boss room");
		if (l.ring) {
			if (l.bossIdx != n / 2) return bad("ring boss not opposite the entrance");
		} else if (l.bossIdx != n - 1) {
			return bad("boss not at the end");
		}
	}
	int claimed = n;
	for (const Branch &b : l.branches) claimed += static_cast<int>(b.cells.size());
	for (const Branch &b : l.loops) claimed += static_cast<int>(b.cells.size());
	if (claimed != used) return bad("cell count");
	return std::string();
}

} // namespace

TEST_CASE("rng: splitmix64 and PCG32 are fixed streams")
{
	CHECK(splitmix64(0) == 0xE220A8397B1DCDAFull);
	CHECK(splitmix64(1) == 0x910A2DEC89025CC1ull);
	Pcg32 a(12345), b(12345), c(12346);
	bool differs = false;
	for (int i = 0; i < 64; ++i) {
		const uint32_t x = a.next();
		CHECK(x == b.next());
		if (x != c.next()) differs = true;
	}
	CHECK(differs);
	Pcg32 r(7);
	for (int i = 0; i < 1000; ++i) {
		CHECK(r.below(7) < 7);
		const int v = r.range(-3, 3);
		CHECK((v >= -3 && v <= 3));
	}
	CHECK(r.below(0) == 0);
	CHECK(r.below(1) == 0);
	CHECK(r.range(5, 2) == 5);
	CHECK_FALSE(r.percent(0));
	CHECK(r.percent(100));
}

TEST_CASE("names")
{
	CHECK(parseShapeName("corridor") == Shape::Corridor);
	CHECK(parseShapeName("SPRAWL") == Shape::Sprawl);
	CHECK_FALSE(parseShapeName("Maze").has_value());
	CHECK(parseSizeName("large") == Size::Large);
	CHECK_FALSE(parseSizeName("huge").has_value());
	for (int s = 0; s < kShapeCount; ++s) CHECK(parseShapeName(shapeName(static_cast<Shape>(s))) == static_cast<Shape>(s));
}

TEST_CASE("every preset has usable rules")
{
	for (int s = 0; s < kShapeCount; ++s)
		for (int z = 0; z < kSizeCount; ++z) {
			const Rules r = presetRules(static_cast<Shape>(s), static_cast<Size>(z));
			INFO(shapeName(static_cast<Shape>(s)) << " " << sizeName(static_cast<Size>(z)));
			CHECK(validateRules(r) == "");
		}
}

TEST_CASE("10k seeds per shape preset meet their rules, at every size")
{
	for (int s = 0; s < kShapeCount; ++s)
		for (int z = 0; z < kSizeCount; ++z) {
			const Shape shape = static_cast<Shape>(s);
			const Size size = static_cast<Size>(z);
			const Rules r = presetRules(shape, size);
			int fallbacks = 0, firstTry = 0, failures = 0;
			uint64_t worstAttempt = 0;
			for (int i = 0; i < kSeeds; ++i) {
				const uint64_t seed = splitmix64(static_cast<uint64_t>(i) + 0x1000);
				const Layout l = generate(r, seed);
				const std::vector<std::string> problems = checkLayout(l, r);
				const std::string mine = independentCheck(l, r);
				if (!l.rulesMet || !problems.empty() || !mine.empty()) {
					if (failures++ < 3)
						MESSAGE(shapeName(shape) << " " << sizeName(size) << " seed " << seed << ": " << (problems.empty() ? mine : problems[0]) << "\n" << toAscii(l));
				}
				if (l.fallback) ++fallbacks;
				if (l.attempt == 0) ++firstTry;
				if (static_cast<uint64_t>(l.attempt) > worstAttempt) worstAttempt = static_cast<uint64_t>(l.attempt);
			}
			if (std::getenv("CARTOGRAPHER_STATS"))
				std::printf("%-9s %-6s seeds %d  rules met %d  fallback %d  first try %d  worst attempt %d\n", shapeName(shape), sizeName(size), kSeeds,
					kSeeds - failures, fallbacks, firstTry, static_cast<int>(worstAttempt));
			INFO(shapeName(shape) << " " << sizeName(size));
			CHECK(failures == 0);
			CHECK(fallbacks == 0); // the presets are loose enough that 32 tries always find a layout
		}
}

TEST_CASE("same seed gives the same layout; different seeds differ")
{
	for (int s = 0; s < kShapeCount; ++s) {
		const Shape shape = static_cast<Shape>(s);
		int distinct = 0;
		uint64_t previous = 0;
		for (uint64_t seed = 1; seed <= 200; ++seed) {
			const Layout a = generate(shape, Size::Medium, seed), b = generate(shape, Size::Medium, seed);
			CHECK(layoutHash(a) == layoutHash(b));
			CHECK(toAscii(a) == toAscii(b));
			if (layoutHash(a) != previous) ++distinct;
			previous = layoutHash(a);
		}
		CHECK(distinct > 150);
	}
}

TEST_CASE("shape rules show in the layouts")
{
	// Corridor is straighter than Winding; Branching has side rooms; Loop and Sprawl are rings; Sprawl has loops.
	auto avgTurns = [](Shape shape) {
		int total = 0;
		for (uint64_t seed = 1; seed <= 300; ++seed) total += generate(shape, Size::Medium, seed).turns();
		return total / 300;
	};
	CHECK(avgTurns(Shape::Corridor) < avgTurns(Shape::Winding));
	int branching = 0, sprawlLoops = 0;
	for (uint64_t seed = 1; seed <= 300; ++seed) {
		branching += static_cast<int>(generate(Shape::Branching, Size::Medium, seed).branches.size());
		const Layout sp = generate(Shape::Sprawl, Size::Large, seed);
		CHECK(sp.ring);
		sprawlLoops += static_cast<int>(sp.loops.size());
		const Layout lp = generate(Shape::Loop, Size::Medium, seed);
		CHECK(lp.ring);
		CHECK(lp.bossIdx == static_cast<int>(lp.mainPath.size()) / 2);
		CHECK(lp.eventIdx == static_cast<int>(lp.mainPath.size()) / 4);
	}
	CHECK(branching >= 300 * 3);
	CHECK(sprawlLoops > 0);
}

TEST_CASE("the fallback meets the rules of every preset on its own")
{
	for (int s = 0; s < kShapeCount; ++s)
		for (int z = 0; z < kSizeCount; ++z) {
			const Rules r = presetRules(static_cast<Shape>(s), static_cast<Size>(z));
			for (uint64_t seed = 1; seed <= 20; ++seed) {
				const Layout l = generateFallback(r, seed);
				INFO(shapeName(static_cast<Shape>(s)) << " " << sizeName(static_cast<Size>(z)) << "\n" << toAscii(l));
				CHECK(l.fallback);
				CHECK(l.rulesMet);
				CHECK(checkLayout(l, r).empty());
			}
		}
}

TEST_CASE("checkLayout catches broken layouts")
{
	const Rules r = presetRules(Shape::Branching, Size::Medium);
	const Layout good = generate(r, 42);
	REQUIRE(checkLayout(good, r).empty());

	Layout noFacing = good; // clear one facing bit
	for (Cell &c : noFacing.cells)
		if (c.mask & kEast) { c.mask = static_cast<uint8_t>(c.mask & ~kEast); break; }
	CHECK_FALSE(checkLayout(noFacing, r).empty());

	Layout moved = good; // lose the boss room
	moved.bossIdx = -1;
	CHECK_FALSE(checkLayout(moved, r).empty());

	Rules stricter = r; // the same layout against tighter rules
	stricter.pathMax = stricter.pathMin - 1 + 0;
	CHECK_FALSE(checkLayout(good, stricter).empty());

	// Two cells that touch without a link: clear the facing bits of one linked pair, leaving both cells in place.
	Layout touching = good;
	bool cut = false;
	for (int y = 0; y < touching.height && !cut; ++y)
		for (int x = 0; x < touching.width && !cut; ++x) {
			Cell &a = touching.at(x, y);
			if (a.role == Role::Empty || !(a.mask & kEast) || !(touching.at(x + 1, y).mask & kWest)) continue;
			if (bitsIn(a.mask) < 2 || bitsIn(touching.at(x + 1, y).mask) < 2) continue;
			a.mask = static_cast<uint8_t>(a.mask & ~kEast);
			touching.at(x + 1, y).mask = static_cast<uint8_t>(touching.at(x + 1, y).mask & ~kWest);
			cut = true;
		}
	REQUIRE(cut);
	bool sawTouch = false;
	for (const std::string &m : checkLayout(touching, r))
		if (m.find("touch without a link") != std::string::npos) sawTouch = true;
	CHECK(sawTouch);

	// Broken connectivity: move the layout's last used cell far away is not possible, so cut a branch off at its root.
	REQUIRE_FALSE(good.branches.empty());
	Layout split = good;
	{
		const Branch &b = good.branches[0];
		const Point c = b.cells[0];
		const Point root = b.root;
		const int dx = c.x - root.x, dy = c.y - root.y;
		const uint8_t toChild = dx == 1 ? kEast : dx == -1 ? kWest : dy == 1 ? kSouth : kNorth;
		const uint8_t toRoot = dx == 1 ? kWest : dx == -1 ? kEast : dy == 1 ? kNorth : kSouth;
		split.at(root.x, root.y).mask = static_cast<uint8_t>(split.at(root.x, root.y).mask & ~toChild);
		split.at(c.x, c.y).mask = static_cast<uint8_t>(split.at(c.x, c.y).mask & ~toRoot);
	}
	bool sawSplit = false;
	for (const std::string &m : checkLayout(split, r))
		if (m.find("not connected") != std::string::npos) sawSplit = true;
	CHECK(sawSplit);

	// A side room that is not a dead end: take an extra loop and file it as a plain side room. Its last cell still
	// links back into the layout, so the checker must complain.
	Rules loopy = presetRules(Shape::Sprawl, Size::Large);
	bool sawLoop = false;
	for (uint64_t seed = 1; seed <= 200 && !sawLoop; ++seed) {
		Layout l = generate(loopy, seed);
		REQUIRE(checkLayout(l, loopy).empty());
		if (l.loops.empty()) continue;
		sawLoop = true;
		l.branches.push_back(l.loops[0]);
		l.branches.back().closes = false;
		l.loops.erase(l.loops.begin());
		CHECK_FALSE(checkLayout(l, loopy).empty());
	}
	REQUIRE(sawLoop);
}

TEST_CASE("validateRules rejects impossible rules")
{
	Rules r = presetRules(Shape::Winding, Size::Small);
	r.pathMax = 100;
	CHECK_FALSE(validateRules(r).empty());
	r = presetRules(Shape::Loop, Size::Small);
	r.pathMin = r.pathMax = 9;
	CHECK_FALSE(validateRules(r).empty());
	r = presetRules(Shape::Winding, Size::Small);
	r.turnsMin = 5;
	r.turnsMax = 2;
	CHECK_FALSE(validateRules(r).empty());
	// A too-tight rule set still returns a layout, with rulesMet false.
	r = presetRules(Shape::Winding, Size::Small);
	r.pathMin = r.pathMax = 36;
	r.turnsMin = 30;
	r.turnsMax = 30;
	const Layout l = generate(r, 1);
	CHECK(l.fallback);
	CHECK_FALSE(l.rulesMet);
}

TEST_CASE("a huge count in shapes.txt is an error, not a hang")
{
	auto errorsOf = [](const std::string &text) { return parseShapesTxt(text, "shapes.txt").errors; };
	const std::vector<std::string> branches = errorsOf("Name\tMinBranches\tMaxBranches\tMinLoops\tMaxLoops\nX\t999999\t999999\t5\t9\n");
	REQUIRE(branches.size() == 1);
	CHECK(branches[0].find("MaxBranches") != std::string::npos);
	CHECK(errorsOf("Name\tMinLoops\tMaxLoops\nX\t1\t999999\n").size() == 1);
	// Leading zeros are not extra digits: "0000008" is 8, not "too large".
	CHECK(errorsOf("Name\tMinPath\nX\t0000010\n").empty());
	CHECK(errorsOf("Name\tMinPath\nX\t00000000000000000008\n").empty());
	CHECK(errorsOf("Name\tLoop\tWidth\tHeight\tMinPath\tMaxPath\nX\t1\t32\t32\t500\t600\n").size() == 1);
	CHECK(errorsOf("Name\tLoop\tWidth\tHeight\tMinPath\tMaxPath\nX\t1\t32\t32\t500\t512\n").size() == 1);
	const std::vector<std::string> big = errorsOf("Name\tMinPath\nX\t99999999\n");
	REQUIRE(big.size() == 1);
	CHECK(big[0].find("too large") != std::string::npos);
	CHECK(big[0].find("whole number") == std::string::npos);
	CHECK(errorsOf("Name\tMinPath\nX\t-1000000\n").size() == 1);
	const std::vector<std::string> bad = errorsOf("Name\tMinPath\nX\tfive\n");
	REQUIRE(bad.size() == 1);
	CHECK(bad[0].find("whole number") != std::string::npos);
}

TEST_CASE("shapes.txt: rows at the caps are answered")
{
	const std::string header =
		"Name\tBase\tWidth\tHeight\tMinPath\tMaxPath\tMinBranches\tMaxBranches\tMinBranchLen\tMaxBranchLen\tMinLoops\tMaxLoops\tMinTurns\tMaxTurns\n";
	// Fields after Base: Width Height MinPath MaxPath MinBranches MaxBranches MinBranchLen MaxBranchLen MinLoops MaxLoops MinTurns MaxTurns
	const std::vector<std::string> rows = {
		"X\tBranching\t32\t32\t\t\t512\t512\t512\t512\t\t\t\t\n",
		"X\tBranching\t32\t32\t\t\t0\t512\t1\t512\t\t\t\t\n",
		"X\tBranching\t32\t32\t\t\t1\t512\t512\t512\t\t\t\t\n",
		"X\tSprawl\t32\t32\t200\t200\t0\t512\t1\t512\t0\t512\t\t\n",
		"X\tSprawl\t32\t32\t200\t200\t512\t512\t1\t1\t\t\t\t\n",
		"X\tSprawl\t32\t32\t200\t200\t100\t512\t100\t512\t100\t512\t\t\n",
		"X\tSprawl\t32\t32\t\t\t\t\t\t\t512\t512\t\t\n",
		"X\tLoop\t32\t32\t200\t200\t\t\t\t\t\t\t\t\n",
		"X\tLoop\t32\t32\t200\t200\t100\t512\t1\t512\t0\t512\t\t\n",
		"X\tWinding\t32\t32\t900\t1024\t\t\t\t\t\t\t0\t999\n",
		"X\tWinding\t32\t32\t4\t1024\t0\t512\t1\t512\t\t\t\t\n",
		"X\tWinding\t32\t32\t1024\t1024\t512\t512\t512\t512\t512\t512\t999\t999\n",
		"X\tWinding\t2\t2\t4\t4\t1\t1\t1\t1\t1\t1\t\t\n",
	};
	for (const std::string &row : rows) {
		const ParseResult p = parseShapesTxt(header + row);
		INFO(row);
		// Accepted or rejected does not matter here, only that it answers (the work budget bounds the time, not a clock).
		CHECK((p.ok() || !p.errors.empty()));
	}
}

TEST_CASE("validateRules refuses side rooms that can't fit")
{
	Rules r = presetRules(Shape::Winding, Size::Small); // 6 x 6 = 36 cells
	r.pathMin = r.pathMax = 10;
	r.branchesMin = r.branchesMax = 6;
	r.branchLenMin = r.branchLenMax = 5; // 30 cells asked for, 26 free
	CHECK_FALSE(validateRules(r).empty());
	r.branchLenMin = r.branchLenMax = 2; // 12 cells
	CHECK(validateRules(r).empty());
}

TEST_CASE("a rule set that can't be built gives up on a budget and falls back")
{
	// Far too many side rooms to place: generate() must still answer at once, with the fallback.
	Rules r = presetRules(Shape::Branching, Size::Large);
	r.width = r.height = 32;
	r.branchesMin = r.branchesMax = 300;
	r.branchLenMin = r.branchLenMax = 3;
	const Layout l = generate(r, 1);
	CHECK(l.fallback);
	CHECK(l.attempt == kRetries);
	// Pinned: the layout depends on the work charges, so a change to them shows up here.
	CHECK(layoutHash(l) == 10216947217312690002ULL);
}

TEST_CASE("a Loop row with extra loops stays buildable under the work budget")
{
	Rules r = presetRules(Shape::Loop, Size::Small);
	r.width = 11;
	r.height = 9;
	r.pathMin = 38;
	r.pathMax = 40;
	r.turnsMin = 0;
	r.turnsMax = 11;
	r.branchesMin = 0;
	r.branchesMax = 5;
	r.branchLenMin = r.branchLenMax = 1;
	r.loopsMin = 2;
	r.loopsMax = 4;
	r.straightPct = 81;
	r.bossRoom = true;
	REQUIRE(validateRules(r).empty());
	for (uint64_t seed = 1; seed <= 200; ++seed) {
		const Layout l = generate(r, seed);
		INFO("seed " << seed);
		CHECK(l.rulesMet);
		CHECK_FALSE(l.fallback);
	}
}

TEST_CASE("shapes.txt: a valid table")
{
	const std::string text =
		"Name\tBase\tMinPath\tMaxPath\tStraight\tMinTurns\tMaxTurns\tMinBranches\tMaxBranches\tLoop\tBossRoom\r\n"
		"Snake\tWinding\t12\t16\t30\t8\t99\t0\t1\t\t\r\n"
		"\r\n"
		"Fat Snake\tSnake\t\t\t\t\t\t2\t3\t\t\n"
		"Arena\tLoop\t\t\t\t\t\t\t\t1\t0\n";
	const ParseResult p = parseShapesTxt(text);
	REQUIRE(p.errors.empty());
	REQUIRE(p.table.shapes.size() == 3);

	Rules r;
	std::string e;
	REQUIRE(resolveRules(p.table, "snake", Size::Medium, r, e));
	CHECK(r.pathMin == 12);
	CHECK(r.pathMax == 16);
	CHECK(r.width == 9); // the size's grid is kept
	REQUIRE(resolveRules(p.table, "Fat Snake", Size::Medium, r, e));
	CHECK(r.pathMin == 12); // inherited from Snake
	CHECK(r.branchesMin == 2);
	REQUIRE(resolveRules(p.table, "Arena", Size::Large, r, e));
	CHECK(r.loop);
	CHECK_FALSE(r.bossRoom);
	REQUIRE(resolveRules(p.table, "Corridor", Size::Small, r, e));
	CHECK_FALSE(resolveRules(p.table, "Nope", Size::Small, r, e));
	CHECK(e.find("Nope") != std::string::npos);

	// The custom shapes build.
	for (const char *name : {"Snake", "Fat Snake", "Arena"}) {
		REQUIRE(resolveRules(p.table, name, Size::Medium, r, e));
		for (uint64_t seed = 1; seed <= 300; ++seed) {
			const Layout l = generate(r, seed);
			CHECK(l.rulesMet);
			CHECK(checkLayout(l, r).empty());
		}
	}
}

TEST_CASE("shapes.txt: plain-English errors")
{
	auto errorsOf = [](const std::string &text) { return parseShapesTxt(text, "shapes.txt").errors; };
	std::vector<std::string> e = errorsOf("Name\tMinPath\nA\tlots\n");
	REQUIRE(e.size() == 1);
	CHECK(e[0].find("line 2") != std::string::npos);
	CHECK(e[0].find("MinPath") != std::string::npos);
	CHECK(errorsOf("Name\tBogus\nA\t1\n").size() == 1);
	CHECK(errorsOf("Base\nWinding\n").size() == 1);                       // no Name column
	CHECK(errorsOf("Name\nCorridor\n").size() == 1);                      // a built-in name
	CHECK(errorsOf("Name\nA\nA\n").size() == 1);                          // twice
	CHECK(errorsOf("Name\tBase\nA\tZed\n").size() == 1);                  // unknown base
	CHECK(errorsOf("Name\tMinPath\tMaxPath\nA\t30\t20\n").size() == 1);   // min > max
	CHECK(errorsOf("Name\tMinPath\tMaxPath\nA\t30\t60\n").size() == 1);   // does not fit the Small grid
	CHECK(errorsOf("Name\tLoop\nA\tmaybe\n").size() == 1);
	CHECK(errorsOf("Name\tBase\nA\tB\nB\tA\n").size() >= 1);              // base must be an earlier row
	CHECK(errorsOf("").empty());
	CHECK(errorsOf("Name\n").empty());
	// Too tight to build: every cell of the grid in one path with many turns and many branches.
	CHECK_FALSE(errorsOf("Name\tMinPath\tMaxPath\tMinBranches\tMaxBranches\tMinBranchLen\tMaxBranchLen\nTight\t36\t36\t5\t5\t3\t3\n").empty());
}

TEST_CASE("golden layouts")
{
	struct Golden {
		Shape shape;
		Size size;
		uint64_t seed;
		uint64_t hash;
	};
	// seed 1 and seed 0xC0FFEE of every shape at Medium, plus Small/Large samples.
	static const Golden kGolden[] = {
		{Shape::Corridor, Size::Small, 0x1ull, 0x335D0D0166D02B77ull},
		{Shape::Corridor, Size::Medium, 0x1ull, 0xD8A57E5D108A5708ull},
		{Shape::Corridor, Size::Large, 0x1ull, 0x621E6DDF2527486Aull},
		{Shape::Corridor, Size::Small, 0xC0FFEEull, 0xC3FF8F3D18B2D504ull},
		{Shape::Corridor, Size::Medium, 0xC0FFEEull, 0x3EA9D9DA8D42A7BBull},
		{Shape::Corridor, Size::Large, 0xC0FFEEull, 0xDD05A1CACF0B3E44ull},
		{Shape::Winding, Size::Small, 0x1ull, 0xEFEAB22D0829BEA3ull},
		{Shape::Winding, Size::Medium, 0x1ull, 0x2A2D0DD09B5A5BBBull},
		{Shape::Winding, Size::Large, 0x1ull, 0x83AAFEB9C0C00A9Bull},
		{Shape::Winding, Size::Small, 0xC0FFEEull, 0xA422A6CC1643D2A7ull},
		{Shape::Winding, Size::Medium, 0xC0FFEEull, 0x7A1555E4F5286942ull},
		{Shape::Winding, Size::Large, 0xC0FFEEull, 0x97778CB82BECCB27ull},
		{Shape::Branching, Size::Small, 0x1ull, 0x853D7C7C7A5B22DFull},
		{Shape::Branching, Size::Medium, 0x1ull, 0x0EEC1DA9827A6C4Eull},
		{Shape::Branching, Size::Large, 0x1ull, 0x51DA4949BB0FF089ull},
		{Shape::Branching, Size::Small, 0xC0FFEEull, 0x31A58DFB0814B385ull},
		{Shape::Branching, Size::Medium, 0xC0FFEEull, 0xD98CF3DF4F59925Cull},
		{Shape::Branching, Size::Large, 0xC0FFEEull, 0xBD1E28941AB2699Eull},
		{Shape::Loop, Size::Small, 0x1ull, 0x90C630B533CD7DAEull},
		{Shape::Loop, Size::Medium, 0x1ull, 0xF7D9B1461F8B2E18ull},
		{Shape::Loop, Size::Large, 0x1ull, 0xC77A00BB8EF0CD6Dull},
		{Shape::Loop, Size::Small, 0xC0FFEEull, 0x06F706B5AAFF4819ull},
		{Shape::Loop, Size::Medium, 0xC0FFEEull, 0xA66773FFE6FF3FEAull},
		{Shape::Loop, Size::Large, 0xC0FFEEull, 0x9387954586F004BFull},
		{Shape::Sprawl, Size::Small, 0x1ull, 0xECEE52C9FCCA9765ull},
		{Shape::Sprawl, Size::Medium, 0x1ull, 0x272EB22710073C4Aull},
		{Shape::Sprawl, Size::Large, 0x1ull, 0xAAFE4E2EAE9224A1ull},
		{Shape::Sprawl, Size::Small, 0xC0FFEEull, 0xA6E1843017E6C8EBull},
		{Shape::Sprawl, Size::Medium, 0xC0FFEEull, 0x6D0B61977647383Aull},
		{Shape::Sprawl, Size::Large, 0xC0FFEEull, 0xF64C39D28B426C3Aull},
	};
	if (std::getenv("CARTOGRAPHER_PRINT_GOLDEN")) {
		for (int s = 0; s < kShapeCount; ++s)
			for (uint64_t seed : {1ull, 0xC0FFEEull})
				for (int z = 0; z < kSizeCount; ++z) {
					const Layout l = generate(static_cast<Shape>(s), static_cast<Size>(z), seed);
					std::printf("\t{Shape::%s, Size::%s, 0x%llXull, 0x%016llXull},\n", shapeName(static_cast<Shape>(s)), sizeName(static_cast<Size>(z)),
						static_cast<unsigned long long>(seed), static_cast<unsigned long long>(layoutHash(l)));
				}
		return;
	}
	for (const Golden &g : kGolden) {
		INFO(shapeName(g.shape) << " " << sizeName(g.size) << " seed " << g.seed);
		CHECK(layoutHash(generate(g.shape, g.size, g.seed)) == g.hash);
	}
}

TEST_CASE("golden ASCII dumps (Medium, seed 1)")
{
	CHECK(toAscii(generate(Shape::Corridor, Size::Medium, 1)) == R"(. . #-@ . . . . .
    |
. . # . . . . . .
    |
. . # . . . . . .
    |
. . # . . . . . .
    |
. . # . . . . . B
    |           |
. . #-o . . . . #
    |           |
. . # . . . . . #
    |           |
. . !-#-#-#-#-#-#

. . . . . . . . .
)");
	CHECK(toAscii(generate(Shape::Winding, Size::Medium, 1)) == R"(@-# . . . . . . .
  |
. #-#-# . . . . .
      |
. . . # . . . . .
      |
. #-#-# . . . . .
  |
!-# . . . . . . .
|
# . . . B . . . .
|       |
# . . . #-o . . .
|       |
# . . #-# . . . .
|     |
#-#-#-# . . . . .
)");
	CHECK(toAscii(generate(Shape::Loop, Size::Medium, 1)) == R"(. . . o-#-#-# . .
        |   |
. . . . # . #-# .
        |     |
. . . . # . . @ .
        |     |
. . . . B . . # .
        |     |
. . . . #-# . #-o
          |   |
. . . . . #-!-# .

. . . . . . . . .

. . . . . . . . .

. . . . . . . . .
)");
}
