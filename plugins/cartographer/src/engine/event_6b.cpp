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

enum PartId { DeathDrop, DropRoller, DropLevel, SpawnObject, FreeSpot, CreateObject, DataTables, NameToRow, PartCount };

Part g_parts[PartCount] = {
	{"DeathDrop", sig::DeathDrop, 15, false, nullptr, {}, {}},
	{"DropRoller", sig::DropRoller, 0, false, nullptr, {}, {}},
	{"DropLevel", sig::DropLevel, 0, false, nullptr, {}, {}},
	{"SpawnObject", sig::SpawnObject, 0, false, nullptr, {}, {}},
	{"FreeSpot", nullptr, 0, false, nullptr, {}, {}},  // no signature: the call target inside SpawnObject
	{"CreateObject", sig::CreateObject, 0, false, nullptr, {}, {}},
	{"DataTablesBase", nullptr, 0, false, nullptr, {}, {}},  // shared with event_apply.cpp (copied in at load)
	{"NameToRow", nullptr, 0, false, nullptr, {}, {}},       // shared with event_apply.cpp (copied in at load)
};

const D2RL::PluginContext* g_context = nullptr;
DeathDropFn g_original = nullptr;
bool g_populatorOn = false;            // the room populator hook (event_apply.cpp) is in; set once at load
std::atomic<bool> g_dropHook{false};   // the death hook is installed and every part it calls was found
std::atomic<bool> g_dropArmed{false};  // a Boss event with a resolved Drop is armed (read on every monster death: lock only when set)

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
	return std::string("Shrine events ") + (shrine ? "on" : "off") + ", Boss Drop " + (g_dropHook.load() ? "on" : "off") + " (written, not verified in game)";
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
	g_state.dropArmed = false;
	g_state.bossTracked = false;
	g_state.dropped = false;
	g_state.shrineReady = false;
	g_state.shrinePlaced = false;
	g_dropArmed = false;
}

void Event6bArmBossDrop(const ev::Event& event, int level, void* player) {
	Event6bMapOpened();
	if (event.kind != ev::Kind::Boss || event.drop.empty()) {
		return;
	}
	if (!g_dropHook.load()) {
		plog::Warn("Drop '%s' of event '%s' is not applied: the monster death hook is off (see the parts above)", event.drop.c_str(), event.name.c_str());
		return;
	}
	std::string why;
	const uint8_t* dt = player == nullptr ? nullptr : TablesBase(At<uint8_t>(player, UnitDataFlagOffset), &why);
	void* map = dt == nullptr ? nullptr : At<void*>(dt, DtTcMapOffset);  // re-read, never cached
	if (dt == nullptr || map == nullptr) {
		plog::Warn("Drop '%s' of event '%s' is not applied: %s", event.drop.c_str(), event.name.c_str(), dt == nullptr ? (player == nullptr ? "no player unit" : why.c_str()) : "the treasure class name map is null");
		return;
	}
	const uint64_t tcCount = At<uint64_t>(dt, DtTcCountOffset);
	const int32_t found = Call<NameToRowFn>(NameToRow)(map, event.drop.c_str(), 0);
	std::string tcWhy;
	const int tc = e6::tcIndexFromLookup(found, tcCount, &tcWhy);
	plog::Info("eventprobe: boss drop: Drop '%s': name map %p, the game has %llu treasure classes, lookup gave %d%s%s", event.drop.c_str(), map, static_cast<unsigned long long>(tcCount), found,
	           tc < 0 ? ", refused: " : "", tc < 0 ? tcWhy.c_str() : "");
	if (tc < 0) {
		plog::Warn("Drop '%s' of event '%s' is not applied: %s", event.drop.c_str(), event.name.c_str(), tcWhy.c_str());
		return;
	}
	std::lock_guard<std::mutex> lock(g_lock);
	g_state.dropArmed = true;
	g_state.dropName = event.drop;
	g_state.dropClass = tc;
	g_state.dropLevel = level;
	g_dropArmed = true;
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
}

}  // namespace carto
