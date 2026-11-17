// Host tests for the map drop behaviours: the chance rolls with a seeded generator, the extra-drop and Drop TC picks, the socket count and the test spec.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "behaviours.h"

using namespace cartographer::behaviours;

TEST_CASE("the generator is deterministic for a seed and never stuck at zero")
{
	Rng a(42), b(42), z(0);
	for (int i = 0; i < 20; ++i) {
		CHECK(a.next() == b.next());
	}
	CHECK(z.next() != z.next());
	CHECK(Rng(1).below(0) == 0);
}

TEST_CASE("chance: 0 never, 100 and above always, between roughly that share")
{
	Rng rng(7);
	int half = 0;
	for (int i = 0; i < 10000; ++i) {
		CHECK_FALSE(chance(0, rng));
		CHECK_FALSE(chance(-5, rng));
		CHECK(chance(100, rng));
		CHECK(chance(250, rng));
		half += chance(50, rng) ? 1 : 0;
	}
	CHECK(half > 4700);
	CHECK(half < 5300);
}

TEST_CASE("chance is the same sequence for the same seed")
{
	Rng a(99), b(99);
	for (int i = 0; i < 200; ++i) {
		CHECK(chance(33, a) == chance(33, b));
	}
}

TEST_CASE("extra drops: a full 100 is a sure drop, the rest is a chance")
{
	Rng rng(3);
	for (int i = 0; i < 1000; ++i) {
		CHECK(extraDrops(0, rng) == 0);
		CHECK(extraDrops(-10, rng) == 0);
		CHECK(extraDrops(100, rng) == 1);
		const int two = extraDrops(250, rng);
		CHECK((two == 2 || two == 3));
		const int one = extraDrops(50, rng);
		CHECK((one == 0 || one == 1));
	}
}

TEST_CASE("hitTcs returns the indexes of the classes that hit, in order")
{
	Rng rng(5);
	const std::vector<std::pair<std::string, int>> tcs = {{"A", 100}, {"B", 0}, {"C", 100}, {"D", 100}};
	for (int i = 0; i < 100; ++i) {
		CHECK(hitTcs(tcs, rng) == std::vector<size_t>{0, 2, 3});
	}
	CHECK(hitTcs({}, rng).empty());
}

TEST_CASE("hitTcs rolls each class on its own")
{
	Rng rng(11);
	const std::vector<std::pair<std::string, int>> tcs = {{"A", 50}, {"B", 50}};
	int both = 0, none = 0;
	for (int i = 0; i < 4000; ++i) {
		const size_t n = hitTcs(tcs, rng).size();
		both += n == 2 ? 1 : 0;
		none += n == 0 ? 1 : 0;
	}
	CHECK(both > 800);
	CHECK(none > 800);
}

TEST_CASE("socketCount is 1 or 2")
{
	Rng rng(13);
	int ones = 0, twos = 0;
	for (int i = 0; i < 1000; ++i) {
		const int n = socketCount(rng);
		ones += n == 1 ? 1 : 0;
		twos += n == 2 ? 1 : 0;
	}
	CHECK(ones + twos == 1000);
	CHECK(ones > 300);
	CHECK(twos > 300);
}

TEST_CASE("any is false only for an empty map")
{
	MapBehaviours b;
	CHECK_FALSE(any(b));
	b.extraBoss = 1;
	CHECK(any(b));
	b = {};
	b.dropTcs.emplace_back("X", 0);
	CHECK(any(b));
}

TEST_CASE("parseSpec reads every item, a TC name may hold spaces and colons are split from the right")
{
	MapBehaviours b;
	std::string why;
	REQUIRE(parseSpec("tc=Act 5 Good:100;tc=Other:25;bonus=50;eth=100;sock=100;boss=1", &b, &why));
	CHECK(b.dropTcs.size() == 2);
	CHECK(b.dropTcs[0].first == "Act 5 Good");
	CHECK(b.dropTcs[0].second == 100);
	CHECK(b.dropTcs[1].second == 25);
	CHECK(b.dropBonusPct == 50);
	CHECK(b.etherealPct == 100);
	CHECK(b.socketedPct == 100);
	CHECK(b.extraBoss == 1);
}

TEST_CASE("parseSpec refuses bad items and leaves the output alone")
{
	MapBehaviours b;
	b.extraBoss = 7;
	std::string why;
	for (const char* bad : {"tc=NoPct", "tc=:5", "tc=A:x", "bonus=", "eth=-1", "boss=10", "boss=x", "nonsense=1", "bonus"}) {
		CHECK_FALSE(parseSpec(bad, &b, &why));
		CHECK_FALSE(why.empty());
		CHECK(b.extraBoss == 7);
	}
	CHECK(parseSpec("", &b, &why));
	CHECK_FALSE(any(b));
}

TEST_CASE("extra_groups is cut to 0..13")
{
	CHECK(clampExtraGroups(-3) == 0);
	CHECK(clampExtraGroups(0) == 0);
	CHECK(clampExtraGroups(5) == 5);
	CHECK(clampExtraGroups(13) == 13);
	CHECK(clampExtraGroups(99) == kMaxExtraGroups);
}

