#include <algorithm>
#include "outdoor_hooks.h"

#include "core/part_signatures.h"
#include "core/text.h"
#include "automap_reset.h"
#include "log.h"
#include "maze_hooks.h"
#include "steer.h"

#include "outdoor_plan.h"
#include "outdoor_tilesets.h"
#include "outdoor_seed.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <atomic>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

namespace carto {

namespace {

namespace co = cartographer::outdoor;

// --- Engine facts. Offsets are into D2R's own structures (build 93847); each is recorded with its evidence in
// docs/reverse-engineering/cartographer.md (index): sections 1, 4, 8 and 8a. The functions are found by signature, never by address.
constexpr int LvlType = 0x00;         // int32: 1 maze, 2 preset, 3 outdoor
constexpr int LvlRoomCount = 0x08;    // int32
constexpr int LvlFirstRoom = 0x10;    // room*: the newest room, next at RoomNext
constexpr int LvlGrid = 0x18;         // the outdoor block: four grids and the cell counts (0x340 bytes, allocated with the level)
constexpr int LvlOriginX = 0x24;      // int32, tiles: still 0 when the level build starts for an outdoor slot (section 4, S1)
constexpr int LvlOriginY = 0x28;
constexpr int LvlSizeX = 0x2c;        // int32, tiles
constexpr int LvlSizeY = 0x30;
constexpr int LvlDrlg = 0x1c8;        // drlg*
constexpr int LvlTileType = 0x1e0;     // int32: the LevelType
constexpr int LvlSeed = 0x1e4;        // int32 pair: the level RNG state (the reseed writes both)
constexpr int LvlSeed2 = 0x1e8;
constexpr int LvlId = 0x1f8;          // int32
constexpr int DrlgSeedBase = 0x860;   // uint32
constexpr int GridCellsW = 0xb0;      // int32 in the outdoor block: the size in grid cells
constexpr int GridCellsH = 0xb4;
constexpr int GridPieces = 0x08;      // grid object: the piece id at a piece's origin cell
constexpr int GridFlags = 0x58;       // grid object: 0x200 covered, 0x100 void, file index in bits 16..19
constexpr int GridData = 0x18;        // in a grid object: uint64* cells
constexpr int GridRows = 0x20;        // in a grid object: int32* index of each row's first cell (a cell is data[rows[y] + x])
constexpr int ServerRoomDrlg = 0x18;  // a server room's DRLG room (UnitRoom's result; section 5, "Arrival room")
constexpr int RoomFlags = 0x50;       // uint32; bits 16 and 17 (co::kArrivalFlags) make the level-entry finder pick the room
constexpr int RoomPiece = 0x40, RoomNext = 0x48, RoomX = 0x60, RoomY = 0x64, RoomW = 0x68, RoomH = 0x6c;
constexpr int PieceId = 0x00, PieceFile = 0x04;
constexpr int32_t OutdoorType = 3;
constexpr int32_t NativeTailType = 0;  // no branch of the level build's dispatch matches it, so only its native tail runs
constexpr int32_t SecondSeedWord = 0x29a;
constexpr int MaxRooms = 4096;
constexpr int MaxGridCells = 64 * 64;

using EntryFn = void (*)(uint8_t dataFlag, void* level) noexcept;  // 0x3ee320, and the Act 5 builder it calls (same shape)
using StampFn = void (*)(uint8_t dataFlag, void* level, int32_t cx, int32_t cy, int32_t lvlPrest, int32_t file, char last) noexcept;  // 0x3ecfc0
using VoidFn = void (*)(void* level, int32_t x, int32_t y) noexcept;                                                             // 0x3ede00

// The three arrival parts are only checked (never called or hooked), as in the maze steering; a missing one leaves the arrival to the game and
// does not turn the outdoor build off.
enum OutdoorPartId { Act5Builder, EntryPart, StampPart, VoidPart, ArrivalFinderPart, ArrivalRoomFinderPart, ArrivalFlagTestPart, ArrivalSpotPart, LevelLookupPart, RoomDataLoadPart, FreeSpotSearchPart, OwnLevelBuild, OutdoorPartCount };
constexpr int OutdoorNeededParts = ArrivalFinderPart;  // the parts every build needs; LevelBuild is added only when the maze hook did not take it

Part g_parts[OutdoorPartCount] = {
	{"OutdoorAct5Builder", sig::OutdoorAct5Builder, 16, true, nullptr, {}, {}},
	{"OutdoorEntry", sig::OutdoorEntry, 0, true, nullptr, {}, {}},
	{"PieceStamp", sig::PieceStamp, 0, true, nullptr, {}, {}},
	{"VoidCell", sig::VoidCell, 0, true, nullptr, {}, {}},
	{"ArrivalFinder", sig::ArrivalFinder, 0, false, nullptr, {}, {}},
	{"ArrivalRoomFinder", sig::ArrivalRoomFinder, 0, false, nullptr, {}, {}},
	{"ArrivalFlagTest", sig::ArrivalFlagTest, 0, false, nullptr, {}, {}},
	{"ArrivalSpot", sig::ArrivalSpot, 15, false, nullptr, {}, {}},
	{"LevelLookup", sig::LevelLookup, 0, false, nullptr, {}, {}},
	{"RoomDataLoad", sig::RoomDataLoad, 0, false, nullptr, {}, {}},
	{"FreeSpotSearch", sig::FreeSpotSearch, 0, false, nullptr, {}, {}},
	{"LevelBuild", sig::LevelBuild, 14, true, nullptr, {}, {}},
};

// 0x2ef8b0: puts a unit on a level after a portal (cl, game, level id, exit index, &x, &y, size); only its first and last arguments matter here.
using ArrivalSpotFn = uint64_t (*)(uint64_t cl, void* game, uint64_t levelId, uint64_t exit, int32_t* outX, int32_t* outY, uint32_t size) noexcept;
using LevelLookupFn = void* (*)(uint64_t cl, void* game, uint64_t levelId) noexcept;           // 0x3267c0
using RoomDataLoadFn = void* (*)(uint64_t cl, void* room) noexcept;                            // 0x3289a0: the engine room of a DRLG room
using FreeSpotSearchFn = void* (*)(void* engineRoom, int32_t* xy, uint32_t size, uint32_t mask, uint64_t zero) noexcept;  // 0x362f90
using ArrivalFinderFn = void* (*)(uint64_t cl, void* gameData, uint64_t levelId, uint64_t exit, int32_t* x, int32_t* y) noexcept;  // 0x3da650
constexpr uint32_t ArrivalSpotMask = 0x1c09;  // the collision mask 0x2ef8b0 asks the search for

EntryFn g_act5Original = nullptr;
ArrivalSpotFn g_spotOriginal = nullptr;
LevelBuildFn g_ownLevelOriginal = nullptr;
bool g_ownLevelBuild = false;  // this plugin hooked the level build itself (the maze hook did not): written at load only

std::atomic<bool> g_live{false};     // every part is on and the hooks are in
std::atomic<bool> g_probe{false};
std::atomic<int> g_slotLevel{-1};  // the slot level of the open map (Jungle's until a map is opened); -1 with no outdoor slot
// One outdoor slot per row of the tileset table (outdoor_tilesets.h); the index is the row's. An unknown tileset gets row 0.
constexpr int SlotCount = static_cast<int>(co::OutdoorTilesetCount);
struct SlotLevels {
	std::atomic<int> level[SlotCount];
	SlotLevels() {
		for (std::atomic<int>& l : level) {
			l = -1;
		}
	}
	std::atomic<int>& operator[](int i) noexcept { return level[i]; }
	std::atomic<int>* begin() noexcept { return level; }
	std::atomic<int>* end() noexcept { return level + SlotCount; }
};
SlotLevels g_slotLevels;  // the slot level id of each row, -1 with no slot row
auto SlotIndexFor(std::string_view tileset) noexcept -> int {
	return static_cast<int>(co::outdoorTilesetIndex(tileset));
}
// The row of the outdoor slot whose level is `levelId`, or -1 (an arena, or no slot level).
auto OutdoorSlotIndexOf(int levelId) noexcept -> int {
	for (int i = 0; i < SlotCount; ++i) {
		if (levelId > 0 && g_slotLevels[i].load() == levelId) {
			return i;
		}
	}
	return -1;
}

struct Job {
	uint64_t id = 0;
	std::string name, tileset, shape, size;
	uint64_t seed = 0;
	cartographer::layout::ShapeTable shapes;
};
struct SlotGeometry {
	bool known = false;
	int offsetX = 0, offsetY = 0, sizeX = 0, sizeY = 0;
	int borderWest = 0, borderEast = 0;  // the Siege slot's border rows (Slot::borderWest), 0 for the others
};
std::mutex g_lock;
std::shared_ptr<const Job> g_jobs[SlotCount];  // the map opened through each outdoor slot row (guarded by g_lock)
SlotGeometry g_geometry[SlotCount];
std::string g_slotRefusal[SlotCount];  // OutdoorSlotRefusal of each slot row; empty when the slot may be built
uint64_t g_nextJobId = 1;
co::PlanCache g_cache(4);
// What the last slot build left for the events and the probe (tiles, absolute). Guarded by g_lock; cleared when the game is left.
MazeEventRect g_eventRooms[SlotCount], g_bossRooms[SlotCount];  // per outdoor slot row
struct ArrivalFact {
	bool known = false;
	int32_t levelId = 0;
	int32_t cellX = 0, cellY = 0, cellW = 0, cellH = 0;  // the entrance cell
};
std::vector<ArrivalFact> g_arrivals;  // one per level id: an arena's entry must not replace a map's (guarded by g_lock)
auto ArrivalOf(int32_t levelId) -> ArrivalFact {  // g_lock held
	for (const ArrivalFact& f : g_arrivals) {
		if (f.levelId == levelId) {
			return f;
		}
	}
	return {};
}
void SetArrival(const ArrivalFact& fact) {  // g_lock held
	for (ArrivalFact& f : g_arrivals) {
		if (f.levelId == fact.levelId) {
			f = fact;
			return;
		}
	}
	g_arrivals.push_back(fact);
}
// A fixed arena (arenas.txt, issue #486): a level row of the mod's own, built from a plan made when the tables load. It is not a map open: it never touches g_jobs or g_slotStates.
struct ArenaEntry {
	ArenaDef def;
	std::shared_ptr<const co::MapPlan> plan;
};
std::vector<std::shared_ptr<const ArenaEntry>> g_arenas;  // guarded by g_lock
std::vector<int32_t> g_arenaFellBack;  // arena levels whose last build was left to the game (guarded by g_lock)
auto ArenaOf(int32_t levelId) -> std::shared_ptr<const ArenaEntry> {
	std::lock_guard<std::mutex> lock(g_lock);
	for (const std::shared_ptr<const ArenaEntry>& a : g_arenas) {
		if (a->def.level == levelId) {
			return a;
		}
	}
	return nullptr;
}
// The box of the last slot build, for `cartographer where` (guarded by g_lock): the level id, its origin in tiles, the macro cell size in tiles and the macro grid.
struct WhereBox {
	bool known = false;
	int32_t levelId = 0, x = 0, y = 0, w = 0, h = 0, macroTiles = 0, macroMinX = 0, macroMinY = 0, macroW = 0, macroH = 0;
};
WhereBox g_whereBox;
std::atomic<bool> g_arrivalLogged{false};
std::atomic<int> g_arrivalResult{0};  // OutdoorArrivalResult: what the arrival probe found (AutoCheck reads it); reset in ArrangeRooms and OutdoorGameLeft
cartographer::maze::SlotBuildState g_slotStates[SlotCount];  // per outdoor slot row: the level was built with a map open (or is being built) in this game (cleared on GameLeft)

// One build of the slot level, on the thread that runs it: the Act 5 hook stamps this plan, and only for this level.
struct BuildCtx {
	void* level = nullptr;
	const co::MapPlan* plan = nullptr;
	bool fired = false;
	int stamped = 0;
	int voided = 0;
};
thread_local BuildCtx* t_ctx = nullptr;

// --- Reading and writing the game's memory ----------------------------------------------------------------------------

auto Readable(const void* p, size_t n) noexcept -> bool {
	if (p == nullptr) {
		return false;
	}
	MEMORY_BASIC_INFORMATION mbi{};
	if (VirtualQuery(p, &mbi, sizeof(mbi)) == 0 || mbi.State != MEM_COMMIT || (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)) != 0) {
		return false;
	}
	const auto* end = static_cast<const uint8_t*>(mbi.BaseAddress) + mbi.RegionSize;
	return static_cast<const uint8_t*>(p) + n <= end;
}

template <typename T>
auto Read(const void* base, int offset) noexcept -> T {
	T value{};
	if (Readable(static_cast<const uint8_t*>(base) + offset, sizeof(T))) {
		std::memcpy(&value, static_cast<const uint8_t*>(base) + offset, sizeof(T));
	}
	return value;
}

template <typename T>
void Write(void* base, int offset, T value) noexcept {
	std::memcpy(static_cast<uint8_t*>(base) + offset, &value, sizeof(T));
}

template <typename Fn>
auto Call(OutdoorPartId id) noexcept -> Fn {
	return reinterpret_cast<Fn>(const_cast<uint8_t*>(g_parts[id].address));
}

auto IsSlotLevel(const void* level) noexcept -> bool {
	if (level == nullptr || !Readable(level, 0x200)) {
		return false;
	}
	const int id = Read<int32_t>(level, LvlId);
	for (const std::atomic<int>& slot : g_slotLevels) {
		if (slot.load() > 0 && slot.load() == id) {
			return true;
		}
	}
	return ArenaOf(id) != nullptr;
}

// --- The grids, read as the game's grid reader (0x3fa790) does: data[rows[y] + x] ---------------------------------------------

auto GridCell(const void* block, int gridOffset, int x, int y, bool* ok) noexcept -> uint64_t {
	const auto* grid = static_cast<const uint8_t*>(block) + gridOffset;
	const auto* rows = Read<const uint8_t*>(grid, GridRows);
	const auto* data = Read<const uint8_t*>(grid, GridData);
	if (rows == nullptr || data == nullptr || !Readable(rows + static_cast<size_t>(y) * 4, 4)) {
		*ok = false;
		return 0;
	}
	const int64_t at = static_cast<int64_t>(Read<int32_t>(rows, y * 4)) + x;
	if (at < 0 || at > (1 << 20) || !Readable(data + static_cast<size_t>(at) * 8, 8)) {
		*ok = false;
		return 0;
	}
	return Read<uint64_t>(data, static_cast<int>(at * 8));
}

// The built level's piece and flag grids, if they are readable and the size the plan expects. `why` says what is wrong otherwise.
auto SampleGrid(const void* level, const co::StampPlan& plan, co::GridSample* out, std::string* why) -> bool {
	const void* block = Read<const void*>(level, LvlGrid);
	if (block == nullptr || !Readable(block, 0x340)) {
		*why = "the level has no readable outdoor block";
		return false;
	}
	const int w = Read<int32_t>(block, GridCellsW), h = Read<int32_t>(block, GridCellsH);
	if (w != plan.cellsW || h != plan.cellsH) {
		*why = "the game's grid is " + std::to_string(w) + " x " + std::to_string(h) + " cells, the plan's is " + std::to_string(plan.cellsW) + " x " + std::to_string(plan.cellsH);
		return false;
	}
	if (w <= 0 || h <= 0 || w * h > MaxGridCells) {
		*why = "the grid size is out of range";
		return false;
	}
	out->piece.assign(static_cast<size_t>(w) * h, 0);
	out->flags.assign(static_cast<size_t>(w) * h, 0);
	bool ok = true;
	for (int y = 0; y < h; ++y) {
		for (int x = 0; x < w; ++x) {
			out->piece[static_cast<size_t>(y) * w + x] = GridCell(block, GridPieces, x, y, &ok);
			out->flags[static_cast<size_t>(y) * w + x] = GridCell(block, GridFlags, x, y, &ok);
		}
	}
	if (!ok) {
		*why = "a grid cell could not be read";
	}
	return ok;
}

// --- Probes (OutdoorProbe = 1 in cartographer.txt, or AutoCheck for the arrival probe): what the game does, for the checks that need a game ------------------------

void ProbeLevel(const void* level, const char* when) noexcept {
	if (!g_probe.load()) {
		return;
	}
	const void* block = Read<const void*>(level, LvlGrid);
	plog::Info("[outdoor probe] %s: level %d type %d levelType %d origin (%d,%d) size (%d,%d) rooms %d first room %p seed %08x %08x outdoor block %p (%d x %d cells)", when,
	           Read<int32_t>(level, LvlId), Read<int32_t>(level, LvlType), Read<int32_t>(level, LvlTileType), Read<int32_t>(level, LvlOriginX), Read<int32_t>(level, LvlOriginY),
	           Read<int32_t>(level, LvlSizeX), Read<int32_t>(level, LvlSizeY), Read<int32_t>(level, LvlRoomCount), Read<const void*>(level, LvlFirstRoom),
	           Read<uint32_t>(level, LvlSeed), Read<uint32_t>(level, LvlSeed2), block, Read<int32_t>(block, GridCellsW), Read<int32_t>(block, GridCellsH));
}

// The grid as characters: '#' a piece's origin cell, '+' covered by a piece, ' ' void, '.' none of these.
void ProbeGrid(const co::StampPlan& plan, const co::GridSample& grid) noexcept {
	if (!g_probe.load()) {
		return;
	}
	try {
		plog::Info("[outdoor probe] grid %d x %d cells ('#' piece, '+' covered, ' ' void, '.' none), planned stamps:\n%s", plan.cellsW, plan.cellsH, co::stampPlanAscii(plan).c_str());
		std::string row;
		for (int y = 0; y < plan.cellsH; ++y) {
			row.clear();
			for (int x = 0; x < plan.cellsW; ++x) {
				const size_t i = static_cast<size_t>(y) * plan.cellsW + x;
				row += grid.piece[i] != 0 ? '#' : (grid.flags[i] & 0x200) != 0 ? '+' : (grid.flags[i] & 0x100) != 0 ? ' ' : '.';
			}
			plog::Info("[outdoor probe] grid %2d |%s|", y, row.c_str());
		}
	} catch (...) {
	}
}

void ProbeRooms(const void* level, int originX, int originY) noexcept {
	if (!g_probe.load()) {
		return;
	}
	int n = 0;
	for (const void* room = Read<const void*>(level, LvlFirstRoom); room != nullptr && n < MaxRooms && Readable(room, 0x98); room = Read<const void*>(room, RoomNext), ++n) {
		const void* piece = Read<const void*>(room, RoomPiece);
		plog::Info("[outdoor probe] room %s%d file %d at (%d,%d) %d x %d", piece != nullptr ? "piece " : "fill ", piece != nullptr ? Read<int32_t>(piece, PieceId) : -1,
		           piece != nullptr ? Read<int32_t>(piece, PieceFile) : -1, Read<int32_t>(room, RoomX) - originX, Read<int32_t>(room, RoomY) - originY,
		           Read<int32_t>(room, RoomW), Read<int32_t>(room, RoomH));
	}
}

// --- The plan --------------------------------------------------------------------------------------------------------

auto PlanForJob(const Job& jobRef, const SlotGeometry& geometry, const void* level, std::string* why) -> std::shared_ptr<const co::MapPlan>;

// The slot row name of an outdoor tileset, for messages.
auto SlotNameOf(const std::string& tileset) -> const char* {
	return co::OutdoorTilesets[SlotIndexFor(tileset)].slot;
}

// The plan for the opened map, for this slot level. Null, with a reason, when the game must build its own level. May throw (strings, vectors).
auto PrepareSlot(const void* level, std::string* why) -> std::shared_ptr<const co::MapPlan> {
	const int slot = OutdoorSlotIndexOf(Read<int32_t>(level, LvlId));
	std::shared_ptr<const Job> job;
	SlotGeometry geometry;
	std::string refusal;
	{
		std::lock_guard<std::mutex> lock(g_lock);
		if (slot >= 0) {
			job = g_jobs[slot];
			geometry = g_geometry[slot];
			refusal = g_slotRefusal[slot];
		}
	}
	if (job == nullptr) {
		*why = "no outdoor map was opened for this slot level in this game";
		return nullptr;
	}
	if (!refusal.empty()) {
		*why = refusal;  // before any write or engine call: the game builds the level itself
		return nullptr;
	}
	if (!geometry.known) {
		*why = std::string("levels.txt could not be read for the ") + SlotNameOf(job->tileset) + " slot level's OffsetX, OffsetY, SizeX and SizeY";
		return nullptr;
	}
	const int want = OutdoorLevelType(job->tileset);
	const int have = Read<int32_t>(level, LvlTileType);
	if (want < 0 || have != want) {
		*why = "the " + std::string(SlotNameOf(job->tileset)) + " slot level's LevelType is " + std::to_string(have) + " but a " + job->tileset + " map needs " + std::to_string(want) + " (levels.txt LevelType)";
		return nullptr;
	}
	std::shared_ptr<const co::MapPlan> plan = PlanForJob(*job, geometry, level, why);
	return plan;
}

// The plan of an arena, made when the tables loaded. Null, with a reason, when the game must build the level itself.
auto PrepareArena(const void* level, const ArenaEntry& arena, std::string* why) -> std::shared_ptr<const co::MapPlan> {
	const int have = Read<int32_t>(level, LvlTileType);
	const co::framefill::FrameFillTileset* ts = co::framefill::findFrameFillTileset(arena.def.tileset);
	if (ts == nullptr) {
		*why = "arena '" + arena.def.name + "': '" + arena.def.tileset + "' is not a frame-plus-fill tileset";
		return nullptr;
	}
	if (have != ts->levelType) {
		*why = "arena '" + arena.def.name + "': the level's LevelType is " + std::to_string(have) + " but a " + ts->name + " arena needs " + std::to_string(ts->levelType) + " (levels.txt LevelType)";
		return nullptr;
	}
	if (arena.plan == nullptr || !arena.plan->ok) {
		*why = arena.plan != nullptr ? arena.plan->reason : std::string("the arena has no plan");
		return nullptr;
	}
	return arena.plan;
}

// The plan for `job` on the slot geometry (cached by request), or its default map when the request failed. Null, with a reason, when both failed. `level` is only for the log line. May throw.
auto PlanForJob(const Job& jobRef, const SlotGeometry& geometry, const void* level, std::string* why) -> std::shared_ptr<const co::MapPlan> {
	const Job* job = &jobRef;
	co::MapRequest request;
	request.tileset = job->tileset;
	request.shape = job->shape;
	request.size = job->size;
	request.seed = job->seed;
	request.shapes = &job->shapes;
	request.options.originX = geometry.offsetX;
	request.options.originY = geometry.offsetY;
	request.options.maxTilesX = geometry.sizeX;
	request.options.maxTilesY = geometry.sizeY;
	request.options.borderWest = geometry.borderWest;
	request.options.borderEast = geometry.borderEast;
	bool built = false;
	bool usedDefault = false;
	// A failed request plans the tileset's default map (Winding, Large, built-in shapes) on the same seed instead, so the slot level is still built by this plugin: the
	// game's own build of it draws Act 5 pieces and its exits are not ours to place (issue #471). The portal check uses the same call, so it opens that map too.
	std::shared_ptr<const co::MapPlan> plan = co::planOrDefault(g_cache, request, co::builtinJungleCatalogue(), &usedDefault, &built);
	if (!plan->ok) {
		*why = plan->reason;
		return nullptr;
	}
	if (usedDefault) {
		plog::Warn("outdoor slot level %d: the plan for '%s' failed; building the default %s map instead", level != nullptr ? Read<int32_t>(level, LvlId) : g_slotLevel.load(), job->name.c_str(), job->tileset.c_str());
	}
	if (built) {
		plog::Info("outdoor slot level %d: planned '%s' (%s %s %s, seed %llu): %d stamps, %d void cells, level %d x %d tiles at (%d,%d), plan %016llx", level != nullptr ? Read<int32_t>(level, LvlId) : g_slotLevel.load(),
		           job->name.c_str(), job->tileset.c_str(), job->shape.c_str(), job->size.c_str(), static_cast<unsigned long long>(job->seed), static_cast<int>(plan->stamps.stamps.size()),
		           static_cast<int>(plan->stamps.voidCells.size()), plan->stamps.tilesW, plan->stamps.tilesH, plan->stamps.originX, plan->stamps.originY,
		           static_cast<unsigned long long>(plan->hash));
		if (g_probe.load()) { // stamp list for the game checks: LvlPrest id, DS1 file (-1 = the engine draws it), top-left tile
			const int cell = plan->stamps.cellsW > 0 ? plan->stamps.tilesW / plan->stamps.cellsW : 0;
			for (const co::Stamp& s : plan->stamps.stamps) {
				plog::Info("[outdoor probe] stamp lvlprest %d file %d at tile (%d,%d)", s.lvlPrest, s.file, plan->stamps.originX + s.cx * cell, plan->stamps.originY + s.cy * cell);
			}
		}
	}
	return plan;
}

// --- The entry, event and boss rooms of the built level ----------------------------------------------------------------

auto ToRect(const void* level, const std::vector<void*>& rooms, int index) -> MazeEventRect {
	if (index < 0 || static_cast<size_t>(index) >= rooms.size()) {
		return {};
	}
	const void* room = rooms[static_cast<size_t>(index)];
	return {true, Read<int32_t>(level, LvlId), Read<int32_t>(room, RoomX), Read<int32_t>(room, RoomY), Read<int32_t>(room, RoomW), Read<int32_t>(room, RoomH)};
}

// After a build that is the plan: the room at the entrance cell's centre becomes the level entry (the game's finder takes the first room with
// bits 16 or 17 of its flags, so every other room loses them), and the rooms at the event and boss cells' centres are recorded for the map
// events. Not noexcept: it allocates; the caller catches.
void ArrangeRooms(const void* level, const co::StampPlan& plan, bool asPlanned) {
	std::vector<void*> rooms;
	std::vector<co::BuiltRoom> rects;
	for (void* room = Read<void*>(level, LvlFirstRoom); room != nullptr && rooms.size() < static_cast<size_t>(MaxRooms) && Readable(room, 0x98); room = Read<void*>(room, RoomNext)) {
		rooms.push_back(room);
		rects.push_back({Read<int32_t>(room, RoomX), Read<int32_t>(room, RoomY), Read<int32_t>(room, RoomW), Read<int32_t>(room, RoomH), Read<uint32_t>(room, RoomFlags)});
	}
	const int id = Read<int32_t>(level, LvlId);
	if (!asPlanned) {
		// The level is built twice per entry (server, then client). Whatever an earlier build recorded stays: a build that is not the plan must not replace it.
		plog::Info("outdoor arrival: left to the game, because the build is not the plan");
		return;
	}
	const MazeEventRect event = ToRect(level, rooms, co::chooseCellRoom(plan.event, rects));
	const MazeEventRect boss = ToRect(level, rooms, co::chooseCellRoom(plan.boss, rects));
	plog::Info("eventprobe: outdoor event room %s, boss room %s (the rooms at the centre of the planned cells)", event.known ? "known" : "not known", boss.known ? "known" : "not known");
	{
		std::lock_guard<std::mutex> lock(g_lock);
		const int slot = OutdoorSlotIndexOf(id);  // an arena is not a map: its event and boss cells are not kept
		if (slot >= 0 && event.known) {
			g_eventRooms[slot] = event;  // a known room is never replaced by an unknown one
		}
		if (slot >= 0 && boss.known) {
			g_bossRooms[slot] = boss;
		}
	}
	for (int i = ArrivalFinderPart; i <= ArrivalFlagTestPart; ++i) {
		if (!g_parts[i].status.on) {
			plog::Info("outdoor arrival: left to the game, because the part %s was not found", g_parts[i].name);
			return;
		}
	}
	const co::ArrivalMark mark = co::planArrival(plan, rects);
	if (!mark.mark) {
		plog::Info("outdoor arrival: left to the game, because %s", mark.why.c_str());
		return;
	}
	const auto flagsOf = [&](int i) { return Read<uint32_t>(rooms[static_cast<size_t>(i)], RoomFlags); };
	Write<uint32_t>(rooms[static_cast<size_t>(mark.room)], RoomFlags, flagsOf(mark.room) | co::kArrivalFlags);
	for (const int i : mark.clear) {
		Write<uint32_t>(rooms[static_cast<size_t>(i)], RoomFlags, flagsOf(i) & ~co::kArrivalFlags);
	}
	const bool arena = ArenaOf(id) != nullptr;  // an arena build must not reset the probe state of an open map (g_probe is per game, one map)
	{
		std::lock_guard<std::mutex> lock(g_lock);
		SetArrival({true, id, plan.entrance.x, plan.entrance.y, plan.entrance.w, plan.entrance.h});
		if (!arena) {
			g_arrivalLogged = false;
			g_arrivalResult = 0;
		}
	}
	plog::Info("outdoor arrival: room %d of %d (at %d,%d) marked as the level entry, in the entrance cell (%d,%d); %d other room(s) lost the mark", mark.room, static_cast<int>(rooms.size()),
	           rects[static_cast<size_t>(mark.room)].x, rects[static_cast<size_t>(mark.room)].y, plan.entrance.x, plan.entrance.y, static_cast<int>(mark.clear.size()));
}

// Before the game places a unit on the slot level (0x2ef8b0): when the room marked as the level entry has no free spot (the game would log
// "Unable to find spawn location for act start" and the portal could not be entered, issue #456), move the mark to the first room of
// co::arrivalCandidates that has one, using the game's own search with the game's own arguments. Not noexcept: it allocates; the caller catches.
void MoveArrivalToFreeSpot(uint64_t cl, void* game, uint64_t levelId, uint64_t exit, uint32_t size) {
	if (game == nullptr || !Readable(game, 0x78)) {
		return;
	}
	// 0x2ef8b0 hands the finder and the level lookup the pointer at +0x70 of its second argument (0x2ef8b6).
	void* const gameData = Read<void*>(game, 0x70);
	void* const level = Call<LevelLookupFn>(LevelLookupPart)(cl, gameData, levelId);
	if (!IsSlotLevel(level)) {
		return;
	}
	// The game builds the level inside its own room finder (0x3da650), after this hook and before the free-spot search: run the finder
	// here first (it builds the level, which marks the arrival room), so the rooms and the mark exist when the check runs.
	if (Read<void*>(level, LvlFirstRoom) == nullptr) {
		int32_t x = 0, y = 0;
		Call<ArrivalFinderFn>(ArrivalFinderPart)(cl, gameData, levelId, exit, &x, &y);
	}
	ArrivalFact fact;
	{
		std::lock_guard<std::mutex> lock(g_lock);
		fact = ArrivalOf(static_cast<int32_t>(levelId));
	}
	if (!fact.known) {
		return;
	}
	std::vector<void*> rooms;
	std::vector<co::BuiltRoom> rects;
	for (void* room = Read<void*>(level, LvlFirstRoom); room != nullptr && rooms.size() < static_cast<size_t>(MaxRooms) && Readable(room, 0x98); room = Read<void*>(room, RoomNext)) {
		rooms.push_back(room);
		rects.push_back({Read<int32_t>(room, RoomX), Read<int32_t>(room, RoomY), Read<int32_t>(room, RoomW), Read<int32_t>(room, RoomH), Read<uint32_t>(room, RoomFlags)});
	}
	int marked = -1;
	for (size_t i = 0; i < rects.size() && marked < 0; ++i) {
		if ((rects[i].flags & co::kArrivalFlags) != 0) marked = static_cast<int>(i);
	}
	if (marked < 0) {
		return;  // no mark of ours (the game keeps its own arrival)
	}
	co::TileRect cell;
	cell.known = true;
	cell.x = fact.cellX;
	cell.y = fact.cellY;
	cell.w = fact.cellW;
	cell.h = fact.cellH;
	const auto load = Call<RoomDataLoadFn>(RoomDataLoadPart);
	const auto search = Call<FreeSpotSearchFn>(FreeSpotSearchPart);
	for (const int i : co::arrivalCandidates(cell, rects)) {
		void* const engineRoom = load(cl, rooms[static_cast<size_t>(i)]);
		if (engineRoom == nullptr) {
			continue;
		}
		const co::BuiltRoom& r = rects[static_cast<size_t>(i)];
		// The spot the game starts from: the room's centre tile, in subtiles, and 3 in (0x2ef8b0 does x = tile * 5 + 3).
		int32_t xy[2] = {(r.x + r.w / 2) * 5 + 3, (r.y + r.h / 2) * 5 + 3};
		if (search(engineRoom, xy, size, ArrivalSpotMask, 0) == nullptr) {
			continue;
		}
		if (i != marked) {
			Write<uint32_t>(rooms[static_cast<size_t>(marked)], RoomFlags, Read<uint32_t>(rooms[static_cast<size_t>(marked)], RoomFlags) & ~co::kArrivalFlags);
			Write<uint32_t>(rooms[static_cast<size_t>(i)], RoomFlags, Read<uint32_t>(rooms[static_cast<size_t>(i)], RoomFlags) | co::kArrivalFlags);
			static std::atomic<int> moved{0};
			plog::Info("outdoor arrival: no free spot in room %d (%d,%d); moved to room %d (%d,%d), free spot at subtile (%d,%d) (move %d)", marked, rects[static_cast<size_t>(marked)].x,
			           rects[static_cast<size_t>(marked)].y, i, r.x, r.y, xy[0], xy[1], ++moved);
		}
		plog::Info("outdoor arrival: room %d (%d,%d) has a free spot at subtile (%d,%d) (size %u); kept", i, r.x, r.y, xy[0], xy[1], size);
		return;
	}
	plog::Warn("outdoor arrival: no room of the level has a free spot; the game will refuse the arrival");
}

auto ArrivalSpotHook(uint64_t cl, void* game, uint64_t levelId, uint64_t exit, int32_t* outX, int32_t* outY, uint32_t size) noexcept -> uint64_t {
	if (g_spotOriginal == nullptr) {
		return 0;  // the loader gave no way back (logged at install)
	}
	try {
		MoveArrivalToFreeSpot(cl, game, levelId, exit, size);
	} catch (...) {
		plog::Warn("outdoor arrival: the free-spot check failed; the game's own arrival runs");
	}
	return g_spotOriginal(cl, game, levelId, exit, outX, outY, size);
}

// --- The hooks -------------------------------------------------------------------------------------------------------

// 0x3fe2e0 for the slot: replay the plan with the game's own piece stamper, then mark the unused cells void. For every other level (and for the
// slot when no build of ours is running) the game's own Act 5 builder runs. The grids were allocated by the entry function before this runs.
void Act5Hook(uint8_t dataFlag, void* level) noexcept {
	if (g_act5Original == nullptr) {
		return;  // the loader gave no way back (logged at install); there is nothing to call
	}
	BuildCtx* ctx = t_ctx;
	if (ctx == nullptr || ctx->level != level || ctx->plan == nullptr) {
		g_act5Original(dataFlag, level);
		return;
	}
	ctx->fired = true;
	const co::StampPlan& plan = ctx->plan->stamps;
	const auto stamp = Call<StampFn>(StampPart);
	for (const co::Stamp& s : plan.stamps) {
		stamp(dataFlag, level, s.cx, s.cy, s.lvlPrest, s.file, 0);
		++ctx->stamped;
	}
	const auto mark = Call<VoidFn>(VoidPart);
	for (const cartographer::layout::Point& p : plan.voidCells) {
		mark(level, p.x, p.y);
		++ctx->voided;
	}
}

// The slot level's whole build, from the level build hook: rectangle, seed, the entry function (which reaches Act5Hook and then makes the
// rooms), then the level build's native tail. No allocation and nothing that throws past the logging, which is wrapped.
auto RunBuild(uint8_t dataFlag, void* level, LevelBuildFn original, const co::MapPlan& plan, bool arena) noexcept -> uint64_t {
	const int id = Read<int32_t>(level, LvlId);
	const co::LevelRect rect = co::levelRectOf(plan.stamps);
	ProbeLevel(level, "before the slot build");
	if (!arena) {  // an arena must not replace the open map's box for `cartographer where`
		std::lock_guard<std::mutex> lock(g_lock);
		const co::StampPlan& sp = plan.stamps;
		g_whereBox = {true, id, rect.x, rect.y, rect.w, rect.h, sp.macroW > 0 ? sp.tilesW / sp.macroW : 0, sp.macroMinX, sp.macroMinY, sp.macroW, sp.macroH};
	}
	// The rectangle is written before the build, as act start-up does for the game's own outdoor levels: it is 0 for a slot (section 4, S1).
	Write<int32_t>(level, LvlOriginX, rect.x);
	Write<int32_t>(level, LvlOriginY, rect.y);
	Write<int32_t>(level, LvlSizeX, rect.w);
	Write<int32_t>(level, LvlSizeY, rect.h);
	// The level RNG decides each piece's DS1 file when the plan leaves it to the engine. It is seeded from the plan (a function of the map's seed and
	// tables only), not from the act seed, so a client that has only the host's map seed draws the same files (issue #146, section 8a).
	const co::SlotRngSeed seed = co::slotRngSeedFor(plan.hash, id);
	Write<int32_t>(level, LvlSeed, seed.first);
	Write<int32_t>(level, LvlSeed2, seed.second);
	AutomapResetArm(plan.hash, id);  // a new Snow map starts with an empty automap (Jungle and Siege share the town's layer, so nothing is skipped for them)

	BuildCtx ctx;
	ctx.level = level;
	ctx.plan = &plan;
	BuildCtx* previous = t_ctx;
	t_ctx = &ctx;
	Call<EntryFn>(EntryPart)(dataFlag, level);
	t_ctx = previous;

	// The native tail (saved room bits, the warp-spot scan): type 0 matches no branch of the dispatch, so D2RCore's outdoor generator is never reached.
	Write<int32_t>(level, LvlType, NativeTailType);
	const uint64_t result = original(dataFlag, level);
	Write<int32_t>(level, LvlType, OutdoorType);

	bool asPlanned = false;
	try {
		if (!ctx.fired) {
			plog::Warn("outdoor slot level %d: the game did not take its Act 5 branch, so the plan was NOT stamped: the outdoor slot level must be in Act 5 (levels.txt Act 5)", id);
		}
		ProbeLevel(level, "after the slot build");
		co::GridSample grid;
		std::string why;
		if (!SampleGrid(level, plan.stamps, &grid, &why)) {
			plog::Warn("outdoor slot level %d: the built grid was not checked: %s", id, why.c_str());
		} else {
			co::GridCounts counts;
			const std::vector<std::string> problems = co::compareGrid(plan.stamps, grid, &counts);
			if (ctx.fired && problems.empty()) {
				plog::Info("outdoor slot level %d: built as planned (%d stamps, %d void cells; grid %d cells: %d pieces, %d covered, %d void, %d neither), %d rooms, plan %016llx%s", id, ctx.stamped, ctx.voided,
				           counts.cells, counts.pieces, counts.covered, counts.voided, counts.other, Read<int32_t>(level, LvlRoomCount), static_cast<unsigned long long>(plan.hash), arena ? " (arena)" : "");
			} else if (!problems.empty()) {
				plog::Warn("outdoor slot level %d: the built grid is not the plan: %s%s", id, problems.front().c_str(), problems.size() > 1 ? " (and more; turn OutdoorProbe on)" : "");
				for (size_t i = 1; i < problems.size() && g_probe.load(); ++i) {
					plog::Info("[outdoor probe] grid difference: %s", problems[i].c_str());
				}
			}
			ProbeGrid(plan.stamps, grid);
			asPlanned = ctx.fired && problems.empty();
		}
		if (plan.stamps.entrance.known) {
			plog::Info("outdoor slot level %d: entrance cell at tile (%d,%d) %d x %d; event %s; boss %s", id, plan.stamps.entrance.x, plan.stamps.entrance.y, plan.stamps.entrance.w, plan.stamps.entrance.h,
			           plan.stamps.event.known ? "planned" : "none", plan.stamps.boss.known ? "planned" : "none");
		}
		ProbeRooms(level, rect.x, rect.y);
	} catch (...) {
	}
	try {
		ArrangeRooms(level, plan.stamps, asPlanned);
	} catch (...) {
	}
	return result;
}

// Whether to build the slot level ourselves, and if so, do it. See the header.
auto BuildSlotLevel(uint8_t dataFlag, void* level, LevelBuildFn original, uint64_t* result, const ArenaEntry* arena) noexcept -> bool {
	const int id = Read<int32_t>(level, LvlId);
	if (Read<int32_t>(level, LvlType) != OutdoorType) {
		plog::Warn("outdoor slot level %d: not an outdoor level (levels.txt DrlgType must be 3); building the game's own level", id);
		return false;
	}
	if (t_ctx != nullptr) {
		return false;  // a build of ours is running on this thread: never nest
	}
	const void* drlg = Read<const void*>(level, LvlDrlg);
	const void* block = Read<const void*>(level, LvlGrid);
	if (drlg == nullptr || !Readable(drlg, DrlgSeedBase + 4) || block == nullptr || !Readable(block, 0x340)) {
		plog::Warn("outdoor slot level %d: the level has no readable drlg or outdoor block; building the game's own level", id);
		return false;
	}
	if (Read<int32_t>(level, LvlRoomCount) != 0 || Read<const void*>(level, LvlFirstRoom) != nullptr) {
		plog::Warn("outdoor slot level %d: the level already holds %d rooms before the build; building the game's own level", id, Read<int32_t>(level, LvlRoomCount));
		return false;
	}
	std::shared_ptr<const co::MapPlan> plan;
	std::string why;
	try {
		plan = arena != nullptr ? PrepareArena(level, *arena, &why) : PrepareSlot(level, &why);
	} catch (...) {
		plog::Error("outdoor slot level %d: an exception while preparing the plan; building the game's own level", id);
		return false;
	}
	if (plan == nullptr) {
		try {
			plog::Warn("outdoor slot level %d: building the game's own level: %s", id, why.c_str());
		} catch (...) {
		}
		return false;
	}
	*result = RunBuild(dataFlag, level, original, *plan, arena != nullptr);  // from here the level is ours; `plan` keeps the cached plan alive for the build
	return true;
}

auto OwnLevelBuildHook(uint8_t dataFlag, void* level) noexcept -> uint64_t {
	if (g_ownLevelOriginal == nullptr) {
		return 0;  // the loader gave no way back (logged at install): every level build is skipped until a restart
	}
	uint64_t result = 0;
	if (OutdoorLevelBuild(dataFlag, level, g_ownLevelOriginal, &result)) {
		return result;
	}
	return g_ownLevelOriginal(dataFlag, level);
}

}  // namespace

