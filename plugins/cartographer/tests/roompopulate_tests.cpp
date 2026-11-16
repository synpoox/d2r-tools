// Host tests for the room-populate subscriber list. No game, no SDK: the code is pure C++.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <stdexcept>
#include <string>
#include <vector>

#include "roompopulate.h"

using namespace cartographer::roompopulate;

namespace {

int gGame = 0;
int gRoom = 0;

}  // namespace

TEST_CASE("a removed subscriber is not called, and an unknown handle is refused")
{
	Subscribers subs;
	int calls = 0;
	const uint32_t h = subs.add("a", [&](void*, void*, int32_t) { ++calls; return 0u; }, nullptr);
	CHECK(subs.size() == 1);
	CHECK(subs.runBefore(&gGame, &gRoom, 202, nullptr) == 0);
	CHECK(calls == 1);
	CHECK(subs.remove(h));
	CHECK_FALSE(subs.remove(h));
	CHECK_FALSE(subs.remove(999));
	CHECK(subs.size() == 0);
	subs.runBefore(&gGame, &gRoom, 202, nullptr);
	CHECK(calls == 1);
}

TEST_CASE("subscribers run in subscribe order")
{
	Subscribers subs;
	std::vector<int> order;
	subs.add("a", [&](void*, void*, int32_t) { order.push_back(1); return 0u; }, nullptr);
	subs.add("b", [&](void*, void*, int32_t) { order.push_back(2); return 0u; }, nullptr);
	subs.add("c", [&](void*, void*, int32_t) { order.push_back(3); return 0u; }, nullptr);
	subs.runBefore(&gGame, &gRoom, 1, nullptr);
	REQUIRE(order.size() == 3);
	CHECK(order[0] == 1);
	CHECK(order[1] == 2);
	CHECK(order[2] == 3);
}

TEST_CASE("SkipNative from the second of three subscribers skips, and all three still run")
{
	Subscribers subs;
	int calls = 0;
	subs.add("a", [&](void*, void*, int32_t) { ++calls; return 0u; }, nullptr);
	subs.add("b", [&](void*, void*, int32_t) { ++calls; return SkipNative; }, nullptr);
	subs.add("c", [&](void*, void*, int32_t) { ++calls; return 0u; }, nullptr);
	const uint32_t flags = subs.runBefore(&gGame, &gRoom, 202, nullptr);
	CHECK((flags & SkipNative) != 0);
	CHECK(calls == 3);
}

TEST_CASE("the arguments reach every callback unchanged")
{
	Subscribers subs;
	void* seenGame = nullptr;
	void* seenRoom = nullptr;
	int32_t seenLevel = 0;
	subs.add("a", [&](void* g, void* r, int32_t l) { seenGame = g; seenRoom = r; seenLevel = l; return 0u; }, nullptr);
	subs.runBefore(&gGame, &gRoom, 202, nullptr);
	CHECK(seenGame == &gGame);
	CHECK(seenRoom == &gRoom);
	CHECK(seenLevel == 202);
}

TEST_CASE("a before that throws is dropped and named, the others still run, and the next run skips it")
{
	Subscribers subs;
	int good = 0;
	subs.add("bad", [](void*, void*, int32_t) -> uint32_t { throw std::runtime_error("boom"); }, nullptr);
	subs.add("good", [&](void*, void*, int32_t) { ++good; return 0u; }, nullptr);
	std::vector<std::string> dropped;
	subs.runBefore(&gGame, &gRoom, 1, &dropped);
	REQUIRE(dropped.size() == 1);
	CHECK(dropped[0] == "bad");
	CHECK(good == 1);
	CHECK(subs.size() == 1);
	dropped.clear();
	subs.runBefore(&gGame, &gRoom, 1, &dropped);
	CHECK(dropped.empty());
	CHECK(good == 2);
}

TEST_CASE("an after that throws is dropped the same way")
{
	Subscribers subs;
	int good = 0;
	subs.add("bad", nullptr, [](void*, void*, int32_t, bool) { throw std::runtime_error("boom"); });
	subs.add("good", nullptr, [&](void*, void*, int32_t, bool) { ++good; });
	std::vector<std::string> dropped;
	subs.runAfter(&gGame, &gRoom, 1, false, &dropped);
	REQUIRE(dropped.size() == 1);
	CHECK(dropped[0] == "bad");
	CHECK(good == 1);
	CHECK(subs.size() == 1);
}

TEST_CASE("a callback may unsubscribe itself and another subscriber")
{
	Subscribers subs;
	uint32_t self = 0;
	uint32_t other = 0;
	int selfCalls = 0;
	int otherCalls = 0;
	self = subs.add("self", [&](void*, void*, int32_t) { ++selfCalls; subs.remove(self); subs.remove(other); return 0u; }, nullptr);
	other = subs.add("other", [&](void*, void*, int32_t) { ++otherCalls; return 0u; }, nullptr);
	subs.runBefore(&gGame, &gRoom, 1, nullptr);
	CHECK(selfCalls == 1);
	CHECK(otherCalls == 0);  // removed before its turn: not called
	CHECK(subs.size() == 0);
	subs.runBefore(&gGame, &gRoom, 1, nullptr);
	CHECK(selfCalls == 1);
}

TEST_CASE("a null before or after is skipped, not called")
{
	Subscribers subs;
	int afterCalls = 0;
	subs.add("only-after", nullptr, [&](void*, void*, int32_t, bool) { ++afterCalls; });
	CHECK(subs.runBefore(&gGame, &gRoom, 1, nullptr) == 0);
	subs.runAfter(&gGame, &gRoom, 1, true, nullptr);
	CHECK(afterCalls == 1);
	subs.add("only-before", [](void*, void*, int32_t) { return SkipNative; }, nullptr);
	CHECK(subs.runBefore(&gGame, &gRoom, 1, nullptr) == SkipNative);
}
