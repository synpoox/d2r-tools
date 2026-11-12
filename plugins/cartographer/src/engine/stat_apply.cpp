#include "engine/stat_apply.h"

#include "core/part_signatures.h"
#include "core/text.h"
#include "engine/scan.h"
#include "log.h"
#include "hookguard.h"
#include "statplan.h"

#include <windows.h>

#include <atomic>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <mutex>
#include <string>

namespace carto {

namespace {

namespace sp = cartographer::statplan;

// --- Engine facts. Build 93847; each is recorded with its evidence in docs/reverse-engineering/cartographer.md section 6.
constexpr int UnitTypeOffset = 0x00;       // int32: 0 player, 1 monster
constexpr int UnitIdOffset = 0x08;         // uint32 (what the game's own unit id helper 0x34a330 returns)
constexpr int UnitDataFlagOffset = 0x1bd;  // uint8 (what the game's own data flag helper 0x34a0e0 returns)
constexpr int UnitOwnerListOffset = 0x88;  // the unit's own stat list (the engine's 0x34b870 returns it): the chain hangs on it, not on the unit
constexpr int ListHeadOffset = 0x90;       // on that owner list: the chain head, or the second head at 0x98 (bit 13 lists); next at 0x68
constexpr int ListHeadAltOffset = 0x98;
constexpr int ListStateOffset = 0x20;       // the state SetListState writes (0x2f72f0); the finder matches it
constexpr int ListFlagsOffset = 0x1c;      // the engine walks +0x90/+0x98 only when this int is negative (0x2f5bdd, 0x2f6e93)
constexpr int ListNextOffset = 0x68;
constexpr int MaxChain = 512;              // a chain longer than this is not walked further
constexpr int32_t UnitPlayer = 0;
constexpr int32_t UnitMonster = 1;
constexpr int SpawnRoomOffset = 0x08;      // the monster setup's first argument: +8 is the room (the spawn block)
constexpr int32_t StatLevel = 12;          // 0xc: the monster level stat
constexpr int32_t StatMaxLife = 7;         // maxhp, in 1/256 life
constexpr int MonsterProbes = 6;           // monsters logged in full per game
constexpr int MaxEntries = 32;             // stats read back per probe
constexpr int DtStatesRowsOffset = 0x290;  // on the data tables base: the compiled states rows pointer; the 64-bit count is at +0x298
constexpr int DtStatesCountOffset = 0x298;
constexpr uint32_t DataFlagLimit = 4;      // the base accessor takes a data flag below 4

struct Game;
struct Unit;

using NewStatListFn = void* (*)(int32_t flags, int32_t expire, int32_t type, int32_t id) noexcept;
using SetListStatFn = void (*)(uint32_t dataFlag, void* list, int32_t stat, int32_t value, int32_t layer) noexcept;
using AttachStatListFn = void (*)(Unit*, void* list, int32_t one) noexcept;
using DetachStatListFn = void (*)(Unit*, void* list) noexcept;
using FreeStatListFn = void (*)(uint32_t dataFlag, void* list) noexcept;
using MonsterSetupFn = uint64_t (*)(void* params, Unit* monster) noexcept;
using UnitRoomFn = void* (*)(Unit*) noexcept;
using RoomLevelFn = int32_t (*)(void* room) noexcept;
using UnitStatFn = int32_t (*)(Unit*, int32_t stat, int32_t layer) noexcept;
using SetListStateFn = void (*)(void* list, int32_t state) noexcept;
using SetStateBitFn = void (*)(Unit*, int32_t state, int32_t on) noexcept;
using StateOnFn = int32_t (*)(Unit*, int32_t state) noexcept;
using FindStateListFn = void* (*)(Unit*, int32_t state) noexcept;
using DataTablesBaseFn = uint8_t* (*)(uint32_t dataFlag) noexcept;

enum PartId {
	NewStatList, SetListStat, AttachStatList, DetachStatList, FreeStatList, MonsterSetup, UnitRoom, RoomLevelId, UnitStat,
	SetListState, SetStateBit, StateOn, FindStateList, StatDataTablesBase, PartCount
};

// UnitRoom, RoomLevelId and UnitStat are the portal's parts, found again here so this half stands alone.
Part g_parts[PartCount] = {
	{"NewStatList", sig::NewStatList, 0, false, nullptr, {}, {}},
	{"SetListStat", sig::SetListStat, 0, false, nullptr, {}, {}},
	{"AttachStatList", sig::AttachStatList, 0, false, nullptr, {}, {}},
	{"DetachStatList", sig::DetachStatList, 0, false, nullptr, {}, {}},
	{"FreeStatList", sig::FreeStatList, 0, false, nullptr, {}, {}},
	{"MonsterSetup", sig::MonsterSetup, 14, false, nullptr, {}, {}},
	{"StatUnitRoom", sig::UnitRoom, 0, false, nullptr, {}, {}},
	{"StatRoomLevelId", sig::RoomLevelId, 0, false, nullptr, {}, {}},
	{"StatUnitStat", sig::UnitStat, 0, false, nullptr, {}, {}},
	{"SetListState", sig::SetListState, 0, false, nullptr, {}, {}},
	{"SetStateBit", sig::SetStateBit, 0, false, nullptr, {}, {}},
	{"StateOn", sig::StateOn, 0, false, nullptr, {}, {}},
	{"FindStateList", sig::FindStateList, 0, false, nullptr, {}, {}},
	{"StatDataTablesBase", sig::DataTablesBase, 0, false, nullptr, {}, {}},
};

std::mutex g_lock;
std::shared_ptr<const sp::Plan> g_plan;  // the active map's plan; null: no map
int g_slotLevel = -1;
sp::PlayerLists g_lists;                 // lists put on players; guarded by g_lock
std::atomic<bool> g_busy{false};         // a plan is active or a list is out: the hooks look at nothing else when false
std::atomic<bool> g_monstersOn{false};   // the monster hook is in
std::atomic<bool> g_playersOn{false};    // the player half can apply (parts found and the portal's tick hook is in)
std::atomic<bool> g_playerSetting{false};  // PlayerStats=1 in cartographer.txt; off by default: nothing is attached to the player
uint64_t g_generation = 0;               // counts games left; guarded by g_lock. A list made in an earlier game is never tracked
std::atomic<int> g_monsterCount{0};      // monsters given a list in this map
std::atomic<int> g_monsterFailed{0};
std::atomic<int> g_playerCount{0};
std::atomic<bool> g_chainLogged{false};  // the first player's list chain was logged this game
std::atomic<bool> g_stateRefused{false};  // the state for the player's list was refused this map: logged once, nothing more is tried
const D2RL::PluginContext* g_context = nullptr;
const D2RL::DataTableService* g_dataTables = nullptr;
MonsterSetupFn g_original = nullptr;

template <typename Fn>
auto Call(PartId id) noexcept -> Fn {
	return reinterpret_cast<Fn>(const_cast<uint8_t*>(g_parts[id].address));
}

auto On(PartId id) noexcept -> bool {
	return g_parts[id].address != nullptr && g_parts[id].status.on;
}

void UpdateBusy() noexcept {  // g_lock must be held
	g_busy = g_plan != nullptr || !g_lists.empty();
}

// --- The game's own stat lists ----------------------------------------------------------------------------------------

auto UnitLevelId(Unit* unit) noexcept -> int32_t {
	void* room = Call<UnitRoomFn>(UnitRoom)(unit);
	return room != nullptr ? Call<RoomLevelFn>(RoomLevelId)(room) : -1;
}

// New list on `unit` holding `entries` (layer 0). The recipe is the engine's own (cartographer.md section 6, Q1): NewStatList,
// SetListStat for each stat, AttachStatList. No state is set here; a player's list gets one after (SetPlayerState).
auto MakeList(Unit* unit, int32_t type, const std::vector<sp::Entry>& entries) noexcept -> void* {
	const int32_t id = At<int32_t>(unit, UnitIdOffset);
	void* list = Call<NewStatListFn>(NewStatList)(0, 0, type, id);
	if (list == nullptr) {
		return nullptr;
	}
	const uint32_t dataFlag = At<uint8_t>(unit, UnitDataFlagOffset);
	for (const sp::Entry& e : entries) {
		Call<SetListStatFn>(SetListStat)(dataFlag, list, e.stat, e.value, 0);
	}
	Call<AttachStatListFn>(AttachStatList)(unit, list, 1);
	return list;
}

// A pointer the chain walk may read: committed, readable memory. The first live run (issue #80) faulted because the walk
// started at the unit instead of its owner list (+0x88), so every step of the walk is checked and stops at anything else.
auto Readable(const void* p, size_t n) noexcept -> bool {
	if (reinterpret_cast<uintptr_t>(p) < 0x10000) {
		return false;
	}
	MEMORY_BASIC_INFORMATION mbi{};
	if (VirtualQuery(p, &mbi, sizeof(mbi)) == 0 || mbi.State != MEM_COMMIT || (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)) != 0) {
		return false;
	}
	return reinterpret_cast<uintptr_t>(p) + n <= reinterpret_cast<uintptr_t>(mbi.BaseAddress) + mbi.RegionSize;
}

// How many lists are on the unit's chain, and whether `find` is one of them. A link that is not readable ends the walk.
auto Chain(Unit* unit, const void* find, bool* found) noexcept -> int {
	int n = 0;
	*found = false;
	const uint8_t* owner = At<const uint8_t*>(unit, UnitOwnerListOffset);
	if (!Readable(owner, ListHeadAltOffset + sizeof(void*))) {
		return 0;
	}
	if (At<int32_t>(owner, ListFlagsOffset) >= 0) {   // not an extended list: the engine does not read the heads (readable, checked above)
		return 0;
	}
	for (const int head : {ListHeadOffset, ListHeadAltOffset}) {
		const uint8_t* p = At<const uint8_t*>(owner, head);
		for (int i = 0; p != nullptr && i < MaxChain; ++i, ++n) {
			if (!Readable(p + ListNextOffset, sizeof(void*))) {
				break;
			}
			*found = *found || (find != nullptr && p == find);
			p = At<const uint8_t*>(p, ListNextOffset);
		}
	}
	return n;
}

// Reads each entry's stat total on the unit, for a probe line: "76=130 25=100".
auto Totals(Unit* unit, const std::vector<sp::Entry>& entries) noexcept -> std::string {
	std::string out;
	if (!On(UnitStat)) {
		return "(UnitStat part is off)";
	}
	int shown = 0;
	for (const sp::Entry& e : entries) {
		if (++shown > MaxEntries) {
			break;
		}
		char piece[48];
		std::snprintf(piece, sizeof(piece), "%s%d=%d", out.empty() ? "" : " ", e.stat, Call<UnitStatFn>(UnitStat)(unit, e.stat, 0));
		out += piece;
	}
	return out.empty() ? "-" : out;
}

auto Stat(Unit* unit, int32_t stat) noexcept -> int32_t {
	return On(UnitStat) ? Call<UnitStatFn>(UnitStat)(unit, stat, 0) : -1;
}

// --- Monsters -----------------------------------------------------------------------------------------------------------

void ApplyToMonster(void* params, Unit* monster) noexcept {
	std::shared_ptr<const sp::Plan> plan;
	int slot = -1;
	{
		std::lock_guard<std::mutex> lock(g_lock);
		plan = g_plan;
		slot = g_slotLevel;
	}
	if (plan == nullptr || plan->monsters.empty() || monster == nullptr || params == nullptr || At<int32_t>(monster, UnitTypeOffset) != UnitMonster) {
		return;
	}
	void* room = At<void*>(params, SpawnRoomOffset);
	const int32_t levelId = room != nullptr ? Call<RoomLevelFn>(RoomLevelId)(room) : -1;
	if (!sp::monsterGetsStats(true, levelId, slot)) {
		return;
	}
	const int number = g_monsterCount.load() + g_monsterFailed.load();
	const bool probe = number < MonsterProbes;
	int32_t levelBefore = 0, lifeBefore = 0;
	std::string before;
	if (probe) {
		levelBefore = Stat(monster, StatLevel);
		lifeBefore = Stat(monster, StatMaxLife);
		before = Totals(monster, plan->monsters);
	}
	void* list = MakeList(monster, UnitMonster, plan->monsters);
	if (list == nullptr) {
		const int failed = ++g_monsterFailed;
		if (failed == 1 || failed % 100 == 0) {
			plog::Warn("map stats: the game gave no stat list for a monster (%d so far); it spawns without the map's stats", failed);
		}
		return;
	}
	++g_monsterCount;
	if (probe) {
		bool found = false;
		const int chain = Chain(monster, list, &found);
		plog::Info("statprobe: monster #%d id %u class %d level %d: stat %d (level) was %d at the hook, maxhp %d -> %d, totals [%s] -> [%s], list on chain: %s (%d lists)",
		           number + 1, At<uint32_t>(monster, UnitIdOffset), At<int32_t>(params, 0x18), levelId, StatLevel, levelBefore, lifeBefore,
		           Stat(monster, StatMaxLife), before.c_str(), Totals(monster, plan->monsters).c_str(), found ? "yes" : "NO", chain);
	}
}

auto MonsterSetupHook(void* params, Unit* monster) noexcept -> uint64_t {
	if (g_original == nullptr) {
		// Reachable only if the loader patched the call and still returned no original (the install logs it and marks the
		// part off). Nothing to call, so every monster's own setup is skipped until the game is restarted.
		return 0;
	}
	const uint64_t result = g_original(params, monster);
	if (g_busy.load(std::memory_order_relaxed)) {
		try {
			ApplyToMonster(params, monster);
		} catch (...) {
			plog::Error("map stats: applying a monster's stats failed");
		}
	}
	return result;
}

// --- Players ------------------------------------------------------------------------------------------------------------

// The state that carries the player's list to the clients (cartographer.md section 7), or -1 with `why`. The SDK's states table is
// used only after it is shown to be the game's own for this data flag (same rows pointer and count as the data tables base, the
// row size the layout expects); the skills table (the states skills name) needs the expected row size too. Else nothing is guessed.
auto ChoosePlayerState(uint32_t dataFlag, std::string* why) noexcept -> int {
	namespace Tables = D2RL::DataTables;
	if (g_dataTables == nullptr || g_context == nullptr) {
		*why = "D2RLoader has no DataTable service of the expected version";
	} else if (!On(StatDataTablesBase)) {
		*why = "the DataTablesBase part is off";
	} else if (dataFlag >= DataFlagLimit) {
		*why = "the unit's data flag " + std::to_string(dataFlag) + " is not below 4";
	} else {
		const uint8_t* dt = Call<DataTablesBaseFn>(StatDataTablesBase)(dataFlag);
		if (dt == nullptr) {
			*why = "the data tables base is null";
			return -1;
		}
		std::string tried;
		for (uint32_t bank = 1; bank <= 3; ++bank) {
			Tables::TableView states{};
			states.structSize = Tables::TableViewSize;
			const Tables::Result r = g_dataTables->getTable(g_context, static_cast<Tables::Bank>(bank), Tables::TableId::States, &states);
			if (r != Tables::Result::Success || !Tables::HasTableViewField(&states, Tables::TableViewRequiredSize)) {
				tried += (tried.empty() ? "" : "; ") + std::string("bank ") + std::to_string(bank) + ": no view";
				continue;
			}
			const std::string problem = sp::checkTableView(states.rows, states.rowCount, states.rowSize, At<const void*>(dt, DtStatesRowsOffset),
			                                               At<uint64_t>(dt, DtStatesCountOffset), sp::kStateRowSize);
			if (!problem.empty()) {
				tried += (tried.empty() ? "" : "; ") + std::string("bank ") + std::to_string(bank) + ": " + problem;
				continue;
			}
			Tables::TableView skills{};
			skills.structSize = Tables::TableViewSize;
			if (g_dataTables->getTable(g_context, static_cast<Tables::Bank>(bank), Tables::TableId::Skills, &skills) != Tables::Result::Success ||
			    !Tables::HasTableViewField(&skills, Tables::TableViewRequiredSize) || skills.rowSize != sp::kSkillRowSize ||
			    (skills.rows == nullptr && skills.rowCount != 0)) {
				*why = "the skills table (bank " + std::to_string(bank) + ") has no view or not the expected row size, so the states skills use cannot be excluded";
				return -1;
			}
			const sp::StatePick pick = sp::pickPlayerState(static_cast<const uint8_t*>(states.rows), states.rowCount, static_cast<const uint8_t*>(skills.rows), skills.rowCount);
			if (pick.state < 0) {
				for (const std::string& line : sp::explainNoPick(static_cast<const uint8_t*>(states.rows), states.rowCount, static_cast<const uint8_t*>(skills.rows), skills.rowCount)) {
					plog::Warn("statprobe: no state picked, bank %u: %s", bank, line.c_str());
				}
				*why = "no state of the " + std::to_string(states.rowCount) + " in the table has every column empty and is unused; none is picked";
				return -1;
			}
			std::string rowHex;
			for (uint32_t i = 0; i < sp::kStateRowSize; ++i) {
				char b[4];
				std::snprintf(b, sizeof(b), "%02x", static_cast<const uint8_t*>(states.rows)[static_cast<size_t>(pick.state) * sp::kStateRowSize + i]);
				rowHex += b;
			}
			plog::Info("statprobe: state %d carries the player's list (every column of its row is empty): bank %u, %u states, %u skills; row bytes %s", pick.state, bank,
			           states.rowCount, skills.rowCount, rowHex.c_str());
			return pick.state;
		}
		*why = "the SDK's states table is not the game's (" + tried + ")";
	}
	return -1;
}

// Gives the player's attached list its state and turns the state on, which is what makes the game send it with the stats. The caller has
// checked the refusal rule (state off, no list carries it). SetStateBit is last, after the list is attached. Afterwards the game's
// finder must return our list, checked before the bit is set; if not, the bit is never touched and false is returned (the caller takes
// the list off).
auto SetPlayerState(Unit* unit, void* list, int state, std::string* why) noexcept -> bool {
	Call<SetListStateFn>(SetListState)(list, state);
	// The finder reads only the list's state, never the bit: check it first, so a mismatch never toggles the bit.
	if (Call<FindStateListFn>(FindStateList)(unit, state) != list) {
		*why = "the game does not find the player's list by state " + std::to_string(state);
		return false;
	}
	Call<SetStateBitFn>(SetStateBit)(unit, state, 1);
	return true;
}

void RemoveFromPlayer(Unit* unit, uint32_t id) noexcept {
	uintptr_t list = 0;
	int state = 0;
	uint32_t storedFlags = 0, storedState = 0;
	{
		std::lock_guard<std::mutex> lock(g_lock);
		list = g_lists.find(id);
		state = g_lists.stateOf(id);
		storedFlags = g_lists.flagsOf(id);
		storedState = g_lists.rawStateOf(id);
	}
	// The record is dropped on every path below, once the attempt is over; nothing is retried on a later tick.
	const auto forget = [&]() noexcept {
		std::lock_guard<std::mutex> lock(g_lock);
		g_lists.remove(id);
		UpdateBusy();
	};
	if (list == 0) {
		forget();
		return;
	}
	bool found = false;
	const int before = Chain(unit, reinterpret_cast<void*>(list), &found);
	// The state is the one the plugin stored with the list, never read back from the list. If the game freed the list and the
	// address was reused, the finder no longer returns it for that state.
	const bool foundByState = state > 0 && On(SetStateBit) && On(FindStateList) && Call<FindStateListFn>(FindStateList)(unit, state) == reinterpret_cast<void*>(list);
	// Being on the chain proves only that some live list sits at this address. If the game freed ours and reused the address, it is
	// the game's list now, so its own fields must say it is ours (flags 0, our state) before anything is detached or freed. The
	// memory is live only because the chain walk found it; nothing is read for a list that is not on the chain.
	// The flags are compared with what was read right after the attach (the engine may set bits there), not with 0.
	uint32_t readFlags = 0, readState = 0;
	if (found) {
		readFlags = static_cast<uint32_t>(At<int32_t>(reinterpret_cast<void*>(list), ListFlagsOffset));
		readState = static_cast<uint32_t>(At<int32_t>(reinterpret_cast<void*>(list), ListStateOffset));
	}
	const bool owned = found && sp::ownsList(readFlags, readState, storedFlags, storedState, state);
	const sp::RemoveAction action = sp::decideRemoval(found, foundByState, owned);
	if (action == sp::RemoveAction::Forget) {
		// Never detach, free or write a list the unit does not hold, or one that is not ours: it is the game's now (or gone).
		if (!found) {
			plog::Warn("map stats: the player's list is no longer on the unit (%d lists); it is forgotten, not freed", before);
		} else {
			plog::Warn("map stats: the list at the recorded address is on the unit but is not the plugin's (flags 0x%x state %u read, flags 0x%x state %u stored at apply, state %d given); the game freed ours and reused the address; it is forgotten, not freed, and the state bit is left alone", readFlags, readState, storedFlags, storedState, state);
		}
		forget();
		return;
	}
	// The order: state off first (the clients get the state-off packet), then off the chain, then freed. The list is on the unit's
	// chain, so it is ours to take off even when the state lookup failed: leaving it would carry the map's stats into vanilla levels.
	if (action == sp::RemoveAction::StateOffDetachFree) {
		Call<SetStateBitFn>(SetStateBit)(unit, state, 0);
	} else {
		// The state bit is left set: the finder returned another list for this state (or a part is missing), so the game may have
		// its own list on this state and clearing the bit would clear that one too. The bit stays on with no list behind it; the
		// next map's ApplyToPlayer then finds the state in use and keeps player stats off for that map.
		plog::Warn("map stats: the game does not find the player's list by its state %d; it is detached and freed anyway, the state bit is left alone (player stats stay off for the next map)", state);
	}
	Call<DetachStatListFn>(DetachStatList)(unit, reinterpret_cast<void*>(list));
	Call<FreeStatListFn>(FreeStatList)(At<uint8_t>(unit, UnitDataFlagOffset), reinterpret_cast<void*>(list));
	bool still = false;
	const int after = Chain(unit, reinterpret_cast<void*>(list), &still);
	if (still) {
		plog::Error("map stats: the player's list is still on the unit after it was removed (chain %d -> %d lists)", before, after);
	}
	forget();
	plog::Info("statprobe: player %u left the map: list removed (chain %d -> %d lists)", id, before, after);
}

void ApplyToPlayer(Unit* unit, uint32_t id, const sp::Plan& plan, uint64_t generation) noexcept {
	if (g_stateRefused.load()) {
		return;
	}
	std::string why;
	const int state = ChoosePlayerState(At<uint8_t>(unit, UnitDataFlagOffset), &why);
	if (state <= 0) {
		g_stateRefused = true;
		plog::Warn("map stats: player stats are off for this map: %s", why.c_str());
		return;
	}
	const std::string before = Totals(unit, plan.players);
	bool found = false;
	const int chainBefore = Chain(unit, nullptr, &found);
	// The refusal rule is checked before anything is made, so a refused state leaves nothing on the unit.
	if (Call<StateOnFn>(StateOn)(unit, state) != 0 || Call<FindStateListFn>(FindStateList)(unit, state) != nullptr) {
		g_stateRefused = true;
		plog::Warn("map stats: player stats are off for this map: state %d is already in use on the player", state);
		return;
	}
	void* list = MakeList(unit, UnitPlayer, plan.players);
	if (list == nullptr) {
		plog::Warn("map stats: the game gave no stat list for the player; the player has none of the map's stats");
		return;
	}
	if (!SetPlayerState(unit, list, state, &why)) {
		g_stateRefused = true;
		Call<DetachStatListFn>(DetachStatList)(unit, list);
		Call<FreeStatListFn>(FreeStatList)(At<uint8_t>(unit, UnitDataFlagOffset), list);
		plog::Warn("map stats: player stats are off for this map, the list was taken off again: %s", why.c_str());
		return;
	}
	// Read what the attach and the state call left in the list; removal compares against these, never against 0.
	const uint32_t listFlags = static_cast<uint32_t>(At<int32_t>(list, ListFlagsOffset));
	const uint32_t listState = static_cast<uint32_t>(At<int32_t>(list, ListStateOffset));
	bool ok = false, stale = false;
	{
		std::lock_guard<std::mutex> lock(g_lock);
		stale = generation != g_generation;
		ok = !stale && g_lists.add(id, reinterpret_cast<uintptr_t>(list), state, listFlags, listState);
		UpdateBusy();
	}
	if (stale) {
		// The game was left while this list was being made. The unit belongs to that game: record nothing and touch nothing more.
		plog::Warn("map stats: the game was left while the player's list was being made; the list is not tracked");
		return;
	}
	if (!ok) {
		// Table full: take the list straight off again, so no list is out that the plugin cannot remove (state off first).
		Call<SetStateBitFn>(SetStateBit)(unit, state, 0);
		Call<DetachStatListFn>(DetachStatList)(unit, list);
		Call<FreeStatListFn>(FreeStatList)(At<uint8_t>(unit, UnitDataFlagOffset), list);
		plog::Warn("map stats: too many players to track; player %u has none of the map's stats", id);
		return;
	}
	++g_playerCount;
	bool onChain = false;
	const int chainAfter = Chain(unit, list, &onChain);
	plog::Info("statprobe: player %u entered the map: totals [%s] -> [%s], list on chain: %s (%d -> %d lists), list flags 0x%x state %u (state given %d)", id, before.c_str(),
	           Totals(unit, plan.players).c_str(), onChain ? "yes" : "NO", chainBefore, chainAfter, listFlags, listState, state);
}

// --- Level and plan -------------------------------------------------------------------------------------------------------

auto SummaryNow() -> std::string {
	const bool mon = g_monstersOn.load(), pl = g_playersOn.load();
	const char* player = !g_playerSetting.load() ? "off (PlayerStats=0 in cartographer.txt, the default)" : pl ? "on (test slot only)" : "off (a part was not found)";
	return std::string("monster stats ") + (mon ? "on" : "off") + ", player stats " + player + " (monsters checked in game on cheat-spawned monsters; the player half only with Save and Exit)";
}

auto AllOn(std::initializer_list<PartId> ids) noexcept -> bool {
	for (const PartId id : ids) {
		if (!On(id)) {
			return false;
		}
	}
	return true;
}

}  // namespace

