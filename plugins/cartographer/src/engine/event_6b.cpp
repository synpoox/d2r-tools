#include "engine/event_6b.h"

#include "core/part_signatures.h"
#include "engine/map_behaviours.h"
#include "engine/scan.h"
#include "event6b.h"
#include "log.h"
#include "hookguard.h"

#include <windows.h>

#include <algorithm>
#include <atomic>
#include <cstring>
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace carto {

namespace {

namespace e6 = cartographer::event6b;
namespace ep = cartographer::eventplan;
namespace ev = cartographer::events;
namespace beh = cartographer::behaviours;

// --- Engine facts. Build 93847; each is recorded with its evidence in docs/reverse-engineering/cartographer/13-ambush-treasure-shrine.md.
constexpr int UnitTypeOffset = 0x00;         // int32: 1 monster
constexpr int UnitIdOffset = 0x08;           // uint32
constexpr int32_t UnitTypeMonster = 1;
constexpr int GameDataFlagOffset = 0x106;    // uint8 on the game, what the engine's own object and drop code read
constexpr int UnitDataFlagOffset = 0x1bd;    // uint8 on a unit (the same byte event_apply.cpp reads)
constexpr uint32_t DataFlagLimit = 4;
constexpr int DtObjectCountOffset = 0x1530;  // uint32 in the data tables: the objects row count (what 0x38fc70 returns)
constexpr int DtTcMapOffset = 0xd70;         // treasure class name -> index map pointer (re-read on every use)
constexpr int DtTcCountOffset = 0xd80;       // uint64: the treasure class vector's count (the vector is at +0xd78)
constexpr int DeathInfoKillerOffset = 0x10;  // [info + 0x10]: the killer, what 0x447b80 passes on as the roller's third argument
constexpr int32_t ObjectUnitType = 2;        // 0x490410's first argument for an object
constexpr int32_t FreeSpotRadius = 5;        // 0x402f50's search radius as SpawnObject passes it
constexpr uint32_t FreeSpotMask = 0x3e01;    // the flag mask SpawnObject passes (the other caller, 0x401ecc, passes 0x100)
constexpr uint16_t ObjectCreateFlag = 1;     // [rsp+0x30] as SpawnObject passes it
constexpr size_t FreeSpotSite = 0xff;        // the call to 0x402f50 inside the SpawnObject handler (0x40239f - 0x4022a0)
constexpr size_t FreeSpotReach = 0x200;      // how much of the handler is read to find it
constexpr int UnitModeOffset = 0x0c;         // int32 on a unit: the mode (inference from the classic layout, not shown; the chest probe logs it)
// 0x4404f0's stack arg 5 is a forced item quality: 0 rolls the quality (0x4421b0, from the tier below), a nonzero value is used as the quality as it is
// (0x440f9f). 0x5930b0 passes its own second argument there: the chest handler passes 0, 7 or a variable (0x58e5a6, 0x58e709, 0x58e51f); only 0x592cf0 passes 4.
constexpr int32_t ChestSelector = 0;         // no forced quality: the roller rolls it, as a normal chest does
// Arg 6, the tier for that quality roll. 0x5930b0 derives 0..2 from the area level and the act (0x593282..0x5932b1, after 0x34b440, 0x2efc10, 0x3e2b5c8,
// 0x508f80 and 0x32c700); that chain is not reproduced, so the tier stays 0, the lowest (needs-game: does it feel right).
constexpr int32_t ChestRollTier = 0;
constexpr int32_t ChestMaxItems = 6;         // arg 10: the most items the roller writes to the out buffer (0x44114f..0x44115f); 6 pointers fill ChestOutSize
constexpr size_t ChestOutSize = 0x30;        // the out buffer 0x5930b0 zeroes for the roller: 6 item pointers. A Drop that gives more than ChestMaxItems items is cut at 6
constexpr int MaxUntrackedDeathProbes = 5;   // deaths during an Ambush that are not in its wave, logged

// What is around the call (SpawnObject + 0xff) and at its target: the guard for a part that has no signature of its own.
// 41 B9 01 3E 00 00: mov r9d, 0x3e01   41 B8 05 00 00 00: mov r8d, 5   48 89 44 24 20: mov [rsp+0x20], rax
constexpr uint8_t FreeSpotContext[17] = {0x41, 0xB9, 0x01, 0x3E, 0x00, 0x00, 0x41, 0xB8, 0x05, 0x00, 0x00, 0x00, 0x48, 0x89, 0x44, 0x24, 0x20};
constexpr uint8_t FreeSpotPrefix[20] = {0x40, 0x53, 0x55, 0x56, 0x57, 0x41, 0x54, 0x41, 0x55, 0x41, 0x56, 0x41, 0x57, 0x48, 0x81, 0xEC, 0x98, 0x00, 0x00, 0x00};

struct Game;
struct Unit;

using DeathDropFn = uint64_t (*)(Game*, Unit* victim, int32_t arg3, void* info) noexcept;
using DropRollerFn = void (*)(Game*, Unit* victim, void* killer, uint16_t tcIndex, int32_t a5, int32_t a6, int32_t a7, uint64_t a8, uint64_t a9, int32_t a10) noexcept;
using DropLevelFn = int32_t (*)(Unit* unit, int32_t levelId) noexcept;
using FreeSpotFn = uint64_t (*)(void* room, int32_t* xy, int32_t radius, uint32_t mask, void** outRoom) noexcept;
using CreateObjectFn = void* (*)(int32_t unitType, int32_t row, int32_t x, int32_t y, Game* game, void* room, uint16_t flag, int32_t a8, int32_t a9, uint64_t a10) noexcept;
using DataTablesBaseFn = uint8_t* (*)(uint32_t dataFlag) noexcept;
using NameToRowFn = int32_t (*)(void* map, const char* name, int32_t logOnMiss) noexcept;
// The chest handler (objects OperateFn 4) takes the operate context the engine builds on its stack: [0] game, [8] object, [0x10] the operating player.
struct OperateContext {
	Game* game;
	Unit* object;
	Unit* activator;
};
using OperateFn = int32_t (*)(OperateContext*) noexcept;
using ObjectFlagsGetFn = uint32_t (*)(Unit*) noexcept;      // the object data's flag word; the low byte has the locked bit 0x80
using ObjectFlagsSetFn = void (*)(Unit*, uint8_t) noexcept;  // writes that low byte (and clears the high byte)
using TcLookupFn = const void* (*)(uint32_t dataFlag, uint16_t tcIndex, int32_t level) noexcept;
using TcRollerFn = uint64_t (*)(Game*, Unit* a, Unit* b, const void* tc, int32_t s20, int32_t s28, int32_t s30, void* out, int32_t* s40, int32_t s48) noexcept;

// Map drop behaviours (issue #538): ItemMake (game, dropRequest, int) makes one dropped item and returns its unit; the request's +0 is the dropper and +0xc0 a flag word (0x4 = ethereal).
using ItemMakeFn = uint64_t (*)(Game*, void* request, int32_t a3) noexcept;
using ItemFlagFn = int32_t (*)(Unit*) noexcept;                 // ItemSocketable: nonzero when the item can take sockets and has none
using ItemSetSocketsFn = void (*)(Unit*, uint32_t count) noexcept;  // clamps to the item's own maximum
using UnitStatFn = int32_t (*)(Unit*, int32_t stat, int32_t layer) noexcept;
using UnitRoomFn = void* (*)(Unit*) noexcept;
using RoomLevelFn = int32_t (*)(void* room) noexcept;
constexpr int32_t StatAlignment = 172;        // 0 hostile (the same test as the route table, #537)
constexpr int32_t StatSockets = 0xc2;
constexpr int MonsterDataOffset = 0x10;       // unit -> monster data
constexpr int MonsterFlagsOffset = 0x1a;      // uint8 in the monster data; 0x10 is a minion (from the PD2R notes; the flags byte is logged at each kill)
constexpr uint8_t MonsterMinionBit = 0x10;
constexpr int DropRequestDropperOffset = 0x00;
constexpr int DropRequestFlagsOffset = 0xc0;
constexpr uint32_t DropFlagEthereal = 0x4;

enum PartId { DeathDrop, DropRoller, DropLevel, SpawnObject, FreeSpot, CreateObject, ChestOperate, ObjectFlagsGet, ObjectFlagsSet, TcLookup, TcRoller, DataTables, NameToRow,
              ItemMake, ItemSocketable, ItemSetSockets, UnitStat, UnitRoom, RoomLevelId, PartCount };

Part g_parts[PartCount] = {
	{"DeathDrop", sig::DeathDrop, 15, false, nullptr, {}, {}},
	{"DropRoller", sig::DropRoller, 20, false, nullptr, {}, {}},  // hooked for the map bonus picks (issue #538); every other use only calls it
	{"DropLevel", sig::DropLevel, 0, false, nullptr, {}, {}},
	{"SpawnObject", sig::SpawnObject, 0, false, nullptr, {}, {}},
	{"FreeSpot", nullptr, 0, false, nullptr, {}, {}},  // no signature: the call target inside SpawnObject
	{"CreateObject", sig::CreateObject, 0, false, nullptr, {}, {}},
	{"ChestOperate", sig::ChestOperate, 16, false, nullptr, {}, {}},
	{"ObjectFlagsGet", sig::ObjectFlagsGet, 0, false, nullptr, {}, {}},
	{"ObjectFlagsSet", sig::ObjectFlagsSet, 0, false, nullptr, {}, {}},
	{"TcLookup", sig::TcLookup, 0, false, nullptr, {}, {}},
	{"TcRoller", sig::TcRoller, 0, false, nullptr, {}, {}},
	{"DataTablesBase", nullptr, 0, false, nullptr, {}, {}},  // shared with event_apply.cpp (copied in at load)
	{"NameToRow", nullptr, 0, false, nullptr, {}, {}},       // shared with event_apply.cpp (copied in at load)
	{"ItemMake", sig::ItemMake, 20, false, nullptr, {}, {}},
	{"ItemSocketable", sig::ItemSocketable, 0, false, nullptr, {}, {}},
	{"ItemSetSockets", sig::ItemSetSockets, 0, false, nullptr, {}, {}},
	{"BehavioursUnitStat", sig::UnitStat, 0, false, nullptr, {}, {}},
	{"BehavioursUnitRoom", sig::UnitRoom, 0, false, nullptr, {}, {}},
	{"BehavioursRoomLevelId", sig::RoomLevelId, 0, false, nullptr, {}, {}},
};

const D2RL::PluginContext* g_context = nullptr;
DeathDropFn g_original = nullptr;
DropRollerFn g_rollerOriginal = nullptr;
ItemMakeFn g_itemMakeOriginal = nullptr;
std::atomic<bool> g_behavioursHook{false};  // the DropRoller and ItemMake hooks are in (the map drop behaviours can act)
OperateFn g_operateOriginal = nullptr;
bool g_populatorOn = false;            // the room populator hook (event_apply.cpp) is in; set once at load
bool g_deathListener = false;          // the SDK monster death listener is registered (the Ambush needs it to count kills)
std::atomic<bool> g_dropHook{false};   // the death hook is installed and every part it calls was found
std::atomic<bool> g_dropArmed{false};  // a Boss or Treasure monster event with a resolved Drop is armed on some map (read on every monster death: lock only when set)
std::atomic<bool> g_chestHook{false};  // the operate hook is installed and every part the chest roll calls was found
std::atomic<bool> g_chestArmed{false}; // a Treasure chest with a resolved Drop is armed on some map (read on every object use: lock only when set)
std::atomic<bool> g_ambushActive{false};  // an Ambush is armed and not finished on some map (read on every tick and monster death)
std::atomic<uint64_t> g_epoch{0};      // bumped at every map open and game left: an Ambush wave spawned across one is dropped

template <typename Fn>
auto Call(PartId id) noexcept -> Fn {
	return reinterpret_cast<Fn>(const_cast<uint8_t*>(g_parts[id].address));
}

auto On(PartId id) noexcept -> bool {
	return g_parts[id].address != nullptr && g_parts[id].status.on;
}

struct State {
	// Shrine
	bool shrineReady = false;
	bool shrinePlaced = false;
	int shrineRow = -1;
	std::string shrineSpawn;
	// Boss Drop
	bool dropArmed = false;
	std::string dropName;
	int dropClass = -1;  // the treasure class index
	int dropLevel = 0;   // the map's level id, passed to the level function
	bool bossTracked = false;
	uint32_t bossId = 0;
	bool dropped = false;
	// Treasure monster
	// Treasure chest (the object is placed by the Shrine code, tracked here)
	bool objectIsChest = false;
	bool chestArmed = false;
	std::string chestDropName;
	int chestClass = -1;
	bool chestTracked = false;
	uint32_t chestId = 0;
	bool chestRolled = false;
	// Ambush
	bool ambushArmed = false;
	bool ambushActive = false;   // armed and not finished: the tick and the death listener step it
	bool firstWaveDone = false;  // the first wave made a unit (the event is announced then)
	cartographer::event6b::WaveTracker waves;
	ep::Rect ambushRect;         // the event room's server rect: the player's room is matched by it (a room pointer could be freed or reused between ticks)
	std::vector<ep::Spawn> waveSpawns;
	std::string ambushName;
	int untrackedProbes = 0;
	int deathProbes = 0;  // wave deaths logged, capped like untrackedProbes
};
std::mutex g_lock;
// One state per map, keyed by its slot level: a map opened later leaves the earlier maps' Drop, Shrine, Treasure and Ambush alone.
// Guarded by g_lock.
std::map<int, State> g_states;

// The fast-path flags are the OR over every map's state. Caller holds g_lock.
void RecomputeFlagsLocked() noexcept {
	bool drop = false, chest = false, ambush = false;
	for (const auto& entry : g_states) {
		drop = drop || entry.second.dropArmed;
		chest = chest || entry.second.chestArmed;
		ambush = ambush || entry.second.ambushActive;
	}
	g_dropArmed = drop;
	g_chestArmed = chest;
	g_ambushActive = ambush;
}

// The map whose tracked boss (or chest) has unit id `id`; null when no map tracks it. Caller holds g_lock.
auto FindTrackedBoss(uint32_t id) noexcept -> State* {
	for (auto& entry : g_states) {
		if (entry.second.bossTracked && entry.second.bossId == id) {
			return &entry.second;
		}
	}
	return nullptr;
}

auto FindTrackedChest(uint32_t id) noexcept -> State* {
	for (auto& entry : g_states) {
		if (entry.second.chestTracked && entry.second.chestId == id) {
			return &entry.second;
		}
	}
	return nullptr;
}

// The data tables base for a data flag, or null (with `why`).
auto TablesBase(uint32_t dataFlag, std::string* why) noexcept -> const uint8_t* {
	if (!On(DataTables)) {
		*why = "the DataTablesBase part is off";
	} else if (dataFlag >= DataFlagLimit) {
		*why = "the data flag " + std::to_string(dataFlag) + " is not below 4";
	} else {
		const uint8_t* dt = Call<DataTablesBaseFn>(DataTables)(dataFlag);
		if (dt != nullptr) {
			return dt;
		}
		*why = "the data tables base is null";
	}
	return nullptr;
}

// --- Map drop behaviours (issue #538) ----------------------------------------------------------------------------------------
// The death hook opens a context for a hostile, non-minion monster that dies on a map level with behaviours; while it is open (this thread only), the
// DropRoller hook adds the bonus picks and the ItemMake hook makes drops ethereal and socketed and tells the map-drop subscribers.

struct DropContext {
	bool active = false;
	bool extra = false;      // the bonus picks are running: the roller hook passes through
	bool rolled = false;     // the monster's own roll was seen (the bonus applies to it once)
	Unit* victim = nullptr;
	int level = 0;
	beh::MapBehaviours b;
	beh::Rng rng{1};
};
thread_local DropContext t_drop;

// Builds the context for `victim`; false (nothing set) when the behaviours do not apply to it. Logs why, for the game check.
auto OpenDropContext(Game* game, Unit* victim) noexcept -> bool {
	(void)game;
	if (!On(UnitStat) || !On(UnitRoom) || !On(RoomLevelId) || At<int32_t>(victim, UnitTypeOffset) != UnitTypeMonster) {
		return false;
	}
	void* room = Call<UnitRoomFn>(UnitRoom)(victim);
	const int level = room == nullptr ? -1 : Call<RoomLevelFn>(RoomLevelId)(room);
	beh::MapBehaviours b;
	if (level < 0 || !GetMapBehaviours(level, &b)) {
		return false;
	}
	const uint8_t* data = At<const uint8_t*>(victim, MonsterDataOffset);
	const uint8_t flags = data == nullptr ? 0 : data[MonsterFlagsOffset];
	const int32_t alignment = Call<UnitStatFn>(UnitStat)(victim, StatAlignment, 0);
	const bool eligible = alignment == 0 && (flags & MonsterMinionBit) == 0;
	static std::atomic<uint32_t> s_killLogged{0};  // D2RLoader flushes each log line to disk, so log the 1st, 100th, ... of this repeated line
	if (plog::Sample(s_killLogged)) {
		plog::Info("map behaviours: kill of unit %u on level %d: alignment %d, monster flags 0x%02x: %s", At<uint32_t>(victim, UnitIdOffset), level, alignment, flags, eligible ? "behaviours apply" : "not hostile or a minion, skipped");
	}
	if (!eligible) {
		return false;
	}
	DropContext& c = t_drop;
	c.active = true;
	c.extra = false;
	c.rolled = false;
	c.victim = victim;
	c.level = level;
	c.b = b;
	c.rng = beh::Rng(GetTickCount64() * 0x9E3779B97F4A7C15ull ^ At<uint32_t>(victim, UnitIdOffset));
	return true;
}

// The drop_tc rolls of the kill: each class that hits is rolled once, as the game's own drop is. Runs inside the context.
void RollDropTcs(Game* game, Unit* victim, int32_t arg3, void* info) noexcept {
	DropContext& c = t_drop;
	if (c.b.dropTcs.empty()) {
		return;
	}
	std::string why;
	const uint8_t* dt = TablesBase(At<uint8_t>(game, GameDataFlagOffset), &why);
	void* map = dt == nullptr ? nullptr : At<void*>(dt, DtTcMapOffset);
	if (map == nullptr) {
		static std::atomic<uint32_t> s_notRolledLogged{0};  // D2RLoader flushes each log line to disk, so log the 1st, 100th, ... of this repeated line
		if (plog::Sample(s_notRolledLogged)) {
			plog::Warn("map behaviours: drop_tc not rolled: %s", dt == nullptr ? why.c_str() : "the treasure class name map is null");
		}
		return;
	}
	void* killer = At<void*>(info, DeathInfoKillerOffset);
	const int32_t levelValue = Call<DropLevelFn>(DropLevel)(victim, c.level);
	c.extra = true;
	for (const size_t i : beh::hitTcs(c.b.dropTcs, c.rng)) {
		const std::string& name = c.b.dropTcs[i].first;
		std::string tcWhy;
		const int tc = e6::tcIndexFromLookup(Call<NameToRowFn>(NameToRow)(map, name.c_str(), 0), At<uint64_t>(dt, DtTcCountOffset), &tcWhy);
		if (tc < 0) {
			static std::atomic<uint32_t> s_notAppliedLogged{0};  // D2RLoader flushes each log line to disk, so log the 1st, 100th, ... of this repeated line
			if (plog::Sample(s_notAppliedLogged)) {
				plog::Warn("map behaviours: drop_tc '%s' is not applied: %s", name.c_str(), tcWhy.c_str());
			}
			continue;
		}
		static std::atomic<uint32_t> s_dropTcLogged{0};  // D2RLoader flushes each log line to disk, so log the 1st, 100th, ... of this repeated line
		if (plog::Sample(s_dropTcLogged)) {
			plog::Info("map behaviours: drop_tc '%s' (treasure class %d, %d%%) hit: rolling it for unit %u", name.c_str(), tc, c.b.dropTcs[i].second, At<uint32_t>(victim, UnitIdOffset));
		}
		Call<DropRollerFn>(DropRoller)(game, victim, killer, static_cast<uint16_t>(tc), 0, levelValue, arg3, 0, 0, 0);
	}
	c.extra = false;
}

// The monster's own roll, then the bonus picks: the same arguments again, so the same treasure class and level.
void DropRollerHook(Game* game, Unit* victim, void* killer, uint16_t tcIndex, int32_t a5, int32_t a6, int32_t a7, uint64_t a8, uint64_t a9, int32_t a10) noexcept {
	g_rollerOriginal(game, victim, killer, tcIndex, a5, a6, a7, a8, a9, a10);
	DropContext& c = t_drop;
	if (!c.active || c.extra || c.rolled || victim != c.victim) {
		return;
	}
	c.rolled = true;
	const int picks = beh::extraDrops(c.b.dropBonusPct, c.rng);
	static std::atomic<uint32_t> s_dropBonusLogged{0};  // D2RLoader flushes each log line to disk, so log the 1st, 100th, ... of this repeated line
	if (plog::Sample(s_dropBonusLogged)) {
		plog::Info("map behaviours: drop_bonus %d%%: %d extra pick(s) of the monster's own treasure class %u", c.b.dropBonusPct, picks, static_cast<unsigned>(tcIndex));
	}
	c.extra = true;
	for (int i = 0; i < picks; ++i) {
		g_rollerOriginal(game, victim, killer, tcIndex, a5, a6, a7, a8, a9, a10);
	}
	c.extra = false;
}

auto ItemMakeHook(Game* game, void* request, int32_t a3) noexcept -> uint64_t {
	DropContext& c = t_drop;
	if (!c.active || request == nullptr || At<Unit*>(request, DropRequestDropperOffset) != c.victim) {
		return g_itemMakeOriginal(game, request, a3);
	}
	const bool ethereal = beh::chance(c.b.etherealPct, c.rng);
	if (ethereal) {
		*reinterpret_cast<uint32_t*>(static_cast<uint8_t*>(request) + DropRequestFlagsOffset) |= DropFlagEthereal;
	}
	const uint64_t item = g_itemMakeOriginal(game, request, a3);
	if (item == 0) {
		return item;
	}
	auto* unit = reinterpret_cast<Unit*>(item);
	int sockets = 0;
	const bool socketRoll = beh::chance(c.b.socketedPct, c.rng);
	const bool socketable = socketRoll && On(ItemSocketable) && On(ItemSetSockets) && Call<ItemFlagFn>(ItemSocketable)(unit) != 0;
	if (socketable) {
		Call<ItemSetSocketsFn>(ItemSetSockets)(unit, static_cast<uint32_t>(beh::socketCount(c.rng)));
		sockets = Call<UnitStatFn>(UnitStat)(unit, StatSockets, 0);
	}
	static std::atomic<uint32_t> s_itemLogged{0};  // D2RLoader flushes each log line to disk, so log the 1st, 100th, ... of this repeated line
	if (plog::Sample(s_itemLogged)) {
		plog::Info("map behaviours: item %p dropped: ethereal rolled %s, socket roll %s%s, sockets now %d", static_cast<void*>(unit), ethereal ? "yes" : "no", socketRoll ? "hit" : "miss", socketRoll && !socketable ? " (not socketable)" : "", sockets);
	}
	MapDropNotify(unit, c.level, c.b);
	return item;
}

// --- Boss Drop --------------------------------------------------------------------------------------------------------------

void OnDeath(Game* game, Unit* victim, int32_t arg3, void* info) noexcept {
	if (victim == nullptr) {
		return;
	}
	const uint32_t id = At<uint32_t>(victim, UnitIdOffset);
	const int32_t type = At<int32_t>(victim, UnitTypeOffset);
	e6::DropInput in;
	e6::DropDecision decision;
	int tc = -1, level = 0;
	std::string name;
	{
		std::lock_guard<std::mutex> lock(g_lock);
		State* tracked = FindTrackedBoss(id);
		in.armed = tracked != nullptr && tracked->dropArmed;
		in.tracked = tracked != nullptr;
		in.monster = type == UnitTypeMonster;
		in.dropped = tracked != nullptr && tracked->dropped;
		in.hasKiller = info != nullptr;
		decision = e6::decideDrop(in);
		if (tracked != nullptr) {
			if (decision.action == e6::DropAction::Roll) {
				tracked->dropped = true;  // set before the call: a roll that re-enters this hook cannot roll twice
			}
			tc = tracked->dropClass;
			level = tracked->dropLevel;
			name = tracked->dropName;
		}
	}
	if (in.tracked) {
		plog::Info("eventprobe: boss drop: the tracked boss (unit %p, id %u, type %d) died: %s", static_cast<void*>(victim), id, type, e6::dropReasonName(decision.reason));
	}
	if (decision.action != e6::DropAction::Roll) {
		return;
	}
	void* killer = At<void*>(info, DeathInfoKillerOffset);
	const int32_t levelValue = Call<DropLevelFn>(DropLevel)(victim, level);
	plog::Info("eventprobe: boss drop: rolling '%s' (treasure class %d) for unit id %u after the game's own drop: killer %p, level value %d, arg3 %d", name.c_str(), tc, id, killer, levelValue, arg3);
	Call<DropRollerFn>(DropRoller)(game, victim, killer, static_cast<uint16_t>(tc), 0, levelValue, arg3, 0, 0, 0);
	plog::Info("eventprobe: boss drop: the roll returned");
}

auto DeathDropHook(Game* game, Unit* victim, int32_t arg3, void* info) noexcept -> uint64_t {
	if (g_original == nullptr) {
		return 0;  // reachable only if the loader gave no original (the install logs it and marks the part off)
	}
	bool behaviours = false;
	if (g_behavioursHook.load(std::memory_order_relaxed) && MapBehavioursAny() && game != nullptr && victim != nullptr) {
		try {
			behaviours = OpenDropContext(game, victim);
		} catch (...) {
			plog::Error("map behaviours: opening a kill failed");
		}
	}
	const uint64_t result = g_original(game, victim, arg3, info);
	if (behaviours) {
		try {
			RollDropTcs(game, victim, arg3, info);
		} catch (...) {
			plog::Error("map behaviours: rolling drop_tc failed");
		}
		t_drop.active = false;  // closed before the Boss Drop below: that roll is not a map drop
	}
	if (g_dropArmed.load(std::memory_order_relaxed) && game != nullptr) {
		try {
			OnDeath(game, victim, arg3, info);
		} catch (...) {
			plog::Error("boss drop: handling a death failed");
		}
	}
	return result;
}

// --- Treasure chest: roll the Drop when the event's chest is opened ---------------------------------------------------------------

// The event's chest is never locked. The object's InitFn 3 sets the locked bit (0x80 of the object data's flag byte) at random, and a locked chest
// only says "I need a key" and never reaches its Drop. Called once, for the plugin's own chest right after the game created it (an object unit): reads the
// flag byte through the game's getter, writes it back without the bit through the game's setter, and reads it again to check. Off (logged) when a part is missing
// or the check fails: the chest then stays as the game made it.
void UnlockPlacedChest(Unit* unit) noexcept {
	if (!On(ObjectFlagsGet) || !On(ObjectFlagsSet)) {
		plog::Warn("treasure chest: not unlocked: the object flag part (ObjectFlagsGet or ObjectFlagsSet) is off, so the chest may need a key");
		return;
	}
	const auto get = Call<ObjectFlagsGetFn>(ObjectFlagsGet);
	const e6::ChestUnlock unlock = e6::unlockChest(static_cast<uint8_t>(get(unit)));
	if (!unlock.change) {
		plog::Info("eventprobe: treasure chest: unit %p was not locked", static_cast<void*>(unit));
		return;
	}
	Call<ObjectFlagsSetFn>(ObjectFlagsSet)(unit, unlock.value);
	const uint8_t after = static_cast<uint8_t>(get(unit));
	if (after != unlock.value) {
		const char* detail = "off: the flag byte did not read back after it was set";
		g_parts[ObjectFlagsGet].status.on = false;
		g_parts[ObjectFlagsGet].status.detail = detail;
		g_parts[ObjectFlagsSet].status.on = false;
		g_parts[ObjectFlagsSet].status.detail = detail;
		plog::Warn("treasure chest: unlock did not take (flag byte 0x%02x, wrote 0x%02x): the chest may need a key; the chest unlock is off", after, unlock.value);
		return;
	}
	plog::Info("eventprobe: treasure chest: unit %p unlocked (flag byte 0x%02x -> 0x%02x)", static_cast<void*>(unit), static_cast<unsigned>(unlock.value | 0x80), after);
}

// Rolls the chest's Drop with the roller 0x5930b0 uses for a chest, once, when the operate call changed the chest's mode.
void OnChestUsed(Game* game, Unit* activator, Unit* object, int32_t modeBefore, int32_t result) {
	const int32_t modeAfter = At<int32_t>(object, UnitModeOffset);
	e6::ChestInput in;
	e6::ChestReason reason;
	int tc = -1, level = 0;
	std::string name;
	{
		std::lock_guard<std::mutex> lock(g_lock);
		State* tracked = FindTrackedChest(At<uint32_t>(object, UnitIdOffset));
		in.armed = tracked != nullptr && tracked->chestArmed;
		in.tracked = tracked != nullptr;
		in.object = At<int32_t>(object, UnitTypeOffset) == ObjectUnitType;
		in.rolled = tracked != nullptr && tracked->chestRolled;
		in.modeBefore = modeBefore;
		in.modeAfter = modeAfter;
		reason = e6::decideChest(in);
		if (tracked != nullptr) {
			if (reason == e6::ChestReason::Rolled) {
				tracked->chestRolled = true;  // set before the call: a roll that re-enters the hook cannot roll twice
			}
			tc = tracked->chestClass;
			level = tracked->dropLevel;
			name = tracked->chestDropName;
		}
	}
	plog::Info("eventprobe: treasure chest: the operate call on the tracked chest (unit %p, activator %p) returned %d, mode %d -> %d: %s", static_cast<void*>(object), static_cast<void*>(activator), result, modeBefore, modeAfter,
	           e6::chestReasonName(reason));
	if (reason != e6::ChestReason::Rolled) {
		return;
	}
	if (activator == nullptr) {
		plog::Warn("treasure chest: Drop '%s' not rolled: the operate call gave no activator for the roller", name.c_str());
		return;
	}
	const uint32_t dataFlag = At<uint8_t>(game, GameDataFlagOffset);
	if (dataFlag >= DataFlagLimit) {
		plog::Warn("treasure chest: Drop '%s' not rolled: the game's data flag %u is not below 4", name.c_str(), dataFlag);
		return;
	}
	// For an object 0x43e8a0 ignores the level id and uses the area level of the object's own room (0x43e8dd..0x43e917); `level` only matters for a monster.
	const int32_t levelValue = Call<DropLevelFn>(DropLevel)(object, level);
	const void* compiled = Call<TcLookupFn>(TcLookup)(dataFlag, static_cast<uint16_t>(tc), levelValue);
	plog::Info("eventprobe: treasure chest: Drop '%s' (treasure class %d): data flag %u, level value %d, compiled class %p", name.c_str(), tc, dataFlag, levelValue, compiled);
	if (compiled == nullptr) {
		plog::Warn("treasure chest: Drop '%s' not rolled: the game has no compiled treasure class %d for level value %d", name.c_str(), tc, levelValue);
		return;
	}
	alignas(16) uint8_t out[ChestOutSize] = {};
	int32_t extra = 0;
	const uint64_t rolled = Call<TcRollerFn>(TcRoller)(game, object, activator, compiled, ChestSelector, ChestRollTier, 0, out, &extra, ChestMaxItems);
	plog::Info("eventprobe: treasure chest: the roll returned %llu (tier %d, forced quality %d, at most %d items)", static_cast<unsigned long long>(rolled), ChestRollTier, ChestSelector, ChestMaxItems);
}

auto OperateHook(OperateContext* ctx) noexcept -> int32_t {
	if (g_operateOriginal == nullptr) {
		return 0;  // reachable only if the loader gave no original (the install logs it and marks the part off)
	}
	if (!g_chestArmed.load(std::memory_order_relaxed) || ctx == nullptr || ctx->object == nullptr || ctx->game == nullptr) {
		return g_operateOriginal(ctx);
	}
	Game* const game = ctx->game;
	Unit* const object = ctx->object;
	Unit* const activator = ctx->activator;
	bool candidate = false;
	int32_t before = 0;
	try {
		const uint32_t id = At<uint32_t>(object, UnitIdOffset);
		std::lock_guard<std::mutex> lock(g_lock);
		const State* tracked = FindTrackedChest(id);
		candidate = tracked != nullptr && !tracked->chestRolled && At<int32_t>(object, UnitTypeOffset) == ObjectUnitType;
		if (candidate) {
			before = At<int32_t>(object, UnitModeOffset);
		}
	} catch (...) {
		candidate = false;
	}
	const int32_t result = g_operateOriginal(ctx);
	if (candidate) {
		try {
			OnChestUsed(game, activator, object, before, result);
		} catch (...) {
			plog::Error("treasure chest: handling the chest being used failed");
		}
	}
	return result;
}

// --- Ambush: waves of the event's monster, counted by the SDK monster death listener -----------------------------------------------

void __cdecl OnMonsterDeath(const D2RL::PluginContext*, const D2RL::Lifecycle::MonsterDeathEvent* event, void*) noexcept {
	if (!g_ambushActive.load(std::memory_order_relaxed) || event == nullptr) {
		return;
	}
	try {
		std::lock_guard<std::mutex> lock(g_lock);
		const uint32_t id = event->monster.id;
		for (auto& entry : g_states) {
			State& s = entry.second;
			if (s.waves.onDeath(id)) {
				if (s.deathProbes < MaxUntrackedDeathProbes) {
					++s.deathProbes;
					plog::Info("eventprobe: ambush: unit id %u of wave %d died, %zu left", id, s.waves.wavesStarted(), s.waves.remaining());
				}
				return;
			}
		}
		for (auto& entry : g_states) {
			State& s = entry.second;
			if (s.waves.phase() == e6::WaveTracker::Phase::Fighting && s.untrackedProbes < MaxUntrackedDeathProbes) {
				++s.untrackedProbes;
				plog::Info("eventprobe: ambush: a death during wave %d was not one of its units (unit id %u, class %u)", s.waves.wavesStarted(), id, event->monster.classId);
				return;
			}
		}
	} catch (...) {
		plog::Error("ambush: handling a monster death failed");
	}
}

// Resolves an event's Drop to a treasure class index. `tag` names the feature in the log. False (logged) when it cannot be used.
auto ResolveDropClass(const ev::Event& event, void* player, const char* tag, int* tcOut) -> bool {
	std::string why;
	const uint8_t* dt = player == nullptr ? nullptr : TablesBase(At<uint8_t>(player, UnitDataFlagOffset), &why);
	void* map = dt == nullptr ? nullptr : At<void*>(dt, DtTcMapOffset);  // re-read, never cached
	if (dt == nullptr || map == nullptr) {
		plog::Warn("Drop '%s' of event '%s' is not applied: %s", event.drop.c_str(), event.name.c_str(), dt == nullptr ? (player == nullptr ? "no player unit" : why.c_str()) : "the treasure class name map is null");
		return false;
	}
	const uint64_t tcCount = At<uint64_t>(dt, DtTcCountOffset);
	const int32_t found = Call<NameToRowFn>(NameToRow)(map, event.drop.c_str(), 0);
	std::string tcWhy;
	const int tc = e6::tcIndexFromLookup(found, tcCount, &tcWhy);
	plog::Info("eventprobe: %s: Drop '%s': name map %p, the game has %llu treasure classes, lookup gave %d%s%s", tag, event.drop.c_str(), map, static_cast<unsigned long long>(tcCount), found, tc < 0 ? ", refused: " : "",
	           tc < 0 ? tcWhy.c_str() : "");
	if (tc < 0) {
		plog::Warn("Drop '%s' of event '%s' is not applied: %s", event.drop.c_str(), event.name.c_str(), tcWhy.c_str());
		return false;
	}
	*tcOut = tc;
	return true;
}

// Finds 0x402f50 as the target of the call at SpawnObject + 0xff, and checks the bytes around it. Sets the FreeSpot part.
void ResolveFreeSpot(const CodeRange* ranges, int count) noexcept {
	Part& part = g_parts[FreeSpot];
	part.address = nullptr;
	part.status = {part.name, false, ""};
	if (!On(SpawnObject)) {
		part.status.detail = "off: the SpawnObject part it is found from is off";
		return;
	}
	const uint8_t* handler = g_parts[SpawnObject].address;
	const CodeRange* home = nullptr;
	for (int i = 0; i < count; ++i) {
		if (handler >= ranges[i].start && handler < ranges[i].start + ranges[i].size) {
			home = &ranges[i];
		}
	}
	if (home == nullptr) {
		part.status.detail = "off: the SpawnObject handler is not in an executable section";
		return;
	}
	const size_t available = std::min<size_t>(FreeSpotReach, static_cast<size_t>(home->start + home->size - handler));
	const int64_t offset = e6::callTargetOffset(handler, available, FreeSpotSite);
	if (offset == e6::kNoTarget) {
		part.status.detail = "off: no call at SpawnObject + 0xff";
		return;
	}
	if (FreeSpotSite < sizeof(FreeSpotContext) || std::memcmp(handler + FreeSpotSite - sizeof(FreeSpotContext), FreeSpotContext, sizeof(FreeSpotContext)) != 0) {
		part.status.detail = "off: the bytes before the call at SpawnObject + 0xff are not the expected argument setup";
		return;
	}
	const uint8_t* target = handler + offset;
	const CodeRange* targetHome = nullptr;
	for (int i = 0; i < count; ++i) {
		if (target >= ranges[i].start && target + sizeof(FreeSpotPrefix) <= ranges[i].start + ranges[i].size) {
			targetHome = &ranges[i];
		}
	}
	if (targetHome == nullptr) {
		part.status.detail = "off: the call target is not in an executable section";
		return;
	}
	if (std::memcmp(target, FreeSpotPrefix, sizeof(FreeSpotPrefix)) != 0) {
		part.status.detail = "off: the call target does not start with the expected function prologue";
		return;
	}
	part.address = target;
	part.status.on = true;
	part.status.detail = "found as the call target inside SpawnObject";
}

auto SummaryNow(bool populatorOn) -> std::string {
	// A Shrine is placed from the room populator hook, so it needs that too.
	const bool shrine = populatorOn && On(FreeSpot) && On(CreateObject) && On(DataTables);
	return std::string("Shrine events ") + (shrine ? "on" : "off") + ", Boss Drop " + (g_dropHook.load() ? "on" : "off") + ", Ambush " + (g_deathListener ? "on" : "one wave only (no death listener)") +
	       ", Treasure chest Drop " + (shrine && g_chestHook.load() ? "on" : "off") + " (written, not verified in game)";
}

}  // namespace

void InstallEvent6b(const D2RL::PluginContext* context, const std::vector<SignatureOverride>& overrides, const Part& dataTables, const Part& nameToRow, bool populatorOn) {
	g_context = context;
	g_populatorOn = populatorOn;
	// DataTablesBase and NameToRow were already found by event_apply.cpp: share them instead of scanning again.
	g_parts[DataTables] = dataTables;
	g_parts[NameToRow] = nameToRow;
	CodeRange ranges[8];
	const int count = CodeRanges(context->exeBase, ranges, 8);
	for (int i = 0; i < PartCount; ++i) {
		if (i != FreeSpot && i != DataTables && i != NameToRow) {
			ResolvePart(context, overrides, ranges, count, g_parts[i]);
		}
	}
	ResolveFreeSpot(ranges, count);
	// The death hook goes in at load, only when everything it calls was found. Never installed half-on.
	if (On(DeathDrop) && On(DropRoller) && On(DropLevel) && On(DataTables) && On(NameToRow)) {
		Part& part = g_parts[DeathDrop];
		const uint64_t rva = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(part.address) - context->exeBase);
		const bool hooked = context->InstallInlineHook(rva, part.guard, part.hookSize, reinterpret_cast<void*>(&DeathDropHook), reinterpret_cast<void**>(&g_original));
		const bool ok = HookOutcome(part, hooked, g_original != nullptr);
		g_dropHook = ok;
	} else if (On(DeathDrop)) {
		g_parts[DeathDrop].status.on = false;
		g_parts[DeathDrop].status.detail = "off: not hooked, because a part it needs was not found";
	}
	// The map drop behaviours (issue #538): the roller and item hooks, only when the death hook is in and the parts they call were found.
	if (g_dropHook.load() && On(DropRoller) && On(ItemMake) && On(UnitStat) && On(UnitRoom) && On(RoomLevelId)) {
		const auto install = [&](PartId id, void* hook, void** original) {
			Part& part = g_parts[id];
			const uint64_t rva = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(part.address) - context->exeBase);
			const bool hooked = context->InstallInlineHook(rva, part.guard, part.hookSize, hook, original);
			return HookOutcome(part, hooked, *original != nullptr);
		};
		const bool roller = install(DropRoller, reinterpret_cast<void*>(&DropRollerHook), reinterpret_cast<void**>(&g_rollerOriginal));
		const bool item = roller && install(ItemMake, reinterpret_cast<void*>(&ItemMakeHook), reinterpret_cast<void**>(&g_itemMakeOriginal));
		g_behavioursHook = roller && item;
		if (!g_behavioursHook.load()) {
			plog::Warn("map behaviours: off (the DropRoller or ItemMake hook did not go in)");
		}
	} else {
		plog::Info("map behaviours: off (a part it needs was not found, or the death hook is off)");
	}
	// The operate hook, for a Treasure chest's Drop: likewise only when everything the roll calls was found.
	if (On(ChestOperate) && On(TcLookup) && On(TcRoller) && On(DropLevel) && On(DataTables) && On(NameToRow)) {
		Part& part = g_parts[ChestOperate];
		const uint64_t rva = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(part.address) - context->exeBase);
		const bool hooked = context->InstallInlineHook(rva, part.guard, part.hookSize, reinterpret_cast<void*>(&OperateHook), reinterpret_cast<void**>(&g_operateOriginal));
		const bool ok = HookOutcome(part, hooked, g_operateOriginal != nullptr);
		g_chestHook = ok;
	} else if (On(ChestOperate)) {
		g_parts[ChestOperate].status.on = false;
		g_parts[ChestOperate].status.detail = "off: not hooked, because a part it needs was not found";
	}
	// The Ambush counts kills through the SDK's monster death listener; without it an Ambush runs one wave and ends on its time-out.
	const D2RL::LifecycleService* lifecycle = nullptr;
	if (context->QueryService(&lifecycle) == D2RL::ServiceQueryResult::Success && lifecycle != nullptr && D2RL::HasLifecycleServiceField(lifecycle, D2RL::LifecycleServiceRequiredSize) &&
	    lifecycle->registerMonsterDeathListener != nullptr) {
		D2RL::Lifecycle::MonsterDeathListener listener{};
		listener.structSize = D2RL::Lifecycle::MonsterDeathListenerSize;
		listener.callback = &OnMonsterDeath;
		D2RL::Lifecycle::ListenerHandle handle = D2RL::Lifecycle::InvalidHandle;
		g_deathListener = lifecycle->registerMonsterDeathListener(context, &listener, &handle) == D2RL::Lifecycle::Result::Success;
		if (!g_deathListener) {
			plog::Warn("D2RLoader refused the monster death listener: an Ambush runs one wave only");
		}
	} else {
		plog::Warn("D2RLoader has no Lifecycle service of the expected version: an Ambush runs one wave only");
	}
	plog::Info("phase 6b: %s", SummaryNow(populatorOn).c_str());
	for (int i = 0; i < PartCount; ++i) {
		if (i != DataTables && i != NameToRow) {  // logged once, by event_apply.cpp
			plog::Info("part %s: %s", g_parts[i].name, g_parts[i].status.detail.c_str());
		}
	}
}

