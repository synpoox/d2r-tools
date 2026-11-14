// Host tests for the AutomapRestore skip decision (issue #354). No game, no SDK.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "engine/automap_skip.h"

using namespace carto;

namespace {
constexpr int kTown = 79, kCave = 2, kSlotLevel = 138, kTownLevel = 109, kOtherLayer = 27, kOtherLevel = 3;

SkipState Armed() {
	SkipState s;
	SkipArm(s, kTown, kSlotLevel);
	return s;
}
}  // namespace

TEST_CASE("the read made while the player stands in the slot level is skipped once") {
	SkipState s = Armed();
	SkipTick(s, kSlotLevel);
	CHECK(SkipOnRead(s, kCave, kSlotLevel) == SkipVerdict::Skip);
	CHECK(SkipOnRead(s, kCave, kSlotLevel) == SkipVerdict::Keep);
}

TEST_CASE("a read before the player is in the slot level is kept, and the skip stays for the entry") {
	SkipState s = Armed();
	CHECK(SkipOnRead(s, kOtherLayer, kTownLevel) == SkipVerdict::Keep);  // a waypoint trip
	SkipTick(s, kOtherLevel);
	SkipTick(s, kSlotLevel);
	CHECK(SkipOnRead(s, kCave, kSlotLevel) == SkipVerdict::Skip);
}

TEST_CASE("the layer being left is never skipped, nor an unknown one") {
	SkipState s = Armed();
	SkipTick(s, kSlotLevel);
	CHECK(SkipOnRead(s, kTown, kSlotLevel) == SkipVerdict::Keep);
	CHECK(SkipOnRead(s, -1, kSlotLevel) == SkipVerdict::Keep);
	CHECK(SkipOnRead(s, kCave, kSlotLevel) == SkipVerdict::Skip);
}

TEST_CASE("a missed skip (the read came before the tick) is dropped when the player leaves the slot level") {
	SkipState s = Armed();
	CHECK(SkipOnRead(s, kCave, kTownLevel) == SkipVerdict::Keep);  // order flipped: the tick still says town
	SkipTick(s, kSlotLevel);
	SkipTick(s, kTownLevel);                                       // the trip to town
	CHECK_FALSE(s.armed);
	SkipTick(s, kSlotLevel);                                       // the return
	CHECK(SkipOnRead(s, kCave, kSlotLevel) == SkipVerdict::Keep);
}

TEST_CASE("ticks before the player enters the slot level do not disarm") {
	SkipState s = Armed();
	SkipTick(s, kTownLevel);
	SkipTick(s, kOtherLevel);
	CHECK(s.armed);
}

TEST_CASE("an unarmed state never skips, and a new arm starts clean") {
	SkipState s;
	SkipTick(s, kSlotLevel);
	CHECK(SkipOnRead(s, kCave, kSlotLevel) == SkipVerdict::Keep);
	SkipArm(s, kTown, kSlotLevel);
	SkipTick(s, kSlotLevel);
	SkipTick(s, kTownLevel);
	SkipArm(s, kTown, kSlotLevel);
	CHECK_FALSE(s.entered);
	SkipTick(s, kSlotLevel);
	CHECK(SkipOnRead(s, kCave, kSlotLevel) == SkipVerdict::Skip);
}

TEST_CASE("an unknown player level never drops the skip") {
	SkipState s = Armed();
	SkipTick(s, kSlotLevel);
	SkipTick(s, -1);
	CHECK(s.armed);
	CHECK(SkipOnRead(s, kCave, kSlotLevel) == SkipVerdict::Skip);
}

TEST_CASE("two level feeds reporting the same level (host: tick and event) behave as one") {
	SkipState s = Armed();
	SkipTick(s, kSlotLevel);
	SkipTick(s, kSlotLevel);
	CHECK(s.entered);
	CHECK(SkipOnRead(s, kCave, kSlotLevel) == SkipVerdict::Skip);
	SkipTick(s, kSlotLevel);
	SkipTick(s, 109);
	SkipTick(s, 109);
	CHECK_FALSE(s.armed);
}
