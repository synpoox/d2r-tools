#include "engine/event_6b.h"

#include "core/part_signatures.h"
#include "engine/scan.h"
#include "event6b.h"
#include "log.h"

#include <windows.h>

#include <algorithm>
#include <atomic>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

namespace carto {

namespace {

namespace e6 = cartographer::event6b;
namespace ep = cartographer::eventplan;
namespace ev = cartographer::events;

// --- Engine facts. Build 93847; each is recorded with its evidence in docs/reverse-engineering/cartographer.md section 13.
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
constexpr int32_t FleeDuration = 600;        // 0x597030's last argument. Its unit is not known (callers pass 0x14 and skill results); a guess, logged
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
using OperateFn = int32_t (*)(Game*, Unit* activator, Unit* object) noexcept;
using FleeSetupFn = void (*)(Game*, Unit* source, Unit* target, int32_t r9, int32_t arg5, int32_t arg6) noexcept;
using FleeBlockFn = int32_t (*)(Unit*) noexcept;
using FleeTargetFn = uint8_t (*)(Unit*, uint8_t, uint8_t, uint8_t, uint8_t) noexcept;
using TcLookupFn = const void* (*)(uint32_t dataFlag, uint16_t tcIndex, int32_t level) noexcept;
using TcRollerFn = uint64_t (*)(Game*, Unit* a, Unit* b, const void* tc, int32_t s20, int32_t s28, int32_t s30, void* out, int32_t* s40, int32_t s48) noexcept;

enum PartId { DeathDrop, DropRoller, DropLevel, SpawnObject, FreeSpot, CreateObject, OperateObject, FleeSetup, FleeBlock, FleeTarget, TcLookup, TcRoller, DataTables, NameToRow, PartCount };

Part g_parts[PartCount] = {
	{"DeathDrop", sig::DeathDrop, 15, false, nullptr, {}, {}},
	{"DropRoller", sig::DropRoller, 0, false, nullptr, {}, {}},
	{"DropLevel", sig::DropLevel, 0, false, nullptr, {}, {}},
	{"SpawnObject", sig::SpawnObject, 0, false, nullptr, {}, {}},
	{"FreeSpot", nullptr, 0, false, nullptr, {}, {}},  // no signature: the call target inside SpawnObject
	{"CreateObject", sig::CreateObject, 0, false, nullptr, {}, {}},
	{"OperateObject", sig::OperateObject, 16, false, nullptr, {}, {}},
	{"FleeSetup", sig::FleeSetup, 0, false, nullptr, {}, {}},
	{"FleeBlock", sig::FleeBlock, 0, false, nullptr, {}, {}},
	{"FleeTarget", sig::FleeTarget, 0, false, nullptr, {}, {}},
	{"TcLookup", sig::TcLookup, 0, false, nullptr, {}, {}},
	{"TcRoller", sig::TcRoller, 0, false, nullptr, {}, {}},
	{"DataTablesBase", nullptr, 0, false, nullptr, {}, {}},  // shared with event_apply.cpp (copied in at load)
	{"NameToRow", nullptr, 0, false, nullptr, {}, {}},       // shared with event_apply.cpp (copied in at load)
};

const D2RL::PluginContext* g_context = nullptr;
DeathDropFn g_original = nullptr;
OperateFn g_operateOriginal = nullptr;
bool g_populatorOn = false;            // the room populator hook (event_apply.cpp) is in; set once at load
bool g_deathListener = false;          // the SDK monster death listener is registered (the Ambush needs it to count kills)
std::atomic<bool> g_dropHook{false};   // the death hook is installed and every part it calls was found
std::atomic<bool> g_dropArmed{false};  // a Boss or Treasure monster event with a resolved Drop is armed (read on every monster death: lock only when set)
std::atomic<bool> g_chestHook{false};  // the operate hook is installed and every part the chest roll calls was found
std::atomic<bool> g_chestArmed{false}; // a Treasure chest with a resolved Drop is armed (read on every object use: lock only when set)
std::atomic<bool> g_ambushActive{false};  // an Ambush is armed and not finished (read on every tick and monster death)
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
	void* fleeSource = nullptr;  // the player unit the monster flees from (checked again when used); null: none
	uint32_t fleeSourceId = 0;
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
	bool firstWaveDone = false;  // the first wave made a unit (the event is announced then)
	cartographer::event6b::WaveTracker waves;
	ep::Rect ambushRect;         // the event room's server rect: the player's room is matched by it (a room pointer could be freed or reused between ticks)
	std::vector<ep::Spawn> waveSpawns;
	std::string ambushName;
	int untrackedProbes = 0;
};
std::mutex g_lock;
State g_state;

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
		in.armed = g_state.dropArmed;
		in.tracked = g_state.bossTracked && g_state.bossId == id;
		in.monster = type == UnitTypeMonster;
		in.dropped = g_state.dropped;
		in.hasKiller = info != nullptr;
		decision = e6::decideDrop(in);
		if (decision.action == e6::DropAction::Roll) {
			g_state.dropped = true;  // set before the call: a roll that re-enters this hook cannot roll twice
		}
		tc = g_state.dropClass;
		level = g_state.dropLevel;
		name = g_state.dropName;
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
	const uint64_t result = g_original(game, victim, arg3, info);
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

// Rolls the chest's Drop with the roller 0x5930b0 uses for a chest, once, when the operate call changed the chest's mode.
void OnChestUsed(Game* game, Unit* activator, Unit* object, int32_t modeBefore, int32_t result) {
	const int32_t modeAfter = At<int32_t>(object, UnitModeOffset);
	e6::ChestInput in;
	e6::ChestReason reason;
	int tc = -1, level = 0;
	std::string name;
	{
		std::lock_guard<std::mutex> lock(g_lock);
		in.armed = g_state.chestArmed;
		in.tracked = g_state.chestTracked && g_state.chestId == At<uint32_t>(object, UnitIdOffset);
		in.object = At<int32_t>(object, UnitTypeOffset) == ObjectUnitType;
		in.rolled = g_state.chestRolled;
		in.modeBefore = modeBefore;
		in.modeAfter = modeAfter;
		reason = e6::decideChest(in);
		if (reason == e6::ChestReason::Rolled) {
			g_state.chestRolled = true;  // set before the call: a roll that re-enters the hook cannot roll twice
		}
		tc = g_state.chestClass;
		level = g_state.dropLevel;
		name = g_state.chestDropName;
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

auto OperateHook(Game* game, Unit* activator, Unit* object) noexcept -> int32_t {
	if (g_operateOriginal == nullptr) {
		return 0;  // reachable only if the loader gave no original (the install logs it and marks the part off)
	}
	if (!g_chestArmed.load(std::memory_order_relaxed) || object == nullptr || game == nullptr) {
		return g_operateOriginal(game, activator, object);
	}
	bool candidate = false;
	int32_t before = 0;
	try {
		const uint32_t id = At<uint32_t>(object, UnitIdOffset);
		std::lock_guard<std::mutex> lock(g_lock);
		candidate = g_state.chestTracked && !g_state.chestRolled && g_state.chestId == id && At<int32_t>(object, UnitTypeOffset) == ObjectUnitType;
		if (candidate) {
			before = At<int32_t>(object, UnitModeOffset);
		}
	} catch (...) {
		candidate = false;
	}
	const int32_t result = g_operateOriginal(game, activator, object);
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
		if (g_state.waves.onDeath(id)) {
			plog::Info("eventprobe: ambush: unit id %u of wave %d died, %zu left", id, g_state.waves.wavesStarted(), g_state.waves.remaining());
		} else if (g_state.waves.phase() == e6::WaveTracker::Phase::Fighting && g_state.untrackedProbes < MaxUntrackedDeathProbes) {
			++g_state.untrackedProbes;
			plog::Info("eventprobe: ambush: a death during wave %d was not one of its units (unit id %u, class %u)", g_state.waves.wavesStarted(), id, event->monster.classId);
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
	const bool flee = On(FleeSetup) && On(FleeBlock) && On(FleeTarget);
	return std::string("Shrine events ") + (shrine ? "on" : "off") + ", Boss Drop " + (g_dropHook.load() ? "on" : "off") + ", Ambush " + (g_deathListener ? "on" : "one wave only (no death listener)") +
	       ", Treasure flee " + (flee ? "on" : "off") + ", Treasure chest Drop " + (shrine && g_chestHook.load() ? "on" : "off") + " (written, not verified in game)";
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
		const bool ok = hooked && g_original != nullptr;
		part.status.on = ok;
		part.status.detail = ok ? "found by signature, hooked" : !hooked ? "off: D2RLoader refused the hook" : "off: hooked, but D2RLoader gave no way back to the game's own function, so every monster's own drop is skipped until the game is restarted";
		if (hooked && !ok) {
			plog::Error("part %s: D2RLoader installed the hook but gave no original; the game's own monster drop is skipped for EVERY monster until the game is restarted; restart it after removing what conflicts", part.name);
		}
		g_dropHook = ok;
	} else if (On(DeathDrop)) {
		g_parts[DeathDrop].status.on = false;
		g_parts[DeathDrop].status.detail = "off: not hooked, because a part it needs was not found";
	}
	// The operate hook, for a Treasure chest's Drop: likewise only when everything the roll calls was found.
	if (On(OperateObject) && On(TcLookup) && On(TcRoller) && On(DropLevel) && On(DataTables) && On(NameToRow)) {
		Part& part = g_parts[OperateObject];
		const uint64_t rva = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(part.address) - context->exeBase);
		const bool hooked = context->InstallInlineHook(rva, part.guard, part.hookSize, reinterpret_cast<void*>(&OperateHook), reinterpret_cast<void**>(&g_operateOriginal));
		const bool ok = hooked && g_operateOriginal != nullptr;
		part.status.on = ok;
		part.status.detail = ok ? "found by signature, hooked" : !hooked ? "off: D2RLoader refused the hook" : "off: hooked, but D2RLoader gave no way back to the game's own function, so every object use is skipped until the game is restarted";
		if (hooked && !ok) {
			plog::Error("part %s: D2RLoader installed the hook but gave no original; the game's object use is skipped for EVERY object until the game is restarted; restart it after removing what conflicts", part.name);
		}
		g_chestHook = ok;
	} else if (On(OperateObject)) {
		g_parts[OperateObject].status.on = false;
		g_parts[OperateObject].status.detail = "off: not hooked, because a part it needs was not found";
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
	for (const Part& part : g_parts) {
		plog::Info("part %s: %s", part.name, part.status.detail.c_str());
	}
}

auto Event6bPartStatuses() -> std::vector<PartStatus> {
	std::vector<PartStatus> out;
	for (const Part& part : g_parts) {
		out.push_back(part.status);
	}
	return out;
}

auto Event6bSummary() -> std::string {
	return "phase 6b: " + SummaryNow(g_populatorOn);
}

auto Event6bPrepareShrine(const ev::Event& event, const std::vector<std::string>& objectNames, void* player, std::string* why) -> bool {
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
	const e6::ObjectLookup lookup = e6::resolveObject(objectNames, event.spawn, gameRows);
	const std::string text = e6::describeObjectLookup(lookup, event.spawn, objectNames.size(), gameRows);
	plog::Info("eventprobe: shrine: Spawn '%s': %s (objects.txt: %zu lines, game: %llu rows)", event.spawn.c_str(), text.c_str(), objectNames.size(), static_cast<unsigned long long>(gameRows));
	if (lookup.result != e6::ObjectResult::Found) {
		*why = text;
		return false;
	}
	std::lock_guard<std::mutex> lock(g_lock);
	g_state.shrineReady = true;
	g_state.shrinePlaced = false;
	g_state.shrineRow = lookup.row;
	g_state.shrineSpawn = event.spawn;
	return true;
}

void Event6bMapOpened() noexcept {
	std::lock_guard<std::mutex> lock(g_lock);
	g_state = State{};
	g_dropArmed = false;
	g_chestArmed = false;
	g_ambushActive = false;
	++g_epoch;
}

void Event6bArmBossDrop(const ev::Event& event, int level, void* player) {
	Event6bMapOpened();
	if (event.kind == ev::Kind::Treasure && player != nullptr) {
		// A Treasure monster flees even without a Drop: remember the player it flees from.
		std::lock_guard<std::mutex> lock(g_lock);
		g_state.fleeSource = player;
		g_state.fleeSourceId = At<uint32_t>(player, UnitIdOffset);
	}
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
	g_state.dropArmed = true;
	g_state.dropName = event.drop;
	g_state.dropClass = tc;
	g_state.dropLevel = level;
	g_dropArmed = true;
}

auto Event6bPlacesObject(const ev::Event& event, ev::NameKind spawnKind) noexcept -> bool {
	const e6::Route route = e6::routeEvent(event.kind, spawnKind);
	return route == e6::Route::Shrine || route == e6::Route::TreasureChest;
}

auto Event6bPrepareChest(const ev::Event& event, const std::vector<std::string>& objectNames, int level, void* player, std::string* why) -> bool {
	if (!Event6bPrepareShrine(event, objectNames, player, why)) {
		return false;
	}
	{
		std::lock_guard<std::mutex> lock(g_lock);
		g_state.objectIsChest = true;
		g_state.dropLevel = level;
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
		g_state.chestArmed = true;
		g_state.chestDropName = event.drop;
		g_state.chestClass = tc;
		g_chestArmed = true;
	}
	return true;
}

void Event6bBossSpawned(void* unit) noexcept {
	if (!g_dropArmed.load() || unit == nullptr) {
		return;
	}
	try {
		const uint32_t id = At<uint32_t>(unit, UnitIdOffset);
		const int32_t type = At<int32_t>(unit, UnitTypeOffset);
		std::lock_guard<std::mutex> lock(g_lock);
		if (!g_state.dropArmed) {
			return;
		}
		g_state.bossTracked = type == UnitTypeMonster;
		g_state.bossId = id;
		g_state.dropped = false;
		plog::Info("eventprobe: boss drop: tracking unit %p (type %d, id %u)%s", unit, type, id, g_state.bossTracked ? "" : "; not a monster, so no extra drop");
	} catch (...) {
		plog::Error("boss drop: tracking the boss failed");
	}
}

namespace {

// Makes the Treasure monster flee from the player: calls the game's flee setup (0x597030) on it. The two checks the setup starts with are
// run first so the log says why a refusal happened; the setup repeats them itself.
void StartFlee(Game* game, void* unit) noexcept {
	if (!On(FleeSetup)) {
		plog::Warn("treasure monster does not flee: the FleeSetup part is off (see the parts above)");
		return;
	}
	void* source = nullptr;
	uint32_t sourceId = 0;
	{
		std::lock_guard<std::mutex> lock(g_lock);
		source = g_state.fleeSource;
		sourceId = g_state.fleeSourceId;
	}
	// The player the map was opened for, checked again by type and id; else the setup is given no source.
	if (source != nullptr && (At<int32_t>(source, UnitTypeOffset) != 0 || At<uint32_t>(source, UnitIdOffset) != sourceId)) {
		plog::Warn("treasure monster: the remembered player unit %p is not player id %u any more; the flee is set up with no source", source, sourceId);
		source = nullptr;
	}
	auto* target = static_cast<Unit*>(unit);
	const int blocked = On(FleeBlock) ? (Call<FleeBlockFn>(FleeBlock)(target) != 0 ? 1 : 0) : -1;
	const int targetOk = On(FleeTarget) ? (Call<FleeTargetFn>(FleeTarget)(target, 0, 1, 1, 1) != 0 ? 1 : 0) : -1;
	const int32_t modeBefore = At<int32_t>(unit, UnitModeOffset);
	plog::Info("eventprobe: treasure monster flee: unit %p, source %p, duration argument %d: %s", unit, source, FleeDuration, e6::describeFleeChecks(blocked, targetOk).c_str());
	Call<FleeSetupFn>(FleeSetup)(game, static_cast<Unit*>(source), target, 0, 0, FleeDuration);
	plog::Info("eventprobe: treasure monster flee: the setup returned; the unit's mode %d -> %d", modeBefore, At<int32_t>(unit, UnitModeOffset));
}

}  // namespace

void Event6bFirstSpawned(void* game, ev::Kind kind, void* unit) noexcept {
	if (unit == nullptr) {
		return;
	}
	if (kind == ev::Kind::Boss) {
		Event6bBossSpawned(unit);
	} else if (kind == ev::Kind::Treasure) {
		Event6bBossSpawned(unit);  // rolls the Drop on its death (the same death hook as a Boss)
		try {
			StartFlee(static_cast<Game*>(game), unit);
		} catch (...) {
			plog::Error("treasure monster: making it flee failed");
		}
	}
}

void Event6bArmAmbush(const ev::Event& event, const std::vector<ep::Spawn>& spawns, const ep::Rect& serverRect, void* room) noexcept {
	try {
		std::lock_guard<std::mutex> lock(g_lock);
		const int waves = g_deathListener ? (std::max)(1, event.waves) : 1;
		g_state.waves.arm(waves);
		g_state.ambushArmed = true;
		g_state.waveSpawns = spawns;
		g_state.ambushRect = serverRect;
		g_state.firstWaveDone = false;
		g_state.ambushName = event.name;
		g_state.untrackedProbes = 0;
		g_ambushActive = true;
		plog::Info("eventprobe: ambush '%s': armed %d wave(s) of %zu in room %p; the first comes when a player is in that room%s (time-out %llu ms, gap %llu ms)", event.name.c_str(), waves, spawns.size(), room,
		           g_deathListener ? "" : " (no death listener: this is the only wave)", static_cast<unsigned long long>(e6::kWaveTimeoutMs), static_cast<unsigned long long>(e6::kWaveGapMs));
	} catch (...) {
		plog::Error("ambush: arming failed");
	}
}

bool Event6bAmbushTick(void* game, void* playerRoom, AmbushRectFn roomRect, AmbushSpawnFn spawn) noexcept {
	if (!g_ambushActive.load(std::memory_order_relaxed) || spawn == nullptr || roomRect == nullptr || game == nullptr) {
		return false;
	}
	try {
		std::vector<ep::Spawn> spawns;
		uint64_t epoch = 0;
		int waveNumber = 0;
		ep::Rect playerRect;
		const bool rectOk = playerRoom != nullptr && roomRect(playerRoom, &playerRect);  // read before the lock: the engine's room, not ours
		{
			std::lock_guard<std::mutex> lock(g_lock);
			const ep::Rect& want = g_state.ambushRect;
			const bool inRoom = rectOk && playerRect.x == want.x && playerRect.y == want.y && playerRect.w == want.w && playerRect.h == want.h;
			const auto before = g_state.waves.phase();
			const e6::WaveTracker::Step step = g_state.waves.step(inRoom, GetTickCount64());
			if (before == e6::WaveTracker::Phase::Fighting && g_state.waves.phase() != e6::WaveTracker::Phase::Fighting) {
				plog::Info("eventprobe: ambush '%s': wave %d of %d ended: %s", g_state.ambushName.c_str(), g_state.waves.wavesStarted(), g_state.waves.wavesTotal(), e6::waveEndName(g_state.waves.lastEnd()));
			}
			if (step == e6::WaveTracker::Step::Finished) {
				plog::Info("ambush '%s': finished after %d wave(s)", g_state.ambushName.c_str(), g_state.waves.wavesStarted());
				g_ambushActive = false;
				return false;
			}
			if (step != e6::WaveTracker::Step::SpawnWave) {
				return false;
			}
			spawns = g_state.waveSpawns;
			epoch = g_epoch.load();
			waveNumber = g_state.waves.wavesStarted();
		}
		std::vector<uint32_t> ids;
		for (const ep::Spawn& s : spawns) {
			void* unit = nullptr;
			if (spawn(game, playerRoom, s, &unit) && unit != nullptr && At<int32_t>(unit, UnitTypeOffset) == UnitTypeMonster) {
				ids.push_back(At<uint32_t>(unit, UnitIdOffset));
			}
		}
		std::lock_guard<std::mutex> lock(g_lock);
		if (epoch != g_epoch.load()) {
			return false;  // the map changed or the game was left while spawning; the state was reset
		}
		g_state.waves.waveSpawned(ids, GetTickCount64());
		plog::Info("eventprobe: ambush '%s': wave %d of %d: the game made %zu of %zu units in the event room %p (game %p)", g_state.ambushName.c_str(), waveNumber, g_state.waves.wavesTotal(), ids.size(), spawns.size(), playerRoom, game);
		if (!g_state.waves.active()) {
			plog::Warn("ambush '%s' abandoned: %s", g_state.ambushName.c_str(), e6::waveEndName(g_state.waves.lastEnd()));
			g_ambushActive = false;
		}
		if (!ids.empty() && !g_state.firstWaveDone) {
			g_state.firstWaveDone = true;
			return true;  // the first wave spawned: the event counts as placed (announced by the caller)
		}
	} catch (...) {
		plog::Error("ambush: spawning a wave failed");
		g_ambushActive = false;
	}
	return false;
}

bool Event6bPlaceShrine(void* rawGame, void* room, const ep::Rect& serverRect, int scale, uint64_t seed) noexcept {
	try {
		int row = -1;
		{
			std::lock_guard<std::mutex> lock(g_lock);
			if (!g_state.shrineReady || g_state.shrinePlaced) {
				return false;
			}
			g_state.shrinePlaced = true;
			row = g_state.shrineRow;
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
		const std::vector<ep::Spot> spots = ep::placements(serverRect, 1, seed, scale);
		if (spots.empty()) {
			plog::Warn("shrine not placed: the event room is too small to place anything");
			return false;
		}
		int32_t xy[2] = {spots[0].x, spots[0].y};
		void* landed = nullptr;
		plog::Info("eventprobe: shrine: objects row %d, free spot near (%d,%d) from room %p (radius %d, mask 0x%x)", row, xy[0], xy[1], room, FreeSpotRadius, FreeSpotMask);
		Call<FreeSpotFn>(FreeSpot)(room, xy, FreeSpotRadius, FreeSpotMask, &landed);
		if (landed == nullptr) {
			plog::Warn("shrine not placed: the game found no free spot near (%d,%d) in the event room", spots[0].x, spots[0].y);
			return false;
		}
		plog::Info("eventprobe: shrine: free spot (%d,%d) in room %p (%s the event room)", xy[0], xy[1], landed, landed == room ? "in" : "NOT in");
		if (landed != room) {
			// The neighbouring room may not be populated yet; refuse rather than create an object there.
			plog::Warn("shrine not placed: the free spot (%d,%d) is outside the event room (room %p, not %p)", xy[0], xy[1], landed, room);
			return false;
		}
		void* unit = Call<CreateObjectFn>(CreateObject)(ObjectUnitType, row, xy[0], xy[1], game, landed, ObjectCreateFlag, 0, 0, 0);
		if (unit == nullptr) {
			plog::Warn("shrine not placed: the game refused to create objects row %d at (%d,%d)", row, xy[0], xy[1]);
			return false;
		}
		plog::Info("eventprobe: shrine: created objects row %d: unit %p (type %d, id %u) at (%d,%d)", row, unit, At<int32_t>(unit, UnitTypeOffset), At<uint32_t>(unit, UnitIdOffset), xy[0], xy[1]);
		std::lock_guard<std::mutex> lock(g_lock);
		if (g_state.objectIsChest) {  // a Treasure chest: remember it for the Drop roll when it is opened
			g_state.chestTracked = At<int32_t>(unit, UnitTypeOffset) == ObjectUnitType;
			g_state.chestId = At<uint32_t>(unit, UnitIdOffset);
			g_state.chestRolled = false;
			plog::Info("eventprobe: treasure chest: tracking unit %p (id %u, mode %d)%s", unit, g_state.chestId, At<int32_t>(unit, UnitModeOffset), g_state.chestArmed ? "" : "; no armed Drop");
		}
		return true;
	} catch (...) {
		plog::Error("shrine: placing the object failed");
		return false;
	}
}

void Event6bGameLeft() noexcept {
	std::lock_guard<std::mutex> lock(g_lock);
	g_state = State{};
	g_dropArmed = false;
	g_chestArmed = false;
	g_ambushActive = false;
	++g_epoch;
}

}  // namespace carto
