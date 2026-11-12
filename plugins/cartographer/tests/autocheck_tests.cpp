// Host tests for the Cartographer AutoCheck core (issue #173). No game, no SDK: the code is pure C++.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "autocheck.h"

using namespace cartographer::autocheck;

TEST_CASE("autocheck: result lines carry PASS or FAIL, the item name and an optional reason") {
	CHECK(resultLine({Item::Open, true, ""}) == "AUTOCHECK PASS open");
	CHECK(resultLine({Item::Boss, false, "  the game refused it  "}) == "AUTOCHECK FAIL boss: the game refused it");
	CHECK(resultLine({Item::MapSettings, true, "Level 85"}) == "AUTOCHECK PASS monster-settings: Level 85");
	CHECK(std::string(itemName(Item::Arrival)) == "arrival");
	CHECK(std::string(itemName(Item::StatChanges)) == "stat-changes");
}

TEST_CASE("autocheck: items are settled in order, once each") {
	Checklist list;
	CHECK(list.next() == Item::Open);
	list.record(Item::Open, true, "");
	CHECK(list.next() == Item::Arrival);
	list.record(Item::Arrival, false, "first");
	list.record(Item::Arrival, true, "second is ignored");
	CHECK(list.passCount() == 1);
	CHECK(list.failCount() == 1);
	CHECK(list.results()[1].detail == "first");
	CHECK_FALSE(list.complete());
}

TEST_CASE("autocheck: an item recorded out of order is still reported in item order") {
	Checklist list;
	list.record(Item::StatChanges, true, "");
	list.record(Item::Open, true, "");
	const auto results = list.results();
	REQUIRE(results.size() == 2);
	CHECK(results[0].item == Item::Open);
	CHECK(results[1].item == Item::StatChanges);
	CHECK(list.next() == Item::Arrival);
}

TEST_CASE("autocheck: failRest fails open, leaving settled items alone") {
	Checklist list;
	list.record(Item::Open, true, "");
	list.failRest("the portal never opened");
	CHECK(list.complete());
	CHECK(list.passCount() == 1);
	CHECK(list.failCount() == ItemCount - 1);
	CHECK(list.results()[2].detail == "the portal never opened");
}

TEST_CASE("autocheck: the final lines are one per item then DONE, even from a half-done list") {
	Checklist list;
	list.record(Item::Open, true, "");
	const auto lines = finalLines(list);
	REQUIRE(lines.size() == ItemCount + 1);
	CHECK(lines.front() == "AUTOCHECK PASS open");
	CHECK(lines[1] == "AUTOCHECK FAIL arrival: not reached");
	CHECK(lines.back() == "AUTOCHECK DONE pass=1 fail=4");
	CHECK(list.failCount() == 0);  // the caller's list is untouched
}

TEST_CASE("autocheck: the setting is off by default and resolves the map name in any case") {
	CHECK_FALSE(resolveConfig(0, "Colossal Summit", {"Colossal Summit"}).enabled);
	const Config on = resolveConfig(1, "  colossal summit ", {"Other", "Colossal Summit"});
	CHECK(on.enabled);
	CHECK(on.map == "Colossal Summit");
	CHECK(on.problem.empty());
}

TEST_CASE("autocheck: an enabled setting with no usable map says why") {
	const Config blank = resolveConfig(1, "", {"A"});
	CHECK(blank.enabled);
	CHECK(blank.map.empty());
	CHECK(blank.problem.find("empty") != std::string::npos);
	const Config unknown = resolveConfig(1, "Nope", {"A"});
	CHECK(unknown.problem.find("'Nope'") != std::string::npos);
	CHECK(resolveConfig(1, "A", {}).problem.find("not a map") != std::string::npos);
}

TEST_CASE("autocheck: the stand point is inside the boss room, in subtiles") {
	const Point p = bossStandPoint(5000, 2024, 8, 4);
	CHECK(p.x == (5000 + 2) * SubtilesPerTile);
	CHECK(p.y == (2024 + 2) * SubtilesPerTile);
	const Point tiny = bossStandPoint(10, 20, 1, 1);
	CHECK(tiny.x == 11 * SubtilesPerTile);
	CHECK(tiny.y == 21 * SubtilesPerTile);
}

TEST_CASE("autocheck: the no-boss-room reason names the outdoor plan for an outdoor map") {
	CHECK(noBossRoomReason(true).find("outdoor plan") != std::string::npos);
	CHECK(noBossRoomReason(false).find("Cave and Crypt") != std::string::npos);
}

TEST_CASE("autocheck: a stage times out only after the limit and never when the clock steps back") {
	CHECK_FALSE(timedOut(1000, 1500, 500));
	CHECK(timedOut(1000, 1501, 500));
	CHECK_FALSE(timedOut(5000, 100, 500));
}

TEST_CASE("autocheck: the arrival result reads as pass, fail, unreadable or not yet") {
	CHECK_FALSE(arrivalVerdict(0, true).settled);
	CHECK(arrivalVerdict(1, true).settled);
	CHECK(arrivalVerdict(1, true).pass);
	CHECK(arrivalVerdict(1, true).detail == "ARRIVAL IS IN THE ENTRANCE CELL");
	CHECK(arrivalVerdict(2, true).settled);
	CHECK_FALSE(arrivalVerdict(2, true).pass);
	CHECK_FALSE(arrivalVerdict(3, true).pass);
	CHECK(arrivalVerdict(3, true).detail.find("[outdoor probe] arrival") != std::string::npos);
	CHECK(arrivalVerdict(3, false).detail.find("[maze probe] arrival") != std::string::npos);
	CHECK(arrivalVerdict(4, true).settled);
	CHECK_FALSE(arrivalVerdict(4, true).pass);
	CHECK(arrivalVerdict(4, true).detail.find("left to the game") != std::string::npos);
}

TEST_CASE("autocheck: an item that does not apply is written N/A and counted apart from pass") {
	Checklist list;
	list.record(Item::Open, true, "");
	list.recordNotApplicable(Item::Boss, "the map has no Boss");
	list.recordNotApplicable(Item::Boss, "ignored: settled once");
	CHECK(resultLine(list.results()[1]) == "AUTOCHECK N/A boss: the map has no Boss");
	CHECK(list.passCount() == 1);
	CHECK(list.failCount() == 0);
	CHECK(list.notApplicableCount() == 1);
	CHECK(doneLine(list) == "AUTOCHECK DONE pass=1 fail=0 n/a=1");
}
