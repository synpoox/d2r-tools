// Host tests for the Cartographer map stats draw core. No game, no SDK: the code is pure C++.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <set>
#include <string>
#include <utility>
#include <vector>

#include "mapstats.h"

using namespace cartographer::stats;

namespace {

std::vector<PoolRow> testPool()
{
	std::vector<PoolRow> p;
	const char *names[] = {"a", "b", "c", "d", "e", "f", "g", "h", "i", "j"};
	for (int i = 0; i < 10; ++i)
		p.push_back({i % 2 ? Target::Players : Target::Monsters, names[i], 10 + i, 20 + i, 1 + i % 3});
	return p;
}

std::string dump(const std::vector<Property> &v)
{
	std::string s;
	for (const Property &p : v)
		s += std::string(p.target == Target::Monsters ? "M:" : "P:") + p.stat + "=" + std::to_string(p.value) + " ";
	return s;
}

} // namespace

TEST_CASE("N distinct properties for many seeds")
{
	const auto pool = testPool();
	for (int n : {0, 1, 4, 6, 10}) {
		Settings s;
		s.properties = n;
		for (uint64_t seed = 0; seed < 5000; ++seed) {
			const auto r = drawMapStats(seed, MapRow{}, s, pool);
			REQUIRE(static_cast<int>(r.size()) == n);
			std::set<std::pair<int, std::string>> seen;
			for (const Property &p : r) {
				REQUIRE(seen.insert({static_cast<int>(p.target), p.stat}).second);
				for (const PoolRow &row : pool)
					if (row.stat == p.stat) {
						REQUIRE(p.value >= row.min);
						REQUIRE(p.value <= row.max);
					}
			}
		}
	}
}

TEST_CASE("the map's own Properties overrides the setting; negative means unset")
{
	const auto pool = testPool();
	Settings s;
	s.properties = 6;
	MapRow m;
	m.properties = 2;
	CHECK(drawMapStats(1, m, s, pool).size() == 2);
	m.properties = 0;
	CHECK(drawMapStats(1, m, s, pool).empty());
	m.properties = -1;
	CHECK(drawMapStats(1, m, s, pool).size() == 6);
	s.properties = -3;
	CHECK(drawMapStats(1, m, s, pool).empty());
}

TEST_CASE("N above the pool size draws every distinct property once")
{
	const auto pool = testPool();
	Settings s;
	s.properties = 50;
	CHECK(drawMapStats(7, MapRow{}, s, pool).size() == pool.size());
}

TEST_CASE("rows with the same Target and Stat are one property")
{
	std::vector<PoolRow> pool = {
		{Target::Monsters, "x", 1, 1, 5}, {Target::Monsters, "x", 2, 2, 5}, {Target::Players, "x", 3, 3, 1}};
	Settings s;
	s.properties = 5;
	for (uint64_t seed = 0; seed < 200; ++seed) CHECK(drawMapStats(seed, MapRow{}, s, pool).size() == 2);
}

TEST_CASE("weights: zero is never drawn, heavy rows come first more often")
{
	std::vector<PoolRow> pool = {
		{Target::Monsters, "never", 1, 1, 0}, {Target::Monsters, "heavy", 1, 1, 9}, {Target::Monsters, "light", 1, 1, 1}};
	Settings s;
	s.properties = 1;
	int heavy = 0;
	for (uint64_t seed = 0; seed < 4000; ++seed) {
		const auto r = drawMapStats(seed, MapRow{}, s, pool);
		REQUIRE(r.size() == 1);
		REQUIRE(r[0].stat != "never");
		heavy += r[0].stat == "heavy";
	}
	CHECK(heavy > 3400);
	CHECK(heavy < 3800);
}

TEST_CASE("all weights zero draws nothing")
{
	std::vector<PoolRow> pool = {{Target::Monsters, "a", 1, 1, 0}, {Target::Players, "b", 1, 1, -4}};
	Settings s;
	s.properties = 3;
	CHECK(drawMapStats(1, MapRow{}, s, pool).empty());
}

