// Host tests for the phase 6b core: event routing, the Ambush wave bookkeeping, the Treasure chest and flee decisions, the objects name rule, the treasure class index, the boss Drop decision and the call
// target decoder. No game, no SDK.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <string>
#include <vector>

#include "event6b.h"

using namespace cartographer::event6b;

namespace {

// Line order: 0 Test Data, 1 Casket, 2 Shrine, 3 Well, 4 Shrine (a repeat), 5 (an empty Name field), 6 Barrel
std::vector<std::string> objects() { return {"Test Data", "Casket", "Shrine", "Well", "Shrine", "", "Barrel"}; }

}  // namespace

TEST_CASE("resolveObject: a name is its line order") {
	const ObjectLookup r = resolveObject(objects(), "Casket", 7);
	CHECK(r.result == ObjectResult::Found);
	CHECK(r.row == 1);
	CHECK_FALSE(r.numeric);
	// An empty Name field keeps its row, so the line after it is row 6.
	CHECK(resolveObject(objects(), "Barrel", 7).row == 6);
	CHECK(resolveObject(objects(), "Test Data", 7).row == 0);
}

TEST_CASE("resolveObject: exact case first, then any case") {
	std::vector<std::string> names = {"Well", "well", "Chest"};
	CHECK(resolveObject(names, "Well", 3).row == 0);
	CHECK(resolveObject(names, "well", 3).row == 1);
	const ObjectLookup r = resolveObject(names, "CHEST", 3);
	CHECK(r.result == ObjectResult::Found);
	CHECK(r.row == 2);
	CHECK(resolveObject(names, "WELL", 3).result == ObjectResult::Duplicate);
}

TEST_CASE("resolveObject: a repeated name is refused and lists its rows") {
	const ObjectLookup r = resolveObject(objects(), "Shrine", 7);
	CHECK(r.result == ObjectResult::Duplicate);
	CHECK(r.row == -1);
	REQUIRE(r.rows.size() == 2);
	CHECK(r.rows[0] == 2);
	CHECK(r.rows[1] == 4);
	const std::string text = describeObjectLookup(r, "Shrine", 7, 7);
	CHECK(text.find("rows 2, 4") != std::string::npos);
	CHECK(text.find("row number") != std::string::npos);
}

TEST_CASE("resolveObject: names need the file's line count to equal the game's row count") {
	CHECK(resolveObject(objects(), "Casket", 8).result == ObjectResult::CountMismatch);
	CHECK(resolveObject(objects(), "Casket", 6).result == ObjectResult::CountMismatch);
	CHECK(resolveObject(objects(), "Casket", 0).result == ObjectResult::NoGameCount);
	CHECK(resolveObject({}, "Casket", 7).result == ObjectResult::NoTable);
	CHECK(resolveObject(objects(), "Nothing", 7).result == ObjectResult::NotFound);
	CHECK(resolveObject(objects(), "", 7).result == ObjectResult::NotFound);
	CHECK(describeObjectLookup(resolveObject(objects(), "Casket", 8), "Casket", 7, 8).find("7 lines") != std::string::npos);
}

TEST_CASE("resolveObject: a number is a row, below the game's count") {
	ObjectLookup r = resolveObject(objects(), "4", 7);  // a repeated name's row can be given by number
	CHECK(r.result == ObjectResult::Found);
	CHECK(r.row == 4);
	CHECK(r.numeric);
	CHECK(resolveObject(objects(), "6", 7).row == 6);
	CHECK(resolveObject(objects(), "7", 7).result == ObjectResult::BadRow);
	CHECK(resolveObject(objects(), "-1", 7).result == ObjectResult::NotFound);
	CHECK(resolveObject(objects(), "12a", 7).result == ObjectResult::NotFound);
	// With the table unusable, a number still works against the game's count.
	CHECK(resolveObject({}, "3", 7).row == 3);
	CHECK(resolveObject(objects(), "3", 9).row == 3);
	// A line named with digits wins over the number.
	std::vector<std::string> names = {"a", "2", "b"};
	CHECK(resolveObject(names, "2", 3).row == 1);
	CHECK_FALSE(resolveObject(names, "2", 3).numeric);
}

TEST_CASE("isRowNumber") {
	int v = -1;
	CHECK(isRowNumber("0", &v));
	CHECK(v == 0);
	CHECK(isRowNumber("123456789", &v));
	CHECK(v == 123456789);
	CHECK_FALSE(isRowNumber("1234567890", &v));
	CHECK_FALSE(isRowNumber("", &v));
	CHECK_FALSE(isRowNumber("1 ", &v));
	CHECK_FALSE(isRowNumber("+1", &v));
}

