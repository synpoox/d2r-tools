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
// The game's state sender copies at most 16 entries of a list (cartographer/07-map-stats-parts.md, static), so more player stats than
// this would show on the server and not on the sheet.
constexpr size_t kMaxPlayerStats = 16;

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

// Why `stat` (itemstatcost name and id) must not be put on `target`, in plain English, or "" when it may be. The game
// reads these as a total (base plus every list) and writes the result back as the base value, so a list value would be
// baked into the saved character: statpts, newskills, gold and goldbank (spending), hitpoints, mana and stamina
// (current values, rewritten every tick), level and experience. The maxima (maxhp, maxmana, maxstamina) and the
// attributes are item stats and are fine. `level` is also refused on monsters: the game writes it itself during
// monster setup. Matched by name or by the engine's fixed id (4, 5, 6, 8, 10, 12 to 15), so a renamed row is still
// caught. The vanilla itemstatcost has no other stat with this behaviour (lastexp and nextexp are table look-ups).
std::string refusedReason(Target target, std::string_view stat, int id);

// Resolves the drawn properties. A name that is not in `ids`, a value of 0, an id outside 0..kMaxStatId and a stat
// refusedReason() names are skipped, never guessed. With `ids.loaded` false nothing can be resolved and every property
// is skipped. With `playerStats` false every player property is skipped (the PlayerStats setting is off).
Plan buildPlan(const std::vector<Property> &properties, const StatIds &ids, bool playerStats = true);

// One stat read off the map item (ItemStats=1): the game's stat id, layer and value.
struct ItemStat {
	int stat = 0;
	int layer = 0;
	int value = 0;
};

struct ItemProperties {
	std::vector<Property> properties; // in the item's order
	std::vector<std::string> dropped; // one plain-English line for each item stat that was left out
};

// Turns the map item's stats into the map's properties, in the item's order. Each stat is matched by id to the first
// `pool` row whose Stat has that id in `ids`; that row gives the target (monsters or players), so the pool says which
// item stats a map may carry. A stat with a layer, a value of 0, or no pool row is dropped and listed in `dropped`.
// With `ids.loaded` false nothing can be matched: every stat is dropped.
ItemProperties itemStatsToProperties(const std::vector<ItemStat> &stats, const std::vector<PoolRow> &pool, const StatIds &ids);

// Warnings for pool rows whose Stat is not in itemstatcost, or that buildPlan would refuse (row is the 0-based index
// into the pool). Empty when the table was not read (fail open: unchecked, not wrong).
std::vector<PoolIssue> checkPoolStats(const std::vector<PoolRow> &pool, const StatIds &ids);

// One line for the log: "monsters: item_maxhp_percent(76)=+30; players: fireresist(39)=-20".
std::string describe(const Plan &plan);

// A map seed for a map that is being opened. Mixes the clock, the opening player's id and a per-game counter.
uint64_t mixSeed(uint64_t clock, uint32_t playerId, uint32_t openings);

// What to do with a player's stat list on this tick.
enum class PlayerAction { None, Apply, Remove };

// settingOn: PlayerStats is 1; with it off a list already on the player is removed on the next tick.
// planHasPlayers: the active map has player entries. inMap: the player stands in the slot level. hasList: the plugin
// put a list on this player and has not removed it. A list is removed whenever the map no longer applies, whatever
// the reason (left the level, map closed, plan replaced).
PlayerAction decidePlayer(bool planHasPlayers, bool inMap, bool hasList, bool settingOn);

// What to do with a list the plugin put on a player when it has to come off.
enum class RemoveAction { Forget, StateOffDetachFree, DetachFree };

// onChain: the list is still on the unit's list chain. foundByState: the game's finder returns this very list for the state
// the plugin gave it (and the state and the state-bit call are usable). ownedByPlugin: the list's own fields say it is ours
// (ownsList; only read once the list is known to be on the chain, so its memory is live). A list the unit does not hold, or
// one on the chain whose fields are not ours (the game freed ours and reused the address), is only forgotten: never
// detached, freed or written. Only an owned list on the chain comes off, so the map's player stats never follow the player
// into a vanilla level; the state bit is cleared only when the finder also confirms the list.
RemoveAction decideRemoval(bool onChain, bool foundByState, bool ownedByPlugin);

