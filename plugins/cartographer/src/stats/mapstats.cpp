#include "mapstats.h"

#include <cstddef>

#include "../layout/rng.h"

namespace cartographer::stats {

namespace {

// Keeps this stream apart from the layout's stream for the same map seed.
constexpr uint64_t kStatsSalt = 0x4D41505354415453ull; // "MAPSTATS"

PoolRow row(Target t, const char *stat, int mn, int mx, int w)
{
	PoolRow r;
	r.target = t;
	r.stat = stat;
	r.min = mn;
	r.max = mx;
	r.weight = w;
	return r;
}

} // namespace

const std::vector<PoolRow> &builtInPool()
{
	static const std::vector<PoolRow> pool = {
		row(Target::Monsters, "maxhp%", 20, 60, 3), // monster life
		row(Target::Monsters, "damagepercent", 20, 60, 3), // monster damage
		row(Target::Monsters, "fireresist", 10, 40, 2), // monster resistances
		row(Target::Monsters, "coldresist", 10, 40, 2),
		row(Target::Monsters, "lightresist", 10, 40, 2),
		row(Target::Monsters, "poisonresist", 10, 40, 2),
		row(Target::Players, "fireresist", -40, -10, 2), // player resistances
		row(Target::Players, "coldresist", -40, -10, 2),
		row(Target::Players, "lightresist", -40, -10, 2),
		row(Target::Players, "poisonresist", -40, -10, 2),
		row(Target::Players, "hpregen", -30, -10, 1), // player life regen
		row(Target::Players, "item_slow", 10, 30, 1), // player curse effect: slower
	};
	return pool;
}

std::vector<Property> drawMapStats(uint64_t mapSeed, const MapRow &map, const Settings &settings,
	const std::vector<PoolRow> &pool, const std::vector<Property> &itemStats)
{
	if (settings.itemStats && !itemStats.empty()) return itemStats;

	int n = map.properties >= 0 ? map.properties : settings.properties;
	if (n < 0) n = 0;

	const std::vector<PoolRow> &rows = pool.empty() ? builtInPool() : pool;
	layout::Pcg32 rng(mapSeed ^ kStatsSalt);

	std::vector<bool> available(rows.size());
	for (size_t i = 0; i < rows.size(); ++i) available[i] = rows[i].weight > 0;

	std::vector<Property> out;
	for (int k = 0; k < n; ++k) {
		uint64_t total = 0;
		for (size_t i = 0; i < rows.size(); ++i)
			if (available[i]) total += static_cast<uint64_t>(rows[i].weight);
		if (total == 0) break;

		// below() takes 32 bits; clamp huge totals (weights are small in practice).
		const uint32_t cap = total > 0xFFFFFFFFull ? 0xFFFFFFFFu : static_cast<uint32_t>(total);
		uint32_t pick = rng.below(cap);
		size_t chosen = rows.size();
		for (size_t i = 0; i < rows.size(); ++i) {
			if (!available[i]) continue;
			const uint64_t w = static_cast<uint64_t>(rows[i].weight);
			if (pick < w) {
				chosen = i;
				break;
			}
			pick -= static_cast<uint32_t>(w);
		}
		if (chosen == rows.size()) break; // only reachable if the total was clamped

		Property p;
		p.target = rows[chosen].target;
		p.stat = rows[chosen].stat;
		p.value = rng.range(rows[chosen].min, rows[chosen].max);
		out.push_back(p);

		for (size_t i = 0; i < rows.size(); ++i)
			if (rows[i].target == p.target && rows[i].stat == p.stat) available[i] = false;
	}
	return out;
}

} // namespace cartographer::stats