auto Event6bPartStatuses() -> std::vector<PartStatus> {
	std::vector<PartStatus> out;
	for (int i = 0; i < PartCount; ++i) {
		if (i != DataTables && i != NameToRow) {  // listed once, by event_apply.cpp
			out.push_back(g_parts[i].status);
		}
	}
	return out;
}

auto Event6bSummary() -> std::string {
	return "phase 6b: " + SummaryNow(g_populatorOn);
}

auto Event6bPrepareShrine(const ev::Event& event, const std::vector<std::string>& objectClasses, const std::vector<std::string>& objectNames, int level, void* player, std::string* why) -> bool {
	if (!On(FreeSpot) || !On(CreateObject) || !On(DataTables)) {
		*why = "a Shrine part is off (FreeSpot, CreateObject or DataTablesBase; see the parts above)";
		return false;
	}
	if (player == nullptr) {
		*why = "no player unit to read the game's data flag from";
		return false;
	}
	std::string tablesWhy;
	const uint8_t* dt = TablesBase(At<uint8_t>(player, UnitDataFlagOffset), &tablesWhy);
	if (dt == nullptr) {
		*why = tablesWhy;
		return false;
	}
	const uint64_t gameRows = At<uint32_t>(dt, DtObjectCountOffset);
	const e6::ObjectLookup lookup = e6::resolveObject(objectClasses, objectNames, event.spawn, gameRows);
	const std::string text = e6::describeObjectLookup(lookup, event.spawn, objectNames.size(), gameRows);
	plog::Info("eventprobe: shrine: Spawn '%s': %s (objects.txt: %zu lines, game: %llu rows)", event.spawn.c_str(), text.c_str(), objectNames.size(), static_cast<unsigned long long>(gameRows));
	if (lookup.result != e6::ObjectResult::Found) {
		*why = text;
		return false;
	}
	std::lock_guard<std::mutex> lock(g_lock);
	State& s = g_states[level];
	s.shrineReady = true;
	s.shrinePlaced = false;
	s.shrineRow = lookup.row;
	s.shrineSpawn = event.spawn;
	return true;
}

