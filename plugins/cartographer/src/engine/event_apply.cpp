#include "engine/event_apply.h"

#include "core/part_signatures.h"
#include "engine/event_6b.h"
#include "engine/maze_hooks.h"
#include "engine/scan.h"
#include "log.h"

#include <windows.h>

#include <algorithm>
#include <atomic>
#include <cstdio>
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

// --- Engine facts. Build 93847; each is recorded with its evidence in docs/reverse-engineering/cartographer.md section 9.
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
constexpr int MaxPopulateProbes = 12;       // rooms of the map level logged in full
// Spawn names from the game's tables (section 11). `dt` is the data tables base for the unit's data flag.
constexpr int UnitDataFlagOffset = 0x1bd;   // uint8 on a unit (the same byte stat_apply.cpp reads)
constexpr uint32_t DataFlagLimit = 4;       // the base accessor takes a data flag below 4 (anything else is a trap in the engine)
constexpr int DtMonRowsOffset = 0xf58;      // monstats row vector pointer; the 64-bit count is at +0xf60
constexpr int DtMonCountOffset = 0xf60;
constexpr int DtMonMapOffset = 0xf70;       // monstats Id -> row map pointer (re-read on every use: a table load makes a new map)
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
std::atomic<bool> g_bannerOverlay{false};

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
	bool farthestRoom = false;  // the tracker's room is the farthest one of the level's list (a preset map), not a maze plan's
	bool spawned = false;
	bool announced = false;  // the event room was chosen and something spawned: the banner may name the event
	int probes = 0;
};
std::mutex g_lock;
std::shared_ptr<State> g_state;
uint64_t g_generation = 0;

// --- Banner ---------------------------------------------------------------------------------------------------------------

struct Banner {
	std::vector<std::string> lines;
	uint64_t startMs = 0;
	bool active = false;
};
std::mutex g_bannerLock;
Banner g_banner;
const D2RL::OverlayService* g_overlay = nullptr;
D2RL::Overlay::CallbackHandle g_callback = D2RL::Overlay::InvalidCallbackHandle;
std::atomic<bool> g_drawFailedLogged{false};

void __cdecl OnFrame(const D2RL::PluginContext* context, const D2RL::Overlay::Frame* frame, void*) noexcept {
	if (g_overlay == nullptr || frame == nullptr) {
		return;
	}
	std::vector<std::string> lines;
	int alpha = 0;
	{
		std::lock_guard<std::mutex> lock(g_bannerLock);
		if (!g_banner.active) {
			return;
		}
		alpha = ep::bannerAlpha(static_cast<int>(GetTickCount64() - g_banner.startMs));
		if (alpha == 0 && GetTickCount64() - g_banner.startMs >= static_cast<uint64_t>(ep::kBannerTotalMs)) {
			g_banner.active = false;
			return;
		}
		lines = g_banner.lines;
	}
	if (alpha <= 0 || lines.empty()) {
		return;
	}
	const float a = static_cast<float>(alpha) / 255.0f;
	float y = frame->screenHeight * 0.16f;
	for (size_t i = 0; i < lines.size(); ++i) {
		const float size = frame->defaultTextSize * (i == 0 ? 1.7f : 1.25f);
		D2RL::Overlay::MeasureTextRequest measure{};
		measure.structSize = D2RL::Overlay::MeasureTextRequestSize;
		measure.canvas = frame->canvas;
		measure.textSize = size;
		measure.text = lines[i].c_str();
		measure.textLength = static_cast<uint32_t>(lines[i].size());
		D2RL::Overlay::TextMetrics metrics{};
		metrics.structSize = D2RL::Overlay::TextMetricsSize;
		if (g_overlay->measureText(context, &measure, &metrics) != D2RL::Overlay::Result::Success) {
			metrics.width = 0;
			metrics.height = size;
		}
		const D2RL::Overlay::Color colours[3] = {{0.86f, 0.76f, 0.46f, a}, {0.92f, 0.32f, 0.26f, a}, {0.85f, 0.85f, 0.85f, a}};
		const D2RL::Overlay::Color shadow{0.0f, 0.0f, 0.0f, a * 0.85f};
		const float x = (frame->screenWidth - metrics.width) * 0.5f;
		for (int pass = 0; pass < 2; ++pass) {
			D2RL::Overlay::TextRequest request{};
			request.structSize = D2RL::Overlay::TextRequestSize;
			request.canvas = frame->canvas;
			request.position = {x + (pass == 0 ? 2.0f : 0.0f), y + (pass == 0 ? 2.0f : 0.0f)};
			request.color = pass == 0 ? shadow : colours[std::min<size_t>(i, 2)];
			request.textSize = size;
			request.text = lines[i].c_str();
			request.textLength = static_cast<uint32_t>(lines[i].size());
			if (g_overlay->drawText(context, &request) != D2RL::Overlay::Result::Success && !g_drawFailedLogged.exchange(true)) {
				plog::Warn("the entry banner could not draw its text through the overlay; the console shows the message instead from now on");
				g_bannerOverlay = false;
			}
		}
		y += (metrics.height > 0 ? metrics.height : size) * 1.2f;
	}
}

