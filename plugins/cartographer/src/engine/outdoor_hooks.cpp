#include "outdoor_hooks.h"

#include "core/part_signatures.h"
#include "core/text.h"
#include "log.h"
#include "maze_hooks.h"
#include "steer.h"

#include "outdoor_plan.h"
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
enum OutdoorPartId { Act5Builder, EntryPart, StampPart, VoidPart, ArrivalFinderPart, ArrivalRoomFinderPart, ArrivalFlagTestPart, OwnLevelBuild, OutdoorPartCount };
constexpr int OutdoorNeededParts = ArrivalFinderPart;  // the parts every build needs; LevelBuild is added only when the maze hook did not take it

Part g_parts[OutdoorPartCount] = {
	{"OutdoorAct5Builder", sig::OutdoorAct5Builder, 16, true, nullptr, {}, {}},
	{"OutdoorEntry", sig::OutdoorEntry, 0, true, nullptr, {}, {}},
	{"PieceStamp", sig::PieceStamp, 0, true, nullptr, {}, {}},
	{"VoidCell", sig::VoidCell, 0, true, nullptr, {}, {}},
	{"ArrivalFinder", sig::ArrivalFinder, 0, false, nullptr, {}, {}},
	{"ArrivalRoomFinder", sig::ArrivalRoomFinder, 0, false, nullptr, {}, {}},
	{"ArrivalFlagTest", sig::ArrivalFlagTest, 0, false, nullptr, {}, {}},
	{"LevelBuild", sig::LevelBuild, 14, true, nullptr, {}, {}},
};

EntryFn g_act5Original = nullptr;
LevelBuildFn g_ownLevelOriginal = nullptr;
bool g_ownLevelBuild = false;  // this plugin hooked the level build itself (the maze hook did not): written at load only

std::atomic<bool> g_live{false};     // every part is on and the hooks are in
std::atomic<bool> g_enabled{false};  // OutdoorBuild = 1
std::atomic<bool> g_probe{false};
std::atomic<int> g_slotLevel{-1};  // the slot level of the open map (Jungle's until a map is opened); -1 with no outdoor slot
// The two outdoor slots: index 0 is Outdoor (Jungle, LevelType 21), index 1 is OutdoorSnow (Snow, LevelType 31).
constexpr int SlotCount = 2;
std::atomic<int> g_slotLevels[SlotCount] = {-1, -1};
auto SlotIndexFor(std::string_view tileset) noexcept -> int {
	return EqualsNoCase(tileset, "Snow") ? 1 : 0;
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
};
std::mutex g_lock;
std::shared_ptr<const Job> g_job;
SlotGeometry g_geometry[SlotCount];
std::string g_slotRefusal[SlotCount];  // OutdoorSlotRefusal of each slot row; empty when the slot may be built
uint64_t g_nextJobId = 1;
co::PlanCache g_cache(4);
// What the last slot build left for the events and the probe (tiles, absolute). Guarded by g_lock; cleared when the game is left.
MazeEventRect g_eventRoom, g_bossRoom;
struct ArrivalFact {
	bool known = false;
	int32_t levelId = 0;
	int32_t cellX = 0, cellY = 0, cellW = 0, cellH = 0;  // the entrance cell
};
ArrivalFact g_arrival;
std::atomic<bool> g_arrivalLogged{false};
std::atomic<int> g_arrivalResult{0};  // OutdoorArrivalResult: what the arrival probe found (AutoCheck reads it); reset in ArrangeRooms and OutdoorGameLeft
cartographer::maze::SlotBuildState g_slotState;  // the slot level was built with a map open (or is being built) in this game (cleared on GameLeft)

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
	return false;
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
	return SlotIndexFor(tileset) == 1 ? SnowSlot : OutdoorSlot;
}

