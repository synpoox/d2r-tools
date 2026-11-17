// Map drop behaviours (issue #538, pieces D6 and D7 of #536): what a map does to the monsters that die in it and to the items they drop,
// whatever mod uses it. Pure C++, no engine code: the chance rolls, the picks and the test spec. The engine glue is engine/map_behaviours.cpp
// and engine/event_6b.cpp; the values come per map level through SetMapBehaviours (engine/map_behaviours.h).
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

#include "statplan.h"

namespace cartographer::behaviours {

// The behaviour keys the stat router sums per opened map: drop_tc (param = the TC name), drop_bonus, drop_ethereal, drop_socketed, extra_boss.
struct MapBehaviours {
	int dropBonusPct = 0;  // chance of one extra drop of the monster's own treasure class; every full 100 is one sure extra drop
	int etherealPct = 0;   // chance a map drop is ethereal (the game still only makes an ethereal-capable item ethereal)
	int socketedPct = 0;   // chance a socketable map drop gets sockets
	int extraBoss = 0;     // extra spawns of the map's Boss
	std::vector<std::pair<std::string, int>> dropTcs;  // treasure class name, chance in percent of one drop from it per kill
};

inline constexpr int kMaxExtraBoss = 9;

// The behaviours an opened map's item routed (issue #539): drop_bonus, drop_ethereal and drop_socketed are the summed values (never below 0),
// extra_boss is cut to 0..kMaxExtraBoss, and each drop_tc route with a Param (the treasure class name) and a positive sum is one dropTcs entry.
MapBehaviours fromRouting(const cartographer::statplan::MapRouting& routing);

// True when the map changes anything.
bool any(const MapBehaviours& b);

// The generator the rolls draw from: seeded, so the tests are exact. xorshift64*, never zero.
class Rng {
public:
	explicit Rng(uint64_t seed);
	uint32_t next();
	uint32_t below(uint32_t n);  // 0..n-1; 0 for n == 0

private:
	uint64_t s_;
};

// pct <= 0 never, pct >= 100 always, else next() % 100 < pct.
bool chance(int pct, Rng& rng);

// One extra drop per full 100, and one more with the chance of the rest: 50 -> 0 or 1, 250 -> 2 or 3.
int extraDrops(int bonusPct, Rng& rng);

// The indexes of the dropTcs that hit (each rolled once, in order).
std::vector<size_t> hitTcs(const std::vector<std::pair<std::string, int>>& tcs, Rng& rng);

// Sockets for a drop that can take them: 1 or 2 (the game cuts it to what the item's type allows).
int socketCount(Rng& rng);

// The test spec of CARTOGRAPHER_TEST_BEHAVIOURS (a game check only): `;`-separated `tc=<name>:<pct>`, `bonus=<pct>`, `eth=<pct>`, `sock=<pct>`,
// `boss=<n>`. False (with `why`) on anything else; `out` is then unchanged.
bool parseSpec(const std::string& spec, MapBehaviours* out, std::string* why);

}  // namespace cartographer::behaviours