void ShowEntryMessage(const std::vector<std::string>& lines) noexcept {
	if (lines.empty()) {
		return;
	}
	if (g_bannerOverlay.load()) {
		std::lock_guard<std::mutex> lock(g_bannerLock);
		g_banner.lines = lines;
		g_banner.startMs = GetTickCount64();
		g_banner.active = true;
		plog::Info("eventprobe: entry message shown on the overlay (%zu line(s))", lines.size());
		return;
	}
	for (const std::string& line : lines) {
		if (g_context != nullptr) {
			g_context->WriteConsoleMessage(line.c_str());
		}
	}
	plog::Info("eventprobe: entry message written to the console (%zu line(s)); no overlay", lines.size());
}

// --- Spawn names from the game's loaded tables (cartographer.md section 11) -----------------------------------------------
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
auto SpawnOne(Game* game, void* room, const ep::Spawn& s, bool* asSuper, void** unitOut) noexcept -> bool {
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
	void* unit = Call<SpawnMonsterFn>(SpawnMonster)(game, room, x, y, row, SpawnMode, SpawnParam, SpawnFlags);
	*unitOut = unit;
	return unit != nullptr;
}

// True when something was placed, so the banner may announce the event.
auto SpawnEvent(Game* game, void* room, const State& state, const RoomRects& rects, uint64_t seed) noexcept -> bool {
	if (state.event->kind == ev::Kind::Shrine) {
		return Event6bPlaceShrine(game, room, rects.server, ep::spotScale(rects.server, rects.drlg), seed);  // phase 6b
	}
	// Spots come from the server room's rect (its units are the spawn code's), spaced by tiles through the rect-to-tile scale.
	const ep::SpawnPlan plan = ep::planEvent(*state.event, state.spawn, rects.server, seed, ep::spotScale(rects.server, rects.drlg));
	if (!plan.problem.empty()) {
		plog::Warn("event '%s' not spawned: %s", state.event->name.c_str(), plan.problem.c_str());
		return false;
	}
	int made = 0, failed = 0, supers = 0;
	for (const ep::Spawn& s : plan.spawns) {
		bool asSuper = false;
		void* unit = nullptr;
		if (SpawnOne(game, room, s, &asSuper, &unit)) {
			if (state.event->kind == ev::Kind::Boss && &s == &plan.spawns.front()) {
				Event6bBossSpawned(unit);  // phase 6b: the boss is the first spawn; its death rolls the Drop
			}
			++made;
			supers += asSuper ? 1 : 0;
		} else {
			++failed;
		}
	}
	plog::Info("event '%s' (%s): spawned %d of %zu in the event room %s (%d as a superunique, %d refused by the game)", state.event->name.c_str(),
	           ev::kindName(state.event->kind), made, plan.spawns.size(), Describe(rects.server).c_str(), supers, failed);
	return made > 0;
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
	bool fire = false;
	bool planned = false;
	bool farthest = false;
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
			if (state->maze) {
				const MazeEventRect m = MazeEventRoom();
				if (m.known && m.levelId == state->level) {
					plan = ep::Rect{m.x, m.y, m.w, m.h};
				} else {
					plog::Warn("the maze plan's event room is not known; the event room is chosen like a preset map's (the farthest room of the level from the first populated one; by distance, a third of the level's larger side and at most %d tiles, if the room list cannot be read)", ep::kMinEventDistance);
				}
			}
			const int distance = ep::minEventDistance(rects.levelW, rects.levelH);
			if (!plan) {
				// A preset map: this is the first populated room, the entry room. The farthest room of the level's list is the event room.
				std::vector<ep::Rect> all;
				if (LevelRoomRects(rects.level, &all)) {
					plan = ep::farthestRoom(rects.drlg, all);
					if (plan) {
						farthest = true;
						plog::Info("eventprobe: level %d has %zu room(s); the event room is the one farthest from the entry room %s: %s", state->level, all.size(), Describe(rects.drlg).c_str(), Describe(*plan).c_str());
					} else {
						plog::Warn("eventprobe: level %d has no room besides the entry room in its list; the event room is chosen by distance instead", state->level);
					}
				} else {
					plog::Warn("eventprobe: the room list of level %d could not be read; the event room is chosen by distance instead", state->level);
				}
				if (!plan) {
					plog::Info("eventprobe: level %d is %d x %d tiles; the event room is the first room at least %d tiles from the first one", state->level, rects.levelW, rects.levelH, distance);
				}
			}
			state->tracker.emplace(plan, distance);
			state->farthestRoom = farthest;
		}
		planned = state->tracker->planned();
		fire = !state->spawned && state->tracker->onPopulated(rects.drlg);
		if (fire && state->event) {
			state->spawned = true;
		} else {
			fire = false;
		}
	}
	if (number <= MaxPopulateProbes) {
		plog::Info("eventprobe: room #%d of level %d populated: DRLG %s tiles, server %s (%d server unit(s) per DRLG tile: %s), %s%s", number, state->level,
		           Describe(rects.drlg).c_str(), Describe(rects.server).c_str(), ep::spotScale(rects.server, rects.drlg), ep::scaleKindName(ep::scaleKind(rects.server, rects.drlg)),
		           planned ? (state->farthestRoom ? "event room is the farthest room" : "event room from the plan") : "event room by distance", fire ? ": THIS IS THE EVENT ROOM" : "");
	}
	if (fire && SpawnEvent(game, room, *state, rects, ep::eventSeed(state->seed))) {
		// The banner names the event only now that it has spawned. If the player is already in the map it follows at once,
		// else the entry tick shows it with the map's name.
		std::vector<std::string> lines;
		{
			std::lock_guard<std::mutex> lock(g_lock);
			if (generation == g_generation && g_state == state) {
				state->announced = true;
				if (state->entry.inside()) {
					lines = ep::bannerLines(state->mapName, &*state->event, true, true);  // the map name was shown on entry
				}
			}
		}
		ShowEntryMessage(lines);
	}
}