void Event6bMapOpened(int level) noexcept {
	std::lock_guard<std::mutex> lock(g_lock);
	g_states.erase(level);
	RecomputeFlagsLocked();
	++g_epoch;
}

void Event6bArmBossDrop(const ev::Event& event, int level, void* player) {
	if ((event.kind != ev::Kind::Boss && event.kind != ev::Kind::Treasure) || event.drop.empty()) {
		return;
	}
	if (!g_dropHook.load()) {
		plog::Warn("Drop '%s' of event '%s' is not applied: the monster death hook is off (see the parts above)", event.drop.c_str(), event.name.c_str());
		return;
	}
	int tc = -1;
	if (!ResolveDropClass(event, player, event.kind == ev::Kind::Boss ? "boss drop" : "treasure monster", &tc)) {
		return;
	}
	std::lock_guard<std::mutex> lock(g_lock);
	State& s = g_states[level];
	s.dropArmed = true;
	s.dropName = event.drop;
	s.dropClass = tc;
	s.dropLevel = level;
	RecomputeFlagsLocked();
}

auto Event6bPlacesObject(const ev::Event& event, ev::NameKind spawnKind) noexcept -> bool {
	const e6::Route route = e6::routeEvent(event.kind, spawnKind);
	return route == e6::Route::Shrine || route == e6::Route::TreasureChest;
}

