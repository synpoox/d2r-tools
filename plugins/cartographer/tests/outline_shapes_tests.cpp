// Host tests for the Cartographer outline shape generators (issue #148 desk spike). No game, no SDK: pure C++.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <set>
#include <string>
#include <vector>

#include "outline_shapes.h"

using namespace cartographer::outdoor::outline;

namespace {

long long loopAreaSum(const std::vector<Loop> &loops)
{
	long long sum = 0;
	for (const Loop &l : loops) sum += l.area;
	return sum;
}

int holeLoops(const std::vector<Loop> &loops)
{
	int n = 0;
	for (const Loop &l : loops) n += l.hole();
	return n;
}

// Every shape the generators make has to give loops the engine's border walk accepts, whose areas add up to the open cell count.
void expectEngineReady(const Mask &m)
{
	const std::vector<Loop> loops = traceLoops(m);
	CHECK(loopAreaSum(loops) == m.count());
	for (const Loop &l : loops) CHECK(checkLoopForEngine(l.v).empty());
	// The same loops as the engine's cell-index polygon: axis-aligned, no zero-length edge, one per corner loop.
	const std::vector<Loop> cells = traceEngineOutlines(m);
	CHECK(cells.size() == loops.size());
	for (const Loop &l : cells) CHECK(checkLoopForEngine(l.v).empty());
	CHECK(thinCells(m) == 0);
	bool overrun = true;
	engineVoidMarks(m, &overrun);
	CHECK(!overrun);
	CHECK(holeLoops(static_cast<const std::vector<Loop> &>(loops)) == enclosedVoidRegions(m));
}

} // namespace

TEST_CASE("rectangle: one loop of four corners, no void")
{
	const Mask m = rectangleShape(4, 3);
	CHECK(m.count() == 12);
	CHECK(openRegions(m) == 1);
	CHECK(enclosedVoidRegions(m) == 0);
	CHECK(unmarkedVoidCells(m) == 0);
	const std::vector<Loop> loops = traceLoops(m);
	REQUIRE(loops.size() == 1);
	CHECK(loops[0].v.size() == 4);
	CHECK(loops[0].area == 12);
	CHECK(loops[0].v[0] == Vertex{0, 0});
	// The engine's outline of a rectangle runs from cell 0,0 to cell w-1,h-1 (0x3f9b80 decrements the size).
	const std::vector<Loop> cells = traceEngineOutlines(m);
	REQUIRE(cells.size() == 1);
	CHECK(cells[0].v == std::vector<Vertex>{{0, 0}, {3, 0}, {3, 2}, {0, 2}});
	expectEngineReady(m);
}

TEST_CASE("L shape: six corners, the notch is reached by the border rays")
{
	const Mask m = lShape(6, 5, 2, 2);
	CHECK(m.count() == 2 * 5 + 4 * 2);
	CHECK(openRegions(m) == 1);
	CHECK(enclosedVoidRegions(m) == 0);
	CHECK(unmarkedVoidCells(m) == 0);
	const std::vector<Loop> loops = traceLoops(m);
	REQUIRE(loops.size() == 1);
	CHECK(loops[0].v.size() == 6);
	// Void is cols 2..5, rows 0..2; the reflex corner point is (2,3), whose engine vertex is the open cell diagonal to the void, (1,3).
	const std::vector<Loop> cells = traceEngineOutlines(m);
	REQUIRE(cells.size() == 1);
	CHECK(cells[0].v == std::vector<Vertex>{{0, 0}, {1, 0}, {1, 3}, {5, 3}, {5, 4}, {0, 4}});
	expectEngineReady(m);
}

TEST_CASE("cross: twelve corners, the four corner notches are all marked")
{
	const Mask m = crossShape(7, 7, 3);
	CHECK(m.count() == 33);
	CHECK(openRegions(m) == 1);
	CHECK(unmarkedVoidCells(m) == 0);
	const std::vector<Loop> loops = traceLoops(m);
	REQUIRE(loops.size() == 1);
	CHECK(loops[0].v.size() == 12);
	expectEngineReady(m);
}

TEST_CASE("U shape: eight corners; the notch is open to the outside but no border scan starts in it")
{
	const Mask m = uShape(7, 4, 3, 2);
	CHECK(m.count() == 28 - 6);
	CHECK(openRegions(m) == 1);
	CHECK(enclosedVoidRegions(m) == 0);
	CHECK(unmarkedVoidCells(m) == 6); // all four corners are open ground, so every scan ends at once: the plan must mark the notch itself
	const std::vector<Loop> loops = traceLoops(m);
	REQUIRE(loops.size() == 1);
	CHECK(loops[0].v.size() == 8);
	expectEngineReady(m);
}