auto PopulateHook(Game* game, void* room) noexcept -> uint64_t {
	if (g_original == nullptr) {
		return 0;  // reachable only if the loader gave no original (the install logs it and marks the part off)
	}
	const uint64_t result = g_original(game, room);
	if (g_busy.load(std::memory_order_relaxed) && game != nullptr) {
		try {
			OnPopulated(game, room);
		} catch (...) {
			plog::Error("map events: handling a populated room failed");
		}
	}
	return result;
}

auto SummaryNow() -> std::string {
	const std::string spawn = g_spawnOn.load() ? "Boss and Horde events on" : "Boss and Horde events off";
	const std::string banner = !g_tickOn.load() ? "entry message off (a part was not found)" : g_bannerOverlay.load() ? "entry message on the overlay" : "entry message on the console";
	return spawn + ", " + banner + " (not yet verified in game)";
}

}  // namespace

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
		const bool ok = hooked && g_original != nullptr;
		part.status.on = ok;
		part.status.detail = ok ? "found by signature, hooked" : !hooked ? "off: D2RLoader refused the hook" : "off: hooked, but D2RLoader gave no way back to the game's own function, so every room's own population is skipped until the game is restarted";
		if (hooked && !ok) {
			plog::Error("part %s: D2RLoader installed the hook but gave no original; the game's own room population is skipped for EVERY room until the game is restarted; restart it after removing what conflicts", part.name);
		}
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
	// The overlay, best effort; without it the entry message goes to the console.
	if (context->QueryService(&g_overlay) == D2RL::ServiceQueryResult::Success && D2RL::HasOverlayServiceField(g_overlay, D2RL::OverlayServiceRequiredSize)) {
		D2RL::Overlay::CallbackRegistration registration{};
		registration.structSize = D2RL::Overlay::CallbackRegistrationSize;
		registration.phase = D2RL::Overlay::Phase::AfterGameUi;
		registration.callback = OnFrame;
		const auto result = g_overlay->registerFrameCallback(context, &registration, &g_callback);
		g_bannerOverlay = result == D2RL::Overlay::Result::Success;
		if (!g_bannerOverlay) {
			plog::Warn("the overlay refused the banner callback (result %u); the entry message goes to the console", static_cast<uint32_t>(result));
		}
	} else {
		g_overlay = nullptr;
		plog::Warn("no overlay service; the entry message goes to the console");
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
		const bool monsterSpawn = event->kind != ev::Kind::Shrine;
		bool decided = !monsterSpawn;
		if (monsterSpawn && player != nullptr && g_dataTables != nullptr) {
			const uint32_t dataFlag = At<uint8_t>(player, UnitDataFlagOffset);
			const ep::RouteVerdict route = RouteFor(dataFlag);
			if (route.monsters) {
				decided = ResolveFromGameTables(route, dataFlag, event->spawn, tables->spawnNames, &state->spawn);
				if (!decided) {
					state->spawn = ep::resolveSpawn(tables->spawnNames, event->spawn);
					decided = true;
					// A superunique found in the txt: its Class is looked up in the game's proven monstats table, not by txt row count.
					if (state->spawn.kind == ev::NameKind::Unique) {
						const std::string_view cls = ep::superClassOf(tables->spawnNames, state->spawn.row);
						if (const int gameRow = cls.empty() ? -1 : GameMonsterRow(route, dataFlag, cls); gameRow >= 0) {
							state->spawn.classRow = gameRow;
						} else {
							plog::Warn("spawn '%s': its Class '%.*s' (mod's superuniques.txt) is not in the game's monstats table; treated as not found", event->spawn.c_str(), static_cast<int>(cls.size()), cls.data());
							state->spawn = ep::Resolved{};
						}
					}
				}
			}
		}
		if (!decided) {
			state->spawn = ep::resolveSpawn(tables->spawnNames, event->spawn);
		}
		// Checked now with a stand-in room, so a event that cannot be spawned is neither announced nor waited for.
		ep::SpawnPlan check;
		if (monsterSpawn) {
			check = ep::planEvent(*event, state->spawn, ep::Rect{0, 0, 24, 24}, state->seed);
		}
		std::string shrineWhy;
		if (event->kind == ev::Kind::Shrine) {
			// Phase 6b: a Shrine's Spawn is an objects row, not a monster; it is announced only when it resolves to exactly one row.
			if (!g_spawnOn.load()) {
				plog::Warn("event '%s' is decided but will not be spawned or announced: the spawn parts are off", event->name.c_str());
			} else if (!Event6bPrepareShrine(*event, tables->objectNames, player, &shrineWhy)) {
				plog::Warn("event '%s' is decided but will not be spawned or announced: %s", event->name.c_str(), shrineWhy.c_str());
			} else {
				state->event = std::move(event);
			}
		} else if (!check.problem.empty()) {
			plog::Warn("event '%s' is decided but will not be spawned or announced: %s", event->name.c_str(), check.problem.c_str());
		} else if (!g_spawnOn.load()) {
			plog::Warn("event '%s' is decided but will not be spawned or announced: the spawn parts are off", event->name.c_str());
		} else {
			Event6bArmBossDrop(*event, level, player);  // phase 6b: does nothing without a Boss Drop
			state->event = std::move(event);
		}
	}
	std::lock_guard<std::mutex> lock(g_lock);
	g_state = std::move(state);
	g_busy = true;
}

void EventsPlayerTick(void* rawGame, void* rawUnit, uint32_t localPlayer) noexcept {
	(void)rawGame;
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
		const bool inMap = room != nullptr && Call<RoomLevelFn>(EvRoomLevelId)(room) == state->level;
		std::vector<std::string> lines;
		{
			std::lock_guard<std::mutex> lock(g_lock);
			if (g_state != state || !state->entry.update(inMap)) {
				return;
			}
			lines = ep::bannerLines(state->mapName, state->event ? &*state->event : nullptr, state->announced, false);
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
	std::lock_guard<std::mutex> lock(g_bannerLock);
	g_banner = {};
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