auto ArenaBuilt(int32_t levelId) -> bool {
	if (!g_live.load(std::memory_order_relaxed)) {
		return false;
	}
	const std::shared_ptr<const ArenaEntry> arena = ArenaOf(levelId);
	if (arena == nullptr || arena->plan == nullptr || !arena->plan->ok) {
		return false;
	}
	std::lock_guard<std::mutex> lock(g_lock);
	return std::find(g_arenaFellBack.begin(), g_arenaFellBack.end(), levelId) == g_arenaFellBack.end();
}

auto OutdoorLevelBuild(uint8_t dataFlag, void* level, LevelBuildFn original, uint64_t* result) noexcept -> bool {
	if (!g_live.load(std::memory_order_relaxed) || original == nullptr || result == nullptr || !IsSlotLevel(level)) {
		return false;  // every other level runs the game's own build, untouched
	}
	if (const std::shared_ptr<const ArenaEntry> arena = ArenaOf(Read<int32_t>(level, LvlId)); arena != nullptr) {
		// An arena is not a map open: no map record, no slot state (the one-map-per-game gates stay as they are).
		const bool built = BuildSlotLevel(dataFlag, level, original, result, arena.get());
		{
			std::lock_guard<std::mutex> lock(g_lock);
			const int32_t id = arena->def.level;
			g_arenaFellBack.erase(std::remove(g_arenaFellBack.begin(), g_arenaFellBack.end(), id), g_arenaFellBack.end());
			if (!built) {
				g_arenaFellBack.push_back(id);
			}
		}
		if (!built && t_ctx == nullptr && Read<int32_t>(level, LvlType) == OutdoorType && Read<void*>(level, LvlFirstRoom) == nullptr) {
			const co::LevelRect rect = co::fallbackRectOf(arena->def.offsetX, arena->def.offsetY, arena->def.sizeX, arena->def.sizeY);  // issue #471
			Write<int32_t>(level, LvlOriginX, rect.x);
			Write<int32_t>(level, LvlOriginY, rect.y);
			Write<int32_t>(level, LvlSizeX, rect.w);
			Write<int32_t>(level, LvlSizeY, rect.h);
			plog::Warn("arena '%s' (level %d): the game builds its own level in %d x %d tiles at (%d,%d)", arena->def.name.c_str(), arena->def.level, rect.w, rect.h, rect.x, rect.y);
		}
		return built;
	}
	const int slot = OutdoorSlotIndexOf(Read<int32_t>(level, LvlId));
	if (slot < 0) {
		return false;  // IsSlotLevel matched and no arena did, so this is always a slot row: nothing of ours is built otherwise
	}
	bool hadJob = false;
	{
		std::lock_guard<std::mutex> lock(g_lock);
		hadJob = g_jobs[slot] != nullptr;
	}
	g_slotStates[slot].begin();
	const bool ours = BuildSlotLevel(dataFlag, level, original, result, nullptr);
	if (!ours && t_ctx == nullptr && Read<int32_t>(level, LvlType) == OutdoorType && Read<void*>(level, LvlFirstRoom) == nullptr) {
		// The game builds the level itself. Its rectangle is 0 now and must hold the map border, or the game exits (issue #471): give it the slot row's.
		SlotGeometry geometry;
		{
			std::lock_guard<std::mutex> lock(g_lock);
			for (int i = 0; i < SlotCount; ++i) {
				if (g_slotLevels[i].load() == Read<int32_t>(level, LvlId)) {
					geometry = g_geometry[i];
				}
			}
		}
		if (geometry.known) {
			const co::LevelRect rect = co::fallbackRectOf(geometry.offsetX, geometry.offsetY, geometry.sizeX, geometry.sizeY);
			Write<int32_t>(level, LvlOriginX, rect.x);
			Write<int32_t>(level, LvlOriginY, rect.y);
			Write<int32_t>(level, LvlSizeX, rect.w);
			Write<int32_t>(level, LvlSizeY, rect.h);
			plog::Warn("outdoor slot level %d: the game builds its own level in %d x %d tiles at (%d,%d); it has no entry marked, so the portal may not take the player in (issue #471)", Read<int32_t>(level, LvlId), rect.w, rect.h, rect.x, rect.y);
		}
	}
	g_slotStates[slot].end(hadJob);
	return ours;
}

