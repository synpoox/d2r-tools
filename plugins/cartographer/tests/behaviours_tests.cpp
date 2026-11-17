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