auto Event6bPrepareChest(const ev::Event& event, const std::vector<std::string>& objectClasses, const std::vector<std::string>& objectNames, int level, void* player, std::string* why) -> bool {
	if (!Event6bPrepareShrine(event, objectClasses, objectNames, level, player, why)) {
		return false;
	}
	{
		std::lock_guard<std::mutex> lock(g_lock);
		g_states[level].objectIsChest = true;
		g_states[level].dropLevel = level;
	}
	if (event.drop.empty()) {
		plog::Info("treasure chest '%s' has no Drop: it is placed as the game's own object", event.name.c_str());
		return true;
	}
	int tc = -1;
	if (!g_chestHook.load()) {
		plog::Warn("Drop '%s' of event '%s' is not applied: the object operate hook is off (see the parts above); the chest is still placed", event.drop.c_str(), event.name.c_str());
	} else if (ResolveDropClass(event, player, "treasure chest", &tc)) {
		std::lock_guard<std::mutex> lock(g_lock);
		State& s = g_states[level];
		s.chestArmed = true;
		s.chestDropName = event.drop;
		s.chestClass = tc;
		RecomputeFlagsLocked();
	}
	return true;
}

void Event6bBossSpawned(int level, void* unit) noexcept {
	if (!g_dropArmed.load() || unit == nullptr) {
		return;
	}
	try {
		const uint32_t id = At<uint32_t>(unit, UnitIdOffset);
		const int32_t type = At<int32_t>(unit, UnitTypeOffset);
		std::lock_guard<std::mutex> lock(g_lock);
		const auto it = g_states.find(level);
		if (it == g_states.end() || !it->second.dropArmed) {
			return;
		}
		State& s = it->second;
		s.bossTracked = type == UnitTypeMonster;
		s.bossId = id;
		s.dropped = false;
		plog::Info("eventprobe: boss drop: tracking unit %p (type %d, id %u)%s", unit, type, id, s.bossTracked ? "" : "; not a monster, so no extra drop");
	} catch (...) {
		plog::Error("boss drop: tracking the boss failed");
	}
}