auto OutdoorSlotBuilt(int level) -> bool {
	const int slot = OutdoorSlotIndexOf(level);
	return slot >= 0 && g_slotStates[slot].blocked();
}

auto IsOutdoorSlotLevel(int level) -> bool {
	return OutdoorSlotIndexOf(level) >= 0;
}

auto OutdoorPlanProblem(int level) -> std::string {
	try {
		const int slot = OutdoorSlotIndexOf(level);
		std::shared_ptr<const Job> job;
		SlotGeometry geometry;
		std::string refusal;
		{
			std::lock_guard<std::mutex> lock(g_lock);
			if (slot >= 0) {
				job = g_jobs[slot];
				geometry = g_geometry[slot];
				refusal = g_slotRefusal[slot];
			}
		}
		if (job == nullptr) {
			return "Cartographer: no outdoor map is open.";
		}
		if (!refusal.empty()) {
			return "Cartographer: " + refusal + ", so " + job->name + " cannot be built.";
		}
		if (!geometry.known) {
			return std::string("Cartographer: levels.txt could not be read for the ") + SlotNameOf(job->tileset) + " slot level's size, so " + job->name + " cannot be built.";
		}
		std::string why;
		if (PlanForJob(*job, geometry, nullptr, &why) == nullptr) {
			return "Cartographer: " + job->name + " cannot be built: " + why;
		}
		return {};
	} catch (...) {
		return "Cartographer: the outdoor map could not be planned.";
	}
}