// Whether a list on the chain is the plugin's: its flags (list +0x1c) and state (list +0x20) equal the values read right after
// the plugin attached it and gave it the state (the attach is engine code that may set bits in the flags, so they are
// compared with what was read then, not with 0). A game list that reused the address has other values. expectedState is the
// state the plugin gave the list; 0 or less never matches.
bool ownsList(uint32_t listFlags, uint32_t listState, uint32_t storedFlags, uint32_t storedState, int expectedState);

// Whether a monster that just spawned in `levelId` gets the monster stats.
bool monsterGetsStats(bool planActive, int levelId, int slotLevel);

// The lists the plugin has put on players: unit id -> list address and the state it was given. Fixed size, so a hook never allocates.
class PlayerLists {
public:
	static constexpr size_t kMax = 16;
	// The list for `unit`, or 0.
	uintptr_t find(uint32_t unit) const;
	// The state the plugin gave the list of `unit` (never read back from the list), or 0.
	int stateOf(uint32_t unit) const;
	// The list's +0x1c and +0x20 as read right after the state was set (0 when unknown).
	uint32_t flagsOf(uint32_t unit) const;
	uint32_t rawStateOf(uint32_t unit) const;
	// False when the table is full or `list` is 0 (the caller then does not apply).
	bool add(uint32_t unit, uintptr_t list, int state = 0, uint32_t flags = 0, uint32_t rawState = 0);
	void remove(uint32_t unit);
	void clear();
	size_t size() const;
	bool empty() const { return size() == 0; }

private:
	struct Slot {
		uint32_t unit = 0;
		uintptr_t list = 0;
		int state = 0;
		uint32_t flags = 0;
		uint32_t rawState = 0;
	};
	Slot slots_[kMax];
};

// --- The state that carries the player's list to the clients (issue #104, cartographer/07-map-stats-parts.md) -----------------
// The game sends a stat list only as part of a state, so the player's list is given a state. The compiled states row
// (build 93847) is 0x44 bytes: i16 overlay1-4 (+2, +4, +6, +8), castoverlay (+0xa), removerlay (+0xc), pgsvoverlay (+0xe);
// the flag field (+0x10..+0x17, bit k at byte +0x10+k/8 mask 1<<(k%8); bit 0 nosend, bit 17 disguise); i16 stat (+0x18),
// setfunc (+0x1a), remfunc (+0x1c); u16 group (+0x1e); i16 skill (+0x38), missile (+0x3a); byte canstack (+0x3c). A
// name lookup that finds nothing is stored as -1 (0xffff), the other empty columns as 0, itemtrans as 0xff (see statplan.cpp). The skills row is 0x2ec bytes with i16 aurastate (+0xa0), auratargetstate
// (+0xa2), passivestate (+0xb4), state1-3 (+0x2d2, +0x2d4, +0x2d6).
constexpr uint32_t kStateRowSize = 0x44;
constexpr uint32_t kSkillRowSize = 0x2ec;

// Empty when the SDK view of a table is the game's table: the row size is the wanted one, the pointers are equal and not
// null, and the 64-bit count equals the SDK's 32-bit one. Else why not.
std::string checkTableView(const void *viewRows, uint32_t viewCount, uint32_t viewRowSize, const void *dtRows, uint64_t dtCount,
                           uint32_t wantRowSize);

// States the game or the mod uses by a fixed id (a scan of D2R.exe call sites that pass a constant state, plus 11 and 92; an under-count).
bool isStaticState(int state);

struct StatePick {
	int state = -1; // -1: none, and player stats stay off (there is no fallback to a row that has something set)
};

// The lowest state id that nothing uses: above 0 and below `count`; every column of the row empty (the whole flag field
// zero, no overlay, stat, skill, missile, function, group, colour, light, sound, item, graphics or vfx entry; see
// candidate() in statplan.cpp); not named by any skills row; not in the static list. `skills` may be null only with
// skillCount 0 (then no skills row names a state). Rows are raw bytes.
StatePick pickPlayerState(const uint8_t *states, uint32_t count, const uint8_t *skills, uint32_t skillCount);

// Why a table gives no pick, for the log and 'cartographer check': one summary line (how many rows have every column empty and how many of
// those are static or named by a skill), then up to `maxRows` of the rows closest to qualifying (not static, not named by a skill; fewest
// non-empty columns first), each with its first non-empty column, its offset and its value. Empty for a null table.
std::vector<std::string> explainNoPick(const uint8_t *states, uint32_t count, const uint8_t *skills, uint32_t skillCount, size_t maxRows = 4);

} // namespace cartographer::statplan
