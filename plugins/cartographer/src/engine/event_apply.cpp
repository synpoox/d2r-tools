#include "engine/event_apply.h"

#include "core/part_signatures.h"
#include "engine/auto_check.h"
#include "engine/event_6b.h"
#include "engine/game_memory.h"
#include "engine/level_row.h"
#include "engine/map_panel.h"
#include "engine/maze_hooks.h"
#include "engine/outdoor_hooks.h"
#include "engine/scan.h"
#include "log.h"
#include "hookguard.h"

#include <windows.h>

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace carto {

namespace {

namespace ep = cartographer::eventplan;
namespace ev = cartographer::events;

// --- Engine facts. Build 93847; each is recorded with its evidence in docs/reverse-engineering/cartographer/09-map-events.md.
constexpr int GameDataFlagOffset = 0x106;    // uint8 on the game, what the engine's own room lookups read
constexpr int UnitTypeOffset = 0x00;        // int32: 0 player
constexpr int UnitIdOffset = 0x08;          // uint32
constexpr int ServerDrlgRoomOffset = 0x18;  // a server room's DRLG room
constexpr int ServerRectOffset = 0x80;      // a server room's x, y, w, h (int32 each): the rect the spawn search and the point-in-room test use
constexpr int DrlgLevelOffset = 0x90;       // a DRLG room's level record
constexpr int LevelSizeOffset = 0x2c;       // a level record's width and height (int32 each), in tiles (static; section 5)
constexpr int DrlgRectOffset = 0x60;        // a DRLG room's x, y, w, h (int32 each), in tiles
constexpr int GameSuperBitmapOffset = 0x1d8;  // the game's once-per-game superunique bitmap
constexpr int32_t SpawnMode = 1;            // what the superunique code passes to the monster spawn
constexpr int32_t SpawnParam = 0xa;         // the same caller's seventh argument
constexpr uint16_t SpawnFlags = 0;          // 0: the game's normal path, so the map's stats reach event monsters too
constexpr uint16_t SpawnFlagNoCollision = 0x80;  // the spawn core takes a ring spot the collision test refused (static, 0x4941ef; only the last-resort Boss uses it)
constexpr int MaxPopulateProbes = 12;       // rooms of the map level logged in full
// Spawn names from the game's tables (section 11). `dt` is the data tables base for the unit's data flag.
constexpr int UnitDataFlagOffset = 0x1bd;   // uint8 on a unit (the same byte stat_apply.cpp reads)
constexpr uint32_t DataFlagLimit = 4;       // the base accessor takes a data flag below 4 (anything else is a trap in the engine)
constexpr int DtMonRowsOffset = 0xf58;      // monstats row vector pointer; the 64-bit count is at +0xf60
constexpr int DtMonCountOffset = 0xf60;
constexpr int DtLevelCountOffset = 0x1468;  // 64-bit count of the levels rows (the region builder 0x508750 loops to it)
constexpr int MonStatsSpawnFlagOffset = 0x3c;  // byte in a monstats row: the group builder 0x509940 keeps a row only when bit 0 is set
constexpr int MonStatsWeightOffset = 0x64;  // uint8 in a monstats row: what the region builder (0x509c2c) takes as the group's weight
constexpr int DtMonMapOffset = 0xf70;      // monstats Id -> row map pointer (re-read on every use: a table load makes a new map)
constexpr int DtSuperRowsOffset = 0x1078;   // superuniques row vector pointer; the 64-bit count is at +0x1080
constexpr int DtSuperCountOffset = 0x1080;
constexpr int DtSuperMapOffset = 0x1090;    // superuniques name -> row map pointer (re-read on every use)
constexpr int SuperClassOffset = 8;         // the row's Class, a monstats row (read as 16 bits: its width is inferred)

struct Game;
struct Unit;

using PopulateFn = uint64_t (*)(Game*, void* room) noexcept;
using SpawnMonsterFn = void* (*)(Game*, void* room, int32_t x, int32_t y, int32_t monsterId, int32_t mode, int32_t param, uint16_t flags) noexcept;
using SpawnSuperFn = void* (*)(Game*, void* room, int32_t x, int32_t y, int32_t row, uint16_t flags) noexcept;
using UnitRoomFn = void* (*)(Unit*) noexcept;
using RoomLevelFn = int32_t (*)(void* room) noexcept;
using DataTablesBaseFn = uint8_t* (*)(uint32_t dataFlag) noexcept;
using NameToRowFn = int32_t (*)(void* map, const char* name, int32_t logOnMiss) noexcept;

enum PartId { PopulateRoom, SpawnMonster, SpawnSuperUnique, EvUnitRoom, EvRoomLevelId, DataTablesBase, NameToRow, PartCount };

Part g_parts[PartCount] = {
	{"PopulateRoom", sig::PopulateRoom, 12, false, nullptr, {}, {}},
	{"SpawnMonster", sig::SpawnMonster, 0, false, nullptr, {}, {}},
	{"SpawnSuperUnique", sig::SpawnSuperUnique, 0, false, nullptr, {}, {}},
	{"EventUnitRoom", sig::UnitRoom, 0, false, nullptr, {}, {}},
	{"EventRoomLevelId", sig::RoomLevelId, 0, false, nullptr, {}, {}},
	{"DataTablesBase", sig::DataTablesBase, 0, false, nullptr, {}, {}},
	{"NameToRow", sig::NameToRow, 0, false, nullptr, {}, {}},
};

const D2RL::PluginContext* g_context = nullptr;
PopulateFn g_original = nullptr;
std::atomic<bool> g_spawnOn{false};   // the populator hook is in and the spawn parts are found
std::atomic<bool> g_tickOn{false};    // the entry message can be shown (the portal's tick part and the room parts)
std::atomic<bool> g_busy{false};      // a map is armed

template <typename Fn>
auto Call(PartId id) noexcept -> Fn {
	return reinterpret_cast<Fn>(const_cast<uint8_t*>(g_parts[id].address));
}

auto On(PartId id) noexcept -> bool {
	return g_parts[id].address != nullptr && g_parts[id].status.on;
}

// The armed map. Guarded by g_lock; replaced when a map opens, cleared when the game is left.
struct State {
	std::string mapName;
	int level = -1;
	bool maze = false;
	std::optional<ev::Event> event;  // only an event that can be spawned: its Name and Message are what the player is shown
	ep::Resolved spawn;
	uint64_t seed = 0;
	std::optional<ep::EventRoomTracker> tracker;  // made at the first room of the level, when the plan's event room is known
	ep::EntryTracker entry;
	bool spawned = false;
	bool announced = false;  // the event room was chosen and something spawned: the banner may name the event
	// The map's own Boss (maps.txt `Boss`), separate from the event: one monster in the boss room, once per map open.
	std::string bossName;
	ep::Resolved bossSpawn;
	bool bossArmed = false;  // the name resolves and the spawn parts are on
	std::optional<ep::BossRoomPicker> bossPicker;  // made with the event tracker, at the first populated room
	std::vector<void*> bossRooms;                  // the game's room of each picker candidate, once populated (the level's rooms outlive the map's game)
	int probes = 0;
	int bossTestRefuse = 0;  // CARTOGRAPHER_TEST_BOSS_REFUSE: the first N ordinary Boss tries are refused, to reach the fallbacks in a game check
	int bossTries = 0;
};
std::mutex g_lock;
std::shared_ptr<State> g_state;
uint64_t g_generation = 0;

// --- Banner ---------------------------------------------------------------------------------------------------------------

void ShowEntryMessage(const std::vector<std::string>& lines) noexcept {
	if (lines.empty()) {
		return;
	}
	if (ShowEntryBanner(lines)) {
		plog::Info("eventprobe: entry message handed to the banner panel (%zu line(s))", lines.size());
		return;
	}
	for (const std::string& line : lines) {
		if (g_context != nullptr) {
			g_context->WriteConsoleMessage(line.c_str());
		}
	}
	plog::Info("eventprobe: entry message written to the console (%zu line(s)); no banner panel", lines.size());
}

// --- Spawn names from the game's loaded tables (cartographer/11-spawn-names.md) -----------------------------------------------
// Monstats `Id`: the SDK table, scanned. Superuniques: the game's own name map, called (never cached), only after a self-check
// has shown that the SDK views are the game's tables for the unit's data flag and that the monstats map maps every Id back to its
// row. A route that fails its check is off for that (table view revision, data flag) and the mod's .txt files are used instead.

const D2RL::DataTableService* g_dataTables = nullptr;
std::mutex g_routeLock;
ep::RouteCache g_routeCache;

// One table's SDK view beside the game's own vector for it. False when the SDK did not give a view (`busy`: not the game thread).
auto TableFactsOf(D2RL::DataTables::Bank bank, D2RL::DataTables::TableId table, const uint8_t* dt, int rowsOffset, int countOffset, ep::TableFacts* out,
                  bool* busy) noexcept -> bool {
	D2RL::DataTables::TableView view{};
	view.structSize = D2RL::DataTables::TableViewSize;
	const D2RL::DataTables::Result result = g_dataTables->getTable(g_context, bank, table, &view);
	if (result == D2RL::DataTables::Result::Busy) {
		*busy = true;
	}
	if (result != D2RL::DataTables::Result::Success || !D2RL::DataTables::HasTableViewField(&view, D2RL::DataTables::TableViewRequiredSize)) {
		return false;
	}
	out->viewRows = view.rows;
	out->viewCount = view.rowCount;
	out->viewRowSize = view.rowSize;
	out->dtRows = At<const void*>(dt, rowsOffset);
	out->dtCount = At<uint64_t>(dt, countOffset);
	return true;
}

auto BankName(uint32_t bank) -> const char* {
	return bank == 1 ? "Classic" : bank == 2 ? "Lod" : bank == 3 ? "Rotw" : "none";
}

// The data tables base for a data flag, or null (with `why`).
auto TablesBase(uint32_t dataFlag, std::string* why) noexcept -> const uint8_t* {
	if (g_dataTables == nullptr) {
		*why = "D2RLoader has no DataTable service of the expected version";
	} else if (!On(DataTablesBase)) {
		*why = "the DataTablesBase part is off";
	} else if (dataFlag >= DataFlagLimit) {
		*why = "the unit's data flag " + std::to_string(dataFlag) + " is not below 4";
	} else {
		const uint8_t* dt = Call<DataTablesBaseFn>(DataTablesBase)(dataFlag);
		if (dt != nullptr) {
			return dt;
		}
		*why = "the data tables base is null";
	}
	return nullptr;
}

// Looks every distinct monstats Id up in the game's map: each must come back as its own row.
auto RoundTripMonsters(const uint8_t* dt, const ep::TableFacts& mon) noexcept -> ep::RoundTrip {
	ep::RoundTrip trip;
	void* map = At<void*>(dt, DtMonMapOffset);
	trip.mapNull = map == nullptr;
	if (trip.mapNull || !On(NameToRow)) {
		return trip;
	}
	const auto* rows = static_cast<const uint8_t*>(mon.viewRows);
	// Rows by lower-cased Id. A repeated Id keeps every row: which copy the game's map holds is not known (section 11).
	std::unordered_map<std::string, std::vector<int>> byId;
	std::vector<std::pair<int, std::string>> order;  // each distinct Id once, at its first row
	for (uint32_t i = 0; i < mon.viewCount; ++i) {
		const char* field = reinterpret_cast<const char*>(rows + static_cast<size_t>(i) * mon.viewRowSize + ep::kMonStatsIdOffset);
		const std::optional<std::string_view> id = ep::idText(field, ep::kMonStatsIdSize);
		if (id && id->empty()) {
			continue;
		}
		if (!id) {
			++trip.checked;  // an Id with no NUL is not text: it cannot come back
			if (trip.wrong++ == 0) {
				trip.firstWrongRow = static_cast<int>(i);
				trip.firstWrongGot = -2;
			}
			continue;
		}
		std::string key(*id);
		for (char& c : key) {
			c = c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c;
		}
		std::vector<int>& list = byId[key];
		if (list.empty()) {
			order.emplace_back(static_cast<int>(i), std::string(*id));
		}
		list.push_back(static_cast<int>(i));
	}
	for (const auto& [row, id] : order) {
		std::string key = id;
		for (char& c : key) {
			c = c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c;
		}
		const int got = Call<NameToRowFn>(NameToRow)(map, id.c_str(), 0);
		++trip.checked;
		if (!ep::idRowAccepted(byId[key], got) && trip.wrong++ == 0) {
			trip.firstWrongRow = row;
			trip.firstWrongGot = got;
		}
	}
	return trip;
}

// The self-check for one data flag. `transient`: the SDK was busy, so the verdict is not to be cached.
auto EvaluateRoute(uint32_t dataFlag, bool* transient) noexcept -> ep::RouteVerdict {
	ep::RouteVerdict verdict;
	std::string why;
	const uint8_t* dt = TablesBase(dataFlag, &why);
	if (dt == nullptr) {
		verdict.why = why;
		return verdict;
	}
	// The Bank whose monstats view is the game's table for this data flag.
	ep::TableFacts mon;
	std::string tried;
	for (uint32_t bank = 1; bank <= 3 && verdict.bank == 0; ++bank) {
		ep::TableFacts facts;
		bool busy = false;
		if (!TableFactsOf(static_cast<D2RL::DataTables::Bank>(bank), D2RL::DataTables::TableId::MonStats, dt, DtMonRowsOffset, DtMonCountOffset, &facts, &busy)) {
			if (busy) {
				*transient = true;
				verdict.why = "the SDK data tables are busy on this thread";
				return verdict;
			}
			tried += std::string(tried.empty() ? "" : "; ") + BankName(bank) + ": no view";
			continue;
		}
		const std::string problem = ep::checkTable(facts, ep::kMonStatsRowSize);
		if (problem.empty()) {
			verdict.bank = bank;
			mon = facts;
		} else {
			tried += std::string(tried.empty() ? "" : "; ") + BankName(bank) + ": " + problem;
		}
	}
	if (verdict.bank == 0) {
		verdict.why = "no SDK bank matches the game's monstats table for data flag " + std::to_string(dataFlag) + " (" + tried + ")";
		return verdict;
	}
	verdict.monsters = true;
	// The superuniques route: its table, its map, and the round trip through the monstats map.
	ep::TableFacts sup;
	bool busy = false;
	if (!On(NameToRow)) {
		verdict.why = "superuniques by name: the NameToRow part is off";
	} else if (!TableFactsOf(static_cast<D2RL::DataTables::Bank>(verdict.bank), D2RL::DataTables::TableId::SuperUniques, dt, DtSuperRowsOffset, DtSuperCountOffset, &sup, &busy)) {
		*transient = busy;
		verdict.why = "superuniques by name: no SDK view";
	} else if (const std::string problem = ep::checkTable(sup, ep::kSuperRowSize); !problem.empty()) {
		verdict.why = "superuniques by name: " + problem;
	} else if (At<void*>(dt, DtSuperMapOffset) == nullptr) {
		verdict.why = "superuniques by name: the superuniques name map is null";
	} else if (const std::string problem = ep::checkRoundTrip(RoundTripMonsters(dt, mon)); !problem.empty()) {
		verdict.why = "superuniques by name: " + problem;
	} else {
		verdict.supers = true;
	}
	return verdict;
}

// The SDK's own revision of the loaded tables, from the first Bank that gives a monstats view; 0 when none does (or the SDK is
// busy on this thread). It changes on every table load, so no listener is needed.
auto TablesRevision() noexcept -> uint64_t {
	for (uint32_t bank = 1; bank <= 3; ++bank) {
		D2RL::DataTables::TableView view{};
		view.structSize = D2RL::DataTables::TableViewSize;
		if (g_dataTables->getTable(g_context, static_cast<D2RL::DataTables::Bank>(bank), D2RL::DataTables::TableId::MonStats, &view) == D2RL::DataTables::Result::Success &&
		    D2RL::DataTables::HasTableViewField(&view, D2RL::DataTables::TableViewRequiredSize)) {
			return view.revision;
		}
	}
	return 0;
}

// The verdict for this unit's data flag: cached per (table view revision, data flag) once a revision is known.
auto RouteFor(uint32_t dataFlag) noexcept -> ep::RouteVerdict {
	const ep::RouteKey key{TablesRevision(), dataFlag};
	if (key.revision != 0) {
		std::lock_guard<std::mutex> lock(g_routeLock);
		if (const ep::RouteVerdict* hit = g_routeCache.find(key)) {
			return *hit;
		}
	}
	bool transient = false;
	ep::RouteVerdict verdict = EvaluateRoute(dataFlag, &transient);
	const std::string bank = verdict.monsters ? std::string(" (SDK bank ") + BankName(verdict.bank) + ")" : std::string();
	plog::Info("spawn names: data flag %u, table revision %llu: monstats from the game's table %s%s, superuniques by the game's name map %s%s%s", dataFlag,
	           static_cast<unsigned long long>(key.revision), verdict.monsters ? "on" : "OFF", bank.c_str(), verdict.supers ? "on" : "OFF",
	           verdict.why.empty() ? "" : "; ", verdict.why.c_str());
	if (!transient && key.revision != 0) {
		std::lock_guard<std::mutex> lock(g_routeLock);
		g_routeCache.store(key, verdict);
	}
	return verdict;
}

// The game's own tables for `spawn`. Returns false when the route cannot decide (the mod's .txt files then do). `out` stays
// Unresolved for a name the tables do not have. Game thread, at map open.
auto ResolveFromGameTables(const ep::RouteVerdict& route, uint32_t dataFlag, const std::string& spawn, const ep::SpawnTable& txt, ep::Resolved* out) noexcept -> bool {
	std::string why;
	const uint8_t* dt = TablesBase(dataFlag, &why);
	ep::TableFacts mon;
	bool busy = false;
	// Checked against the live table on every call (cheap): a table load since the cached verdict turns the route off here too.
	if (dt == nullptr || !TableFactsOf(static_cast<D2RL::DataTables::Bank>(route.bank), D2RL::DataTables::TableId::MonStats, dt, DtMonRowsOffset, DtMonCountOffset, &mon, &busy) ||
	    !ep::checkTable(mon, ep::kMonStatsRowSize).empty()) {
		plog::Warn("spawn names: the game's monstats table could not be read now%s%s; the mod's .txt files are used", why.empty() ? "" : ": ", why.c_str());
		return false;
	}
	if (const int row = ep::findMonsterRow(static_cast<const uint8_t*>(mon.viewRows), mon.viewCount, mon.viewRowSize, spawn); row >= 0) {
		out->kind = ev::NameKind::Monster;
		out->row = out->classRow = row;
		plog::Info("spawn '%s': monstats row %d from the game's table", spawn.c_str(), row);
		return true;
	}
	if (!route.supers) {
		return false;  // no superunique lookup: the .txt files get to look for a superunique of this name
	}
	if (spawn.empty()) {
		return true;
	}
	ep::TableFacts sup;
	void* map = At<void*>(dt, DtSuperMapOffset);  // re-read, never cached
	if (map == nullptr || !On(NameToRow) || !TableFactsOf(static_cast<D2RL::DataTables::Bank>(route.bank), D2RL::DataTables::TableId::SuperUniques, dt, DtSuperRowsOffset, DtSuperCountOffset, &sup, &busy) ||
	    !ep::checkTable(sup, ep::kSuperRowSize).empty()) {
		plog::Warn("spawn names: the superuniques table or its name map is not available now; the mod's .txt files are used");
		return false;
	}
	const int32_t found = Call<NameToRowFn>(NameToRow)(map, spawn.c_str(), 0);
	const int row = ep::superRowFromLookup(found, sup.viewCount);
	if (row < 0) {
		if (found != -1) {
			plog::Warn("spawn '%s': the superuniques lookup gave %d, which is neither a row nor the game's miss (-1); treated as not found", spawn.c_str(), found);
		}
		return true;
	}
	const uint16_t cls = At<uint16_t>(sup.viewRows, row * static_cast<int>(sup.viewRowSize) + SuperClassOffset);
	// The row must name a monstats row as its Class, and agree with the mod's txt where the txt has the name: else the map's values
	// are not what they were taken for (section 11), and the txt files decide.
	if (cls >= mon.viewCount) {
		plog::Warn("spawn '%s': the superuniques lookup gave row %d whose Class %u is not a monstats row; the mod's .txt files are used", spawn.c_str(), row, static_cast<unsigned>(cls));
		return false;
	}
	const char* idChars = reinterpret_cast<const char*>(static_cast<const uint8_t*>(mon.viewRows) + static_cast<size_t>(cls) * mon.viewRowSize + ep::kMonStatsIdOffset);
	const std::string_view gameId(idChars, strnlen(idChars, ep::kMonStatsIdSize));
	if (ep::checkSuperClass(txt, spawn, gameId) == ep::ClassCheck::Differs) {
		plog::Warn("spawn '%s': the superuniques lookup gave row %d with class monstats row %u, (Id '%.*s'), which the mod's superuniques.txt does not agree with; the .txt files are used", spawn.c_str(), row, static_cast<unsigned>(cls), static_cast<int>(gameId.size()), gameId.data());
		return false;
	}
	out->kind = ev::NameKind::Unique;
	out->row = row;
	out->classRow = static_cast<int>(cls);
	plog::Info("spawn '%s': superuniques row %d from the game's table, class monstats row %d", spawn.c_str(), row, out->classRow);
	return true;
}

// A monstats row by `Id` in the game's own table, or -1 (the table cannot be read now, or no such Id). Game thread, at map open.
auto GameMonsterRow(const ep::RouteVerdict& route, uint32_t dataFlag, std::string_view id) noexcept -> int {
	std::string why;
	const uint8_t* dt = TablesBase(dataFlag, &why);
	ep::TableFacts mon;
	bool busy = false;
	if (dt == nullptr || !TableFactsOf(static_cast<D2RL::DataTables::Bank>(route.bank), D2RL::DataTables::TableId::MonStats, dt, DtMonRowsOffset, DtMonCountOffset, &mon, &busy) ||
	    !ep::checkTable(mon, ep::kMonStatsRowSize).empty()) {
		return -1;
	}
	return ep::findMonsterRow(static_cast<const uint8_t*>(mon.viewRows), mon.viewCount, mon.viewRowSize, id);
}

// --- Rooms and spawns -----------------------------------------------------------------------------------------------------

struct RoomRects {
	bool ok = false;
	ep::Rect drlg;    // tiles
	ep::Rect server;  // the spawn code's units (subtiles, by static reading)
	int levelW = 0, levelH = 0;  // the level's size in tiles; 0 when it cannot be read
	void* level = nullptr;       // the DRLG level record, for the room list walk
};

auto ReadRects(void* serverRoom) noexcept -> RoomRects {
	RoomRects out;
	void* drlg = At<void*>(serverRoom, ServerDrlgRoomOffset);
	if (drlg == nullptr) {
		return out;
	}
	out.drlg = {At<int32_t>(drlg, DrlgRectOffset), At<int32_t>(drlg, DrlgRectOffset + 4), At<int32_t>(drlg, DrlgRectOffset + 8), At<int32_t>(drlg, DrlgRectOffset + 12)};
	out.server = {At<int32_t>(serverRoom, ServerRectOffset), At<int32_t>(serverRoom, ServerRectOffset + 4), At<int32_t>(serverRoom, ServerRectOffset + 8), At<int32_t>(serverRoom, ServerRectOffset + 12)};
	void* level = At<void*>(drlg, DrlgLevelOffset);
	out.level = level;
	if (level != nullptr) {
		out.levelW = At<int32_t>(level, LevelSizeOffset);
		out.levelH = At<int32_t>(level, LevelSizeOffset + 4);
	}
	out.ok = out.drlg.w > 0 && out.drlg.h > 0;
	return out;
}

auto Describe(const ep::Rect& r) -> std::string {
	char text[64];
	std::snprintf(text, sizeof(text), "(%d,%d) %d x %d", r.x, r.y, r.w, r.h);
	return text;
}

// One monster at a position in the server rect's units. Returns whether the game gave a unit.
auto SpawnOne(Game* game, void* room, const ep::Spawn& s, bool* asSuper, void** unitOut, uint16_t flags = SpawnFlags) noexcept -> bool {
	*asSuper = false;
	*unitOut = nullptr;
	const int32_t x = s.spot.x, y = s.spot.y;
	if (s.superUnique && On(SpawnSuperUnique)) {
		const uint8_t* bitmap = reinterpret_cast<const uint8_t*>(game) + GameSuperBitmapOffset;
		if (!ep::superSeen(bitmap, s.row)) {
			void* unit = Call<SpawnSuperFn>(SpawnSuperUnique)(game, room, x, y, s.row, 0);
			const bool setNow = ep::superSeen(bitmap, s.row);
			plog::Info("eventprobe: superunique row %d at (%d,%d): returned %p, once-per-game bit now %s", s.row, x, y, unit, setNow ? "set" : "NOT set");
			if (unit != nullptr) {
				*asSuper = true;
				*unitOut = unit;
				return true;
			}
		} else {
			plog::Info("eventprobe: superunique row %d was already spawned this game; its class (monstats row %d) is spawned instead", s.row, s.classRow);
		}
	}
	const int row = s.superUnique ? s.classRow : s.row;
	if (row < 0) {
		return false;
	}
	void* unit = Call<SpawnMonsterFn>(SpawnMonster)(game, room, x, y, row, SpawnMode, SpawnParam, flags);
	*unitOut = unit;
	return unit != nullptr;
}

// A room's server rect, to recognise the Ambush's event room by it (event_6b.cpp keeps no room pointer).
auto ReadServerRect(void* room, ep::Rect* out) noexcept -> bool {
	if (room == nullptr) {
		return false;
	}
	const RoomRects rects = ReadRects(room);
	*out = rects.server;
	return rects.ok;
}

// The Ambush's wave spawner (event_6b.cpp calls it each wave): one unit, as the Boss and Horde spawns make them.
auto SpawnAmbushUnit(void* game, void* room, const ep::Spawn& s, void** unit) noexcept -> bool {
	bool asSuper = false;
	return SpawnOne(static_cast<Game*>(game), room, s, &asSuper, unit);
}

// True when something was placed, so the banner may announce the event. An Ambush is only armed here: it announces itself when its first wave spawns.
auto SpawnEvent(Game* game, void* room, const State& state, const RoomRects& rects, uint64_t seed) noexcept -> bool {
	if (Event6bPlacesObject(*state.event, state.spawn.kind)) {
		return Event6bPlaceShrine(game, room, rects.server);  // phase 6b: a Shrine, or a Treasure chest
	}
	// Spots come from the server room's rect (its units are the spawn code's), spaced by tiles through the rect-to-tile scale.
	const ep::SpawnPlan plan = ep::planEvent(*state.event, state.spawn, rects.server, seed, ep::spotScale(rects.server, rects.drlg));
	if (!plan.problem.empty()) {
		plog::Warn("event '%s' not spawned: %s", state.event->name.c_str(), plan.problem.c_str());
		return false;
	}
	if (static_cast<int>(plan.spawns.size()) < plan.requested) {
		plog::Warn("event '%s' (%s): the event room %s has room for %zu of the %d monsters asked for", state.event->name.c_str(), ev::kindName(state.event->kind),
		           Describe(rects.server).c_str(), plan.spawns.size(), plan.requested);
	}
	if (state.event->kind == ev::Kind::Ambush) {
		if (!g_tickOn.load()) {
			plog::Warn("event '%s' (Ambush) not armed: the PlayerTick or room parts are off, so no wave could start", state.event->name.c_str());
			return false;
		}
		Event6bArmAmbush(*state.event, plan.spawns, rects.server, room);  // phase 6b: the waves are spawned from the player tick, when a player is in this room
		plog::Info("event '%s' (%s): armed in the event room %s: %zu per wave", state.event->name.c_str(), ev::kindName(state.event->kind), Describe(rects.server).c_str(), plan.spawns.size());
		return false;  // not announced yet: the first wave that spawns announces it
	}
	int made = 0, failed = 0, supers = 0;
	for (const ep::Spawn& s : plan.spawns) {
		bool asSuper = false;
		void* unit = nullptr;
		if (SpawnOne(game, room, s, &asSuper, &unit)) {
			if (&s == &plan.spawns.front()) {
				Event6bFirstSpawned(game, state.event->kind, unit);  // phase 6b: the Boss or Treasure monster is the first spawn; its death rolls the Drop
			}
			++made;
			supers += asSuper ? 1 : 0;
		} else {
			++failed;
		}
	}
	plog::Info("event '%s' (%s): spawned %d of %d in the event room %s (%d as a superunique, %d refused by the game)", state.event->name.c_str(),
	           ev::kindName(state.event->kind), made, plan.requested, Describe(rects.server).c_str(), supers, failed);
	return made > 0;
}

// The map's own Boss, in the boss room: one monster, the centre first and then a few spots around it (a superunique as itself, or as its Class when the game has spawned it this game).
// No minions and no Drop; those are the Boss event's. The map's stats reach it like any monster (flags 0, section 7).
// Made, or Refused: no spot in this room took it, and the caller tries another room (there is no give-up). A plan problem counts as a refusal too:
// the Boss's name is checked when the map opens (ArmMapBoss), so what is left is a room too small to place anything. The check record is told of a
// placed Boss; a refusal is the caller's to report.
// `force` is the last resort (the entrance room, every other room refused): the spot test's collision check is skipped (flag 0x80), as a plain monster,
// because the superunique spawn takes no flags.
ep::BossTry SpawnMapBoss(Game* game, void* room, State& state, const RoomRects& rects, bool force) noexcept {
	const ep::SpawnPlan plan = ep::planMapBoss(state.bossName, state.bossSpawn, rects.server, ep::spotScale(rects.server, rects.drlg));
	if (!plan.problem.empty()) {
		plog::Warn("map Boss '%s' not spawned in room %s: %s", state.bossName.c_str(), Describe(rects.drlg).c_str(), plan.problem.c_str());
		return ep::BossTry::Refused;
	}
	if (!force && state.bossTestRefuse > 0) {
		--state.bossTestRefuse;
		plog::Info("map Boss '%s': refused on purpose in room %s (CARTOGRAPHER_TEST_BOSS_REFUSE, a game check)", state.bossName.c_str(), Describe(rects.drlg).c_str());
		return ep::BossTry::Refused;
	}
	// The game's own free-spot search (p7 = 0xa) walks rings 3, 6, ... 30 subtiles out from the start, so a refusal means no free subtile
	// on any of those rings (about the whole room for a ~40 x 40 maze room; an outdoor clearing may be larger; section 9, "Plan rooms and
	// the map Boss spot"). The spots are a fail-open backstop: starts a few subtiles apart move the ring grid onto subtiles the first start skipped. The superunique is tried at every spot before the first plain spawn, so the Boss
	// keeps its own row (and minions) whenever any spot takes it.
	bool asSuper = false;
	void* unit = nullptr;
	bool made = false;
	const size_t total = plan.spawns.size();
	size_t tried = 0;
	const uint16_t flags = force ? SpawnFlagNoCollision : SpawnFlags;
	const auto attempt = [&](const ep::Spawn& spawn, const char* how) {
		++tried;
		const size_t inPass = (tried - 1) % total + 1;
		made = SpawnOne(game, room, spawn, &asSuper, &unit, flags);
		if (made) {
			if (tried > 1) {
				plog::Info("map Boss '%s': accepted at (%d,%d), attempt %zu of %zu (%s)", state.bossName.c_str(), spawn.spot.x, spawn.spot.y, inPass, total, how);
			}
		} else {
			plog::Info("map Boss '%s': the game refused the spot (%d,%d) (%s, %zu of %zu spots)", state.bossName.c_str(), spawn.spot.x, spawn.spot.y, how, inPass, total);
		}
		return made;
	};
	if (force) {
		// no superunique pass: its spawn takes no flags, and the ordinary tries in this room (and every other) have refused it already
	} else if (state.bossSpawn.kind == ev::NameKind::Unique && !On(SpawnSuperUnique)) {
		plog::Info("map Boss '%s': the superunique spawn is off in this build, so it is spawned as a plain monster", state.bossName.c_str());
	} else if (state.bossSpawn.kind == ev::NameKind::Unique) {
		for (const ep::Spawn& spawn : plan.spawns) {
			ep::Spawn superOnly = spawn;
			superOnly.classRow = -1;  // SpawnOne then makes no plain spawn
			if (attempt(superOnly, "as a superunique")) break;
		}
	}
	if (!made) {
		for (const ep::Spawn& spawn : plan.spawns) {
			ep::Spawn plain = spawn;
			plain.superUnique = false;
			plain.row = spawn.classRow;
			if (attempt(plain, force ? "as a plain monster, collision test skipped" : "as a plain monster")) break;
		}
	}
	const bool fellBack = made && !asSuper && state.bossSpawn.kind == ev::NameKind::Unique;  // the game gave no unit for the row: its base id was spawned instead
	plog::Info("map Boss '%s': %s in the room %s (%s)", state.bossName.c_str(), made ? "spawned" : "refused by the game", Describe(rects.server).c_str(),
	           asSuper ? "as a superunique" : (fellBack ? "as a plain monster of its base id: the superunique spawn returned null or was used already" : "as a plain monster"));
	if (!made) {
		return ep::BossTry::Refused;
	}
	AutoCheckNoteBoss(state.bossName, true, asSuper,
	                  fellBack ? std::string("spawned as a plain monster of its base id: the superunique spawn returned null or was used already") : std::string());
	return ep::BossTry::Made;
}

void OnPopulated(Game* game, void* room) noexcept {
	std::shared_ptr<State> state;
	uint64_t generation = 0;
	{
		std::lock_guard<std::mutex> lock(g_lock);
		state = g_state;
		generation = g_generation;
	}
	if (state == nullptr || state->level <= 0 || room == nullptr || !On(EvRoomLevelId)) {
		return;
	}
	if (Call<RoomLevelFn>(EvRoomLevelId)(room) != state->level) {
		return;
	}
	const RoomRects rects = ReadRects(room);
	if (LevelRowProbeOn()) {
		const std::string where = rects.ok ? Describe(rects.drlg) : std::string("unreadable");
		LevelRowProbeRoom(game, state->level, At<uint8_t>(game, GameDataFlagOffset), where.c_str());
	}
	bool fire = false;
	bool fireBoss = false;
	bool planned = false;
	int number = 0;
	{
		std::lock_guard<std::mutex> lock(g_lock);
		if (generation != g_generation || g_state != state) {
			return;  // the game was left, or another map opened, in between
		}
		number = ++state->probes;
		if (!rects.ok) {
			if (number <= MaxPopulateProbes) {
				plog::Info("eventprobe: room #%d of level %d populated, but its DRLG room is not readable; ignored", number, state->level);
			}
			return;
		}
		if (!state->tracker) {
			std::optional<ep::Rect> plan;
			const bool outdoor = !state->maze && state->level == OutdoorSlotLevelId();  // the Outdoor slot level: its plan's rooms (issue #145)
			if (outdoor) {
				const MazeEventRect m = OutdoorEventRoom();
				if (m.known && m.levelId == state->level) {
					plan = ep::Rect{m.x, m.y, m.w, m.h};
				}
			} else if (state->maze) {
				const MazeEventRect m = MazeEventRoom();
				if (m.known && m.levelId == state->level) {
					plan = ep::Rect{m.x, m.y, m.w, m.h};
				} else {
					plog::Warn("the maze plan's event room is not known; the event room is chosen from the level's room list (a third of the level's larger side and at most %d tiles from the first populated one, capped at the farthest room of the level)", ep::kMinEventDistance);
				}
			}
			int distance = ep::minEventDistance(rects.levelW, rects.levelH);
			// The map's own Boss goes in the plan's boss room (the maze's far cell), apart from the event room. When the plan has none, or every room of
			// its cell refuses, the other rooms of the level are tried, farthest from this entry room first (BossRoomPicker): there is no map without one.
			std::vector<ep::Rect> all;
			const bool haveAll = LevelRoomRects(rects.level, &all);
			if (state->bossArmed) {
				std::vector<ep::Rect> bossRanked;  // the plan's boss room first, then the rest of its cell (a maze cell has several engine rooms)
				if (outdoor || state->maze) {
					const MazeEventRect b = outdoor ? OutdoorBossRoom() : MazeBossRoom();
					if (b.known && b.levelId == state->level) {
						bossRanked.push_back(ep::Rect{b.x, b.y, b.w, b.h});
						for (const MazeRoomRect& o : b.more) {
							bossRanked.push_back(ep::Rect{o.x, o.y, o.w, o.h});
						}
					}
				}
				std::vector<ep::Rect> others = haveAll ? ep::roomsFarthestFirst(rects.drlg, all, bossRanked) : std::vector<ep::Rect>{};
				if (!bossRanked.empty()) {
					plog::Info("eventprobe: the map Boss '%s' goes in the plan's boss room %s (%zu room(s) in the cell and %zu other(s) in the level to fall back on)", state->bossName.c_str(),
					           Describe(bossRanked.front()).c_str(), bossRanked.size() - 1, others.size());
				} else {
					plog::Warn("the map Boss '%s' has no plan boss room I can read (a maze with BossRoom off, an outdoor slot build that did not run as planned, a Snow map too small for a boss cell, or a level not built from a plan): it goes in the room farthest from the entry (%zu room(s) known)",
					           state->bossName.c_str(), others.size());
					if (!others.empty()) {
						bossRanked.push_back(others.front());
						others.erase(others.begin());
					}
				}
				state->bossPicker.emplace(bossRanked, others, rects.drlg);
				state->bossRooms.assign(state->bossPicker->size(), nullptr);
			}
			if (!plan) {
				// No plan event room: this is the first populated room, the entry room. The distance is capped at the farthest room of the
				// level's list, so at least one room qualifies; the event fires on the first populated room at or past it.
				if (haveAll) {
					distance = ep::presetEventDistance(rects.levelW, rects.levelH, rects.drlg, all);
					plog::Info("eventprobe: level %d is %d x %d tiles and has %zu room(s); the event room is the first room at least %d tiles from the entry room %s", state->level, rects.levelW, rects.levelH, all.size(), distance, Describe(rects.drlg).c_str());
				} else {
					plog::Warn("eventprobe: the room list of level %d could not be read; level is %d x %d tiles, the event room is the first room at least %d tiles from the first one", state->level, rects.levelW, rects.levelH, distance);
				}
			}
			state->tracker.emplace(plan, distance);
		}
		planned = state->tracker->planned();
		fire = !state->spawned && state->tracker->onPopulated(rects.drlg);
		if (fire && state->event) {
			state->spawned = true;
		} else {
			fire = false;
		}
		// The map Boss's own room, separate from the event room. Fires once; the first populated room is its entry room too.
		if (state->bossPicker) {
			const int at = state->bossPicker->adopt(rects.drlg);  // a room the level's list did not hold becomes the last candidate
			state->bossRooms.resize(state->bossPicker->size(), nullptr);
			state->bossRooms[static_cast<size_t>(at)] = room;
			fireBoss = state->bossPicker->onPopulated(rects.drlg);
		}
	}
	if (number <= MaxPopulateProbes) {
		plog::Info("eventprobe: room #%d of level %d populated: DRLG %s tiles, server %s (%d server unit(s) per DRLG tile: %s), %s%s", number, state->level,
		           Describe(rects.drlg).c_str(), Describe(rects.server).c_str(), ep::spotScale(rects.server, rects.drlg), ep::scaleKindName(ep::scaleKind(rects.server, rects.drlg)),
		           planned ? "event room from the plan" : "event room by distance", fire ? ": THIS IS THE EVENT ROOM" : "");
	}
	if (fire && SpawnEvent(game, room, *state, rects, ep::eventSeed(state->seed))) {
		// The banner names the event only now that it has spawned. If the player is already in the map it follows at once,
		// else the entry tick shows it.
		std::vector<std::string> lines;
		{
			std::lock_guard<std::mutex> lock(g_lock);
			if (generation == g_generation && g_state == state) {
				state->announced = true;
				if (state->entry.inside()) {
					lines = ep::bannerLines(state->mapName, &*state->event, true, true);  // the game's own Entering text showed the map name on entry
				}
			}
		}
		ShowEntryMessage(lines);
	}
	if (fireBoss) {
		plog::Info("eventprobe: room #%d of level %d is tried for the map Boss (DRLG %s)", number, state->level, Describe(rects.drlg).c_str());
		// The picker decides (pure, tested with this loop as `driveBoss`): the plan's boss room, then the rest of its cell, then the other rooms
		// farthest first, then the entrance room with the collision test skipped. A refusal moves on at once, or waits for the next room to populate.
		const auto noteRoom = [&](const ep::Rect& r) { AutoCheckNoteBossRoom(r.x, r.y, r.w, r.h); };
		size_t first = 0;
		{
			std::lock_guard<std::mutex> lock(g_lock);
			first = state->bossPicker->current();
		}
		bool firstTry = true;
		const ep::BossDrive drive = ep::driveBoss(*state->bossPicker, first, [&](size_t index, ep::BossRoomPicker::Tier tier) -> ep::BossTry {
			void* bossRoom = room;
			RoomRects bossRects = rects;
			ep::Rect planRect{};
			{
				std::lock_guard<std::mutex> lock(g_lock);
				if (generation != g_generation || g_state != state) {
					return ep::BossTry::Stop;  // the game was left, or another map opened, in between
				}
				planRect = state->bossPicker->room(index);
				if (!firstTry) {
					bossRoom = state->bossRooms[index];
					bossRects = ReadRects(bossRoom);
				}
			}
			const char* where = tier == ep::BossRoomPicker::Tier::Cell ? (index == 0 ? "the plan's boss room" : "another room of the boss cell")
			                    : tier == ep::BossRoomPicker::Tier::Other ? "another room of the level, farthest from the entry first"
			                                                              : "the entry room with the collision test skipped (the last resort)";
			if (!firstTry) {
				plog::Warn("map Boss '%s': falling back to %s, DRLG %s", state->bossName.c_str(), where, Describe(planRect).c_str());
			}
			firstTry = false;
			if (!bossRects.ok) {
				plog::Warn("map Boss '%s': that room is not readable; counted as refused", state->bossName.c_str());
				return ep::BossTry::Refused;
			}
			noteRoom(bossRects.drlg);
			const ep::BossTry result = SpawnMapBoss(game, bossRoom, *state, bossRects, tier == ep::BossRoomPicker::Tier::Forced);
			if (result == ep::BossTry::Made) {
				plog::Info("map Boss '%s': boss path: %s", state->bossName.c_str(), where);
			}
			return result;
		});
		if (drive.waiting) {
			if (drive.waitRoom) {
				plog::Info("map Boss '%s': refused in this room; waiting for the next room of the boss cell %s to be populated", state->bossName.c_str(), Describe(*drive.waitRoom).c_str());
				noteRoom(*drive.waitRoom);  // the auto check goes there, which brings the room up
			} else {
				plog::Info("map Boss '%s': every populated room refused it; it goes in the next room to be populated", state->bossName.c_str());
			}
		}
	}
}

auto PopulateHook(Game* game, void* room) noexcept -> uint64_t {
	if (g_original == nullptr) {
		return 0;  // reachable only if the loader gave no original (the install logs it and marks the part off)
	}
	// Unique pace (issue #317): while this room is populated, MonUMax is the share of the unique packs due so far.
	bool paced = false;
	if (game != nullptr && room != nullptr && UniquePaceArmed() && On(EvRoomLevelId)) {
		try {
			paced = UniquePaceBegin(game, Call<RoomLevelFn>(EvRoomLevelId)(room));
		} catch (...) {
		}
	}
	const uint64_t result = g_original(game, room);
	if (paced) {
		UniquePaceEnd();
	}
	if (g_busy.load(std::memory_order_relaxed) && game != nullptr) {
		try {
			OnPopulated(game, room);
		} catch (...) {
			plog::Error("map events: handling a populated room failed");
		}
	}
	return result;
}

// A monster name (an event's Spawn, or a map's Boss) as a monstats row or a superuniques row: the game's loaded tables first (section 11), the mod's
// .txt files only when that route is off or cannot decide. Unresolved when neither has the name. Game thread, at map open.
auto ResolveName(const std::string& name, const ep::SpawnTable& txt, void* player) noexcept -> ep::Resolved {
	ep::Resolved out;
	if (player != nullptr && g_dataTables != nullptr) {
		const uint32_t dataFlag = At<uint8_t>(player, UnitDataFlagOffset);
		const ep::RouteVerdict route = RouteFor(dataFlag);
		if (route.monsters) {
			if (ResolveFromGameTables(route, dataFlag, name, txt, &out)) {
				return out;
			}
			out = ep::resolveSpawn(txt, name);
			// A superunique found in the txt: its Class is looked up in the game's proven monstats table, not by txt row count.
			if (out.kind == ev::NameKind::Unique) {
				const std::string_view cls = ep::superClassOf(txt, out.row);
				if (const int gameRow = cls.empty() ? -1 : GameMonsterRow(route, dataFlag, cls); gameRow >= 0) {
					out.classRow = gameRow;
				} else {
					plog::Warn("spawn '%s': its Class '%.*s' (mod's superuniques.txt) is not in the game's monstats table; treated as not found", name.c_str(), static_cast<int>(cls.size()), cls.data());
					out = ep::Resolved{};
				}
			}
			return out;
		}
	}
	return ep::resolveSpawn(txt, name);
}

// Arms the map's own Boss (the maps.txt `Boss` column) when it can be spawned; else says why not and leaves it off. The rest of the map is untouched.
void ArmMapBoss(State& state, const MapDef& map, const Tables& tables, void* player) noexcept {
	state.bossName = map.boss;
	if (!g_spawnOn.load()) {
		plog::Warn("map Boss '%s' will not be spawned: the spawn parts are off", map.boss.c_str());
		return;
	}
	// The Boss is a bosses.txt entry (checked at load). Its own superuniques row is tried first; without that row (the installer was not run) its
	// base monster is spawned plain. A vanilla superunique is never looked up here.
	const BossDef* entry = nullptr;
	for (const BossDef& def : tables.bosses) {
		if (def.name == map.boss) {
			entry = &def;
		}
	}
	if (entry == nullptr) {
		plog::Warn("map Boss '%s' will not be spawned: it is not an entry in bosses.txt", map.boss.c_str());
		return;
	}
	const std::string key = BossKey(entry->name);
	state.bossSpawn = ResolveName(key, tables.spawnNames, player);
	const bool rowFound = state.bossSpawn.kind == ev::NameKind::Unique;
	if (!rowFound) {
		state.bossSpawn = ResolveName(entry->base, tables.spawnNames, player);
	}
	const BossPath path = PickBossPath(rowFound, state.bossSpawn.kind == ev::NameKind::Monster);
	if (path == BossPath::None) {
		plog::Warn("map Boss '%s' will not be spawned: no superuniques row '%s' and its base monster '%s' is not in monstats", map.boss.c_str(), key.c_str(), entry->base.c_str());
		return;
	}
	if (path == BossPath::PlainMonster) {
		plog::Warn("map Boss '%s': no superuniques row '%s' (run tools\\install.ps1 and restart); spawning its base monster '%s' plain", map.boss.c_str(), key.c_str(), entry->base.c_str());
	}
	const ep::SpawnPlan check = ep::planMapBoss(map.boss, state.bossSpawn, ep::Rect{0, 0, 24, 24});  // a stand-in room
	if (!check.problem.empty()) {
		plog::Warn("map Boss '%s' will not be spawned: %s", map.boss.c_str(), check.problem.c_str());
		return;
	}
	state.bossArmed = true;
	char knob[16] = {};
	if (GetEnvironmentVariableA("CARTOGRAPHER_TEST_BOSS_REFUSE", knob, sizeof(knob)) > 0) {
		state.bossTestRefuse = std::atoi(knob) > 0 ? std::atoi(knob) : 0;
		plog::Warn("map Boss '%s': the first %d try(ies) are refused on purpose (CARTOGRAPHER_TEST_BOSS_REFUSE is set; a game check only)", map.boss.c_str(), state.bossTestRefuse);
	}
	plog::Info("map Boss for '%s': '%s', %s", map.name.c_str(), map.boss.c_str(), path == BossPath::Superunique ? "its own superuniques row" : "a plain monster of its base id");
}

auto SummaryNow() -> std::string {
	const std::string spawn = g_spawnOn.load() ? "map Boss, Boss and Horde events on" : "map Boss, Boss and Horde events off";
	const std::string banner = !g_tickOn.load() ? "entry message off (a part was not found)" : EntryBannerAvailable() ? "entry message on the banner panel" : "entry message on the console";
	return spawn + ", " + banner + " (not yet verified in game)";
}

}  // namespace

auto GameMonsterRows(uint32_t dataFlag, const std::vector<std::string>& ids, std::vector<int>* rows, std::vector<int>* weights, std::string* why) noexcept -> bool {
	try {
		rows->clear();
		weights->clear();
		if (g_dataTables == nullptr) {
			*why = "no DataTable service";
			return false;
		}
		const ep::RouteVerdict route = RouteFor(dataFlag);
		if (!route.monsters) {
			*why = route.why.empty() ? "the game's monstats table did not pass its check" : route.why;
			return false;
		}
		std::string tablesWhy;
		const uint8_t* dt = TablesBase(dataFlag, &tablesWhy);
		ep::TableFacts mon;
		bool busy = false;
		if (dt == nullptr || !TableFactsOf(static_cast<D2RL::DataTables::Bank>(route.bank), D2RL::DataTables::TableId::MonStats, dt, DtMonRowsOffset, DtMonCountOffset, &mon, &busy) ||
		    !ep::checkTable(mon, ep::kMonStatsRowSize).empty()) {
			*why = "the game's monstats table could not be read now";
			return false;
		}
		for (const std::string& id : ids) {
			const int row = GameMonsterRow(route, dataFlag, id);
			if (row < 0 || static_cast<uint32_t>(row) >= mon.viewCount) {
				*why = "'" + id + "' is not in the game's monstats table";
				rows->clear();
				weights->clear();
				return false;
			}
			// The game's group builder (0x509940) drops a monster whose monstats +0x3c lacks bit 0 (the mask at 0x1d996d0 is 1).
			if ((At<uint8_t>(mon.viewRows, row * static_cast<int>(mon.viewRowSize) + MonStatsSpawnFlagOffset) & 1) == 0) {
				plog::Warn("map monsters: '%s' (monstats row %d) is left out: the game does not put it in a level's monster groups", id.c_str(), row);
				continue;
			}
			rows->push_back(row);
			weights->push_back(At<uint8_t>(mon.viewRows, row * static_cast<int>(mon.viewRowSize) + MonStatsWeightOffset));
		}
		if (rows->empty() && !ids.empty()) {
			*why = "the game keeps none of the listed monsters in a level's monster groups";
			return false;
		}
		return true;
	} catch (...) {
		rows->clear();
		weights->clear();
		*why = "the lookup failed";
		return false;
	}
}

auto GameLevelCount(uint32_t dataFlag, int64_t* count, std::string* why) noexcept -> bool {
	try {
		const uint8_t* dt = TablesBase(dataFlag, why);
		if (dt == nullptr) {
			return false;
		}
		if (!Readable(dt + DtLevelCountOffset, sizeof(int64_t))) {
			*why = "the levels count cannot be read";
			return false;
		}
		*count = At<int64_t>(dt, DtLevelCountOffset);
		return true;
	} catch (...) {
		*why = "the levels count lookup failed";
		return false;
	}
}

auto InstallEventApply(const D2RL::PluginContext* context, const std::vector<SignatureOverride>& overrides, std::vector<PartStatus>* parts) -> bool {
	g_context = context;
	CodeRange ranges[8];
	const int count = CodeRanges(context->exeBase, ranges, 8);
	for (Part& part : g_parts) {
		ResolvePart(context, overrides, ranges, count, part);
	}
	// The populator hook goes in at load, only when everything it needs was found. Never installed half-on.
	if (On(PopulateRoom) && On(SpawnMonster) && On(EvRoomLevelId)) {
		Part& part = g_parts[PopulateRoom];
		const uint64_t rva = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(part.address) - context->exeBase);
		const bool hooked = context->InstallInlineHook(rva, part.guard, part.hookSize, reinterpret_cast<void*>(&PopulateHook), reinterpret_cast<void**>(&g_original));
		const bool ok = HookOutcome(part, hooked, g_original != nullptr);
		g_spawnOn = ok;
	} else if (On(PopulateRoom)) {
		g_parts[PopulateRoom].status.on = false;
		g_parts[PopulateRoom].status.detail = "off: not hooked, because a part it needs was not found";
	}
	InstallEvent6b(context, overrides, g_parts[DataTablesBase], g_parts[NameToRow], g_spawnOn.load());  // phase 6b: the Shrine object and the Boss Drop (its own parts and hook)
	// The entry message follows the portal's tick part and needs the room parts.
	bool tickOn = false;
	for (const PartStatus& s : PartStatuses()) {
		tickOn = tickOn || (s.name == "PlayerTick" && s.on);
	}
	g_tickOn = tickOn && On(EvUnitRoom) && On(EvRoomLevelId);
	if (!g_tickOn) {
		plog::Warn("the entry message is off: %s", !tickOn ? "the PlayerTick part is off" : "a room part was not found");
	}
	if (!g_spawnOn) {
		plog::Warn("Boss and Horde events are off (see the parts above)");
	}
	// Spawn names from the game's tables: the SDK table service. Best effort; without it the .txt files are used.
	if (context->QueryService(&g_dataTables) != D2RL::ServiceQueryResult::Success || !D2RL::HasDataTableServiceField(g_dataTables, D2RL::DataTableServiceRequiredSize)) {
		g_dataTables = nullptr;
		plog::Warn("spawn names: no DataTable service; Boss and Horde names come from the mod's monstats.txt and superuniques.txt");
	}
	plog::Info("map events: %s", SummaryNow().c_str());
	if (parts != nullptr) {
		*parts = EventPartStatuses();
	}
	for (const Part& part : g_parts) {
		plog::Info("part %s: %s", part.name, part.status.detail.c_str());
	}
	return g_spawnOn.load();
}

