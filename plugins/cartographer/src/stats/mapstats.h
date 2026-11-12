// Cartographer map stats: draws a map's random properties. Pure integer code on plain structs, no engine headers and
// no table parsing (the loader fills these structs). Deterministic from the map seed, using the layout's own PCG32.
#pragma once
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
	int weight = 1; // rows with weight <= 0 are never drawn
};

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
std::vector<Property> drawMapStats(uint64_t mapSeed, const MapRow &map, const Settings &settings,
	const std::vector<PoolRow> &pool, const std::vector<Property> &itemStats = {});

} // namespace cartographer::stats