// The plan for the opened map, for this slot level. Null, with a reason, when the game must build its own level. May throw (strings, vectors).
auto PrepareSlot(const void* level, std::string* why) -> std::shared_ptr<const co::MapPlan> {
	std::shared_ptr<const Job> job;
	SlotGeometry geometry;
	std::string refusal;
	{
		std::lock_guard<std::mutex> lock(g_lock);
		job = g_job;
		if (job != nullptr) {
			geometry = g_geometry[SlotIndexFor(job->tileset)];
			refusal = g_slotRefusal[SlotIndexFor(job->tileset)];
		}
	}
	if (job == nullptr) {
		*why = "no outdoor map was opened in this game";
		return nullptr;
	}
	if (Read<int32_t>(level, LvlId) != g_slotLevels[SlotIndexFor(job->tileset)].load()) {
		*why = "this is not the slot level of a " + job->tileset + " map (the other outdoor slot's level)";
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
	return PlanForJob(*job, geometry, level, why);
}

// The plan for `job` on the slot geometry (cached by request). Null, with a reason, when the plan failed. `level` is only for the log line. May throw.
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
	bool built = false;
	std::shared_ptr<const co::MapPlan> plan = g_cache.get(request, co::builtinJungleCatalogue(), &built);
	if (!plan->ok) {
		*why = plan->reason;
		return nullptr;
	}
	if (built) {
		plog::Info("outdoor slot level %d: planned '%s' (%s %s %s, seed %llu): %d stamps, %d void cells, level %d x %d tiles at (%d,%d), plan %016llx", level != nullptr ? Read<int32_t>(level, LvlId) : g_slotLevel.load(),
		           job->name.c_str(), job->tileset.c_str(), job->shape.c_str(), job->size.c_str(), static_cast<unsigned long long>(job->seed), static_cast<int>(plan->stamps.stamps.size()),
		           static_cast<int>(plan->stamps.voidCells.size()), plan->stamps.tilesW, plan->stamps.tilesH, plan->stamps.originX, plan->stamps.originY,
		           static_cast<unsigned long long>(plan->hash));
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
		if (event.known) {
			g_eventRoom = event;  // a known room is never replaced by an unknown one
		}
		if (boss.known) {
			g_bossRoom = boss;
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
	{
		std::lock_guard<std::mutex> lock(g_lock);
		g_arrival = {true, id, plan.entrance.x, plan.entrance.y, plan.entrance.w, plan.entrance.h};
		g_arrivalLogged = false;
		g_arrivalResult = 0;
	}
	plog::Info("outdoor arrival: room %d of %d (at %d,%d) marked as the level entry, in the entrance cell (%d,%d); %d other room(s) lost the mark", mark.room, static_cast<int>(rooms.size()),
	           rects[static_cast<size_t>(mark.room)].x, rects[static_cast<size_t>(mark.room)].y, plan.entrance.x, plan.entrance.y, static_cast<int>(mark.clear.size()));
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
auto RunBuild(uint8_t dataFlag, void* level, LevelBuildFn original, const co::MapPlan& plan) noexcept -> uint64_t {
	const int id = Read<int32_t>(level, LvlId);
	const co::LevelRect rect = co::levelRectOf(plan.stamps);
	ProbeLevel(level, "before the slot build");
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
				plog::Info("outdoor slot level %d: built as planned (%d stamps, %d void cells; grid %d cells: %d pieces, %d covered, %d void, %d neither), %d rooms", id, ctx.stamped, ctx.voided,
				           counts.cells, counts.pieces, counts.covered, counts.voided, counts.other, Read<int32_t>(level, LvlRoomCount));
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
auto BuildSlotLevel(uint8_t dataFlag, void* level, LevelBuildFn original, uint64_t* result) noexcept -> bool {
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
		plan = PrepareSlot(level, &why);
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
	*result = RunBuild(dataFlag, level, original, *plan);  // from here the level is ours; `plan` keeps the cached plan alive for the build
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

auto OutdoorLevelBuild(uint8_t dataFlag, void* level, LevelBuildFn original, uint64_t* result) noexcept -> bool {
	if (!g_live.load(std::memory_order_relaxed) || !g_enabled.load(std::memory_order_relaxed) || original == nullptr || result == nullptr || !IsSlotLevel(level)) {
		return false;  // every other level runs the game's own build, untouched
	}
	bool hadJob = false;
	{
		std::lock_guard<std::mutex> lock(g_lock);
		hadJob = g_job != nullptr;
	}
	g_slotState.begin();
	const bool ours = BuildSlotLevel(dataFlag, level, original, result);
	g_slotState.end(hadJob);
	return ours;
}

auto OutdoorSlotBuilt() -> bool {
	return g_slotState.blocked();
}

auto OutdoorPlanProblem() -> std::string {
	try {
		std::shared_ptr<const Job> job;
		SlotGeometry geometry;
		std::string refusal;
		{
			std::lock_guard<std::mutex> lock(g_lock);
			job = g_job;
			if (job != nullptr) {
				geometry = g_geometry[SlotIndexFor(job->tileset)];
				refusal = g_slotRefusal[SlotIndexFor(job->tileset)];
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

auto OutdoorSlotRanAny() -> bool {
	return g_slotState.ranAny();
}

auto OutdoorOpenSeed() -> uint64_t {
	std::lock_guard<std::mutex> lock(g_lock);
	return g_job != nullptr ? g_job->seed : 0;
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

auto OutdoorEventRoom() -> MazeEventRect {
	std::lock_guard<std::mutex> lock(g_lock);
	return g_eventRoom;
}

auto OutdoorBossRoom() -> MazeEventRect {
	std::lock_guard<std::mutex> lock(g_lock);
	return g_bossRoom;
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
		fact = g_arrival;
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

void SetOutdoorTables(const std::shared_ptr<const Tables>& tables) {
	const Slot* found[SlotCount] = {nullptr, nullptr};
	if (tables != nullptr) {
		for (const Slot& s : tables->slots) {
			if (EqualsNoCase(s.name, OutdoorSlot)) {
				found[0] = &s;
			} else if (EqualsNoCase(s.name, SnowSlot)) {
				found[1] = &s;
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
				g_geometry[i] = {true, slot->offsetX, slot->offsetY, slot->sizeX, slot->sizeY};
			}
			g_slotLevels[i] = slot != nullptr ? slot->level : -1;
		}
	}
	g_cache.clear();
	g_slotLevel = found[0] != nullptr ? found[0]->level : (found[1] != nullptr ? found[1]->level : -1);
	g_enabled = tables != nullptr && tables->settings.outdoorBuild != 0;
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
	g_job = job;
	plog::Info("outdoor map '%s' (%s, %s, %s) opened with %sseed %llu", map.name.c_str(), map.tileset.c_str(), map.shape.c_str(), map.size.c_str(), fixedSeed != 0 ? "the host's " : "",
	           static_cast<unsigned long long>(job->seed));
}

auto OutdoorPendingTileset() -> std::string {
	std::lock_guard<std::mutex> lock(g_lock);
	return g_job != nullptr ? g_job->tileset : std::string();
}

void OutdoorMapCancelled() {
	std::lock_guard<std::mutex> lock(g_lock);
	g_job.reset();
}

void OutdoorGameLeft() {
	{
		std::lock_guard<std::mutex> lock(g_lock);
		g_job.reset();
		g_eventRoom = {};
		g_bossRoom = {};
		g_arrival = {};
		g_arrivalLogged = false;
		g_arrivalResult = 0;
	}
	g_cache.clear();
	g_slotState.clear();
}

void InstallOutdoorHooks(const D2RL::PluginContext* context, const std::vector<SignatureOverride>& overrides, const CodeRange* ranges, int count) {
	// The parts are found either way (for 'cartographer check'), but nothing is hooked unless the build is wanted: with OutdoorBuild = 0, or with no
	// Outdoor slot, the game runs exactly as without this code (no hook on the Act 5 builder, none added to the level build).
	for (int i = 0; i < OwnLevelBuild; ++i) {
		ResolvePart(context, overrides, ranges, count, g_parts[i]);
	}
	if (!g_enabled.load() || g_slotLevel.load() <= 0) {
		for (Part& part : g_parts) {
			if (part.status.on) {
				part.status.on = false;
				part.status.detail = "found, not hooked: the outdoor build is off";
			}
		}
		plog::Info(g_slotLevel.load() > 0 ? "outdoor build off (OutdoorBuild = 0); no outdoor hook installed. Restart the game to turn it on"
		                                 : "no Outdoor slot (slots.txt); no outdoor hook installed");
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
	plog::Info("the outdoor build hooks are in%s; the build itself is %s (OutdoorBuild in cartographer.txt)", g_ownLevelBuild ? " (with their own level build hook)" : " (behind the maze steering's level build hook)",
	           g_enabled.load() ? "on" : "off");
}

}  // namespace carto