TEST_CASE("tcIndexFromLookup: the game's name lookup to a roller index") {
	std::string why;
	CHECK(tcIndexFromLookup(5, 100, &why) == 5);
	CHECK(tcIndexFromLookup(99, 100, &why) == 99);
	CHECK(tcIndexFromLookup(-1, 100, &why) == -1);
	CHECK(why.find("no treasure class") != std::string::npos);
	CHECK(tcIndexFromLookup(-7, 100, &why) == -1);
	CHECK(why.find("neither a row") != std::string::npos);
	CHECK(tcIndexFromLookup(0, 100, &why) == -1);
	CHECK(why.find("index 0") != std::string::npos);
	CHECK(tcIndexFromLookup(100, 100, &why) == -1);
	CHECK(why.find("past the game's 100") != std::string::npos);
	CHECK(tcIndexFromLookup(5, 0, &why) == -1);
	CHECK(tcIndexFromLookup(70000, 100000, &why) == -1);
	CHECK(why.find("16-bit") != std::string::npos);
	CHECK(tcIndexFromLookup(0xFFFF, 0x10000, nullptr) == 0xFFFF);
}

TEST_CASE("decideDrop: one extra roll, only for the tracked boss") {
	DropInput in;
	in.armed = in.tracked = in.monster = in.hasKiller = true;
	DropDecision d = decideDrop(in);
	CHECK(d.action == DropAction::Roll);
	CHECK(d.reason == DropReason::Rolled);

	DropInput notArmed = in;
	notArmed.armed = false;
	CHECK(decideDrop(notArmed).action == DropAction::Skip);
	CHECK(decideDrop(notArmed).reason == DropReason::NotArmed);

	DropInput other = in;
	other.tracked = false;
	CHECK(decideDrop(other).reason == DropReason::NotTracked);

	DropInput player = in;
	player.monster = false;
	CHECK(decideDrop(player).reason == DropReason::NotMonster);

	DropInput again = in;
	again.dropped = true;
	CHECK(decideDrop(again).action == DropAction::Skip);
	CHECK(decideDrop(again).reason == DropReason::AlreadyDropped);

	DropInput nobody = in;
	nobody.hasKiller = false;
	CHECK(decideDrop(nobody).reason == DropReason::NoKiller);

	// A unit that is not ours is never rolled for, whatever else is true.
	DropInput stranger;
	stranger.armed = stranger.monster = stranger.hasKiller = true;
	CHECK(decideDrop(stranger).action == DropAction::Skip);
	CHECK(std::string(dropReasonName(DropReason::Rolled)) == "rolled");
}

TEST_CASE("callTargetOffset: the rel32 of an E8 at the site") {
	// 3 filler bytes, then call +0x10 at offset 3: target = 3 + 5 + 0x10 = 0x18.
	const uint8_t code[] = {0x90, 0x90, 0x90, 0xE8, 0x10, 0x00, 0x00, 0x00, 0xC3};
	CHECK(callTargetOffset(code, sizeof(code), 3) == 0x18);
	CHECK(callTargetOffset(code, sizeof(code), 0) == kNoTarget);  // not a call
	CHECK(callTargetOffset(code, sizeof(code), 5) == kNoTarget);  // the call would run past the end
	CHECK(callTargetOffset(code, sizeof(code), 100) == kNoTarget);
	CHECK(callTargetOffset(nullptr, 0, 0) == kNoTarget);
	// A backward call.
	const uint8_t back[] = {0xE8, 0xFB, 0xFF, 0xFF, 0xFF};  // rel32 -5: target is the call itself
	CHECK(callTargetOffset(back, sizeof(back), 0) == 0);
	const uint8_t before[] = {0xE8, 0xF0, 0xFF, 0xFF, 0xFF};  // rel32 -16: before the buffer
	CHECK(callTargetOffset(before, sizeof(before), 0) == -11);
}

// --- Phase 6b second half: routing, Ambush waves, the Treasure chest and flee ------------------------------------------------

namespace ev = cartographer::events;

TEST_CASE("routeEvent: each kind takes its own path") {
	CHECK(routeEvent(ev::Kind::Boss, ev::NameKind::Monster) == Route::Boss);
	CHECK(routeEvent(ev::Kind::Horde, ev::NameKind::Unique) == Route::Horde);
	CHECK(routeEvent(ev::Kind::Ambush, ev::NameKind::Monster) == Route::Ambush);
	CHECK(routeEvent(ev::Kind::Shrine, ev::NameKind::Unresolved) == Route::Shrine);  // a Shrine's Spawn is never a monster
	CHECK(routeEvent(ev::Kind::Shrine, ev::NameKind::Monster) == Route::Shrine);
	CHECK(routeEvent(ev::Kind::Invalid, ev::NameKind::Monster) == Route::None);
}