TEST_CASE("lifestealBlock adds a percent of the hit and never lowers the block")
{
	CHECK(lifestealBlock(0, 1000, 50) == 500);
	CHECK(lifestealBlock(100, 1000, 10) == 200);
	CHECK(lifestealBlock(100, 0, 50) == 100);
	CHECK(lifestealBlock(100, -5, 50) == 100);
	CHECK(lifestealBlock(100, 1000, 0) == 100);
	CHECK(lifestealBlock(0, 1000, 100000) == 10000);  // the percent is cut to 1000
	CHECK(lifestealBlock(kMaxLifestealBlock - 1, 1000000, 100) == kMaxLifestealBlock);
	CHECK(lifestealBlock(kMaxLifestealBlock + 10, 1000, 50) == kMaxLifestealBlock + 10);
}

TEST_CASE("element names")
{
	Element e{};
	CHECK(elementFromName("ltng", &e));
	CHECK(e == Element::Lightning);
	CHECK(elementFromName("pois", &e));
	CHECK(e == Element::Poison);
	CHECK_FALSE(elementFromName("light", &e));
	CHECK_FALSE(elementFromName("", &e));
}

TEST_CASE("physAsStats: percent of the physical damage as element damage")
{
	const auto fire = physAsStats(Element::Fire, 50, 10, 20);
	REQUIRE(fire.size() == 2);
	CHECK(fire[0].stat == 48);
	CHECK(fire[0].value == 5);
	CHECK(fire[1].stat == 49);
	CHECK(fire[1].value == 10);
	const auto cold = physAsStats(Element::Cold, 100, 10, 20);
	REQUIRE(cold.size() == 3);
	CHECK(cold[0].stat == 54);
	CHECK(cold[2].stat == 56);
	CHECK(cold[2].value == kColdLength);
	const auto pois = physAsStats(Element::Poison, 100, 10, 20);
	REQUIRE(pois.size() == 3);
	CHECK(pois[0].stat == 57);
	CHECK(pois[0].value == 10 * 256 / kPoisonLength);
	CHECK(pois[2].value == kPoisonLength);
	CHECK(physAsStats(Element::Magic, 50, 10, 20)[1].stat == 53);
	CHECK(physAsStats(Element::Lightning, 50, 10, 20)[0].stat == 50);
	CHECK(physAsStats(Element::Fire, 0, 10, 20).empty());
	CHECK(physAsStats(Element::Fire, 50, 0, 0).empty());
	CHECK(physAsStats(Element::Fire, 1, 1, 5)[1].value == 1);  // never rounds the max to 0
}

TEST_CASE("parseTestRouting: key[:param]=value pairs")
{
	std::vector<TestRoute> r;
	std::string why;
	REQUIRE(parseTestRouting("extra_groups:quillrat1=3;lifesteal_pct=50;;phys_as_fire=20", &r, &why));
	REQUIRE(r.size() == 3);
	CHECK(r[0].key == "extra_groups");
	CHECK(r[0].param == "quillrat1");
	CHECK(r[0].value == 3);
	CHECK(r[1].key == "lifesteal_pct");
	CHECK(r[1].param.empty());
	CHECK(r[2].value == 20);
	CHECK_FALSE(parseTestRouting("lifesteal_pct=0", &r, &why));
	CHECK_FALSE(parseTestRouting("lifesteal_pct", &r, &why));
	CHECK_FALSE(parseTestRouting("x:=3", &r, &why));
	CHECK_FALSE(why.empty());
	CHECK(r.size() == 3);  // untouched on failure
	REQUIRE(parseTestRouting("", &r, &why));
	CHECK(r.empty());
}

TEST_CASE("fromRouting builds the map's behaviours from the routed sums (issue #539)")
{
	using cartographer::statplan::MapRouting;
	MapRouting r;
	r.behaviours = {{"drop_tc", "Map Rewards", 25}, {"drop_tc", "", 10}, {"drop_tc", "Junk", -5}, {"drop_tc", "Other", 100}, {"drop_bonus", "", 150},
	                {"drop_ethereal", "", 30}, {"drop_socketed", "", 20}, {"extra_boss", "", 50}, {"extra_groups", "", 4}};
	const MapBehaviours b = fromRouting(r);
	CHECK(b.dropBonusPct == 150);
	CHECK(b.etherealPct == 30);
	CHECK(b.socketedPct == 20);
	CHECK(b.extraBoss == kMaxExtraBoss);
	REQUIRE(b.dropTcs.size() == 2);  // a TC with no name or a sum of 0 or less is left out
	CHECK(b.dropTcs[0] == std::make_pair(std::string("Map Rewards"), 25));
	CHECK(b.dropTcs[1] == std::make_pair(std::string("Other"), 100));
	CHECK(any(b));
	CHECK_FALSE(any(fromRouting(MapRouting{})));  // an empty routing clears the map's behaviours
	r.behaviours = {{"drop_bonus", "", -40}};
	CHECK(fromRouting(r).dropBonusPct == 0);  // never below 0
}
