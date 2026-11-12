// Cartographer map stats: draws a map's random properties. Pure integer code on plain structs, no engine headers and
// no table parsing (the loader fills these structs). Deterministic from the map seed, using the layout's own PCG32.
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace cartographer::stats {

enum class Target { Monsters, Players };

// One mapstats.txt row.
struct PoolRow {
	Target target = Target::Monsters;
	std::string stat; // an itemstatcost Stat name
	int min = 0;
	int max = 0;
	int weight = 1; // 0 = off (never drawn, no message); below 0 = error (skipped); above kMaxWeightLimit is clamped
};

// Limits, shared with the events table. Values beyond them are clamped, never wrapped; checkPool() says so.
//  - Min and Max are clamped to [-kMaxBound, kMaxBound], so the width of a roll cannot overflow an int.
//  - Weight is clamped to kMaxWeightLimit. The draw adds weights in 64 bits, so any pool size is safe.
constexpr int kMaxBound = 500000000;
constexpr int kMaxWeightLimit = 1000000;

// A problem found in a mapstats.txt row, for the loader and `cartographer check` to show. An error row is skipped;
// a warning row is still used (clamped). `row` is the 0-based index into the pool.
struct PoolIssue {
	bool error = false;
	size_t row = 0;
	std::string message; // plain English
};

// Checks the pool: negative Weight (error); Weight, Min or Max beyond the limits above, and Max below Min (warnings).
std::vector<PoolIssue> checkPool(const std::vector<PoolRow> &pool);

// The cartographer.txt settings this module reads.
struct Settings {
	int properties = 6; // Properties: random properties per map
	bool itemStats = false; // ItemStats=1
};

// The maps.txt row fields this module reads.
struct MapRow {
	int properties = -1; // the map's own Properties; negative means the column is empty (use the Settings value)
};

// A rolled property: which stat, on whom, and the value.
struct Property {
	Target target = Target::Monsters;
	std::string stat;
	int value = 0;
	friend bool operator==(const Property &a, const Property &b)
	{
		return a.target == b.target && a.stat == b.stat && a.value == b.value;
	}
};

// The small pool used when mapstats.txt is missing. Stat names are itemstatcost names.
const std::vector<PoolRow> &builtInPool();

// The map's properties.
//  - ItemStats on and itemStats non-empty: those stats are used as they are, in order, and nothing is drawn.
//    (ItemStats on but the item has no stats: falls back to a normal draw.)
//  - Otherwise N = map.properties if set, else settings.properties (below 0 counts as 0). N rows are drawn
//    without replacement by weight from `pool` (the built-in pool when `pool` is empty). Two rows with the same
//    Target and Stat are the same property, so drawing one removes both. If fewer than N distinct properties
//    exist, all of them are drawn. Each drawn row then rolls a value uniformly in [Min, Max] (Max below Min gives Min).
//    Min and Max are clamped to +-kMaxBound and Weight to kMaxWeightLimit (a Weight of 0 or below is never drawn).
std::vector<Property> drawMapStats(uint64_t mapSeed, const MapRow &map, const Settings &settings,
	const std::vector<PoolRow> &pool, const std::vector<Property> &itemStats = {});

} // namespace cartographer::stats