void Event6bFirstSpawned(void* /*game*/, int level, ev::Kind kind, void* unit) noexcept {
	if (unit == nullptr) {
		return;
	}
	if (kind == ev::Kind::Boss) {
		Event6bBossSpawned(level, unit);
	} else if (kind == ev::Kind::Treasure) {
		Event6bBossSpawned(level, unit);  // rolls the Drop on its death (the same death hook as a Boss)
	}
}

void Event6bArmAmbush(int level, const ev::Event& event, const std::vector<ep::Spawn>& spawns, const ep::Rect& serverRect, void* room) noexcept {
	try {
		std::lock_guard<std::mutex> lock(g_lock);
		const int waves = g_deathListener ? (std::max)(1, event.waves) : 1;
		State& s = g_states[level];
		s.waves.arm(waves);
		s.ambushArmed = true;
		s.ambushActive = true;
		s.waveSpawns = spawns;
		s.ambushRect = serverRect;
		s.firstWaveDone = false;
		s.ambushName = event.name;
		s.untrackedProbes = 0;
		s.deathProbes = 0;
		RecomputeFlagsLocked();
		plog::Info("eventprobe: ambush '%s': armed %d wave(s) of %zu in room %p; the first comes when a player is in that room%s (time-out %llu ms, gap %llu ms)", event.name.c_str(), waves, spawns.size(), room,
		           g_deathListener ? "" : " (no death listener: this is the only wave)", static_cast<unsigned long long>(e6::kWaveTimeoutMs), static_cast<unsigned long long>(e6::kWaveGapMs));
	} catch (...) {
		plog::Error("ambush: arming failed");
	}
}