auto InstallStatApply(const D2RL::PluginContext* context, const std::vector<SignatureOverride>& overrides, std::vector<PartStatus>* parts) -> bool {
	CodeRange ranges[8];
	const int count = CodeRanges(context->exeBase, ranges, 8);
	for (Part& part : g_parts) {
		ResolvePart(context, overrides, ranges, count, part);
	}

	// Monsters: the setup hook goes in at load, only when everything it calls was found. Never installed half-on.
	if (!AllOn({NewStatList, SetListStat, AttachStatList, RoomLevelId, MonsterSetup})) {
		if (On(MonsterSetup)) {
			g_parts[MonsterSetup].status.on = false;
			g_parts[MonsterSetup].status.detail = "off: not hooked, because a stat function it needs was not found";
		}
	} else {
		Part& part = g_parts[MonsterSetup];
		const uint64_t rva = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(part.address) - context->exeBase);
		const bool hooked = context->InstallInlineHook(rva, part.guard, part.hookSize, reinterpret_cast<void*>(&MonsterSetupHook), reinterpret_cast<void**>(&g_original));
		const bool ok = HookOutcome(part, hooked, g_original != nullptr);
		g_monstersOn = ok;
	}

	// Players: applied from the portal's player tick hook, so that part must be on too.
	bool tickOn = false;
	for (const PartStatus& s : PartStatuses()) {
		tickOn = tickOn || (s.name == "PlayerTick" && s.on);
	}
	g_context = context;
	if (context->QueryService(&g_dataTables) != D2RL::ServiceQueryResult::Success || !D2RL::HasDataTableServiceField(g_dataTables, D2RL::DataTableServiceRequiredSize)) {
		g_dataTables = nullptr;
	}
	const bool partsOn = AllOn({NewStatList, SetListStat, AttachStatList, DetachStatList, FreeStatList, UnitRoom, RoomLevelId, SetListState, SetStateBit, StateOn, FindStateList, StatDataTablesBase});
	g_playersOn = tickOn && partsOn && g_dataTables != nullptr;
	if (!g_playersOn) {
		plog::Warn("player map stats are off: %s", !tickOn ? "the PlayerTick part is off" : !partsOn ? "a stat list or state part was not found" : "D2RLoader has no DataTable service of the expected version");
	}
	if (!g_monstersOn) {
		plog::Warn("monster map stats are off (see the parts above)");
	}
	plog::Info("map stats: %s", SummaryNow().c_str());
	if (parts != nullptr) {
		*parts = StatApplyPartStatuses();
	}
	for (const Part& part : g_parts) {
		plog::Info("part %s: %s", part.name, part.status.detail.c_str());
	}
	return g_monstersOn.load() || g_playersOn.load();
}