auto OutdoorSlotRanAny(int level) -> bool {
	const int slot = OutdoorSlotIndexOf(level);
	return slot >= 0 && g_slotStates[slot].ranAny();
}

auto OutdoorOpenSeed(int level) -> uint64_t {
	const int slot = OutdoorSlotIndexOf(level);
	std::lock_guard<std::mutex> lock(g_lock);
	return slot >= 0 && g_jobs[slot] != nullptr ? g_jobs[slot]->seed : 0;
}

auto OutdoorBuildLive() -> bool {
	return g_live.load();
}

auto OutdoorPartStatuses() -> std::vector<PartStatus> {
	std::vector<PartStatus> out;
	for (int i = 0; i < OutdoorPartCount; ++i) {
		if (i == OwnLevelBuild && !g_ownLevelBuild) {
			continue;  // shared with the maze hook: its part is listed with the maze parts
		}
		if (i >= ArrivalFinderPart && i <= ArrivalFlagTestPart && MazeSteeringLive()) {
			continue;  // the same three guards, listed with the maze parts
		}
		out.push_back(g_parts[i].status);
	}
	return out;
}

auto OutdoorSlotLevelId() -> int {
	return g_slotLevel.load();
}

auto OutdoorEventRoom(int level) -> MazeEventRect {
	const int slot = OutdoorSlotIndexOf(level);
	std::lock_guard<std::mutex> lock(g_lock);
	return slot >= 0 ? g_eventRooms[slot] : MazeEventRect{};
}

