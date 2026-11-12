#include "mapstats.h"

#include <cstddef>
#include <string>

#include "../layout/rng.h"

namespace cartographer::stats {

namespace {

// Keeps this stream apart from the layout's stream for the same map seed.
constexpr uint64_t kStatsSalt = 0x4D41505354415453ull; // "MAPSTATS"

// Min/Max are clamped so Pcg32::range's hi - lo + 1 cannot overflow int. Weights are clamped to kMaxWeight (high
// enough not to distort normal input) and summed in 64 bits, so no pool size can overflow the total.
constexpr int kBound = kMaxBound;
constexpr int kMaxWeight = kMaxWeightLimit;

int clampInt(int v, int lo, int hi)
{
	return v < lo ? lo : (v > hi ? hi : v);
}

uint64_t weightOf(const PoolRow &r)
{
	return r.weight > 0 ? static_cast<uint64_t>(clampInt(r.weight, 0, kMaxWeight)) : 0;
}

// Uniform in [0, n) for a 64-bit n. Below 2^32 this is exactly Pcg32::below (same stream); above it, two 32-bit
// words are combined and rejected the same way, so there is no modulo bias.
uint64_t below64(layout::Pcg32 &rng, uint64_t n)
{
	if (n <= 0xFFFFFFFFull) return rng.below(static_cast<uint32_t>(n));
	const uint64_t threshold = (0ull - n) % n;
	for (;;) {
		const uint64_t r = (static_cast<uint64_t>(rng.next()) << 32) | rng.next();
		if (r >= threshold) return r % n;
	}
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

std::vector<PoolIssue> checkPool(const std::vector<PoolRow> &pool)
{
	std::vector<PoolIssue> out;
	for (size_t i = 0; i < pool.size(); ++i) {
		const PoolRow &r = pool[i];
		const std::string who = "row " + std::to_string(i + 1) + " (" + r.stat + ")";
		if (r.weight < 0) {
			out.push_back({true, i, who + ": Weight is below 0. Use 0 to switch the row off. The row is skipped."});
			continue;
		}
		if (r.weight > kMaxWeight)
			out.push_back({false, i, who + ": Weight " + std::to_string(r.weight) + " is above " +
				std::to_string(kMaxWeight) + " and is used as " + std::to_string(kMaxWeight) + "."});
		if (r.min < -kBound || r.min > kBound || r.max < -kBound || r.max > kBound)
			out.push_back({false, i, who + ": Min and Max are limited to " + std::to_string(-kBound) + " to " +
				std::to_string(kBound) + "; larger values are cut to that."});
		if (r.max < r.min)
			out.push_back({false, i, who + ": Max is below Min, so Min is always rolled."});
	}
	return out;
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

		uint64_t pick = below64(rng, total);
		size_t chosen = 0;
		for (size_t i = 0; i < rows.size(); ++i) {
			if (!available[i]) continue;
			chosen = i; // pick < total, so a row always matches before the scan ends
			const uint64_t w = weightOf(rows[i]);
			if (pick < w) break;
			pick -= w;
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