void EventsMapOpened(const MapDef& map, const std::shared_ptr<const Tables>& tables, uint32_t playerId, uint32_t openings, int level, bool maze, void* player) {
	Event6bMapOpened();  // phase 6b: the previous map's boss Drop and Shrine end here
	if (tables == nullptr || (!g_tickOn.load() && !g_spawnOn.load())) {
		return;
	}
	LevelRowProbeSet(tables->settings.mazeProbe != 0);
	auto state = std::make_shared<State>();
	state->mapName = map.name;
	state->level = level;
	state->maze = maze;
	state->seed = cartographer::statplan::mixSeed(GetTickCount64(), playerId, openings);
	const int chance = ev::effectiveEventChance(map.eventChance >= 0 ? std::optional<int>(map.eventChance) : std::nullopt, tables->settings.eventChance);
	const cartographer::layout::Point unused{0, 0};
	std::optional<ev::Event> event = ev::decide(tables->events, chance, ep::eventSeed(state->seed), &unused);
	if (!event) {
		plog::Info("event for '%s' (seed %llu, chance %d%%): none", map.name.c_str(), static_cast<unsigned long long>(state->seed), chance);
	} else {
		plog::Info("event for '%s' (seed %llu, chance %d%%): %s", map.name.c_str(), static_cast<unsigned long long>(state->seed), chance, ev::describe(*event).c_str());
		// The game's loaded tables first (section 11); the mod's .txt files only when that route is off or cannot decide.
		// A Shrine's Spawn is an objects row (looked up in Event6bPrepareShrine), never a monster name: skip the monster lookup for it.
		if (event->kind != ev::Kind::Shrine) {  // a Treasure's Spawn is looked up too: a miss means a chest's objects name
			state->spawn = ResolveName(event->spawn, tables->spawnNames, player);
		}
		// Checked now with a stand-in room, so a event that cannot be spawned is neither announced nor waited for.
		const bool placesObject = Event6bPlacesObject(*event, state->spawn.kind);
		ep::SpawnPlan check;
		if (!placesObject) {
			check = ep::planEvent(*event, state->spawn, ep::Rect{0, 0, 24, 24}, state->seed);
		}
		std::string shrineWhy;
		if (placesObject) {
			// Phase 6b: a Shrine's Spawn (or a Treasure chest's) is an objects row, not a monster; it is announced only when it resolves to exactly one row.
			if (!g_spawnOn.load()) {
				plog::Warn("event '%s' is decided but will not be spawned or announced: the spawn parts are off", event->name.c_str());
			} else if (!(event->kind == ev::Kind::Shrine ? Event6bPrepareShrine(*event, tables->objectClasses, tables->objectNames, player, &shrineWhy) : Event6bPrepareChest(*event, tables->objectClasses, tables->objectNames, level, player, &shrineWhy))) {
				plog::Warn("event '%s' is decided but will not be spawned or announced: %s", event->name.c_str(), shrineWhy.c_str());
			} else {
				state->event = std::move(event);
			}
		} else if (!check.problem.empty()) {
			plog::Warn("event '%s' is decided but will not be spawned or announced: %s", event->name.c_str(), check.problem.c_str());
		} else if (!g_spawnOn.load()) {
			plog::Warn("event '%s' is decided but will not be spawned or announced: the spawn parts are off", event->name.c_str());
		} else if (event->kind == ev::Kind::Ambush && !g_tickOn.load()) {
			plog::Warn("event '%s' is refused: an Ambush starts its waves from the player tick, and the PlayerTick or room parts are off", event->name.c_str());
		} else {
			Event6bArmBossDrop(*event, level, player);  // phase 6b: does nothing without a Boss or Treasure monster Drop
			state->event = std::move(event);
		}
	}
	// The map's own Boss, apart from the event (it is armed whether or not an event was drawn).
	if (!map.boss.empty()) {
		ArmMapBoss(*state, map, *tables, player);
	}
	std::lock_guard<std::mutex> lock(g_lock);
	g_state = std::move(state);
	g_busy = true;
}