void StatApplyMapOpened(const MapDef& map, const std::shared_ptr<const Tables>& tables, uint32_t playerId, uint32_t openings,
                        const std::vector<cartographer::statplan::ItemStat>& itemStats) {
	if (tables == nullptr || (!g_monstersOn.load() && !g_playersOn.load())) {
		return;
	}
	const int slot = SlotLevel(tables->slots, MazeSlot);
	const uint64_t seed = sp::mixSeed(GetTickCount64(), playerId, openings);
	cartographer::stats::Settings settings;
	settings.properties = tables->settings.properties;
	settings.itemStats = tables->settings.itemStats != 0;
	std::vector<cartographer::stats::Property> fromItem;
	if (settings.itemStats) {
		const auto mapped = sp::itemStatsToProperties(itemStats, tables->pool.empty() ? cartographer::stats::builtInPool() : tables->pool, tables->statIds);
		for (const std::string& line : mapped.dropped) {
			plog::Warn("ItemStats: %s", line.c_str());
		}
		fromItem = mapped.properties;
		plog::Info("ItemStats: %zu item stat(s) read, %zu used%s", itemStats.size(), fromItem.size(), fromItem.empty() ? "; drawing as usual" : "");
	}
	cartographer::stats::MapRow row;
	row.properties = map.properties;
	const auto drawn = cartographer::stats::drawMapStats(seed, row, settings, tables->pool, fromItem);
	const bool playerStats = g_playersOn.load() && tables->settings.playerStats != 0;
	auto plan = std::make_shared<sp::Plan>(sp::buildPlan(drawn, tables->statIds, playerStats));
	plog::Info("map stats for '%s' (seed %llu, slot level %d): %zu drawn, %s", map.name.c_str(), static_cast<unsigned long long>(seed), slot,
	           drawn.size(), sp::describe(*plan).c_str());
	for (const sp::Skipped& s : plan->skipped) {
		plog::Warn("map stats: %s '%s' skipped: %s", s.target == cartographer::stats::Target::Monsters ? "monster" : "player", s.stat.c_str(), s.why.c_str());
	}
	if (!g_monstersOn.load() && !plan->monsters.empty()) {
		plog::Warn("map stats: %zu monster stat(s) drawn, but monster stats are off", plan->monsters.size());
	}
	std::lock_guard<std::mutex> lock(g_lock);
	g_plan = std::move(plan);
	g_slotLevel = slot;
	g_monsterCount = 0;
	g_monsterFailed = 0;
	g_playerCount = 0;
	g_stateRefused = false;
	UpdateBusy();
}