TEST_CASE("routeEvent: a Treasure is a monster when Spawn is one, else a chest") {
	CHECK(routeEvent(ev::Kind::Treasure, ev::NameKind::Monster) == Route::TreasureMonster);
	CHECK(routeEvent(ev::Kind::Treasure, ev::NameKind::Unique) == Route::TreasureMonster);
	CHECK(routeEvent(ev::Kind::Treasure, ev::NameKind::Object) == Route::TreasureChest);
	CHECK(routeEvent(ev::Kind::Treasure, ev::NameKind::Unresolved) == Route::TreasureChest);
	CHECK(std::string(routeName(Route::TreasureChest)) == "treasure chest");
	CHECK(std::string(routeName(Route::None)) == "none");
}

namespace {
WaveTracker armed(int waves, uint64_t timeout = 1000, uint64_t gap = 50) {
	WaveTracker t;
	t.arm(waves, timeout, gap);
	return t;
}
}  // namespace

TEST_CASE("WaveTracker: nothing happens until the player is in the room, then one wave is asked for") {
	WaveTracker t = armed(2);
	CHECK(t.phase() == WaveTracker::Phase::Waiting);
	CHECK(t.active());
	CHECK(t.step(false, 0) == WaveTracker::Step::Wait);
	CHECK(t.step(false, 5000) == WaveTracker::Step::Wait);
	CHECK(t.step(true, 100) == WaveTracker::Step::SpawnWave);
	CHECK(t.wavesStarted() == 1);
	// The caller has not answered yet: it is not asked twice.
	CHECK(t.step(true, 101) == WaveTracker::Step::Wait);
	CHECK(t.wavesStarted() == 1);
	// An unarmed tracker never asks.
	WaveTracker idle;
	CHECK_FALSE(idle.active());
	CHECK(idle.step(true, 0) == WaveTracker::Step::Wait);
}

TEST_CASE("WaveTracker: a wave is cleared when every tracked id died, then the next comes after the gap") {
	WaveTracker t = armed(2);
	REQUIRE(t.step(true, 100) == WaveTracker::Step::SpawnWave);
	t.waveSpawned({7, 8, 9}, 100);
	CHECK(t.phase() == WaveTracker::Phase::Fighting);
	CHECK(t.remaining() == 3);
	CHECK_FALSE(t.onDeath(99));  // a monster that is not in the wave
	CHECK(t.remaining() == 3);
	CHECK(t.onDeath(8));
	CHECK_FALSE(t.onDeath(8));  // reported twice
	CHECK(t.step(true, 200) == WaveTracker::Step::Wait);
	CHECK(t.onDeath(7));
	CHECK(t.onDeath(9));
	CHECK(t.step(true, 300) == WaveTracker::Step::Wait);  // cleared: the gap starts
	CHECK(t.lastEnd() == WaveEnd::Cleared);
	CHECK(t.phase() == WaveTracker::Phase::Gap);
	CHECK(t.step(true, 349) == WaveTracker::Step::Wait);
	CHECK(t.step(false, 400) == WaveTracker::Step::Wait);  // the player left: the wave waits for the room
	CHECK(t.step(true, 400) == WaveTracker::Step::SpawnWave);
	CHECK(t.wavesStarted() == 2);
}

TEST_CASE("WaveTracker: the last wave ends the event") {
	WaveTracker t = armed(2);
	REQUIRE(t.step(true, 0) == WaveTracker::Step::SpawnWave);
	t.waveSpawned({1}, 0);
	t.onDeath(1);
	REQUIRE(t.step(true, 10) == WaveTracker::Step::Wait);
	REQUIRE(t.step(true, 100) == WaveTracker::Step::SpawnWave);
	t.waveSpawned({2, 3}, 100);
	t.onDeath(2);
	t.onDeath(3);
	CHECK(t.step(true, 110) == WaveTracker::Step::Finished);
	CHECK(t.phase() == WaveTracker::Phase::Done);
	CHECK_FALSE(t.active());
	CHECK(t.step(true, 120) == WaveTracker::Step::Finished);
	CHECK_FALSE(t.onDeath(2));
}

