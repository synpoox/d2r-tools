#include "engine/stat_apply.h"

#include "core/part_signatures.h"
#include "core/text.h"
#include "engine/scan.h"
#include "log.h"
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
constexpr int ListHeadOffset = 0x90;       // a unit's stat list chain: head, or the second head at 0x98; next at 0x68
constexpr int ListHeadAltOffset = 0x98;
constexpr int ListNextOffset = 0x68;
constexpr int MaxChain = 512;              // a chain longer than this is not walked further
constexpr int32_t UnitPlayer = 0;
constexpr int32_t UnitMonster = 1;
constexpr int SpawnRoomOffset = 0x08;      // the monster setup's first argument: +8 is the room (the spawn block)
constexpr int32_t StatLevel = 12;          // 0xc: the monster level stat
constexpr int32_t StatMaxLife = 7;         // maxhp, in 1/256 life
constexpr int MonsterProbes = 6;           // monsters logged in full per game
constexpr int MaxEntries = 32;             // stats read back per probe

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

enum PartId { NewStatList, SetListStat, AttachStatList, DetachStatList, FreeStatList, MonsterSetup, UnitRoom, RoomLevelId, UnitStat, PartCount };

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
// SetListStat for each stat, AttachStatList. No state is set (the plugin has no state of its own; it keeps the list address).
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

// How many lists are on the unit's chain, and whether `find` is one of them.
auto Chain(Unit* unit, const void* find, bool* found) noexcept -> int {
	int n = 0;
	*found = false;
	for (const int head : {ListHeadOffset, ListHeadAltOffset}) {
		const uint8_t* p = At<const uint8_t*>(unit, head);
		for (int i = 0; p != nullptr && i < MaxChain; ++i, ++n) {
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

void RemoveFromPlayer(Unit* unit, uint32_t id) noexcept {
	uintptr_t list = 0;
	{
		std::lock_guard<std::mutex> lock(g_lock);
		list = g_lists.find(id);
		g_lists.remove(id);
		UpdateBusy();
	}
	if (list == 0) {
		return;
	}
	bool found = false;
	const int before = Chain(unit, reinterpret_cast<void*>(list), &found);
	if (!found) {
		// Never detach or free a list the unit does not hold: it may already be gone.
		plog::Warn("map stats: the player's list is no longer on the unit (%d lists); it is forgotten, not freed", before);
		return;
	}
	Call<DetachStatListFn>(DetachStatList)(unit, reinterpret_cast<void*>(list));
	Call<FreeStatListFn>(FreeStatList)(At<uint8_t>(unit, UnitDataFlagOffset), reinterpret_cast<void*>(list));
	bool still = false;
	const int after = Chain(unit, reinterpret_cast<void*>(list), &still);
	plog::Info("statprobe: player %u left the map: list removed (chain %d -> %d lists)", id, before, after);
}

void ApplyToPlayer(Unit* unit, uint32_t id, const sp::Plan& plan, uint64_t generation) noexcept {
	const std::string before = Totals(unit, plan.players);
	bool found = false;
	const int chainBefore = Chain(unit, nullptr, &found);
	void* list = MakeList(unit, UnitPlayer, plan.players);
	if (list == nullptr) {
		plog::Warn("map stats: the game gave no stat list for the player; the player has none of the map's stats");
		return;
	}
	bool ok = false, stale = false;
	{
		std::lock_guard<std::mutex> lock(g_lock);
		stale = generation != g_generation;
		ok = !stale && g_lists.add(id, reinterpret_cast<uintptr_t>(list));
		UpdateBusy();
	}
	if (stale) {
		// The game was left while this list was being made. The unit belongs to that game: record nothing and touch nothing more.
		plog::Warn("map stats: the game was left while the player's list was being made; the list is not tracked");
		return;
	}
	if (!ok) {
		// Table full: take the list straight off again, so no list is out that the plugin cannot remove.
		Call<DetachStatListFn>(DetachStatList)(unit, list);
		Call<FreeStatListFn>(FreeStatList)(At<uint8_t>(unit, UnitDataFlagOffset), list);
		plog::Warn("map stats: too many players to track; player %u has none of the map's stats", id);
		return;
	}
	++g_playerCount;
	bool onChain = false;
	const int chainAfter = Chain(unit, list, &onChain);
	plog::Info("statprobe: player %u entered the map: totals [%s] -> [%s], list on chain: %s (%d -> %d lists)", id, before.c_str(),
	           Totals(unit, plan.players).c_str(), onChain ? "yes" : "NO", chainBefore, chainAfter);
}

// --- Level and plan -------------------------------------------------------------------------------------------------------

auto SummaryNow() -> std::string {
	const bool mon = g_monstersOn.load(), pl = g_playersOn.load();
	const char* player = !g_playerSetting.load() ? "off (PlayerStats=0 in cartographer.txt, the default)" : pl ? "on (test slot only)" : "off (a part was not found)";
	return std::string("monster stats ") + (mon ? "on" : "off") + ", player stats " + player + " (not yet verified in game)";
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
		const bool ok = hooked && g_original != nullptr;
		part.status.on = ok;
		part.status.detail = ok ? "found by signature, hooked" : !hooked ? "off: D2RLoader refused the hook" : "off: hooked, but D2RLoader gave no way back to the game's own function, so every monster's own setup is skipped until the game is restarted";
		if (hooked && !ok) {
			plog::Error("part %s: D2RLoader installed the hook but gave no original; the game's own setup is skipped for EVERY monster (not only the map's) until the game is restarted; restart it after removing what conflicts", part.name);
		}
		g_monstersOn = ok;
	}

	// Players: applied from the portal's player tick hook, so that part must be on too.
	bool tickOn = false;
	for (const PartStatus& s : PartStatuses()) {
		tickOn = tickOn || (s.name == "PlayerTick" && s.on);
	}
	g_playersOn = tickOn && AllOn({NewStatList, SetListStat, AttachStatList, DetachStatList, FreeStatList, UnitRoom, RoomLevelId});
	if (!g_playersOn) {
		plog::Warn("player map stats are off: %s", !tickOn ? "the PlayerTick part is off" : "a stat list part was not found");
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

void StatApplyMapOpened(const MapDef& map, const std::shared_ptr<const Tables>& tables, uint32_t playerId, uint32_t openings) {
	if (tables == nullptr || (!g_monstersOn.load() && !g_playersOn.load())) {
		return;
	}
	const int slot = SlotLevel(tables->slots, PresetSlot);
	const uint64_t seed = sp::mixSeed(GetTickCount64(), playerId, openings);
	cartographer::stats::Settings settings;
	settings.properties = tables->settings.properties;
	settings.itemStats = false;  // the ItemStats mode is not applied yet
	if (tables->settings.itemStats != 0) {
		plog::Warn("ItemStats is set, but the map item's own stats are not applied yet: the properties are drawn as usual");
	}
	cartographer::stats::MapRow row;
	row.properties = map.properties;
	const auto drawn = cartographer::stats::drawMapStats(seed, row, settings, tables->pool);
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
			plog::Info("statprobe: first player unit %u of this game holds %d stat lists", At<uint32_t>(unit, UnitIdOffset), Chain(unit, nullptr, &found));
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
}

auto StatApplyPartStatuses() -> std::vector<PartStatus> {
	std::vector<PartStatus> out;
	for (const Part& part : g_parts) {
		out.push_back(part.status);
	}
	return out;
}

auto StatApplySummary() -> std::string {
	return "map stats: " + SummaryNow();
}

}  // namespace carto
