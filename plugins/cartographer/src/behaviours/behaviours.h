// Map drop behaviours (issue #538, pieces D6 and D7 of #536): what a map does to the monsters that die in it and to the items they drop,
// whatever mod uses it. Pure C++, no engine code: the chance rolls, the picks and the test spec. The engine glue is engine/map_behaviours.cpp
// and engine/event_6b.cpp; the values come per map level through SetMapBehaviours (engine/map_behaviours.h).
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

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

// --- Combat behaviours (issue #540, pieces D8 and D9 of #536). Keys the stat router sums per opened map and the engine reads with
// StatApplyMapRouting(slotLevel).behaviour(key[, param]): extra_groups (param = a monstats Id), lifesteal_pct, phys_as_<element>.
inline constexpr const char* kKeyExtraGroups = "extra_groups";
inline constexpr const char* kKeyLifesteal = "lifesteal_pct";
inline constexpr int kMaxExtraGroups = 13;   // the region record holds 13 monster groups
inline constexpr int kMaxLifestealPct = 1000;
inline constexpr int kMaxLifestealBlock = 0x1FFFFFF;  // the game shifts the block's leech by 6 bits, so it stays below 2^25

// N extra groups cut to 0..kMaxExtraGroups.
int clampExtraGroups(int n);

// The leech of a damage block after a hit of `dealt` (1/256 life, as the game's block holds it): `current` plus pct percent of `dealt`, at most
// kMaxLifestealBlock; `current` is never lowered. A `dealt` or `pct` of 0 or less changes nothing.
int lifestealBlock(int current, int dealt, int pct);

enum class Element { Fire, Lightning, Cold, Magic, Poison };

// The behaviour key suffix: "fire", "ltng", "cold", "mag", "pois"; `out` is set and true, or false for any other name.
bool elementFromName(const std::string& name, Element* out);

struct StatValue {
	int stat = 0;
	int value = 0;
};

// The stats that add `pct` percent of a monster's physical damage (`minDmg`, `maxDmg`: stats 21 and 22) as `element` damage. Fire, lightning and
// magic: min and max (48/49, 50/51, 52/53). Cold: 54/55 and the length 56. Poison: 57/58 are damage per frame, so the percent is spread over the
// length (59): value * 256 / length. Empty when pct or the damage is 0 or less. The percent is cut to kMaxPhysAsPct.
inline constexpr int kMaxPhysAsPct = 1000;
inline constexpr int kColdLength = 50;     // frames
inline constexpr int kPoisonLength = 100;  // frames
std::vector<StatValue> physAsStats(Element element, int pct, int minDmg, int maxDmg);

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

// The test routing of CARTOGRAPHER_TEST_ROUTING (a game check only; the route table's own keys, set without an item): `;`-separated
// `<key>[:<param>]=<value>`, e.g. `extra_groups:quillrat1=3;lifesteal_pct=50;phys_as_fire=50`. The value is a whole number above 0.
struct TestRoute {
	std::string key, param;
	int value = 0;
};
bool parseTestRouting(const std::string& spec, std::vector<TestRoute>* out, std::string* why);

// The test spec of CARTOGRAPHER_TEST_BEHAVIOURS (a game check only): `;`-separated `tc=<name>:<pct>`, `bonus=<pct>`, `eth=<pct>`, `sock=<pct>`,
// `boss=<n>`. False (with `why`) on anything else; `out` is then unchanged.
bool parseSpec(const std::string& spec, MapBehaviours* out, std::string* why);

}  // namespace cartographer::behaviours