TEST_CASE("WaveTracker: a wave that is not cleared times out and the next one still comes") {
	WaveTracker t = armed(2, 1000, 50);
	REQUIRE(t.step(true, 0) == WaveTracker::Step::SpawnWave);
	t.waveSpawned({1, 2}, 0);
	t.onDeath(1);
	CHECK(t.step(true, 999) == WaveTracker::Step::Wait);
	CHECK(t.step(true, 1000) == WaveTracker::Step::Wait);  // timed out: the gap starts
	CHECK(t.lastEnd() == WaveEnd::TimedOut);
	CHECK(t.remaining() == 0);
	CHECK_FALSE(t.onDeath(2));  // too late to count
	CHECK(t.step(true, 1050) == WaveTracker::Step::SpawnWave);
	t.waveSpawned({5}, 1050);
	CHECK(t.step(true, 2049) == WaveTracker::Step::Wait);
	CHECK(t.step(true, 2050) == WaveTracker::Step::Finished);  // the last wave timing out ends it
	CHECK(t.lastEnd() == WaveEnd::TimedOut);
}

TEST_CASE("WaveTracker: a wave the game made no unit of abandons the event") {
	WaveTracker t = armed(3);
	REQUIRE(t.step(true, 0) == WaveTracker::Step::SpawnWave);
	t.waveSpawned({}, 0);
	CHECK(t.lastEnd() == WaveEnd::Empty);
	CHECK(t.phase() == WaveTracker::Phase::Done);
	CHECK(t.step(true, 1) == WaveTracker::Step::Finished);
	CHECK(std::string(waveEndName(WaveEnd::Empty)).find("none") != std::string::npos);
}

TEST_CASE("WaveTracker: one wave only, waves below 1 count as 1, arm starts over, reset clears") {
	WaveTracker t;
	t.arm(0, 100, 10);
	CHECK(t.wavesTotal() == 1);
	REQUIRE(t.step(true, 0) == WaveTracker::Step::SpawnWave);
	t.waveSpawned({1}, 0);
	t.onDeath(1);
	CHECK(t.step(true, 1) == WaveTracker::Step::Finished);
	t.arm(2, 100, 10);
	CHECK(t.phase() == WaveTracker::Phase::Waiting);
	CHECK(t.wavesStarted() == 0);
	CHECK(t.lastEnd() == WaveEnd::None);
	t.reset();
	CHECK_FALSE(t.active());
	CHECK(t.step(true, 0) == WaveTracker::Step::Wait);
	// waveSpawned outside the Spawning phase is ignored.
	t.waveSpawned({4}, 0);
	CHECK(t.remaining() == 0);
}

TEST_CASE("WaveTracker: a unit that dies while the wave is being spawned is counted") {
	WaveTracker t = armed(1);
	REQUIRE(t.step(true, 100) == WaveTracker::Step::SpawnWave);
	CHECK_FALSE(t.onDeath(7));  // died before waveSpawned()
	CHECK_FALSE(t.onDeath(55));  // not in the wave: ignored later
	t.waveSpawned({7, 8}, 100);
	CHECK(t.remaining() == 1);
	CHECK(t.onDeath(8));
	CHECK(t.step(true, 200) == WaveTracker::Step::Finished);
	CHECK(t.lastEnd() == WaveEnd::Cleared);
}

TEST_CASE("decideChest: roll once when the chest's mode changed during the call") {
	ChestInput in;
	in.armed = in.tracked = in.object = true;
	in.modeBefore = 0;
	in.modeAfter = 2;
	CHECK(decideChest(in) == ChestReason::Rolled);
	in.rolled = true;
	CHECK(decideChest(in) == ChestReason::AlreadyRolled);
	in.rolled = false;
	in.modeAfter = 0;
	CHECK(decideChest(in) == ChestReason::NotOpened);
	in.modeAfter = 2;
	in.object = false;
	CHECK(decideChest(in) == ChestReason::NotObject);
	in.object = true;
	in.tracked = false;
	CHECK(decideChest(in) == ChestReason::NotTracked);
	in.armed = false;
	CHECK(decideChest(in) == ChestReason::NotArmed);
	CHECK(std::string(chestReasonName(ChestReason::NotOpened)).find("mode") != std::string::npos);
}

TEST_CASE("describeFleeChecks: says which check would stop the flee") {
	CHECK(describeFleeChecks(0, 1).find("should set the flee") != std::string::npos);
	CHECK(describeFleeChecks(1, 1).find("will return without") != std::string::npos);
	CHECK(describeFleeChecks(0, 0).find("will return without") != std::string::npos);
	CHECK(describeFleeChecks(-1, -1).find("not fully probed") != std::string::npos);
	CHECK(describeFleeChecks(-1, 0).find("will return without") != std::string::npos);
}
