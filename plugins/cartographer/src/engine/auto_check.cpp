#include "engine/auto_check.h"

#include "autocheck.h"
#include "core/part_signatures.h"
#include "engine/game_memory.h"
#include "engine/maze_hooks.h"
#include "engine/stat_apply.h"
#include "log.h"

#include <windows.h>

#include <atomic>
#include <mutex>
#include <optional>
#include <string>

namespace carto {

namespace {

namespace ac = cartographer::autocheck;

constexpr int UnitTypeOffset = 0x00;  // int32: 0 for a player
constexpr int UnitIdOffset = 0x08;    // uint32: the unit id
constexpr int HellDifficulty = 2;
constexpr uint64_t StartDelayMs = 4000;        // after the player is ready: town has loaded and the first frames ran
constexpr uint64_t ArrivalWaitMs = 180000;     // for the player to walk into the portal (no native for it yet)
constexpr uint64_t ArrivalReadMs = 6000;       // for the arrival probe to read the room after the player is in the level
constexpr uint64_t BossWaitMovedMs = 20000;    // after the player was put in the boss room
constexpr uint64_t BossWaitManualMs = 120000;  // when the player must walk there
constexpr uint64_t StatGraceMs = 1500;         // after the Boss: its stat list is put on in the same call, this only lets the tick settle

constexpr int GameActsOffset = 0x190;      // on the game: {pointer to the act pointers, count} (read from the 'goto' handler, cartographer.md section 16)
constexpr int GameDataFlagOffset = 0x106;  // uint8 on the game, what the engine's own room lookups read
constexpr uint64_t MaxActs = 8;

using Game = void;
using Unit = void;
// (game, unit, room, x, y, 0, 0): puts the unit at a subtile position in `room` (cartographer.md section 16). 0 when it could not.
using UnitMoveFn = int32_t (*)(Game*, Unit*, void* room, int32_t x, int32_t y, int32_t a6, int32_t a7) noexcept;
// (dataFlag, act, tileX, tileY): the room holding that tile of the act's levels, or null.
using RoomAtTileFn = void* (*)(uint32_t dataFlag, void* act, int32_t tileX, int32_t tileY) noexcept;
// (game, room): builds and populates the room if it is not up yet.
using RoomBringUpFn = void (*)(Game*, void* room) noexcept;

enum PartId { MovePart, RoomAtPart, BringUpPart, PartCount };
Part g_parts[PartCount] = {
    {"UnitMove", sig::UnitMove, 0, false, nullptr, {}, {}},
    {"RoomAtTile", sig::RoomAtTile, 0, false, nullptr, {}, {}},
    {"RoomBringUp", sig::RoomBringUp, 0, false, nullptr, {}, {}},
};

auto MoverReady() -> bool {
	return g_parts[MovePart].address != nullptr && g_parts[RoomAtPart].address != nullptr && g_parts[BringUpPart].address != nullptr;
}

enum class Phase { Idle, Delay, Open, Arrival, ArrivalRead, MoveToBoss, WaitBoss, Stats, Done };

struct Run {
	Phase phase = Phase::Idle;
	ac::Checklist list;
	ac::Config config;
	MapDef map;
	CheckOpen opened;
	uint64_t phaseStartMs = 0;
	bool moved = false;       // the player was put in the boss room by UnitMove
	bool bossNoted = false;   // event_apply reported the map Boss
	bool bossSpawned = false;
	bool bossSuper = false;
	std::string bossProblem;
};

// g_run is shared by the server thread (tick, Step) and the lifecycle thread (game entered / left) and the Boss note: every access holds g_lock.
// Step releases it around every call into the engine or into another module (a room brought up can populate, which reaches AutoCheckNoteBoss,
// and g_lock is not recursive), so g_gen counts games: a Step that finds it changed after such a call drops what it was doing.
std::mutex g_lock;
std::shared_ptr<const Tables> g_tables;
Run g_run;
uint64_t g_gen = 0;
std::atomic<bool> g_on{false};      // AutoCheck=1: the only thing the tick reads when the setting is off
std::atomic<bool> g_active{false};  // a run is going (not Idle, not Done)

auto NowMs() noexcept -> uint64_t {
	return GetTickCount64();
}

void Enter(Phase phase) noexcept {
	g_run.phase = phase;
	g_run.phaseStartMs = NowMs();
	g_active = phase != Phase::Idle && phase != Phase::Done;
}

void ResetRun() {
	g_run = Run{};
	g_active = false;
}

// Runs `work` with the lock released, and takes it again after (also when `work` throws).
template <class F>
auto Outside(std::unique_lock<std::mutex>& lock, F&& work) {
	struct Relock {
		std::unique_lock<std::mutex>& held;
		~Relock() {
			held.lock();
		}
	} relock{lock};
	lock.unlock();
	return work();
}

void Line(const std::string& text) noexcept {
	plog::Info("%s", text.c_str());
}

// The result line is also the log line the checker greps for.
void Settle(ac::Item item, bool pass, const std::string& detail) noexcept {
	if (g_run.list.settled(item)) {
		return;
	}
	g_run.list.record(item, pass, detail);
	Line(ac::resultLine(ac::Result{item, pass, detail, false}));
}

// An item that does not apply to this map: logged as N/A and kept out of pass=.
void SettleNotApplicable(ac::Item item, const std::string& detail) noexcept {
	if (g_run.list.settled(item)) {
		return;
	}
	g_run.list.recordNotApplicable(item, detail);
	Line(ac::resultLine(ac::Result{item, true, detail, true}));
}

// Writes every unsettled item as a failure, then the DONE line. After this the run is over.
void Finish(const std::string& reason) noexcept {
	if (!g_run.list.complete()) {
		for (std::optional<ac::Item> next = g_run.list.next(); next; next = g_run.list.next()) {
			Settle(*next, false, reason);
		}
	}
	Line(ac::doneLine(g_run.list));
	// No native call for leaving a game was found (cartographer.md section 16): the checker leaves with Save and Exit.
	Line("AUTOCHECK LEAVE not done: no native call to leave a game was found; Save and Exit to finish");
	Enter(Phase::Done);
}

auto MapAsksForMonsterColumns(const MapDef& map) -> bool {
	return map.level != DefaultMonsterLevel || map.density > 0 || map.uniques > 0 || !map.monsters.empty();
}

auto MapColumnsText(const MapDef& map) -> std::string {
	return "Level " + std::to_string(map.level) + ", Density " + std::to_string(map.density) + "%, Uniques " + std::to_string(map.uniques) + "%, Monsters " +
	       std::to_string(map.monsters.size());
}

void SettleMonsterSettings() noexcept {
	const std::string columns = MapColumnsText(g_run.map);
	if (!MapAsksForMonsterColumns(g_run.map)) {
		SettleNotApplicable(ac::Item::MapSettings, "the map asks for nothing beyond the slot level's own monsters (" + columns + ")");
	} else if (g_run.opened.shaped) {
		Settle(ac::Item::MapSettings, true, "written for the slot level (" + columns + "); see the 'map monsters:' lines for each field");
	} else {
		Settle(ac::Item::MapSettings, false, "asked for " + columns + " but nothing was written (see the 'map monsters:' lines above for why)");
	}
	if (g_run.map.champions > 0) {
		Line("AUTOCHECK note: Champions " + std::to_string(g_run.map.champions) + "% is not applied by design (the game has no field for it)");
	}
}

void SettleStats(const StatApplyCounts& counts) noexcept {
	Line("AUTOCHECK stats: drawn " + std::to_string(counts.drawnMonster) + " monster and " + std::to_string(counts.drawnPlayer) + " player propert" +
	     (counts.drawnMonster + counts.drawnPlayer == 1 ? "y" : "ies") + (counts.drawnText.empty() ? "" : " (" + counts.drawnText + ")") + "; " +
	     std::to_string(counts.monstersChanged) + " monster(s) given the list, " + std::to_string(counts.monstersFailed) + " failed, player stats applied " +
	     std::to_string(counts.playersChanged) + " time(s)");
	if (counts.drawnMonster == 0) {
		Settle(ac::Item::StatChanges, false, "no monster property was drawn for the map (monster stats off, or an empty pool; see the 'map stats' lines)");
	} else if (counts.monstersChanged > 0) {
		Settle(ac::Item::StatChanges, true, std::to_string(counts.monstersChanged) + " monster(s) took " + std::to_string(counts.drawnMonster) + " drawn propert" +
		                                        (counts.drawnMonster == 1 ? "y" : "ies"));
	} else {
		Settle(ac::Item::StatChanges, false, "properties were drawn but no monster was given them (" + std::to_string(counts.monstersFailed) + " failed)");
	}
}

void SettleBossFromNote() noexcept {
	if (g_run.bossSpawned && g_run.bossProblem.empty()) {
		Settle(ac::Item::Boss, true, "'" + g_run.map.boss + "' spawned once as " + (g_run.bossSuper ? "a superunique" : "a plain monster") + (g_run.moved ? "" : "; the player walked to the boss room"));
	} else {
		Settle(ac::Item::Boss, false, "'" + g_run.map.boss + "' " + (g_run.bossSpawned ? "" : "was not spawned: ") + (g_run.bossProblem.empty() ? "unknown" : g_run.bossProblem));
	}
}

// The room holding the stand point, found the way the console command 'goto' finds it: the tile lookup over each act, kept only when the room is in `level`.
auto FindRoomAt(Game* game, int level, int subtileX, int subtileY, std::string* why) noexcept -> void* {
	const uint8_t* g = static_cast<const uint8_t*>(game);
	if (!Readable(g + GameActsOffset, 16)) {
		*why = "the game's act list could not be read";
		return nullptr;
	}
	void* const* acts = *reinterpret_cast<void* const* const*>(g + GameActsOffset);
	const uint64_t count = *reinterpret_cast<const uint64_t*>(g + GameActsOffset + 8);
	if (acts == nullptr || count == 0 || count > MaxActs || !Readable(acts, count * sizeof(void*))) {
		*why = "the game's act list looks wrong (" + std::to_string(count) + " acts)";
		return nullptr;
	}
	const auto roomAt = reinterpret_cast<RoomAtTileFn>(const_cast<uint8_t*>(g_parts[RoomAtPart].address));
	const uint32_t dataFlag = At<uint8_t>(game, GameDataFlagOffset);
	for (uint64_t i = 0; i < count; ++i) {
		if (acts[i] == nullptr || !Readable(static_cast<const uint8_t*>(acts[i]) + 0x70, sizeof(void*))) {
			continue;
		}
		void* room = roomAt(dataFlag, acts[i], subtileX / ac::SubtilesPerTile, subtileY / ac::SubtilesPerTile);
		if (room != nullptr && RoomLevelOf(room) == level) {
			return room;
		}
	}
	*why = "no room of level " + std::to_string(level) + " holds tile (" + std::to_string(subtileX / ac::SubtilesPerTile) + "," + std::to_string(subtileY / ac::SubtilesPerTile) + ")";
	return nullptr;
}

enum class MoveKind { Moved, Left, Failed };
struct MoveResult {
	MoveKind kind = MoveKind::Failed;
	std::string why;
};

// Engine calls, lock released. First re-checks the player is in the slot level of a Hell game (the guard that keeps this off vanilla levels), then
// the 'goto' route: find the room, bring it up, UnitMove into it. Fails with the reason instead of moving anything it cannot place.
auto MovePlayerToBoss(Game* game, Unit* unit, int level, ac::Point stand) noexcept -> MoveResult {
	MoveResult out;
	if (PlayerLevelOf(unit) != level || GameDifficultyOf(game) != HellDifficulty) {
		out.kind = MoveKind::Left;
		out.why = "the player is no longer in the map's level of a Hell game";
		return out;
	}
	if (!MoverReady()) {
		out.why = "a part the move needs was not found";
		return out;
	}
	void* room = FindRoomAt(game, level, stand.x, stand.y, &out.why);
	if (room == nullptr) {
		return out;
	}
	reinterpret_cast<RoomBringUpFn>(const_cast<uint8_t*>(g_parts[BringUpPart].address))(game, room);
	if (reinterpret_cast<UnitMoveFn>(const_cast<uint8_t*>(g_parts[MovePart].address))(game, unit, room, stand.x, stand.y, 0, 0) == 0) {
		out.why = "the game refused the move (UnitMove returned 0)";
		return out;
	}
	out.kind = MoveKind::Moved;
	return out;
}

// One step of the run, with g_lock held (`lock`); it is released around every engine call and `gen` says whether the game changed meanwhile.
void Step(void* game, void* unit, std::unique_lock<std::mutex>& lock, uint64_t gen) noexcept {
	switch (g_run.phase) {
	case Phase::Idle:
	case Phase::Done:
		return;
	case Phase::Delay: {
		if (!ac::timedOut(g_run.phaseStartMs, NowMs(), StartDelayMs)) {
			return;
		}
		if (GameDifficultyOf(game) != HellDifficulty) {
			Line("AUTOCHECK skipped: this is not a Hell game (maps open in Hell only)");
			Enter(Phase::Done);
			return;
		}
		if (!g_run.config.problem.empty()) {
			Line("AUTOCHECK " + g_run.config.problem);
			Settle(ac::Item::Open, false, g_run.config.problem);
			Finish("the map to check could not be chosen");
			return;
		}
		Line("AUTOCHECK start: map '" + g_run.config.map + "' in a Hell game");
		Enter(Phase::Open);  // opened on the next tick
		return;
	}
	case Phase::Open: {
		const std::string name = g_run.config.map;
		MapDef map;
		const CheckOpen opened = Outside(lock, [&] { return OpenMapForCheck(game, unit, name, &map); });
		if (g_gen != gen) {
			return;
		}
		g_run.opened = opened;
		if (!opened.opened) {
			Settle(ac::Item::Open, false, opened.why);
			Finish("the map did not open");
			return;
		}
		g_run.map = map;
		Settle(ac::Item::Open, true, "'" + g_run.map.name + "' (" + g_run.map.tileset + ") opened through level " + std::to_string(opened.level));
		SettleMonsterSettings();
		Line("AUTOCHECK waiting: walk into the red portal (no native call to enter it was found, and entering by hand keeps the arrival check real); up to " +
		     std::to_string(ArrivalWaitMs / 1000) + " s");
		Enter(Phase::Arrival);
		return;
	}
	case Phase::Arrival: {
		if (PlayerLevelOf(unit) == g_run.opened.level) {
			Line("AUTOCHECK arrival: the player is in level " + std::to_string(g_run.opened.level));
			Enter(Phase::ArrivalRead);
			return;
		}
		if (ac::timedOut(g_run.phaseStartMs, NowMs(), ArrivalWaitMs)) {
			Settle(ac::Item::Arrival, false, "the player did not enter the portal level within " + std::to_string(ArrivalWaitMs / 1000) + " s");
			Finish("the player never reached the map");
		}
		return;
	}
	case Phase::ArrivalRead: {
		if (!MapIsMaze(g_run.map)) {
			SettleNotApplicable(ac::Item::Arrival, "a " + g_run.map.tileset + " map has no maze entrance cell to compare; the player is in the portal level");
			Enter(Phase::MoveToBoss);
			return;
		}
		const int result = Outside(lock, [&] { return MazeArrivalResult(); });
		if (g_gen != gen) {
			return;
		}
		if (result == 1) {
			Settle(ac::Item::Arrival, true, "ARRIVAL IS IN THE ENTRANCE CELL");
		} else if (result == 2) {
			Settle(ac::Item::Arrival, false, "arrival is NOT in the entrance cell");
		} else if (result == 3 || ac::timedOut(g_run.phaseStartMs, NowMs(), ArrivalReadMs)) {
			Settle(ac::Item::Arrival, false, "the arrival room could not be read (see the '[maze probe] arrival' line)");
		} else {
			return;  // the arrival probe has not run yet
		}
		Enter(Phase::MoveToBoss);
		return;
	}
	case Phase::MoveToBoss: {
		if (g_run.map.boss.empty()) {
			SettleNotApplicable(ac::Item::Boss, "the map has no Boss");
			Enter(Phase::Stats);
			return;
		}
		const MazeEventRect room = Outside(lock, [&] { return MazeBossRoom(); });
		if (g_gen != gen) {
			return;
		}
		const int level = g_run.opened.level;
		if (!room.known || room.levelId != level) {
			Settle(ac::Item::Boss, false, "no boss room to go to: the plan has none I can read (only Cave and Crypt maps with BossRoom on have one)");
			Enter(Phase::Stats);
			return;
		}
		const ac::Point stand = ac::bossStandPoint(room.x, room.y, room.w, room.h);
		const std::string where = "tiles " + std::to_string(room.x) + "," + std::to_string(room.y) + ", " + std::to_string(room.w) + " x " + std::to_string(room.h);
		Line("AUTOCHECK boss: moving the player to subtile (" + std::to_string(stand.x) + "," + std::to_string(stand.y) + ") in the boss room (" + where + ")");
		const MoveResult moved = Outside(lock, [&] { return MovePlayerToBoss(game, unit, level, stand); });
		if (g_gen != gen) {
			return;
		}
		if (moved.kind == MoveKind::Left) {
			Settle(ac::Item::Boss, false, "the player left the map before the move: " + moved.why);
			Enter(Phase::Stats);
			return;
		}
		g_run.moved = moved.kind == MoveKind::Moved;
		if (!g_run.moved) {
			Line("AUTOCHECK boss: the player was not moved (" + moved.why + "), so the player must walk to the boss room (" + where + "); up to " +
			     std::to_string(BossWaitManualMs / 1000) + " s");
		}
		Enter(Phase::WaitBoss);
		return;
	}
	case Phase::WaitBoss: {
		if (g_run.bossNoted) {
			SettleBossFromNote();
			Enter(Phase::Stats);
			return;
		}
		const int level = g_run.opened.level;
		if (PlayerLevelOf(unit) != level || GameDifficultyOf(game) != HellDifficulty) {
			Settle(ac::Item::Boss, false, "the player left the map before the Boss was reported");
			Enter(Phase::Stats);
			return;
		}
		if (ac::timedOut(g_run.phaseStartMs, NowMs(), g_run.moved ? BossWaitMovedMs : BossWaitManualMs)) {
			Settle(ac::Item::Boss, false, std::string("the map Boss was not reported within the wait") + (g_run.moved ? " after moving the player (the boss room may not have been populated)" : ""));
			Enter(Phase::Stats);
		}
		return;
	}
	case Phase::Stats: {
		if (!ac::timedOut(g_run.phaseStartMs, NowMs(), StatGraceMs)) {
			return;
		}
		const StatApplyCounts counts = Outside(lock, [&] { return StatApplyCountsNow(); });
		if (g_gen != gen) {
			return;
		}
		SettleStats(counts);
		Finish("not reached");
		return;
	}
	}
}

}  // namespace

void InstallAutoCheck(const D2RL::PluginContext* context, const std::vector<SignatureOverride>& overrides, const CodeRange* ranges, int count) {
	try {
		for (Part& part : g_parts) {
			ResolvePart(context, overrides, ranges, count, part);
			plog::Info("part %s: %s", part.name, part.status.detail.c_str());
		}
	} catch (...) {
		plog::Error("AutoCheck: looking for the unit mover failed; the player must walk to the boss room");
	}
}

void AutoCheckSetTables(const std::shared_ptr<const Tables>& tables) {
	std::lock_guard<std::mutex> lock(g_lock);
	g_tables = tables;
	g_on = tables != nullptr && tables->settings.autoCheck != 0;
}

void AutoCheckGameEntered() {
	if (!g_on.load()) {
		return;
	}
	std::lock_guard<std::mutex> lock(g_lock);
	if (g_tables == nullptr) {
		return;
	}
	std::vector<std::string> names;
	for (const MapDef& map : g_tables->maps) {
		names.push_back(map.name);
	}
	++g_gen;
	ResetRun();
	g_run.config = ac::resolveConfig(g_tables->settings.autoCheck, g_tables->settings.autoCheckMap, names);
	if (g_run.config.enabled) {
		Enter(Phase::Delay);
		plog::Info("AutoCheck is on (test slots only): the check starts in %llu ms", static_cast<unsigned long long>(StartDelayMs));
	}
}

void AutoCheckGameLeft() {
	if (!g_on.load() && !g_active.load()) {
		return;
	}
	std::lock_guard<std::mutex> lock(g_lock);
	++g_gen;
	if (g_run.phase != Phase::Idle && g_run.phase != Phase::Done) {
		// The game ended under the check: still give every item its line, so a log is never left without a DONE.
		Finish("the game was left before the check finished");
	}
	ResetRun();
}

void AutoCheckTick(void* game, void* unit, uint32_t localPlayer) noexcept {
	if (!g_on.load(std::memory_order_relaxed) || !g_active.load(std::memory_order_relaxed) || game == nullptr || unit == nullptr) {
		return;
	}
	if (At<int32_t>(unit, UnitTypeOffset) != 0 || At<uint32_t>(unit, UnitIdOffset) != localPlayer) {
		return;
	}
	std::unique_lock<std::mutex> lock(g_lock);
	if (g_run.phase == Phase::Idle || g_run.phase == Phase::Done) {
		return;
	}
	const uint64_t gen = g_gen;
	try {
		Step(game, unit, lock, gen);
	} catch (...) {
		if (!lock.owns_lock()) {
			lock.lock();
		}
		if (g_gen == gen) {
			plog::Error("AutoCheck: a step failed");
			Settle(g_run.list.next().value_or(ac::Item::Open), false, "a step threw");
			Finish("a step threw");
		}
	}
}

void AutoCheckNoteBoss(const std::string& name, bool spawned, bool asSuper, const std::string& problem) noexcept {
	if (!g_on.load(std::memory_order_relaxed)) {
		return;
	}
	try {
		std::lock_guard<std::mutex> lock(g_lock);
		if (g_run.phase == Phase::Idle || g_run.bossNoted) {
			return;  // the Boss is logged once
		}
		g_run.bossNoted = true;
		g_run.bossSpawned = spawned;
		g_run.bossSuper = asSuper;
		g_run.bossProblem = problem;
		plog::Info("AUTOCHECK boss: '%s' %s%s%s", name.c_str(), spawned ? "spawned as " : "was not spawned", spawned ? (asSuper ? "a superunique" : "a plain monster") : "",
		           problem.empty() ? "" : (": " + problem).c_str());
	} catch (...) {
	}
}

}  // namespace carto