TEST_CASE("ring: the hole is an enclosed void that the border rays do not mark")
{
	const Mask m = ringShape(6, 6, 2);
	CHECK(m.count() == 32);
	CHECK(openRegions(m) == 1);
	CHECK(enclosedVoidRegions(m) == 1);
	CHECK(unmarkedVoidCells(m) == 4);
	const std::vector<Loop> loops = traceLoops(m);
	REQUIRE(loops.size() == 2);
	CHECK(loops[0].area == 36);
	CHECK(loops[1].area == -4);
	CHECK(loops[1].hole());
	CHECK(loops[1].v.size() == 4);
	expectEngineReady(m);
	// The hole must show as 'x' in the picture: it is the part a plan has to mark itself.
	CHECK(maskAscii(m).find('x') != std::string::npos);
}

TEST_CASE("generators refuse sizes they cannot draw")
{
	CHECK(rectangleShape(0, 3).w == 0);
	CHECK(lShape(5, 4, 5, 1).w == 0);
	CHECK(lShape(1, 4, 1, 1).w == 0);
	CHECK(lShape(5, 4, 2, 0).w == 0);
	CHECK(crossShape(5, 5, 6).w == 0);
	CHECK(ringShape(4, 6, 2).w == 0);
	CHECK(ringShape(5, 5, 0).w == 0);
	CHECK(uShape(7, 4, 3, 4).w == 0);
	CHECK(uShape(7, 4, 7, 1).w == 0);
	CHECK(blobShape(0, 5, 50, 1, false).w == 0);
}

TEST_CASE("two cells touching only at a corner are two loops, never one crossing loop")
{
	Mask m = makeMask(2, 2, false);
	m.set(0, 0, true);
	m.set(1, 1, true);
	CHECK(openRegions(m) == 2);
	const std::vector<Loop> loops = traceLoops(m);
	REQUIRE(loops.size() == 2);
	CHECK(loops[0].area == 1);
	CHECK(loops[1].area == 1);
	CHECK(loopAreaSum(loops) == m.count());
	CHECK(thinCells(m) == 2); // one-cell features are not a level the engine's border walk can outline
}

TEST_CASE("blob: one connected region without holes, deterministic, engine-ready, over many seeds")
{
	std::set<std::string> distinct;
	for (uint64_t seed = 1; seed <= 300; ++seed) {
		const Mask m = blobShape(20, 16, 55, seed, false);
		REQUIRE(m.w >= 2);
		CHECK(m.w <= 20);
		CHECK(m.h <= 16);
		CHECK(m.count() >= 4 * (10 * 8 * 55 / 100));
		CHECK(openRegions(m) == 1);
		CHECK(enclosedVoidRegions(m) == 0);
		CHECK(blobShape(20, 16, 55, seed, false).open == m.open);
		expectEngineReady(m);
		distinct.insert(maskAscii(m));
	}
	CHECK(distinct.size() > 250);
}

TEST_CASE("blob with holes allowed is still one region, and its holes are traced as hole loops")
{
	bool sawHole = false;
	for (uint64_t seed = 1; seed <= 300; ++seed) {
		const Mask m = blobShape(20, 16, 80, seed, true);
		CHECK(openRegions(m) == 1);
		expectEngineReady(m);
		sawHole = sawHole || enclosedVoidRegions(m) > 0;
	}
	CHECK(sawHole);
}

TEST_CASE("blob: the engine's void pass often leaves void behind, so the plan marks every void cell itself")
{
	int withUnmarked = 0;
	for (uint64_t seed = 1; seed <= 100; ++seed) withUnmarked += unmarkedVoidCells(blobShape(20, 16, 55, seed, false)) > 0;
	CHECK(withUnmarked > 50);
}

TEST_CASE("a one-cell neck is reported as thin")
{
	CHECK(thinCells(lShape(5, 4, 1, 1)) > 0);
	CHECK(thinCells(ringShape(6, 6, 1)) > 0);  // a one-thick wall is thin everywhere
	CHECK(thinCells(ringShape(8, 8, 2)) == 0);
}

TEST_CASE("blob extremes")
{
	CHECK(blobShape(5, 5, 100, 9, false).count() == 16); // 2 x 2 half-size cells, doubled
	CHECK(blobShape(5, 5, 1, 9, false).count() == 4);
	CHECK(blobShape(2, 2, 50, 9, false).count() == 4);
	CHECK(blobShape(1, 5, 50, 9, false).w == 0);
}

TEST_CASE("checkLoopForEngine: only axis-aligned, non-zero edges")
{
	CHECK(checkLoopForEngine({{0, 0}, {4, 0}, {4, 3}, {0, 3}}).empty());
	CHECK(checkLoopForEngine({{0, 0}, {4, 0}, {0, 3}}).size() == 1);                // fewer than 4 corners
	CHECK(!checkLoopForEngine({{0, 0}, {4, 0}, {4, 3}, {2, 5}, {0, 3}}).empty());   // two diagonal edges
	CHECK(!checkLoopForEngine({{0, 0}, {0, 0}, {4, 0}, {4, 3}, {0, 3}}).empty());   // a zero-length edge
}