void StatApplySetPlayerStats(bool on) noexcept {
	g_playerSetting = on;
}

void StatApplyPlayerTick(void* game, void* rawUnit) noexcept {
	(void)game;
	auto* unit = static_cast<Unit*>(rawUnit);
	if (unit == nullptr) {
		return;
	}
	try {
		if (!g_chainLogged.load(std::memory_order_relaxed) && At<int32_t>(unit, UnitTypeOffset) == UnitPlayer) {
			g_chainLogged = true;  // once per game: a list left on the player by an earlier map (a save and reload) would show here
			bool found = false;
			const int lists = Chain(unit, nullptr, &found);
			const uint8_t* owner = At<const uint8_t*>(unit, UnitOwnerListOffset);
			const bool ok = Readable(owner, 0xa0);
			plog::Info("statprobe: first player unit %u of this game holds %d stat lists (owner list %p, heads +0x%x = %p, +0x%x = %p)", At<uint32_t>(unit, UnitIdOffset), lists,
			           static_cast<const void*>(owner), ListHeadOffset, ok ? At<const void*>(owner, ListHeadOffset) : nullptr, ListHeadAltOffset,
			           ok ? At<const void*>(owner, ListHeadAltOffset) : nullptr);
		}
		if (!g_busy.load(std::memory_order_relaxed) || !g_playersOn.load() || At<int32_t>(unit, UnitTypeOffset) != UnitPlayer) {
			return;
		}
		const uint32_t id = At<uint32_t>(unit, UnitIdOffset);
		std::shared_ptr<const sp::Plan> plan;
		int slot = -1;
		bool hasList = false;
		uint64_t generation = 0;
		{
			std::lock_guard<std::mutex> lock(g_lock);
			generation = g_generation;
			plan = g_plan;
			slot = g_slotLevel;
			hasList = g_lists.find(id) != 0;
		}
		const bool inMap = plan != nullptr && slot > 0 && UnitLevelId(unit) == slot;
		switch (sp::decidePlayer(plan != nullptr && !plan->players.empty(), inMap, hasList, g_playerSetting.load())) {
		case sp::PlayerAction::Apply:
			ApplyToPlayer(unit, id, *plan, generation);
			break;
		case sp::PlayerAction::Remove:
			RemoveFromPlayer(unit, id);
			break;
		case sp::PlayerAction::None:
			break;
		}
	} catch (...) {
		plog::Error("map stats: the player tick failed");
	}
}

