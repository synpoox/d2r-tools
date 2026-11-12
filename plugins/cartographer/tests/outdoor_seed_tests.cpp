// Host tests for the outdoor multiplayer layout sync (issue #146): the plan and the slot level's RNG seed are pure functions of the
// map's seed and table data, so a host and a client given the same seed build the same level. No game, no SDK.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "outdoor_plan.h"
#include "outdoor_seed.h"

using namespace cartographer::outdoor;

namespace {

MapRequest request(uint64_t seed)
{
	MapRequest r;
	r.tileset = "Jungle";
	r.shape = "Winding";
	r.size = "Medium";
	r.seed = seed;
	r.options.originX = 6000;
	r.options.originY = 3000;
	r.options.maxTilesX = 384;
	r.options.maxTilesY = 384;
	return r;
}

} // namespace

TEST_CASE("the slot RNG seed is a function of the map seed and the level id only")
{
	const SlotRngSeed a = slotRngSeedFor(0x0123456789abcdefULL, 139);
	const SlotRngSeed b = slotRngSeedFor(0x0123456789abcdefULL, 139);
	CHECK(a.first == b.first);
	CHECK(a.second == 0x29a);
	CHECK(slotRngSeedFor(0x0123456789abcdeeULL, 139).first != a.first);
	CHECK(slotRngSeedFor(0x0123456789abcdefULL, 140).first != a.first);
}

TEST_CASE("a host and a client given the same seed and tables plan the identical level")
{
	const Catalogue &cat = builtinJungleCatalogue();
	for (const uint64_t seed : {1ULL, 77ULL, 0x0123456789abcdefULL, 0xffffffffffffffffULL}) {
		const MapPlan host = planOutdoorMap(request(seed), cat);
		const MapPlan client = planOutdoorMap(request(seed), cat);
		REQUIRE(host.ok);
		CHECK(client.ok);
		CHECK(host.hash == client.hash);
		CHECK(host.cacheKey == client.cacheKey);
		CHECK(host.stamps.stamps.size() == client.stamps.stamps.size());
		CHECK(host.stamps.voidCells.size() == client.stamps.voidCells.size());
		CHECK(host.stamps.tilesW == client.stamps.tilesW);
		CHECK(host.stamps.tilesH == client.stamps.tilesH);
	}
}

TEST_CASE("the plan is the same cached or fresh, and in any order of building")
{
	const Catalogue &cat = builtinJungleCatalogue();
	PlanCache cache(4);
	const uint64_t first = cache.get(request(5), cat)->hash;
	const uint64_t other = cache.get(request(6), cat)->hash;
	CHECK(first != other);
	PlanCache clientCache(4);  // the client's own cache, filled in the other order
	CHECK(clientCache.get(request(6), cat)->hash == other);
	CHECK(clientCache.get(request(5), cat)->hash == first);
	CHECK(planOutdoorMap(request(5), cat).hash == first);
}

TEST_CASE("only the request decides the plan: a different seed or different table data is a different plan")
{
	const Catalogue &cat = builtinJungleCatalogue();
	const uint64_t base = planOutdoorMap(request(5), cat).hash;
	CHECK(planOutdoorMap(request(6), cat).hash != base);
	MapRequest big = request(5);
	big.size = "Large";
	big.options.maxTilesX = big.options.maxTilesY = 512;
	CHECK(planOutdoorMap(big, cat).hash != base);
}
