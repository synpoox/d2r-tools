#include "engine/auto_check.h"

#include "autocheck.h"
#include "core/part_signatures.h"
#include "engine/maze_hooks.h"
#include "engine/stat_apply.h"
#include "log.h"

#include <windows.h>

#include <atomic>
#include <mutex>
#include <optional>

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

using Game = void;
using Unit = void;
// (game, unit, room or null, x, y, 0, 0): puts the unit at a subtile position in the level it is in (cartographer.md section 16).
using UnitMoveFn = uint64_t (*)(Game*, Unit*, void* room, int32_t x, int32_t y, int32_t a6, int32_t a7) noexcept;

Part g_move = {"UnitMove", sig::UnitMove, 0, false, nullptr, {}, {}};

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

std::mutex g_lock;
std::shared_ptr<const Tables> g_tables;
Run g_run;
std::atomic<bool> g_on{false};  // AutoCheck=1: the only thing the tick reads when the setting is off

auto NowMs() noexcept -> uint64_t {
	return GetTickCount64();
}

void Enter(Phase phase) noexcept {
	g_run.phase = phase;
	g_run.phaseStartMs = NowMs();
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
	Line(ac::resultLine(ac::Result{item, pass, detail}));
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
		Settle(ac::Item::MapSettings, true, "n/a: the map asks for nothing beyond the slot level's own monsters (" + columns + ")");
	} else if (g_run.opened.shaped) {
		Settle(ac::Item::MapSettings, true, "written for the slot level (" + columns + "); see the 'map monsters:' lines for each field");
	} else {
		Settle(ac::Item::MapSettings, false, "asked for " + columns + " but nothing was written (see the 'map monsters:' lines above for why)");
	}
	if (g_run.map.champions > 0) {
		Line("AUTOCHECK note: Champions " + std::to_string(g_run.map.champions) + "% is not applied by design (the game has no field for it)");
	}
}

void SettleStats() noexcept {
	const StatApplyCounts counts = StatApplyCountsNow();
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
	if (g_run.bossSpawned) {
		Settle(ac::Item::Boss, true, "'" + g_run.map.boss + "' spawned once as " + (g_run.bossSuper ? "a superunique" : "a plain monster") + (g_run.moved ? "" : "; the player walked to the boss room"));
	} else {
		Settle(ac::Item::Boss, false, "'" + g_run.map.boss + "' was not spawned: " + (g_run.bossProblem.empty() ? "unknown" : g_run.bossProblem));
	}
}

void Step(void* game, void* unit) noexcept {
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
		g_run.opened = OpenMapForCheck(game, unit, g_run.config.map, &g_run.map);
		if (!g_run.opened.opened) {
			Settle(ac::Item::Open, false, g_run.opened.why);
			Finish("the map did not open");
			return;
		}
		Settle(ac::Item::Open, true, "'" + g_run.map.name + "' (" + g_run.map.tileset + ") opened through level " + std::to_string(g_run.opened.level));
		SettleMonsterSettings();
		Line("AUTOCHECK waiting: walk into the red portal (no native call to enter it was found); up to " + std::to_string(ArrivalWaitMs / 1000) + " s");
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
			Settle(ac::Item::Arrival, true, "n/a for a " + g_run.map.tileset + " map: it has no maze entrance cell to compare; the player is in the portal level");
			Enter(Phase::MoveToBoss);
			return;
		}
		const int result = MazeArrivalResult();
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
			Settle(ac::Item::Boss, true, "n/a: the map has no Boss");
			Enter(Phase::Stats);
			return;
		}
		const MazeEventRect room = MazeBossRoom();
		if (!room.known || room.levelId != g_run.opened.level) {
			Settle(ac::Item::Boss, false, "no boss room to go to: the plan has none I can read (only Cave and Crypt maps with BossRoom on have one)");
			Enter(Phase::Stats);
			return;
		}
		const ac::Point stand = ac::bossStandPoint(room.x, room.y, room.w, room.h);
		if (g_move.address == nullptr) {
			Line("AUTOCHECK boss: the unit mover was not found, so the player must walk to the boss room (tiles " + std::to_string(room.x) + "," + std::to_string(room.y) + ", " +
			     std::to_string(room.w) + " x " + std::to_string(room.h) + "); up to " + std::to_string(BossWaitManualMs / 1000) + " s");
		} else {
			Line("AUTOCHECK boss: moving the player to subtile (" + std::to_string(stand.x) + "," + std::to_string(stand.y) + ") in the boss room (tiles " + std::to_string(room.x) + "," +
			     std::to_string(room.y) + ", " + std::to_string(room.w) + " x " + std::to_string(room.h) + ")");
			reinterpret_cast<UnitMoveFn>(const_cast<uint8_t*>(g_move.address))(game, unit, nullptr, stand.x, stand.y, 0, 0);
			g_run.moved = true;
		}
		Enter(Phase::WaitBoss);
		return;
	}
	case Phase::WaitBoss: {
		bool noted = false;
		{
			std::lock_guard<std::mutex> lock(g_lock);
			noted = g_run.bossNoted;
		}
		if (noted) {
			SettleBossFromNote();
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
		SettleStats();
		Finish("not reached");
		return;
	}
	}
}

}  // namespace

void InstallAutoCheck(const D2RL::PluginContext* context, const std::vector<SignatureOverride>& overrides, const CodeRange* ranges, int count) {
	try {
		ResolvePart(context, overrides, ranges, count, g_move);
		plog::Info("part %s: %s", g_move.name, g_move.status.detail.c_str());
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
	std::shared_ptr<const Tables> tables;
	{
		std::lock_guard<std::mutex> lock(g_lock);
		tables = g_tables;
	}
	if (tables == nullptr) {
		return;
	}
	std::vector<std::string> names;
	for (const MapDef& map : tables->maps) {
		names.push_back(map.name);
	}
	std::lock_guard<std::mutex> lock(g_lock);
	g_run = Run{};
	g_run.config = ac::resolveConfig(tables->settings.autoCheck, tables->settings.autoCheckMap, names);
	if (g_run.config.enabled) {
		Enter(Phase::Delay);
		plog::Info("AutoCheck is on (test slots only): the check starts in %llu ms", static_cast<unsigned long long>(StartDelayMs));
	}
}

void AutoCheckGameLeft() {
	if (!g_on.load() && g_run.phase == Phase::Idle) {
		return;
	}
	std::lock_guard<std::mutex> lock(g_lock);
	if (g_run.phase != Phase::Idle && g_run.phase != Phase::Done) {
		// The game ended under the check: still give every item its line, so a log is never left without a DONE.
		Finish("the game was left before the check finished");
	}
	g_run = Run{};
}

void AutoCheckTick(void* game, void* unit, uint32_t localPlayer) noexcept {
	if (!g_on.load(std::memory_order_relaxed) || g_run.phase == Phase::Idle || g_run.phase == Phase::Done || game == nullptr || unit == nullptr) {
		return;
	}
	if (At<int32_t>(unit, UnitTypeOffset) != 0 || At<uint32_t>(unit, UnitIdOffset) != localPlayer) {
		return;
	}
	try {
		Step(game, unit);
	} catch (...) {
		plog::Error("AutoCheck: a step failed");
		Settle(g_run.list.next().value_or(ac::Item::Open), false, "a step threw");
		Finish("a step threw");
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