TEST_CASE("removal reweights later draws")
{
	// heavy:9, light:1, other:1. Heavy first (~9/11); then light and other are equally likely second.
	std::vector<PoolRow> pool = {
		{Target::Monsters, "heavy", 1, 1, 9}, {Target::Monsters, "light", 1, 1, 1}, {Target::Monsters, "other", 1, 1, 1}};
	Settings s;
	s.properties = 2;
	int heavyFirst = 0, lightSecondAfterHeavy = 0, otherSecondAfterHeavy = 0, heavySecondAfterLight = 0, lightFirst = 0;
	for (uint64_t seed = 0; seed < 6000; ++seed) {
		const auto r = drawMapStats(seed, MapRow{}, s, pool);
		REQUIRE(r.size() == 2);
		REQUIRE(r[0].stat != r[1].stat);
		if (r[0].stat == "heavy") {
			++heavyFirst;
			lightSecondAfterHeavy += r[1].stat == "light";
			otherSecondAfterHeavy += r[1].stat == "other";
		} else if (r[0].stat == "light") {
			++lightFirst;
			heavySecondAfterLight += r[1].stat == "heavy";
		}
	}
	CHECK(heavyFirst > 4800); // expected 4909
	CHECK(heavyFirst < 5000);
	CHECK(lightSecondAfterHeavy + otherSecondAfterHeavy == heavyFirst);
	CHECK(lightSecondAfterHeavy > heavyFirst * 0.44);
	CHECK(lightSecondAfterHeavy < heavyFirst * 0.56);
	// Light first leaves heavy:9 vs other:1, so heavy is ~90% of those second draws.
	REQUIRE(lightFirst > 300);
	CHECK(heavySecondAfterLight > lightFirst * 0.82);
	CHECK(heavySecondAfterLight < lightFirst * 0.97);
}

TEST_CASE("huge Min and Max are bounded, not undefined")
{
	std::vector<PoolRow> pool = {{Target::Players, "x", -2000000000, 2000000000, 1}};
	Settings s;
	s.properties = 1;
	for (uint64_t seed = 0; seed < 200; ++seed) {
		const int v = drawMapStats(seed, MapRow{}, s, pool)[0].value;
		CHECK(v >= -500000000);
		CHECK(v <= 500000000);
	}
}

TEST_CASE("deterministic and seed dependent")
{
	const auto pool = testPool();
	Settings s;
	CHECK(drawMapStats(42, MapRow{}, s, pool) == drawMapStats(42, MapRow{}, s, pool));
	int differing = 0;
	for (uint64_t seed = 0; seed < 100; ++seed)
		differing += !(drawMapStats(seed, MapRow{}, s, pool) == drawMapStats(seed + 1, MapRow{}, s, pool));
	CHECK(differing > 90);
}

TEST_CASE("Max below Min gives Min")
{
	std::vector<PoolRow> pool = {{Target::Players, "x", 9, 3, 1}};
	Settings s;
	s.properties = 1;
	CHECK(drawMapStats(1, MapRow{}, s, pool)[0].value == 9);
}

TEST_CASE("missing mapstats.txt uses the built-in pool")
{
	Settings s;
	const auto r = drawMapStats(5, MapRow{}, s, {});
	CHECK(r.size() == 6);
	CHECK(r == drawMapStats(5, MapRow{}, s, builtInPool()));
	bool monsters = false, players = false;
	for (int seed = 0; seed < 100; ++seed)
		for (const Property &p : drawMapStats(seed, MapRow{}, s, {}))
			(p.target == Target::Monsters ? monsters : players) = true;
	CHECK(monsters);
	CHECK(players);
}

TEST_CASE("ItemStats modes")
{
	const auto pool = testPool();
	const std::vector<Property> item = {{Target::Monsters, "item_maxhp_percent", 33}, {Target::Players, "fireresist", -7}};
	Settings s;
	// Off: the item's stats are ignored.
	s.itemStats = false;
	CHECK(drawMapStats(3, MapRow{}, s, pool, item).size() == 6);
	// On: the item's stats, verbatim, in place of a draw, whatever Properties says.
	s.itemStats = true;
	CHECK(drawMapStats(3, MapRow{}, s, pool, item) == item);
	MapRow m;
	m.properties = 1;
	CHECK(drawMapStats(99, m, s, pool, item) == item);
	// On, but the item has none: a normal draw.
	CHECK(drawMapStats(3, MapRow{}, s, pool, {}).size() == 6);
}

TEST_CASE("golden draws")
{
	const auto pool = testPool();
	Settings s;
	s.properties = 4;
	CHECK(dump(drawMapStats(1, MapRow{}, s, pool)) == "M:i=28 P:d=18 P:f=24 M:g=22 ");
	CHECK(dump(drawMapStats(12345, MapRow{}, s, pool)) == "M:a=11 M:e=17 M:c=12 P:h=23 ");
	Settings d;
	CHECK(dump(drawMapStats(0xDEADBEEFCAFEull, MapRow{}, d, {})) == "M:coldresist=17 M:fireresist=20 M:item_maxhp_percent=59 P:coldresist=-18 P:poisonresist=-21 P:fireresist=-32 ");
}