auto OutdoorBossRoom(int level) -> MazeEventRect {
	const int slot = OutdoorSlotIndexOf(level);
	std::lock_guard<std::mutex> lock(g_lock);
	return slot >= 0 ? g_bossRooms[slot] : MazeEventRect{};
}

auto OutdoorArrivalResult() -> int {
	return g_arrivalResult.load();
}

auto OutdoorArrivalPending() -> bool {
	return g_probe.load(std::memory_order_relaxed) && !g_arrivalLogged.load(std::memory_order_relaxed) && g_slotLevel.load(std::memory_order_relaxed) > 0;
}

// `room` is what UnitRoom returned: the player's server room; its DRLG room (+0x18) carries the tile rect, as for the maze probe.
void OutdoorProbeArrival(void* room, int32_t levelId) {
	if (!g_probe.load() || room == nullptr || levelId != g_slotLevel.load() || g_arrivalLogged.exchange(true)) {
		return;
	}
	ArrivalFact fact;
	{
		std::lock_guard<std::mutex> lock(g_lock);
		fact = ArrivalOf(levelId);
	}
	const void* drlg = Read<const void*>(room, ServerRoomDrlg);
	if (drlg == nullptr) {
		plog::Info("[outdoor probe] arrival: the player's server room %p has no DRLG room", room);
		g_arrivalResult = 3;
		return;
	}
	const int32_t x = Read<int32_t>(drlg, RoomX), y = Read<int32_t>(drlg, RoomY), w = Read<int32_t>(drlg, RoomW), h = Read<int32_t>(drlg, RoomH);
	if (w <= 0 || h <= 0) {
		plog::Info("[outdoor probe] arrival: the player's server room %p has no readable DRLG room (%p)", room, drlg);
		g_arrivalResult = 3;
		return;
	}
	const bool have = fact.known && fact.levelId == levelId;
	const bool inCell = have && x >= fact.cellX && y >= fact.cellY && x + w <= fact.cellX + fact.cellW && y + h <= fact.cellY + fact.cellH;
	g_arrivalResult = have ? (inCell ? 1 : 2) : 4;  // no entrance mark was placed: the arrival is the game's own
	plog::Info("[outdoor probe] arrival: the player is in DRLG room %p (server room %p) of level %d at (%d,%d) size %d x %d; the entrance cell is %s (%d,%d) %d x %d%s", drlg, room, levelId, x, y, w, h,
	           have ? "at" : "unknown, last seen at", fact.cellX, fact.cellY, fact.cellW, fact.cellH,
	           have ? (inCell ? ": ARRIVAL IS IN THE ENTRANCE CELL" : ": arrival is NOT in the entrance cell") : "");
}

