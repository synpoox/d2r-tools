#include "mapstats.h"

#include <cstddef>

#include "../layout/rng.h"

namespace cartographer::stats {

namespace {

// Keeps this stream apart from the layout's stream for the same map seed.
constexpr uint64_t kStatsSalt = 0x4D41505354415453ull; // "MAPSTATS"

// Min/Max are clamped so Pcg32::range's hi - lo + 1 cannot overflow int; weights are clamped so the
// total of any realistic pool (under 4 million rows) fits the 32 bits below() takes.
constexpr int kBound = 500000000;
constexpr int kMaxWeight = 1000;

int clampInt(int v, int lo, int hi)
{
	return v < lo ? lo : (v > hi ? hi : v);
}

uint64_t weightOf(const PoolRow &r)
{
	return r.weight > 0 ? static_cast<uint64_t>(clampInt(r.weight, 0, kMaxWeight)) : 0;
}

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
		row(Target::Monsters, "item_maxhp_percent", 20, 60, 3), // monster life
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
	for (size_t i = 0; i < rows.size(); ++i) available[i] = weightOf(rows[i]) > 0;

	std::vector<Property> out;
	for (int k = 0; k < n; ++k) {
		uint64_t total = 0;
		for (size_t i = 0; i < rows.size(); ++i)
			if (available[i]) total += weightOf(rows[i]);
		if (total == 0) break;

		uint32_t pick = rng.below(static_cast<uint32_t>(total));
		size_t chosen = 0;
		for (size_t i = 0; i < rows.size(); ++i) {
			if (!available[i]) continue;
			chosen = i; // pick < total, so a row always matches before the scan ends
			const uint64_t w = weightOf(rows[i]);
			if (pick < w) break;
			pick -= static_cast<uint32_t>(w);
		}

		Property p;
		p.target = rows[chosen].target;
		p.stat = rows[chosen].stat;
		p.value = rng.range(clampInt(rows[chosen].min, -kBound, kBound), clampInt(rows[chosen].max, -kBound, kBound));
		out.push_back(p);

		for (size_t i = 0; i < rows.size(); ++i)
			if (rows[i].target == p.target && rows[i].stat == p.stat) available[i] = false;
	}
	return out;
}

} // namespace cartographer::stats
