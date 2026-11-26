// Host tests for the slot map automap memory (issue #306 follow-up: an earlier map's walls drawn after a town trip). No game, no SDK.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "engine/automap_memory.h"

using namespace carto;

namespace {
constexpr int kJail = 18, kTown = 79, kSlotLevel = 159, kTownLevel = 109, kStockJailLevel = 29;

// A save of `cells` in list 1 (walls) for layer `key`.
auto Save(AutomapMemory& m, int key, std::initializer_list<AutomapCell> cells) -> bool {
	return m.OnSave(key, [&cells](auto&& add) {
		for (const AutomapCell& cell : cells) {
			add(1, cell);
		}
	});
}
}  // namespace

TEST_CASE("a town trip plays back the map's own cells and none reach the file") {
	AutomapMemory m;
	m.NewMap(kSlotLevel);
	CHECK(m.OnRead(kJail, kSlotLevel, true) == ReadVerdict::Empty);  // entry: the skip
	CHECK(Save(m, kJail, {{3, 100, 200}, {3, 108, 204}}));            // leave to town: kept, not written
	CHECK(m.OnRead(kTown, kTownLevel, false) == ReadVerdict::File);
	CHECK_FALSE(Save(m, kTown, {{1, 5, 5}}));                         // the town's own save goes to the file
	CHECK(m.OnRead(kJail, kSlotLevel, false) == ReadVerdict::Replay);  // the return: memory, not the file
	REQUIRE(m.Cells(kSlotLevel) != nullptr);
	CHECK(m.Cells(kSlotLevel)->at(1).size() == 2);
	CHECK(Save(m, kJail, {{3, 116, 208}}));                            // the second leave adds the new cells
	CHECK(m.Cells(kSlotLevel)->at(1).size() == 3);
}

TEST_CASE("a new map through the same slot level starts empty") {
	AutomapMemory m;
	m.NewMap(kSlotLevel);
	m.OnRead(kJail, kSlotLevel, true);
	Save(m, kJail, {{3, 100, 200}});
	m.NewMap(kSlotLevel);
	CHECK(m.Cells(kSlotLevel)->at(1).empty());
	CHECK(m.OnRead(kJail, kSlotLevel, false) == ReadVerdict::File);  // not entered yet: the entry skip decides, not the memory
}

TEST_CASE("the game level that shares the layer keeps the file") {
	AutomapMemory m;
	m.NewMap(kSlotLevel);
	m.OnRead(kJail, kSlotLevel, true);
	Save(m, kJail, {{3, 100, 200}});
	CHECK(m.OnRead(kJail, kStockJailLevel, false) == ReadVerdict::File);  // a waypoint to the real Jail
	CHECK_FALSE(Save(m, kJail, {{3, 1, 1}}));
	CHECK(m.Cells(kSlotLevel)->at(1).size() == 1);
}

TEST_CASE("a save after the game was left is still kept out of the file") {
	AutomapMemory m;
	m.NewMap(kSlotLevel);
	m.OnRead(kJail, kSlotLevel, true);
	m.Clear();
	CHECK(Save(m, kJail, {{3, 100, 200}}));
	CHECK(m.OnRead(kJail, kSlotLevel, false) == ReadVerdict::File);  // the next game: nothing learnt for the slot level
}

TEST_CASE("duplicate cells are kept once, unknown lists and layers are ignored") {
	AutomapMemory m;
	m.NewMap(kSlotLevel);
	m.OnRead(kJail, kSlotLevel, true);
	Save(m, kJail, {{3, 100, 200}, {3, 100, 200}});
	CHECK(m.Cells(kSlotLevel)->at(1).size() == 1);
	CHECK(m.OnSave(kJail, [](auto&& add) { add(7, AutomapCell{1, 2, 3}); }));
	CHECK(m.OnRead(-1, kSlotLevel, false) == ReadVerdict::File);
	CHECK_FALSE(Save(m, kJail, {{1, 1, 1}}));  // no layer current any more
}
