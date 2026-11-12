// Cartographer apply plan: turns the drawn map properties into the stat entries the engine part applies, and decides
// when the player's list goes on and off. Pure C++ on plain structs (no engine, no SDK); the engine part only carries
// the plan out. Stat ids come from the game's itemstatcost.txt, never from this code.
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "mapstats.h"

namespace cartographer::statplan {

using stats::PoolIssue;
using stats::PoolRow;
using stats::Property;
using stats::Target;

// Highest stat id the engine part will pass to the game (itemstatcost has a few hundred rows).
constexpr int kMaxStatId = 2047;

// itemstatcost.txt: stat name -> id.
struct StatIds {
	std::vector<std::pair<std::string, int>> byName;
	bool loaded = false; // false: the table could not be read, so names are not checked and nothing can be applied

	// The id of `name` (exact), or -1.
	int find(std::string_view name) const;
	// The listed name closest to `name` (for a "did you mean" message), or "" when nothing is close.
	std::string suggest(std::string_view name) const;
};

// Reads itemstatcost.txt text. The id is the `*ID` column when there is one, else the row number (0-based, as the
// game counts rows). Rows without a Stat name are skipped. `loaded` is true when a Stat column was found.
StatIds parseItemStatCost(std::string_view text);

// One stat on one target, ready for SetListStat.
struct Entry {
	std::string name;
	int stat = 0;
	int value = 0;
};

// A drawn property that cannot be applied, and why (plain English).
struct Skipped {
	Target target = Target::Monsters;
	std::string stat;
	std::string why;
};

struct Plan {
	std::vector<Entry> monsters;
	std::vector<Entry> players;
	std::vector<Skipped> skipped;
	bool empty() const { return monsters.empty() && players.empty(); }
};

// Resolves the drawn properties. A name that is not in `ids`, a value of 0 and an id outside 0..kMaxStatId are
// skipped, never guessed. With `ids.loaded` false nothing can be resolved and every property is skipped.
Plan buildPlan(const std::vector<Property> &properties, const StatIds &ids);

// Warnings for pool rows whose Stat is not in itemstatcost (row is the 0-based index into the pool). Empty when the
// table was not read (fail open: unchecked, not wrong).
std::vector<PoolIssue> checkPoolStats(const std::vector<PoolRow> &pool, const StatIds &ids);

// One line for the log: "monsters: item_maxhp_percent(76)=+30; players: fireresist(39)=-20".
std::string describe(const Plan &plan);

// A map seed for a map that is being opened. Mixes the clock, the opening player's id and a per-game counter.
uint64_t mixSeed(uint64_t clock, uint32_t playerId, uint32_t openings);

// What to do with a player's stat list on this tick.
enum class PlayerAction { None, Apply, Remove };

// planHasPlayers: the active map has player entries. inMap: the player stands in the slot level. hasList: the plugin
// put a list on this player and has not removed it. A list is removed whenever the map no longer applies, whatever
// the reason (left the level, map closed, plan replaced).
PlayerAction decidePlayer(bool planHasPlayers, bool inMap, bool hasList);

// Whether a monster that just spawned in `levelId` gets the monster stats.
bool monsterGetsStats(bool planActive, int levelId, int slotLevel);

// The lists the plugin has put on players: unit id -> list address. Fixed size, so a hook never allocates.
class PlayerLists {
public:
	static constexpr size_t kMax = 16;
	// The list for `unit`, or 0.
	uintptr_t find(uint32_t unit) const;
	// False when the table is full or `list` is 0 (the caller then does not apply).
	bool add(uint32_t unit, uintptr_t list);
	void remove(uint32_t unit);
	void clear();
	size_t size() const;
	bool empty() const { return size() == 0; }

private:
	struct Slot {
		uint32_t unit = 0;
		uintptr_t list = 0;
	};
	Slot slots_[kMax];
};

} // namespace cartographer::statplan