void StatApplyGameLeft() noexcept {
	int monsters = 0, players = 0;
	{
		std::lock_guard<std::mutex> lock(g_lock);
		monsters = g_monsterCount.load();
		players = g_playerCount.load();
		const bool had = g_plan != nullptr;
		++g_generation;
		g_plan.reset();
		g_slotLevel = -1;
		g_lists.clear();  // the game frees the units and their lists; nothing is touched here
		UpdateBusy();
		if (had) {
			plog::Info("statprobe: left the game: %d monster(s) and %d player(s) were given the map's stats", monsters, players);
		}
	}
	g_monsterCount = 0;
	g_monsterFailed = 0;
	g_playerCount = 0;
	g_chainLogged = false;
	g_stateRefused = false;
}

auto StatApplyPartStatuses() -> std::vector<PartStatus> {
	std::vector<PartStatus> out;
	for (const Part& part : g_parts) {
		out.push_back(part.status);
	}
	return out;
}

auto StatApplyCountsNow() -> StatApplyCounts {
	StatApplyCounts out;
	std::shared_ptr<const sp::Plan> plan;
	{
		std::lock_guard<std::mutex> lock(g_lock);
		plan = g_plan;
	}
	if (plan != nullptr) {
		out.drawnMonster = static_cast<int>(plan->monsters.size());
		out.drawnPlayer = static_cast<int>(plan->players.size());
		for (const sp::Entry& e : plan->monsters) {
			out.drawnText += (out.drawnText.empty() ? "" : ", ") + std::string("monsters ") + e.name + " " + std::to_string(e.value);
		}
		for (const sp::Entry& e : plan->players) {
			out.drawnText += (out.drawnText.empty() ? "" : ", ") + std::string("players ") + e.name + " " + std::to_string(e.value);
		}
	}
	out.monstersChanged = g_monsterCount.load();
	out.monstersFailed = g_monsterFailed.load();
	out.playersChanged = g_playerCount.load();
	return out;
}

