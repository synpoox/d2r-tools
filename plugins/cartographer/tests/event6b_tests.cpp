// Host tests for the phase 6b core: the objects name rule, the treasure class index, the boss Drop decision and the call
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