bool Event6bAmbushTick(void* game, int playerLevel, void* playerRoom, AmbushRectFn roomRect, AmbushSpawnFn spawn) noexcept {
	if (!g_ambushActive.load(std::memory_order_relaxed) || spawn == nullptr || roomRect == nullptr || game == nullptr) {
		return false;
	}
	// One wave to spawn: its map's level, its units, and the epoch it was planned in.
	struct Wave {
		int level;
		std::vector<ep::Spawn> spawns;
		uint64_t epoch;
		int number;
	};
	try {
		std::vector<Wave> waves;
		ep::Rect playerRect;
		const bool rectOk = playerRoom != nullptr && roomRect(playerRoom, &playerRect);  // read before the lock: the engine's room, not ours
		{
			std::lock_guard<std::mutex> lock(g_lock);
			const uint64_t now = GetTickCount64();
			for (auto& entry : g_states) {
				State& s = entry.second;
				if (!s.ambushActive) {
					continue;
				}
				// Only the Ambush of the map the player is on can see the player in its event room.
				const ep::Rect& want = s.ambushRect;
				const bool inRoom = entry.first == playerLevel && rectOk && playerRect.x == want.x && playerRect.y == want.y && playerRect.w == want.w && playerRect.h == want.h;
				const auto before = s.waves.phase();
				const e6::WaveTracker::Step step = s.waves.step(inRoom, now);
				if (before == e6::WaveTracker::Phase::Fighting && s.waves.phase() != e6::WaveTracker::Phase::Fighting) {
					plog::Info("eventprobe: ambush '%s': wave %d of %d ended: %s", s.ambushName.c_str(), s.waves.wavesStarted(), s.waves.wavesTotal(), e6::waveEndName(s.waves.lastEnd()));
				}
				if (step == e6::WaveTracker::Step::Finished) {
					plog::Info("ambush '%s': finished after %d wave(s)", s.ambushName.c_str(), s.waves.wavesStarted());
					s.ambushActive = false;
					continue;
				}
				if (step == e6::WaveTracker::Step::SpawnWave) {
					waves.push_back(Wave{entry.first, s.waveSpawns, g_epoch.load(), s.waves.wavesStarted()});
				}
			}
			RecomputeFlagsLocked();
		}
		bool started = false;
		for (const Wave& wave : waves) {
			// A wave only starts while the player is in its room (the step above), so `playerRoom` is that room.
			std::vector<uint32_t> ids;
			for (const ep::Spawn& s : wave.spawns) {
				void* unit = nullptr;
				if (spawn(game, playerRoom, s, &unit) && unit != nullptr && At<int32_t>(unit, UnitTypeOffset) == UnitTypeMonster) {
					ids.push_back(At<uint32_t>(unit, UnitIdOffset));
				}
			}
			std::lock_guard<std::mutex> lock(g_lock);
			const auto it = g_states.find(wave.level);
			if (wave.epoch != g_epoch.load() || it == g_states.end()) {
				continue;  // the map changed or the game was left while spawning; the state was reset
			}
			State& s = it->second;
			s.waves.waveSpawned(ids, GetTickCount64());
			plog::Info("eventprobe: ambush '%s': wave %d of %d: the game made %zu of %zu units in the event room %p (game %p)", s.ambushName.c_str(), wave.number, s.waves.wavesTotal(), ids.size(), wave.spawns.size(), playerRoom, game);
			if (!s.waves.active()) {
				plog::Warn("ambush '%s' abandoned: %s", s.ambushName.c_str(), e6::waveEndName(s.waves.lastEnd()));
				s.ambushActive = false;
			}
			if (!ids.empty() && !s.firstWaveDone) {
				s.firstWaveDone = true;
				started = started || wave.level == playerLevel;  // the first wave spawned: the event counts as placed (announced by the caller)
			}
		}
		if (!waves.empty()) {
			std::lock_guard<std::mutex> lock(g_lock);
			RecomputeFlagsLocked();
		}
		return started;
	} catch (...) {
		plog::Error("ambush: spawning a wave failed");
		std::lock_guard<std::mutex> lock(g_lock);
		for (auto& entry : g_states) {
			entry.second.ambushActive = false;  // a failed spawn stops every Ambush: the state it came from is not known here
		}
		RecomputeFlagsLocked();
	}
	return false;
}