auto OutdoorWhereText(int32_t levelId, int32_t subX, int32_t subY) -> std::string {
	WhereBox box;
	{
		std::lock_guard<std::mutex> lock(g_lock);
		box = g_whereBox;
	}
	if (!box.known || box.levelId != levelId || box.macroTiles <= 0) {
		return "not in the outdoor slot level";
	}
	const int tx = subX / 5, ty = subY / 5;  // subtiles to tiles
	const int rx = tx - box.x, ry = ty - box.y;
	const bool inside = rx >= 0 && ry >= 0 && rx < box.w && ry < box.h;
	char buf[200];
	std::snprintf(buf, sizeof buf, "tile (%d,%d), box tile (%d,%d) of %d x %d, plan cell (%d,%d) of x %d..%d, y %d..%d%s", tx, ty, rx, ry, box.w, box.h, rx / box.macroTiles + box.macroMinX,
	              ry / box.macroTiles + box.macroMinY, box.macroMinX, box.macroMinX + box.macroW - 1, box.macroMinY, box.macroMinY + box.macroH - 1, inside ? "" : " (OUTSIDE the box)");
	return buf;
}

void SetOutdoorTables(const std::shared_ptr<const Tables>& tables) {
	const Slot* found[SlotCount] = {};
	if (tables != nullptr) {
		for (const Slot& s : tables->slots) {
			for (int i = 0; i < SlotCount; ++i) {
				if (EqualsNoCase(s.name, co::OutdoorTilesets[i].slot)) {
					found[i] = &s;
					break;
				}
			}
		}
	}
	{
		std::lock_guard<std::mutex> lock(g_lock);
		for (int i = 0; i < SlotCount; ++i) {
			const Slot* slot = found[i];
			g_geometry[i] = {};
			g_slotRefusal[i] = slot != nullptr ? OutdoorSlotRefusal(*slot) : std::string();
			if (slot != nullptr && slot->haveGeometry) {
				g_geometry[i] = {true, slot->offsetX, slot->offsetY, slot->sizeX, slot->sizeY, slot->borderWest, slot->borderEast};
			}
			g_slotLevels[i] = slot != nullptr ? slot->level : -1;
		}
	}
	std::vector<std::shared_ptr<const ArenaEntry>> arenas;
	if (tables != nullptr) {
		for (const ArenaDef& def : tables->arenas) {
			auto entry = std::make_shared<ArenaEntry>();
			entry->def = def;
			co::framefill::FrameFillRequest request;
			request.cellsW = def.width;
			request.cellsH = def.height;
			request.arenaFill = def.fill;
			request.entryX = def.entryX;
			request.entryY = def.entryY;
			request.options.originX = def.offsetX;
			request.options.originY = def.offsetY;
			request.options.maxTilesX = def.sizeX;
			request.options.maxTilesY = def.sizeY;
			const co::framefill::FrameFillTileset* ts = co::framefill::findFrameFillTileset(def.tileset);
			if (ts == nullptr) {
				// LoadArenas only accepts tileset names with a row, so this is a table fault: plan nothing and let the game build the level.
				co::MapPlan none;
				none.reason = "'" + def.tileset + "' is not a frame-plus-fill tileset";
				entry->plan = std::make_shared<const co::MapPlan>(std::move(none));
			} else {
				entry->plan = std::make_shared<const co::MapPlan>(co::planArenaMap(*ts, request));
			}
			if (entry->plan->ok) {
				plog::Info("arena '%s' (level %d): planned %d x %d cells, %d stamps, level %d x %d tiles at (%d,%d), plan %016llx", def.name.c_str(), def.level, def.width, def.height,
				           static_cast<int>(entry->plan->stamps.stamps.size()), entry->plan->stamps.tilesW, entry->plan->stamps.tilesH, entry->plan->stamps.originX, entry->plan->stamps.originY,
				           static_cast<unsigned long long>(entry->plan->hash));
			} else {
				plog::Warn("arena '%s' (level %d): %s; the game builds that level itself", def.name.c_str(), def.level, entry->plan->reason.c_str());
			}
			arenas.push_back(std::move(entry));
		}
	}
	{
		std::lock_guard<std::mutex> lock(g_lock);
		g_arenas = std::move(arenas);
		g_arenaFellBack.clear();
	}
	g_cache.clear();
	int firstLevel = -1;  // the first slot in table order that has a row
	for (int i = 0; i < SlotCount && firstLevel < 0; ++i) {
		firstLevel = found[i] != nullptr ? found[i]->level : -1;
	}
	g_slotLevel = firstLevel;
	g_probe = tables != nullptr && (tables->settings.outdoorProbe != 0 || tables->settings.autoCheck != 0);  // AutoCheck reads the arrival probe
}