void EventsPlayerTick(void* rawGame, void* rawUnit, uint32_t localPlayer) noexcept {
	auto* unit = static_cast<Unit*>(rawUnit);
	if (!g_busy.load(std::memory_order_relaxed) || !g_tickOn.load() || unit == nullptr || At<int32_t>(unit, UnitTypeOffset) != 0 || At<uint32_t>(unit, UnitIdOffset) != localPlayer) {
		return;
	}
	try {
		std::shared_ptr<State> state;
		{
			std::lock_guard<std::mutex> lock(g_lock);
			state = g_state;
		}
		if (state == nullptr || state->level <= 0) {
			return;
		}
		void* room = Call<UnitRoomFn>(EvUnitRoom)(unit);
		// Phase 6b: starts the next Ambush wave when the player is in the event room. True once, when the first wave spawned: the Ambush counts as placed then.
		// The room must also be on the event's level: the level id is checked as well as the server rect.
		const bool inMap = room != nullptr && Call<RoomLevelFn>(EvRoomLevelId)(room) == state->level;
		const bool ambushStarted = Event6bAmbushTick(rawGame, inMap ? room : nullptr, &ReadServerRect, &SpawnAmbushUnit);
		std::vector<std::string> lines;
		{
			std::lock_guard<std::mutex> lock(g_lock);
			if (g_state != state) {
				return;
			}
			const bool late = ambushStarted && state->event;
			if (late) {
				state->announced = true;
			}
			const bool wasInside = state->entry.inside();
			const bool entered = state->entry.update(inMap);
			if (entered) {
				lines = ep::bannerLines(state->mapName, state->event ? &*state->event : nullptr, state->announced, false);
			} else if (late && wasInside) {
				lines = ep::bannerLines(state->mapName, &*state->event, true, true);  // late banner: the game's own Entering text showed the map name on entry
			} else {
				return;
			}
		}
		ShowEntryMessage(lines);
	} catch (...) {
		plog::Error("map events: the entry message failed");
	}
}

void EventsGameLeft() noexcept {
	Event6bGameLeft();
	{
		std::lock_guard<std::mutex> lock(g_lock);
		++g_generation;
		g_state.reset();
		g_busy = false;
	}
}

auto EventPartStatuses() -> std::vector<PartStatus> {
	std::vector<PartStatus> out;
	for (const Part& part : g_parts) {
		out.push_back(part.status);
	}
	for (PartStatus& status : Event6bPartStatuses()) {
		out.push_back(std::move(status));
	}
	return out;
}

auto EventsSummary() -> std::string {
	return "map events: " + SummaryNow() + "; " + Event6bSummary();
}

}  // namespace carto