bool Event6bPlaceShrine(void* rawGame, int level, void* room, const ep::Rect& serverRect) noexcept {
	try {
		int row = -1;
		{
			std::lock_guard<std::mutex> lock(g_lock);
			const auto it = g_states.find(level);
			if (it == g_states.end() || !it->second.shrineReady || it->second.shrinePlaced) {
				return false;
			}
			it->second.shrinePlaced = true;
			row = it->second.shrineRow;
		}
		auto* game = static_cast<Game*>(rawGame);
		if (game == nullptr || room == nullptr || !On(FreeSpot) || !On(CreateObject) || !On(DataTables)) {
			plog::Warn("shrine not placed: %s", game == nullptr || room == nullptr ? "no game or room" : "a part is off");
			return false;
		}
		// The row is checked again against the game's count now: a table load since map open would make it stale.
		std::string why;
		const uint8_t* dt = TablesBase(At<uint8_t>(game, GameDataFlagOffset), &why);
		if (dt == nullptr || row < 0 || static_cast<uint64_t>(row) >= At<uint32_t>(dt, DtObjectCountOffset)) {
			plog::Warn("shrine not placed: objects row %d is not below the game's row count%s%s", row, dt == nullptr ? ": " : "", dt == nullptr ? why.c_str() : "");
			return false;
		}
		// The engine's search takes the first clear footprint in a growing square, which can be in the next room (game check of
		// 2026-11-12). Try several start points and smaller footprints and keep only a spot inside the event room's rect.
		const e6::SpotBox box{serverRect.x, serverRect.y, serverRect.w, serverRect.h};
		const std::vector<e6::SpotAttempt> attempts = e6::freeSpotAttempts(box, FreeSpotRadius);
		if (attempts.empty()) {
			plog::Warn("shrine not placed: the event room is too small to place anything");
			return false;
		}
		int32_t xy[2] = {0, 0};
		void* landed = nullptr;
		bool found = false;
		int tried = 0;
		for (const e6::SpotAttempt& attempt : attempts) {
			xy[0] = attempt.x;
			xy[1] = attempt.y;
			landed = nullptr;
			++tried;
			Call<FreeSpotFn>(FreeSpot)(room, xy, attempt.radius, FreeSpotMask, &landed);
			const bool inside = landed == room && e6::spotInBox(box, xy[0], xy[1]);
			plog::Info("eventprobe: shrine: objects row %d, free spot try %d: near (%d,%d) radius %d (mask 0x%x) gave %s (%d,%d) room %p (%s the event room)", row, tried, attempt.x, attempt.y, attempt.radius, FreeSpotMask, landed == nullptr ? "nothing" : "a spot", xy[0], xy[1], landed, inside ? "in" : "NOT in");
			if (inside) {
				found = true;
				break;
			}
		}
		if (!found) {
			// The neighbouring room may not be populated yet; refuse rather than create an object there.
			plog::Warn("shrine not placed: none of %d free-spot tries gave a spot inside the event room (%d,%d) %d x %d", tried, box.x, box.y, box.w, box.h);
			return false;
		}
		void* unit = Call<CreateObjectFn>(CreateObject)(ObjectUnitType, row, xy[0], xy[1], game, landed, ObjectCreateFlag, 0, 0, 0);
		if (unit == nullptr) {
			plog::Warn("shrine not placed: the game refused to create objects row %d at (%d,%d)", row, xy[0], xy[1]);
			return false;
		}
		plog::Info("eventprobe: shrine: created objects row %d: unit %p (type %d, id %u) at (%d,%d)", row, unit, At<int32_t>(unit, UnitTypeOffset), At<uint32_t>(unit, UnitIdOffset), xy[0], xy[1]);
		std::lock_guard<std::mutex> lock(g_lock);
		const auto it = g_states.find(level);
		if (it != g_states.end() && it->second.objectIsChest) {  // a Treasure chest: remember it for the Drop roll when it is opened
			State& s = it->second;
			s.chestTracked = At<int32_t>(unit, UnitTypeOffset) == ObjectUnitType;
			s.chestId = At<uint32_t>(unit, UnitIdOffset);
			s.chestRolled = false;
			if (s.chestTracked) {
				UnlockPlacedChest(static_cast<Unit*>(unit));
			}
			plog::Info("eventprobe: treasure chest: tracking unit %p (id %u, mode %d)%s", unit, s.chestId, At<int32_t>(unit, UnitModeOffset), s.chestArmed ? "" : "; no armed Drop");
		}
		return true;
	} catch (...) {
		plog::Error("shrine: placing the object failed");
		return false;
	}
}