void OutdoorMapOpened(const MapDef& map, const Tables& tables, uint64_t seedSalt, uint64_t fixedSeed) {
	auto job = std::make_shared<Job>();
	job->name = map.name;
	job->tileset = map.tileset;
	job->shape = map.shape;
	job->size = map.size;
	job->shapes = tables.shapes;
	std::lock_guard<std::mutex> lock(g_lock);
	if (g_slotLevels[SlotIndexFor(job->tileset)].load() > 0) {
		g_slotLevel = g_slotLevels[SlotIndexFor(job->tileset)].load();  // the open map's slot level, for the events and the arrival probe
	}
	job->id = g_nextJobId++;
	job->seed = fixedSeed != 0 ? fixedSeed : cartographer::maze::mixSeed(seedSalt, job->id);
	g_jobs[SlotIndexFor(job->tileset)] = job;
	plog::Info("outdoor map '%s' (%s, %s, %s) opened with %sseed %llu", map.name.c_str(), map.tileset.c_str(), map.shape.c_str(), map.size.c_str(), fixedSeed != 0 ? "the host's " : "",
	           static_cast<unsigned long long>(job->seed));
}

void OutdoorMapCancelled(int level) {
	const int slot = OutdoorSlotIndexOf(level);
	if (slot < 0) {
		return;
	}
	std::lock_guard<std::mutex> lock(g_lock);
	g_jobs[slot].reset();
}

