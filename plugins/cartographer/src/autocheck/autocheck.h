// AutoCheck (issue #173): the pure parts of the game check the plugin can run by itself in a test slot. The checklist the engine fills in,
// the one-line results it writes to the plugin log, how the setting names a map, and where to stand to wake the map Boss. No engine
// code here; src/engine/auto_check.cpp drives it from the player tick.
#pragma once

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace cartographer::autocheck {

// The things one check looks at, in the order they are settled.
enum class Item { Open, Arrival, Boss, MapSettings, StatChanges };
inline constexpr size_t ItemCount = 5;

// The name written after PASS or FAIL in the log.
auto itemName(Item item) -> const char *;

struct Result {
	Item item = Item::Open;
	bool pass = false;
	std::string detail;  // one short line of why; may be empty
	bool notApplicable = false;  // the check does not apply to this map: settled as a pass, but counted apart (n/a=, not pass=)
};

// What has been settled so far. Each item is settled once: a second record for the same item is ignored.
class Checklist {
  public:
	// The first item not yet settled, in order, or nothing when all are.
	auto next() const -> std::optional<Item>;
	auto settled(Item item) const -> bool;
	void record(Item item, bool pass, const std::string &detail);
	// Settles an item that does not apply (a map with no Boss, say): not a pass and not a fail.
	void recordNotApplicable(Item item, const std::string &detail);
	// Settles every item not yet settled as a failure with `reason` (a missing part or a step that cannot go on). Fails open: the later items still get their line.
	void failRest(const std::string &reason);
	auto complete() const -> bool;
	auto passCount() const -> size_t;
	auto failCount() const -> size_t;
	auto notApplicableCount() const -> size_t;
	// The settled results in the order of Item, whatever order they were recorded in.
	auto results() const -> std::vector<Result>;

  private:
	bool done_[ItemCount] = {};
	Result result_[ItemCount];
};

// "AUTOCHECK PASS open", "AUTOCHECK FAIL boss: the game refused it" or "AUTOCHECK N/A boss: the map has no Boss".
auto resultLine(const Result &result) -> std::string;
// "AUTOCHECK DONE pass=3 fail=2", with " n/a=1" added when an item did not apply.
auto doneLine(const Checklist &list) -> std::string;
// Every result line in item order, then the DONE line. An unsettled item is written as a failure first (the copy is not changed).
auto finalLines(Checklist list) -> std::vector<std::string>;

// cartographer.txt: AutoCheck (0 or 1) and AutoCheckMap (a map's Name in maps.txt, any case).
struct Config {
	bool enabled = false;
	std::string map;      // the name as maps.txt writes it, once resolved
	std::string problem;  // enabled, but the map cannot be used: why
};
// `mapNames` are the Name cells of maps.txt. Enabled with an empty or unknown AutoCheckMap gives a problem, not a map.
auto resolveConfig(int autoCheck, const std::string &autoCheckMap, const std::vector<std::string> &mapNames) -> Config;

// Where the player stands to wake the boss room: tiles in, the point in subtiles (5 per tile, the unit the game positions units in).
// Inside the room, a quarter of its width from the left edge and at half its height, so the player is next to the boss at its centre.
struct Point {
	int x = 0, y = 0;
};
inline constexpr int SubtilesPerTile = 5;
auto bossStandPoint(int tileX, int tileY, int tileW, int tileH) -> Point;

// Why the boss step found no plan boss room to go to, for an outdoor map or a maze map.
auto noBossRoomReason(bool outdoor) -> std::string;

// What the arrival probe's result (0 not yet, 1 in the entrance cell, 2 not in it, 3 could not be told, 4 no entrance cell was marked) means for the
// arrival item. `settled` is false while the probe has not run; `notApplicable` settles it as N/A instead of pass/fail. `outdoor` picks the log line the failure detail points at.
struct ArrivalVerdict {
	bool settled = false;
	bool pass = false;
	std::string detail;
	bool notApplicable = false;
};
auto arrivalVerdict(int result, bool outdoor) -> ArrivalVerdict;

// Whether a stage that began at `startMs` has run past `limitMs` at `nowMs` (the clock may step back: an earlier now is not a timeout).
auto timedOut(unsigned long long startMs, unsigned long long nowMs, unsigned long long limitMs) -> bool;

}  // namespace cartographer::autocheck