bool Event6bPlaceObjectAt(void* rawGame, void* room, const ep::Rect& serverRect, const std::vector<std::string>& objectClasses, const std::vector<std::string>& objectNames, const std::string& spec, int x, int y,
                          int* placedX, int* placedY) noexcept {
	try {
		auto* game = static_cast<Game*>(rawGame);
		if (game == nullptr || room == nullptr || !On(FreeSpot) || !On(CreateObject) || !On(DataTables)) {
			plog::Warn("arena spot object '%s' not placed: %s", spec.c_str(), game == nullptr || room == nullptr ? "no game or room" : "a part is off");
			return false;
		}
		std::string why;
		const uint8_t* dt = TablesBase(At<uint8_t>(game, GameDataFlagOffset), &why);
		if (dt == nullptr) {
			plog::Warn("arena spot object '%s' not placed: %s", spec.c_str(), why.c_str());
			return false;
		}
		const e6::ObjectLookup lookup = e6::resolveObject(objectClasses, objectNames, spec, At<uint32_t>(dt, DtObjectCountOffset));
		if (lookup.result != e6::ObjectResult::Found) {
			plog::Warn("arena spot object not placed: %s", e6::describeObjectLookup(lookup, spec, objectNames.size(), At<uint32_t>(dt, DtObjectCountOffset)).c_str());
			return false;
		}
		// The spot first, then the engine's free-spot search with a growing footprint; a result in another room is refused (the neighbour may not be populated yet).
		const e6::SpotBox box{serverRect.x, serverRect.y, serverRect.w, serverRect.h};
		for (const int radius : {1, 3, FreeSpotRadius}) {
			int32_t xy[2] = {x + 1, y + 1};  // the engine's free-spot search (0x402f50) starts at (-1,-1) from this, so it tries the spot itself first
			void* landed = nullptr;
			Call<FreeSpotFn>(FreeSpot)(room, xy, radius, FreeSpotMask, &landed);
			const bool inside = landed == room && e6::spotInBox(box, xy[0], xy[1]);
			plog::Info("arena spot: objects row %d, free spot near (%d,%d) radius %d gave (%d,%d) room %p (%s the room)", lookup.row, x, y, radius, xy[0], xy[1], landed, inside ? "in" : "NOT in");
			if (!inside) {
				continue;
			}
			void* unit = Call<CreateObjectFn>(CreateObject)(ObjectUnitType, lookup.row, xy[0], xy[1], game, landed, ObjectCreateFlag, 0, 0, 0);
			if (unit == nullptr) {
				plog::Warn("arena spot object not placed: the game refused to create objects row %d at (%d,%d)", lookup.row, xy[0], xy[1]);
				return false;
			}
			*placedX = xy[0];
			*placedY = xy[1];
			return true;
		}
		plog::Warn("arena spot object '%s' not placed: no free spot near (%d,%d) inside the room", spec.c_str(), x, y);
		return false;
	} catch (...) {
		plog::Error("arena spot: placing the object failed");
		return false;
	}
}

void Event6bGameLeft() noexcept {
	std::lock_guard<std::mutex> lock(g_lock);
	g_states.clear();
	RecomputeFlagsLocked();
	++g_epoch;
}

}  // namespace carto
