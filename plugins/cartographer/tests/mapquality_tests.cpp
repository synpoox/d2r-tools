// Host tests for the map item quality decision. No game, no SDK: the code is pure C++.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "mapquality.h"

using namespace cartographer::mapquality;

TEST_CASE("a map that rolled magic becomes normal")
{
	CHECK(mapQuality(Magic, true) == Normal);
}

TEST_CASE("a map keeps every other quality it rolled")
{
	for (int quality = Low; quality <= Crafted; ++quality) {
		if (quality != Magic) {
			CHECK(mapQuality(quality, true) == quality);
		}
	}
	CHECK(mapQuality(Rare, true) == Rare);
	CHECK(mapQuality(Unique, true) == Unique);
	CHECK(mapQuality(Normal, true) == Normal);
}

TEST_CASE("an item that is not a map keeps its quality, magic included")
{
	for (int quality = 0; quality <= 9; ++quality) {
		CHECK(mapQuality(quality, false) == quality);
	}
}

TEST_CASE("a map takes the normal fallback instead of the magic one")
{
	CHECK(useNormalFallback(true));
	CHECK_FALSE(useNormalFallback(false));
}

TEST_CASE("a value outside the game's qualities passes through")
{
	CHECK(mapQuality(0, true) == 0);
	CHECK(mapQuality(-1, true) == -1);
	CHECK(mapQuality(99, true) == 99);
}

TEST_CASE("the roll counts name every quality and add up")
{
	const std::uint32_t counts[10] = {0, 3, 10, 4, 0, 1, 2, 5, 0, 0};
	CHECK(rollCounts(counts, 2) == "25 map rolls: 3 low, 10 normal (2 of them rolled magic), 4 superior, 0 magic, 1 set, 2 rare, 5 unique, 0 crafted");
}