void OutdoorGameLeft() {
	{
		std::lock_guard<std::mutex> lock(g_lock);
		for (int i = 0; i < SlotCount; ++i) {
			g_jobs[i].reset();
			g_eventRooms[i] = {};
			g_bossRooms[i] = {};
			g_slotStates[i].clear();
		}
		g_arrivals.clear();
		g_arrivalLogged = false;
		g_arrivalResult = 0;
	}
	g_cache.clear();
}

void InstallOutdoorHooks(const D2RL::PluginContext* context, const std::vector<SignatureOverride>& overrides, const CodeRange* ranges, int count) {
	// The parts are found either way (for 'cartographer check'), but nothing is hooked without an
	// Outdoor slot, the game runs exactly as without this code (no hook on the Act 5 builder, none added to the level build).
	for (int i = 0; i < OwnLevelBuild; ++i) {
		ResolvePart(context, overrides, ranges, count, g_parts[i]);
	}
	bool anyArena = false;
	{
		std::lock_guard<std::mutex> lock(g_lock);
		anyArena = !g_arenas.empty();
	}
	if (g_slotLevel.load() <= 0 && !anyArena) {
		for (Part& part : g_parts) {
			if (part.status.on) {
				part.status.on = false;
				part.status.detail = "found, not hooked: no Outdoor slot";
			}
		}
		plog::Info("no Outdoor slot (slots.txt) and no arena (arenas.txt); no outdoor hook installed");
		return;
	}
	bool allOn = true;
	for (int i = 0; i < OutdoorNeededParts; ++i) {
		allOn = allOn && g_parts[i].status.on;
	}
	const auto turnOff = [](const char* detail) {
		for (Part& part : g_parts) {
			if (part.status.on) {
				part.status.on = false;
				part.status.detail = detail;
			}
		}
	};
	if (!allOn) {
		// Nothing is hooked: one missing part turns only the outdoor build off, and a half-installed set could not be taken out.
		turnOff("off: not hooked, because another outdoor part was not found");
		plog::Warn("the outdoor build is off: not every outdoor part was found (see 'cartographer check'); the game builds the Outdoor slot level itself");
		return;
	}
	// The level build (0x3271c0) is hooked once. When the maze steering took it, its hook calls ours for every level; otherwise this plugin hooks it.
	const bool shared = MazeSteeringLive();
	if (!shared && MazeLevelBuildHookTried()) {
		turnOff("off: not hooked, because the level build hook of the maze steering was refused or gave no way back");
		plog::Warn("the outdoor build is off: the level build hook of the maze steering did not go in cleanly (see 'cartographer check')");
		return;
	}
	if (!shared) {
		ResolvePart(context, overrides, ranges, count, g_parts[OwnLevelBuild]);
		if (!g_parts[OwnLevelBuild].status.on) {
			turnOff("off: not hooked, because the LevelBuild part was not found");
			plog::Warn("the outdoor build is off: the LevelBuild part was not found (see 'cartographer check')");
			return;
		}
		g_ownLevelBuild = true;
	}
	const auto install = [&](OutdoorPartId id, void* target, void** original) {
		Part& part = g_parts[id];
		const uint64_t rva = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(part.address) - context->exeBase);
		const bool hooked = context->InstallInlineHook(rva, part.guard, part.hookSize, target, original);
		return HookOutcome(part, hooked, *original != nullptr);
	};
	// The inner hook first; the level build, which switches the build on, last.
	bool ok = install(Act5Builder, reinterpret_cast<void*>(&Act5Hook), reinterpret_cast<void**>(&g_act5Original));
	if (ok && g_ownLevelBuild) {
		ok = install(OwnLevelBuild, reinterpret_cast<void*>(&OwnLevelBuildHook), reinterpret_cast<void**>(&g_ownLevelOriginal));
	}
	if (!ok) {
		plog::Warn("the outdoor build is off: a hook was refused (see 'cartographer check'); the hooks that went in pass straight through");
		return;
	}
	g_live = true;
	// The arrival spot hook is optional: without it (a part missing, or the hook refused) the game's own arrival runs as before.
	bool spotParts = true;
	for (int i = ArrivalFinderPart; i <= FreeSpotSearchPart; ++i) {
		spotParts = spotParts && g_parts[i].status.on;
	}
	if (spotParts && !install(ArrivalSpotPart, reinterpret_cast<void*>(&ArrivalSpotHook), reinterpret_cast<void**>(&g_spotOriginal))) {
		plog::Warn("the arrival spot check is off: its hook was refused (see 'cartographer check')");
	}
	plog::Info("the outdoor build hooks are in%s", g_ownLevelBuild ? " (with their own level build hook)" : " (behind the maze steering's level build hook)");
}

}  // namespace carto