auto StatApplySummary() -> std::string {
	return "map stats: " + SummaryNow();
}

auto StatApplyStateReport() -> std::vector<std::string> {
	namespace Tables = D2RL::DataTables;
	std::vector<std::string> out;
	if (g_dataTables == nullptr || g_context == nullptr) {
		out.push_back("player stat state: D2RLoader has no DataTable service of the expected version");
		return out;
	}
	// Read-only, no unit needed: the SDK views are read here without the data tables base cross-check that a pick at run time makes.
	for (uint32_t bank = 1; bank <= 3; ++bank) {
		Tables::TableView states{};
		states.structSize = Tables::TableViewSize;
		Tables::TableView skills{};
		skills.structSize = Tables::TableViewSize;
		if (g_dataTables->getTable(g_context, static_cast<Tables::Bank>(bank), Tables::TableId::States, &states) != Tables::Result::Success ||
		    !Tables::HasTableViewField(&states, Tables::TableViewRequiredSize) || states.rowSize != sp::kStateRowSize || states.rows == nullptr ||
		    g_dataTables->getTable(g_context, static_cast<Tables::Bank>(bank), Tables::TableId::Skills, &skills) != Tables::Result::Success ||
		    !Tables::HasTableViewField(&skills, Tables::TableViewRequiredSize) || skills.rowSize != sp::kSkillRowSize || (skills.rows == nullptr && skills.rowCount != 0)) {
			continue;
		}
		const sp::StatePick pick = sp::pickPlayerState(static_cast<const uint8_t*>(states.rows), states.rowCount, static_cast<const uint8_t*>(skills.rows), skills.rowCount);
		const std::string head = "player stat state, bank " + std::to_string(bank) + " (" + std::to_string(states.rowCount) + " states): ";
		if (pick.state >= 0) {
			out.push_back(head + "state " + std::to_string(pick.state) + " would be picked");
			continue;
		}
		out.push_back(head + "none would be picked");
		for (const std::string& line : sp::explainNoPick(static_cast<const uint8_t*>(states.rows), states.rowCount, static_cast<const uint8_t*>(skills.rows), skills.rowCount)) {
			out.push_back("  " + line);
		}
	}
	if (out.empty()) {
		out.push_back("player stat state: no bank gave a states and skills table of the expected row sizes");
	}
	return out;
}

}  // namespace carto
