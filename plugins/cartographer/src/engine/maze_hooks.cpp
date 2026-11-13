#include "maze_hooks.h"

#include "core/part_signatures.h"
#include "automap_reset.h"
#include "core/text.h"
#include "log.h"
#include "hookguard.h"
#include "outdoor_hooks.h"
#include "steer.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstring>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

namespace carto {

namespace {

namespace cm = cartographer::maze;

// --- Engine facts. Offsets are into D2R's own structures (build 93847); each is recorded with its evidence in
// docs/reverse-engineering/cartographer.md (index): sections 1, 1a and 5. The functions are found by signature, never by address.
constexpr int LvlType = 0x00;         // int32: 1 maze, 2 preset, 3 outdoor
constexpr int LvlRoomCount = 0x08;    // int32
constexpr int LvlFirstRoom = 0x10;    // room*: the newest room (0x360ba0 adds at the head), next at RoomNext
constexpr int LvlMazeRow = 0x18;      // int32[7]*: the LvlMaze row {Level, Rooms[3], SizeX, SizeY, Merge}
constexpr int LvlOriginX = 0x24;      // int32 (never written here)
constexpr int LvlOriginY = 0x28;
constexpr int LvlSizeX = 0x2c;        // int32: the declared size, tiles
constexpr int LvlSizeY = 0x30;
constexpr int LvlNext = 0x1b8;        // level*: the next level in the game's list
constexpr int LvlDrlg = 0x1c8;        // drlg*: its level list head is at DrlgLevelList
constexpr int LvlTileType = 0x1e0;    // int32: the LevelType the maze case dispatches on (3 Cave, 4 Crypt)
constexpr int LvlSeed = 0x1e4;        // uint32
constexpr int LvlId = 0x1f8;          // int32
constexpr int DrlgLevelList = 0x868;
constexpr int ServerRoomDrlg = 0x18;  // a server room's DRLG room (UnitRoom's result; section 5, "Arrival room")
constexpr int RoomPiece = 0x40;       // piece*: {id +0, file +4, flags +0x10}
constexpr int RoomNext = 0x48;
constexpr int RoomFlags = 0x50;       // uint32; bits 16 and 17 (cm::kArrivalFlags) make the level-entry finder pick the room
constexpr int RoomX = 0x60, RoomY = 0x64, RoomW = 0x68, RoomH = 0x6c;
constexpr int PieceId = 0x00, PieceFlags = 0x10;
constexpr uint32_t PieceFixed = 2;
constexpr int32_t MazeType = 1;
constexpr int MaxRooms = 4096;        // a sanity bound for the walks of the room list (the probe's and the event room's)
constexpr int MaxLevels = 400;        // the same for the level list

using WalkFn = void (*)(void* level) noexcept;
using SpecialFn = void (*)(void* level, int32_t* entry, uint32_t* rot) noexcept;
using RingFn = void (*)(void* level, int32_t n) noexcept;
using AddRoomFn = void* (*)(void* room, int32_t dir, int32_t merge) noexcept;
using LinkFn = uint64_t (*)(void* a, void* b, int32_t dir) noexcept;
using PickFn = uint64_t (*)(void* room, int32_t clearFixed) noexcept;

enum MazePartId { LevelBuild, MazeWalk, MazeEntranceExit, MazeRingGrower, MazeAddRoom, MazeLinkRooms, MazePiecePicker,
                  // Only checked, never called or hooked: the level-entry finder, its first-room-with-flags helper and the flag test. They gate
                  // MarkArrival alone; a missing one leaves the arrival to the game and does not turn the steering off.
                  ArrivalFinder, ArrivalRoomFinder, ArrivalFlagTest, MazePartCount };
constexpr int MazeSteerParts = ArrivalFinder;  // the parts the steering needs

Part g_parts[MazePartCount] = {
	{"LevelBuild", sig::LevelBuild, 14, true, nullptr, {}, {}},
	{"MazeWalk", sig::MazeWalk, 18, true, nullptr, {}, {}},
	{"MazeEntranceExit", sig::MazeEntranceExit, 17, true, nullptr, {}, {}},
	{"MazeRingGrower", sig::MazeRingGrower, 14, true, nullptr, {}, {}},
	{"MazeAddRoom", sig::MazeAddRoom, 0, true, nullptr, {}, {}},
	{"MazeLinkRooms", sig::MazeLinkRooms, 0, true, nullptr, {}, {}},
	{"MazePiecePicker", sig::MazePiecePicker, 0, true, nullptr, {}, {}},
	{"ArrivalFinder", sig::ArrivalFinder, 0, false, nullptr, {}, {}},
	{"ArrivalRoomFinder", sig::ArrivalRoomFinder, 0, false, nullptr, {}, {}},
	{"ArrivalFlagTest", sig::ArrivalFlagTest, 0, false, nullptr, {}, {}},
};

LevelBuildFn g_levelOriginal = nullptr;
std::atomic<bool> g_levelBuildTried{false};  // InstallMazeHooks reached the level build hook (it may still have been refused)
WalkFn g_walkOriginal = nullptr;
SpecialFn g_specialOriginal = nullptr;
RingFn g_ringOriginal = nullptr;

std::atomic<bool> g_live{false};
std::atomic<int> g_slotLevel{-1};
std::atomic<int> g_dataFlag{-1};  // the data flag the level build was last called with in this game (-1: none yet); on a remote client every build is its own
std::atomic<bool> g_probe{false};
cm::SlotBuildState g_slotState;  // a map's slot build is running or has run in this game (cleared on GameLeft)

// The map that was opened, and the plan for it. Guarded by g_lock.
struct Job {
	uint64_t id = 0;
	std::string name, tileset, shape, size;
	uint64_t seed = 0;
	cartographer::layout::ShapeTable shapes;
};
struct Prepared {
	std::shared_ptr<const Job> job;
	std::shared_ptr<const cm::MapPlan> plan;
	const int32_t* row = nullptr;  // the private LvlMaze row; never freed, because the level record keeps the pointer
};
std::mutex g_lock;
std::shared_ptr<const Job> g_job;
Prepared g_cache;
uint64_t g_nextJobId = 1;
std::atomic<bool> g_arrivalLogged{false};
std::atomic<int> g_arrivalResult{0};  // MazeArrival: what the arrival probe found (AutoCheck reads it)
// What the slot level held when the hooks first saw it in this game, before any write: the declared size (the limit every
// plan uses) and the game's own LvlMaze row. Keyed by level id, cleared on GameLeft. Guarded by g_lock.
cm::DeclaredSizes g_declared;
std::map<int, const int32_t*> g_ownRows;
struct SlotOriginal {
	int sizeX = 0, sizeY = 0;
	const int32_t* row = nullptr;
};
struct Entrance {
	bool known = false;       // exactly one plan cell holds an entrance room
	int32_t x = 0, y = 0;     // that cell's top-left corner, tiles (the last one seen when not known)
	int32_t levelId = 0;
	int32_t minX = 0, minY = 0, unit = 0;  // the rooms' box corner and the plan cell size, tiles: a room's cell = (pos - min) / unit
};
Entrance g_entrance;  // guarded by g_lock
MazeEventRect g_eventRoom;  // guarded by g_lock
MazeEventRect g_bossRoom;   // guarded by g_lock

// One build of the slot level, on the thread that runs it. The walk, special-room and ring hooks read it; they only act
// while it is set and the level is the one being built.
struct BuildCtx {
	void* level = nullptr;
	SlotOriginal orig;
	Prepared prep;
	bool steer = false;
	bool fellBack = false;
	std::string why;
	cm::SpecialState special;
	std::vector<void*> rooms;
};
thread_local BuildCtx* t_ctx = nullptr;

template <typename Fn>
auto Call(MazePartId id) noexcept -> Fn {
	return reinterpret_cast<Fn>(const_cast<uint8_t*>(g_parts[id].address));
}

// --- Reading the game's memory (probes and sanity checks only) -----------------------------------------------------

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

// --- Probes (MazeProbe = 1 in cartographer.txt): what the game does, for the checks that need a game ------------------

void ProbeLevel(const void* level, const char* when) noexcept {
	if (!g_probe.load()) {
		return;
	}
	plog::Info("[maze probe] %s: level %d type %d levelType %d origin (%d,%d) size (%d,%d) rooms %d seed %08x lvlmaze row %p", when, Read<int32_t>(level, LvlId),
	           Read<int32_t>(level, LvlType), Read<int32_t>(level, LvlTileType), Read<int32_t>(level, LvlOriginX), Read<int32_t>(level, LvlOriginY),
	           Read<int32_t>(level, LvlSizeX), Read<int32_t>(level, LvlSizeY), Read<int32_t>(level, LvlRoomCount), Read<uint32_t>(level, LvlSeed),
	           Read<const void*>(level, LvlMazeRow));
}

// Every level in the game's list: id, origin and size (do the slot level's rect and the others overlap?).
void ProbeLevelList(const void* level) noexcept {
	if (!g_probe.load()) {
		return;
	}
	const void* drlg = Read<const void*>(level, LvlDrlg);
	if (drlg == nullptr) {
		plog::Info("[maze probe] level list: the level has no drlg pointer");
		return;
	}
	const void* node = Read<const void*>(drlg, DrlgLevelList);
	char line[800];
	size_t used = 0;
	int count = 0;
	line[0] = '\0';
	for (int i = 0; node != nullptr && i < MaxLevels && Readable(node, 0x200); ++i, node = Read<const void*>(node, LvlNext)) {
		char one[80];
		const int n = std::snprintf(one, sizeof(one), " %d:(%d,%d)+(%d,%d)", Read<int32_t>(node, LvlId), Read<int32_t>(node, LvlOriginX), Read<int32_t>(node, LvlOriginY),
		                            Read<int32_t>(node, LvlSizeX), Read<int32_t>(node, LvlSizeY));
		if (n > 0 && used + static_cast<size_t>(n) < sizeof(line) - 1) {
			std::memcpy(line + used, one, static_cast<size_t>(n) + 1);
			used += static_cast<size_t>(n);
		} else {
			plog::Info("[maze probe] levels (id:origin+size)%s", line);
			used = 0;
			line[0] = '\0';
			if (n > 0) {
				std::memcpy(line, one, static_cast<size_t>(n) + 1);
				used = static_cast<size_t>(n);
			}
		}
		++count;
	}
	if (used > 0) {
		plog::Info("[maze probe] levels (id:origin+size)%s", line);
	}
	plog::Info("[maze probe] level list: %d levels", count);
}

// The engine's rooms and the plan's cells as two character grids (one character per plan cell, each holding 3 x 3 engine rooms;
// '#' room, 'E' entrance), so the built shape can be compared with the planned one by eye, and the engine's room sizes (is a
// room the plan's room size?).
void ProbeShape(const void* level, const cm::Tileset& ts, const cm::Plan& plan, const char* when, int32_t minX, int32_t minY) noexcept {
	constexpr int GridMax = 40;
	const int unit = ts.roomTiles > 0 ? ts.roomTiles : 24;
	char built[GridMax][GridMax + 1] = {};
	char planned[GridMax][GridMax + 1] = {};
	for (int y = 0; y < GridMax; ++y) {
		std::memset(built[y], '.', GridMax);
		std::memset(planned[y], '.', GridMax);
	}
	int sizes[4][3] = {};  // distinct (w, h) and how many rooms have it
	int distinct = 0, outside = 0, withPiece = 0, count = 0, fixedRooms = 0, sharedPieces = 0;
	// The pieces the rooms point at, to find rooms that share one: collected here, sorted and counted after the walk (a
	// thread_local buffer, not the hook's stack).
	thread_local static const void* pieces[MaxRooms];
	int pieceCount = 0;
	const void* room = Read<const void*>(level, LvlFirstRoom);
	for (; room != nullptr && count < MaxRooms && Readable(room, 0x98); room = Read<const void*>(room, RoomNext), ++count) {
		const int32_t w = Read<int32_t>(room, RoomW), h = Read<int32_t>(room, RoomH);
		int k = 0;
		while (k < distinct && !(sizes[k][0] == w && sizes[k][1] == h)) {
			++k;
		}
		if (k == distinct && distinct < 4) {
			sizes[distinct][0] = w;
			sizes[distinct][1] = h;
			++distinct;
		}
		if (k < distinct) {
			++sizes[k][2];
		}
		const int cx = (Read<int32_t>(room, RoomX) - minX) / unit, cy = (Read<int32_t>(room, RoomY) - minY) / unit;
		const void* piece = Read<const void*>(room, RoomPiece);
		withPiece += piece != nullptr ? 1 : 0;
		if (piece != nullptr) {
			fixedRooms += (Read<uint32_t>(piece, PieceFlags) & PieceFixed) != 0 ? 1 : 0;
			pieces[pieceCount++] = piece;  // pieceCount <= count < MaxRooms
		}
		if (cx < 0 || cy < 0 || cx >= GridMax || cy >= GridMax) {
			++outside;
			continue;
		}
		built[cy][cx] = piece != nullptr && cm::isPrevPiece(ts, Read<int32_t>(piece, PieceId)) ? 'E' : (built[cy][cx] == 'E' ? 'E' : '#');
	}
	std::sort(pieces, pieces + pieceCount);
	const int seenCount = static_cast<int>(std::unique(pieces, pieces + pieceCount) - pieces);
	sharedPieces = pieceCount - seenCount;
	int pMinX = 1 << 30, pMinY = 1 << 30;
	for (const cm::Room& r : plan.rooms) {
		pMinX = std::min(pMinX, r.dx);
		pMinY = std::min(pMinY, r.dy);
	}
	for (size_t i = 0; i < plan.rooms.size(); ++i) {
		const int cx = plan.rooms[i].dx - pMinX, cy = plan.rooms[i].dy - pMinY;
		if (cx >= 0 && cy >= 0 && cx < GridMax && cy < GridMax) {
			planned[cy][cx] = static_cast<int>(i) == plan.entrance.room && plan.entrance.place ? 'E' : '#';
		}
	}
	char sz[160];
	size_t used = 0;
	sz[0] = '\0';
	for (int k = 0; k < distinct && used + 40 < sizeof(sz); ++k) {
		const int n = std::snprintf(sz + used, sizeof(sz) - used, " %dx%d:%d", sizes[k][0], sizes[k][1], sizes[k][2]);
		used += n > 0 ? static_cast<size_t>(n) : 0;
	}
	plog::Info("[maze probe] %s: room sizes (w x h : rooms)%s; rooms with a piece %d (distinct piece structs %d, rooms sharing one %d, Fixed flag set on %d), outside the grid %d; one character below = %d tiles", when, sz, withPiece, seenCount, sharedPieces, fixedRooms, outside, unit);
	for (int pass = 0; pass < 2; ++pass) {
		char (*grid)[GridMax + 1] = pass == 0 ? built : planned;
		int rows = 0, cols = 0;
		for (int y = 0; y < GridMax; ++y) {
			for (int x = 0; x < GridMax; ++x) {
				if (grid[y][x] != '.') {
					rows = std::max(rows, y + 1);
					cols = std::max(cols, x + 1);
				}
			}
		}
		plog::Info("[maze probe] %s: %s grid %d x %d", when, pass == 0 ? "built" : "planned", cols, rows);
		for (int y = 0; y < rows; ++y) {
			grid[y][cols] = '\0';
			plog::Info("[maze probe] %s: %s %s", when, pass == 0 ? "built  " : "planned", grid[y]);
		}
	}
}

// The rooms the walk made: how many, their bounding box, and the entrance room (a Prev piece).
void ProbeRooms(const void* level, const cm::Tileset& ts, const cm::Plan& plan, const char* when) noexcept {
	const void* room = Read<const void*>(level, LvlFirstRoom);
	int count = 0;
	int32_t minX = 0, minY = 0, maxX = 0, maxY = 0;
	for (; room != nullptr && count < MaxRooms && Readable(room, 0x98); room = Read<const void*>(room, RoomNext)) {
		const int32_t x = Read<int32_t>(room, RoomX), y = Read<int32_t>(room, RoomY), w = Read<int32_t>(room, RoomW), h = Read<int32_t>(room, RoomH);
		if (count == 0) {
			minX = x;
			minY = y;
			maxX = x + w;
			maxY = y + h;
		} else {
			minX = std::min(minX, x);
			minY = std::min(minY, y);
			maxX = std::max(maxX, x + w);
			maxY = std::max(maxY, y + h);
		}
		++count;
	}
	// The entrance is counted once per plan cell: a cell becomes 3 x 3 engine rooms and the Prev piece sits on each of them. The
	// cells are known only once the box is, so this is a second walk of the same list.
	const int unit = ts.roomTiles > 0 ? ts.roomTiles : 24;
	constexpr int CellsKept = 8;
	int64_t cells[CellsKept] = {};
	int entranceRooms = 0, entranceCells = 0;
	int32_t entX = 0, entY = 0;
	room = Read<const void*>(level, LvlFirstRoom);
	for (int i = 0; i < count && room != nullptr && Readable(room, 0x98); ++i, room = Read<const void*>(room, RoomNext)) {
		const void* piece = Read<const void*>(room, RoomPiece);
		if (piece == nullptr || !cm::isPrevPiece(ts, Read<int32_t>(piece, PieceId))) {
			continue;
		}
		++entranceRooms;
		const int32_t cx = (Read<int32_t>(room, RoomX) - minX) / unit, cy = (Read<int32_t>(room, RoomY) - minY) / unit;
		const int64_t key = (static_cast<int64_t>(cx) << 32) | static_cast<uint32_t>(cy);
		bool seen = false;
		for (int k = 0; k < std::min(entranceCells, CellsKept) && !seen; ++k) {
			seen = cells[k] == key;
		}
		if (!seen) {
			if (entranceCells < CellsKept) {
				cells[entranceCells] = key;
			}
			++entranceCells;  // past CellsKept it can only be "more than one", which is all the test needs
		}
		entX = minX + cx * unit;
		entY = minY + cy * unit;
	}
	{
		std::lock_guard<std::mutex> lock(g_lock);
		g_entrance = {entranceCells == 1, entX, entY, Read<int32_t>(level, LvlId), minX, minY, unit};
	}
	if (!g_probe.load()) {
		return;
	}
	plog::Info("[maze probe] %s: %d rooms in the list (level count %d, plan %d), box (%d,%d)-(%d,%d) = %d x %d tiles, entrance: %d cell(s) (%d engine rooms with the Prev piece), cell at (%d,%d) size %d", when, count,
	           Read<int32_t>(level, LvlRoomCount), static_cast<int>(plan.rooms.size()), minX, minY, maxX, maxY, maxX - minX, maxY - minY, entranceCells, entranceRooms, entX, entY, unit);
	plog::Info("[maze probe] %s: level rect now origin (%d,%d) size (%d,%d); the plan asked for %d x %d", when, Read<int32_t>(level, LvlOriginX), Read<int32_t>(level, LvlOriginY),
	           Read<int32_t>(level, LvlSizeX), Read<int32_t>(level, LvlSizeY), plan.levelTilesX, plan.levelTilesY);
	ProbeShape(level, ts, plan, when, minX, minY);
}

// The arrival: the game's level-entry finder (0x3da650) takes the first room whose flags (+0x50) hold bit 16 or 17, and the
// maze pieces carry none, so the player lands wherever the finder's fallbacks put them (docs/reverse-engineering/cartographer.md
// section 5, "Arrival room: why the player arrives elsewhere"). Give the room in the middle of the planned entrance cell those
// bits and take them off every other room. Does nothing, and says why, when a guard part is missing or the rooms do not match the
// plan; the game then arrives as it always did.
// Not noexcept: it allocates, and a bad_alloc must reach the catch in LevelBuildHook instead of calling std::terminate.
void MarkArrival(const void* level, const cm::Plan& plan, int unit) {
	static const char* const names[] = {"ArrivalFinder", "ArrivalRoomFinder", "ArrivalFlagTest"};
	for (int i = ArrivalFinder; i <= ArrivalFlagTest; ++i) {
		if (!g_parts[i].status.on) {
			plog::Info("maze arrival: left to the game, because the part %s was not found", names[i - ArrivalFinder]);
			return;
		}
	}
	std::vector<void*> rooms;
	std::vector<cm::RoomRect> rects;
	for (void* room = Read<void*>(level, LvlFirstRoom); room != nullptr && rooms.size() < static_cast<size_t>(MaxRooms) && Readable(room, 0x98);
	     room = Read<void*>(room, RoomNext)) {
		rooms.push_back(room);
		rects.push_back({Read<int32_t>(room, RoomX), Read<int32_t>(room, RoomY), Read<int32_t>(room, RoomW), Read<int32_t>(room, RoomH),
		                 Read<uint32_t>(room, RoomFlags)});
	}
	const cm::ArrivalMark mark = cm::planArrival(plan, unit, rects);
	if (!mark.mark) {
		plog::Info("maze arrival: left to the game, because %s", mark.why.c_str());
		return;
	}
	const auto flagsOf = [&](int i) { return Read<uint32_t>(rooms[static_cast<size_t>(i)], RoomFlags); };
	Write<uint32_t>(rooms[static_cast<size_t>(mark.room)], RoomFlags, flagsOf(mark.room) | cm::kArrivalFlags);
	for (const int i : mark.clear) {
		Write<uint32_t>(rooms[static_cast<size_t>(i)], RoomFlags, flagsOf(i) & ~cm::kArrivalFlags);
	}
	{
		std::lock_guard<std::mutex> lock(g_lock);
		g_entrance = {true, mark.originX, mark.originY, Read<int32_t>(level, LvlId), mark.minX, mark.minY, unit};
	}
	plog::Info("maze arrival: room %d of %d (at %d,%d) marked as the level entry, in plan cell (%d,%d); %d other room(s) lost the mark", mark.room, static_cast<int>(rooms.size()),
	           rects[static_cast<size_t>(mark.room)].x, rects[static_cast<size_t>(mark.room)].y, mark.cellX, mark.cellY, static_cast<int>(mark.clear.size()));
}

// The plan's event room and boss room: the rect of the built DRLG room in each plan cell, which the event spawn compares with the room the
// game populates. ctx.rooms[plan room] is the walk's record of that room, but once the build splits the cell that record is stale: it is
// the cell's top-left 8 x 8 room, or no room at all (freed memory, read as (1,0) 0 x 0; issue #184, static from the dump plus the
// hell-check-2 log, see docs/reverse-engineering/cartographer/09-map-events.md). So each room is matched by rect in the level's list against its plan cell, as
// the arrival is. A room that cannot be found is refused here with the reason: the event then falls back to the distance rule and
// the map Boss is left off.
void RecordEventRoom(const void* level, const cm::Plan& plan, int unit) {
	std::vector<cm::RoomRect> rects;
	for (const void* room = Read<const void*>(level, LvlFirstRoom); room != nullptr && rects.size() < static_cast<size_t>(MaxRooms) && Readable(room, 0x98);
	     room = Read<const void*>(room, RoomNext)) {
		rects.push_back({Read<int32_t>(room, RoomX), Read<int32_t>(room, RoomY), Read<int32_t>(room, RoomW), Read<int32_t>(room, RoomH), 0});
	}
	const auto find = [&](int planRoom, const char* what, const char* instead) {
		MazeEventRect out;
		if (planRoom < 0) {
			plog::Info("eventprobe: the maze plan has no %s room", what);
			return out;
		}
		const cm::CellRoom c = cm::planCellRoom(plan, unit, planRoom, rects);
		if (c.room < 0) {
			plog::Warn("eventprobe: the maze plan's %s room (plan room %d) was not found: %s; %s", what, planRoom, c.why.c_str(), instead);
			return out;
		}
		const cm::RoomRect& r = rects[static_cast<size_t>(c.room)];
		out = {true, Read<int32_t>(level, LvlId), r.x, r.y, r.w, r.h};
		plog::Info("eventprobe: maze %s room (plan room %d, cell %d,%d) is the DRLG room at list index %d, level %d, rect (%d,%d) %d x %d", what, planRoom, c.cellX, c.cellY, c.room,
		           out.levelId, out.x, out.y, out.w, out.h);
		return out;
	};
	const MazeEventRect rect = find(plan.eventRoom, "event", "the event room is chosen by the distance rule instead");
	const MazeEventRect boss = find(plan.bossRoom, "boss", "the map Boss is left off");
	std::lock_guard<std::mutex> lock(g_lock);
	g_eventRoom = rect;
	g_bossRoom = boss;
}

// --- The plan --------------------------------------------------------------------------------------------------------

// The plan for the current map at the size this level declared when it was first seen in this game (`declX` x `declY`, from
// g_declared, never the level record's current size, which the hooks overwrite); cached per game, so the server's build and
// the client's build get the same plan. Empty, with a reason, when no map was opened.
auto Prepare(void* level, int declX, int declY, std::string* why) -> std::optional<Prepared> {
	std::shared_ptr<const Job> job;
	{
		std::lock_guard<std::mutex> lock(g_lock);
		job = g_job;
	}
	if (job == nullptr) {
		*why = "no maze map was opened in this game";
		return std::nullopt;
	}
	std::lock_guard<std::mutex> lock(g_lock);
	if (g_cache.job == job && g_cache.plan != nullptr) {
		const cm::Plan& cached = g_cache.plan->seedPlan.plan;
		if (g_cache.plan->steer && (cached.levelTilesX > declX || cached.levelTilesY > declY)) {
			*why = "the level declares " + std::to_string(declX) + " x " + std::to_string(declY) + " tiles, less than the plan's " + std::to_string(cached.levelTilesX) + " x " +
			       std::to_string(cached.levelTilesY);
			return std::nullopt;
		}
		return g_cache;
	}
	Prepared prep;
	prep.job = job;
	prep.plan = std::make_shared<const cm::MapPlan>(cm::planForMap(job->tileset, job->shapes, job->shape, job->size, job->seed, declX, declY));
	if (prep.plan->steer) {
		const auto row = cm::lvlMazeRow(Read<int32_t>(level, LvlId), prep.plan->seedPlan.plan);
		auto* copy = new int32_t[row.size()];  // deliberately never freed
		std::memcpy(copy, row.data(), sizeof(int32_t) * row.size());
		prep.row = copy;
	}
	g_cache = prep;
	return prep;
}

// --- The hooks -------------------------------------------------------------------------------------------------------

// Puts back what the slot level declared and its own LvlMaze row, so the game's walk (and any later unsteered build) runs
// against the level it was written for, not the plan's tight box and room count.
void RestoreSlot(void* level, const SlotOriginal& orig) noexcept {
	if (level == nullptr || orig.sizeX < 1) {
		return;
	}
	Write<int32_t>(level, LvlSizeX, orig.sizeX);
	Write<int32_t>(level, LvlSizeY, orig.sizeY);
	Write<const int32_t*>(level, LvlMazeRow, orig.row);
}

void Fallback(BuildCtx& ctx, const std::string& why) noexcept {
	ctx.steer = false;
	ctx.fellBack = true;
	RestoreSlot(ctx.level, ctx.orig);
	try {
		if (ctx.why.empty()) {
			ctx.why = why;
		}
	} catch (...) {  // out of memory: the fall back still happens; only the reason is lost from the summary
	}
	plog::Warn("maze slot level %d: falling back to the game's own maze build: %s", ctx.level != nullptr ? Read<int32_t>(ctx.level, LvlId) : -1, why.c_str());
}

// Replaces the walk: the plan's rooms in creation order, then its loop links, then the piece of every room.
// False (with ctx.why set by the caller from `why`) when the engine refuses a step; the caller then lets the game's walk finish.
auto SteeredWalk(void* level, BuildCtx& ctx, std::string* why) -> bool {  // may throw (strings, vectors); WalkHook catches
	const cm::Plan& plan = ctx.prep.plan->seedPlan.plan;
	void* first = Read<void*>(level, LvlFirstRoom);
	if (first == nullptr || Read<int32_t>(level, LvlRoomCount) != 1 || Read<void*>(first, RoomNext) != nullptr) {
		*why = "the level already holds " + std::to_string(Read<int32_t>(level, LvlRoomCount)) + " rooms before the walk, not just the first";
		return false;
	}
	const size_t n = plan.rooms.size();
	ctx.rooms.assign(n, nullptr);
	ctx.rooms[0] = first;
	const auto add = Call<AddRoomFn>(MazeAddRoom);
	for (const cm::AddStep& step : plan.adds) {
		if (step.parent < 0 || step.room <= 0 || static_cast<size_t>(step.parent) >= n || static_cast<size_t>(step.room) >= n || ctx.rooms[static_cast<size_t>(step.parent)] == nullptr) {
			*why = "the plan names a room that does not exist";
			return false;
		}
		void* made = add(ctx.rooms[static_cast<size_t>(step.parent)], step.dir, 0);
		if (made == nullptr) {
			*why = "the game refused room " + std::to_string(step.room) + " of " + std::to_string(n) + " (it does not fit the level as declared)";
			return false;
		}
		ctx.rooms[static_cast<size_t>(step.room)] = made;
	}
	const auto link = Call<LinkFn>(MazeLinkRooms);
	for (const cm::LinkStep& step : plan.links) {
		if (step.a < 0 || step.b < 0 || static_cast<size_t>(step.a) >= n || static_cast<size_t>(step.b) >= n || ctx.rooms[static_cast<size_t>(step.a)] == nullptr ||
		    ctx.rooms[static_cast<size_t>(step.b)] == nullptr) {
			*why = "the plan links a room that does not exist";
			return false;
		}
		(void)link(ctx.rooms[static_cast<size_t>(step.a)], ctx.rooms[static_cast<size_t>(step.b)], step.dir);
	}
	const auto pick = Call<PickFn>(MazePiecePicker);
	for (size_t i = 0; i < n; ++i) {
		(void)pick(ctx.rooms[i], 1);
	}
	for (size_t i = 0; i < n; ++i) {
		if (!cm::fixAfterWalk(plan, static_cast<int>(i))) {
			continue;
		}
		uint8_t* piece = Read<uint8_t*>(ctx.rooms[i], RoomPiece);
		if (piece != nullptr && Readable(piece + PieceFlags, 4)) {
			Write<uint32_t>(piece, PieceFlags, Read<uint32_t>(piece, PieceFlags) | PieceFixed);
		}
	}
	return true;
}

void WalkHook(void* level) noexcept {
	if (g_walkOriginal == nullptr) {
		return;  // unreachable unless the loader gave no way back; there is nothing to call
	}
	BuildCtx* ctx = t_ctx;
	if (ctx == nullptr || !ctx->steer || ctx->fellBack || ctx->level != level) {
		g_walkOriginal(level);
		return;
	}
	std::string why;
	bool done = false;
	try {
		done = SteeredWalk(level, *ctx, &why);
	} catch (...) {
		try {
			why = "an exception while building the rooms";
		} catch (...) {  // out of memory: Fallback logs an empty reason
		}
	}
	if (!done) {
		Fallback(*ctx, why);
		g_walkOriginal(level);  // the game grows the rest of the maze from the rooms that exist
	}
}

void SpecialHook(void* level, int32_t* entry, uint32_t* rot) noexcept {
	if (g_specialOriginal == nullptr) {
		return;
	}
	BuildCtx* ctx = t_ctx;
	if (ctx == nullptr || !ctx->steer || ctx->fellBack || ctx->level != level) {
		g_specialOriginal(level, entry, rot);
		return;
	}
	const cm::Plan& plan = ctx->prep.plan->seedPlan.plan;
	const uint32_t before = rot != nullptr ? *rot : 0xffffffffu;
	// The four entries of the group around the call: the call passes group[rot] and rot. Only a readable table is used.
	cm::Group group{};
	const cm::Group* groupPtr = nullptr;
	const uint8_t* base = nullptr;
	if (rot != nullptr && before <= 3 && entry != nullptr) {
		base = reinterpret_cast<const uint8_t*>(entry) - static_cast<size_t>(before) * sizeof(cm::TableEntry);
		if (Readable(base, sizeof(group))) {
			std::memcpy(&group, base, sizeof(group));
			groupPtr = &group;
		}
	}
	cm::SpecialAnswer answer;
	try {
		answer = cm::answerSpecial(ctx->prep.plan->tileset, plan, ctx->special, groupPtr, before);
	} catch (...) {  // out of memory while writing the fault text: stop steering, the game's walk finishes
		ctx->steer = false;
		ctx->fellBack = true;
		RestoreSlot(level, ctx->orig);
		g_specialOriginal(level, entry, rot);
		return;
	}
	switch (answer.kind) {
	case cm::SpecialAnswer::Kind::Original:
		g_specialOriginal(level, entry, rot);
		break;
	case cm::SpecialAnswer::Kind::Redirect:
		*rot = static_cast<uint32_t>(answer.index);  // the game's own epilogue then stores index + 1 mod 4
		g_specialOriginal(level, reinterpret_cast<int32_t*>(const_cast<uint8_t*>(base) + static_cast<size_t>(answer.index) * sizeof(cm::TableEntry)), rot);
		*rot = cm::rotationAfter(static_cast<uint32_t>(answer.index));
		break;
	case cm::SpecialAnswer::Kind::Suppress:
		if (rot != nullptr) {
			*rot = cm::rotationAfter(before <= 3 ? before : 0);  // what the game's epilogue would store: stays in 0..3
		}
		break;
	}
	if (g_probe.load()) {
		char index[16] = "";
		if (answer.kind == cm::SpecialAnswer::Kind::Redirect) {
			std::snprintf(index, sizeof(index), "%d", answer.index);
		}
		const char* what = answer.kind == cm::SpecialAnswer::Kind::Original ? "passed" : answer.kind == cm::SpecialAnswer::Kind::Redirect ? "redirected" : "suppressed";
		plog::Info("[maze probe] special-room call %s: entry {%d,%d,%d,%d} rot %u -> %u%s%s", what, entry != nullptr ? entry[0] : -1, entry != nullptr ? entry[1] : -1,
		           entry != nullptr ? entry[2] : -1, entry != nullptr ? entry[3] : -1, before, rot != nullptr ? *rot : 0xffffffffu,
		           answer.kind == cm::SpecialAnswer::Kind::Redirect ? ", plan entry index " : "", index);
	}
}

void RingHook(void* level, int32_t n) noexcept {
	if (g_ringOriginal == nullptr) {
		return;
	}
	BuildCtx* ctx = t_ctx;
	if (ctx != nullptr && ctx->steer && !ctx->fellBack && ctx->level == level && ctx->prep.plan->seedPlan.plan.ring.noOp) {
		if (g_probe.load()) {
			plog::Info("[maze probe] ring grower skipped (n %d)", n);
		}
		return;
	}
	g_ringOriginal(level, n);
}

auto IsSlotLevel(const void* level) noexcept -> bool {
	const int slot = g_slotLevel.load();
	return slot > 0 && level != nullptr && Readable(level, 0x200) && Read<int32_t>(level, LvlId) == slot;
}

auto LevelBuildHook(uint8_t dataFlag, void* level) noexcept -> uint64_t {
	if (g_levelOriginal == nullptr) {
		return 0;  // the loader gave no way back (logged at install): every level build is skipped until a restart
	}
	g_dataFlag = dataFlag;  // read by the map sync's client apply (MazeLevelDataFlag); the game's behaviour is unchanged
	{
		// The outdoor build shares this hook (the level build is hooked once): it builds the Outdoor slot level, if it is on, and no other level.
		uint64_t outdoorResult = 0;
		if (OutdoorLevelBuild(dataFlag, level, g_levelOriginal, &outdoorResult)) {
			return outdoorResult;
		}
	}
	if (!g_live.load() || !IsSlotLevel(level)) {
		return g_levelOriginal(dataFlag, level);  // every other level runs the game's own build, untouched
	}
	BuildCtx ctx;
	ctx.level = level;
	const int id = Read<int32_t>(level, LvlId);
	if (Read<int32_t>(level, LvlType) != MazeType) {
		// Not a maze level (levels.txt DrlgType must be 1): nothing of ours is recorded or written to it.
		plog::Warn("maze slot level %d: not a maze level (levels.txt DrlgType must be 1); building the game's own level", id);
		return g_levelOriginal(dataFlag, level);
	}
	ProbeLevel(level, "before the slot build");
	ProbeLevelList(level);
	bool hadJob = false;
	try {
		{
			// The first time this level is seen in the game, nothing of ours has touched it: record what it declares.
			std::lock_guard<std::mutex> lock(g_lock);
			hadJob = g_job != nullptr;
			// Marked as running in the same hold that reads the job, so a remote apply (MazeRemoteMapOpened) sees this build or this build sees its job.
			g_slotState.begin();
			const cm::DeclaredSizes::Size size = g_declared.record(id, Read<int32_t>(level, LvlSizeX), Read<int32_t>(level, LvlSizeY));
			ctx.orig.sizeX = size.x;
			ctx.orig.sizeY = size.y;
			ctx.orig.row = g_ownRows.emplace(id, Read<const int32_t*>(level, LvlMazeRow)).first->second;
		}
		std::string why;
		if (const std::optional<Prepared> prep = Prepare(level, ctx.orig.sizeX, ctx.orig.sizeY, &why)) {
			ctx.prep = *prep;
			if (!prep->plan->steer) {
				why = prep->plan->reason;
			} else if (Read<int32_t>(level, LvlTileType) != prep->plan->tileset.levelType) {
				why = "the Maze slot level's LevelType is " + std::to_string(Read<int32_t>(level, LvlTileType)) + " but a " + prep->plan->tileset.name + " map needs " +
				      std::to_string(prep->plan->tileset.levelType) + " (levels.txt LevelType)";
			} else {
				ctx.steer = true;
			}
		}
		if (!ctx.steer) {
			// A cancelled or replaced job, or an earlier build, may have left the plan's size and private row on the level.
			RestoreSlot(level, ctx.orig);
			plog::Warn("maze slot level %d: building the game's own maze: %s", id, why.c_str());
		} else {
			const cm::Plan& plan = ctx.prep.plan->seedPlan.plan;
			// The size goes in before the original runs (the first room is centred in it and the span checks read it), and
			// never the origin: a world lookup can reach this level from elsewhere (docs/reverse-engineering/cartographer/05-maze-slot.md Q2).
			Write<int32_t>(level, LvlSizeX, plan.levelTilesX);
			Write<int32_t>(level, LvlSizeY, plan.levelTilesY);
			Write<const int32_t*>(level, LvlMazeRow, ctx.prep.row);
			AutomapResetArm();  // the new map starts with an empty automap: skip the read of an earlier map's revealed cells in this layer
			plog::Info("maze slot level %d: steering '%s' (%s %s %s, seed %llu): %d rooms, level %d x %d tiles, plan %016llx, %d refits", id, ctx.prep.job->name.c_str(),
			           ctx.prep.plan->tileset.name.c_str(), ctx.prep.job->shape.c_str(), ctx.prep.job->size.c_str(), static_cast<unsigned long long>(ctx.prep.job->seed),
			           static_cast<int>(plan.rooms.size()), plan.levelTilesX, plan.levelTilesY, static_cast<unsigned long long>(ctx.prep.plan->hash), ctx.prep.plan->seedPlan.refits);
		}
	} catch (...) {
		ctx.steer = false;
		RestoreSlot(level, ctx.orig);  // a no-op until the declared size was recorded
		plog::Error("maze slot level %d: an exception while preparing the plan; building the game's own maze", id);
	}
	BuildCtx* previous = t_ctx;
	t_ctx = ctx.steer ? &ctx : nullptr;
	const uint64_t result = g_levelOriginal(dataFlag, level);
	g_slotState.end(hadJob);
	t_ctx = previous;
	if (ctx.steer) {
		try {
			const cm::Plan& plan = ctx.prep.plan->seedPlan.plan;
			const std::string problem = ctx.fellBack ? ctx.why : cm::specialProblem(plan, ctx.special);
			if (!problem.empty()) {
				plog::Warn("maze slot level %d: %s%s", id, ctx.fellBack ? "built partly by the game's own walk: " : "the entrance was not placed as planned: ", problem.c_str());
			} else {
				plog::Info("maze slot level %d: built as planned (%d rooms, entrance %s)", id, static_cast<int>(plan.rooms.size()), plan.entrance.place ? "placed" : "an ordinary room");
			}
			ProbeRooms(level, ctx.prep.plan->tileset, plan, "after the slot build");
			if (!ctx.fellBack && problem.empty()) {
				MarkArrival(level, plan, ctx.prep.plan->tileset.roomTiles > 0 ? ctx.prep.plan->tileset.roomTiles : 24);
			} else {
				plog::Info("maze arrival: left to the game, because the build is not the plan");
			}
			RecordEventRoom(level, plan, ctx.prep.plan->tileset.roomTiles > 0 ? ctx.prep.plan->tileset.roomTiles : 24);
		} catch (...) {
		}
	}
	return result;
}

}  // namespace

auto MazeLevelBuildHookTried() -> bool {
	return g_levelBuildTried.load();
}

auto MazeSteeringLive() -> bool {
	return g_live.load();
}

auto MazePartStatuses() -> std::vector<PartStatus> {
	std::vector<PartStatus> out;
	for (const Part& part : g_parts) {
		out.push_back(part.status);
	}
	out.push_back(AutomapResetPartStatus());
	return out;
}

auto MazeSlotLevelId() -> int {
	return g_slotLevel.load();
}

void SetMazeTables(const std::shared_ptr<const Tables>& tables) {
	g_slotLevel = tables != nullptr ? SlotLevel(tables->slots, MazeSlot) : -1;
	g_probe = tables != nullptr && (tables->settings.mazeProbe != 0 || tables->settings.autoCheck != 0);  // AutoCheck reads the arrival probe
}

namespace {

// Makes `map` the map the next slot build uses. `seedSalt` is mixed with the job id into the layout seed; when `fixedSeed` is non-zero it is
// the seed as it is (a client takes the host's). With `requireNoBuild` the job is installed only if no slot build has started, tested under the
// same lock hold as the install; returns whether it was installed.
auto SetJob(const MapDef& map, const Tables& tables, uint64_t seedSalt, uint64_t fixedSeed, bool requireNoBuild) -> bool {
	auto job = std::make_shared<Job>();
	job->name = map.name;
	job->tileset = map.tileset;
	job->shape = map.shape;
	job->size = map.size;
	job->shapes = tables.shapes;
	std::lock_guard<std::mutex> lock(g_lock);
	if (requireNoBuild && g_slotState.ranAny()) {
		return false;
	}
	job->id = g_nextJobId++;
	job->seed = fixedSeed != 0 ? fixedSeed : cm::mixSeed(seedSalt, job->id);
	g_job = job;
	g_cache = {};
	g_entrance = {};
	g_eventRoom = {};
	g_bossRoom = {};
	g_arrivalLogged = false;
	g_arrivalResult = 0;
	plog::Info("maze map '%s' (%s, %s, %s) opened with %sseed %llu", map.name.c_str(), map.tileset.c_str(), map.shape.c_str(), map.size.c_str(), fixedSeed != 0 ? "the host's " : "",
	           static_cast<unsigned long long>(job->seed));
	return true;
}

}  // namespace

void MazeMapOpened(const MapDef& map, const Tables& tables, uint64_t seedSalt) {
	SetJob(map, tables, seedSalt, 0, false);
}

auto MazeRemoteMapOpened(const MapDef& map, const Tables& tables, uint64_t hostSeed) -> bool {
	if (hostSeed == 0 || !g_live.load()) {
		return false;  // no plan to take, or steering off
	}
	return SetJob(map, tables, 0, hostSeed, true);  // false when the slot level exists already: it is not rebuilt
}

auto MazeSlotRanAny() -> bool {
	return g_slotState.ranAny();
}

auto MazeLevelDataFlag() -> int {
	return g_dataFlag.load();
}

auto MazeOpenSeed() -> uint64_t {
	std::lock_guard<std::mutex> lock(g_lock);
	return g_job != nullptr ? g_job->seed : 0;
}

void MazeMapCancelled() {
	std::lock_guard<std::mutex> lock(g_lock);
	g_job.reset();
	g_cache = {};
}

void MazeGameLeft() {
	g_dataFlag = -1;
	std::lock_guard<std::mutex> lock(g_lock);
	g_job.reset();
	g_cache = {};
	g_declared.clear();
	g_ownRows.clear();
	g_slotState.clear();
	g_entrance = {};
	g_eventRoom = {};
	g_bossRoom = {};
	g_arrivalLogged = false;
	g_arrivalResult = 0;
}

auto LevelRoomRects(const void* level, std::vector<cartographer::eventplan::Rect>* out) noexcept -> bool {
	out->clear();
	if (level == nullptr) {
		return false;
	}
	const void* room = Read<const void*>(level, LvlFirstRoom);
	if (room == nullptr) {
		return false;
	}
	try {
		int count = 0;
		for (; room != nullptr; room = Read<const void*>(room, RoomNext), ++count) {
			if (count >= MaxRooms || !Readable(room, 0x98)) {
				out->clear();
				return false;
			}
			out->push_back({Read<int32_t>(room, RoomX), Read<int32_t>(room, RoomY), Read<int32_t>(room, RoomW), Read<int32_t>(room, RoomH)});
		}
	} catch (...) {
		out->clear();
		return false;
	}
	return !out->empty();
}

auto MazeEventRoom() -> MazeEventRect {
	std::lock_guard<std::mutex> lock(g_lock);
	return g_eventRoom;
}

auto MazeArrivalResult() -> int {
	return g_arrivalResult.load();
}

auto MazeBossRoom() -> MazeEventRect {
	std::lock_guard<std::mutex> lock(g_lock);
	return g_bossRoom;
}

auto MazeSlotBuilt() -> bool {
	return g_slotState.blocked();
}

auto MazeArrivalPending() -> bool {
	return g_probe.load(std::memory_order_relaxed) && !g_arrivalLogged.load(std::memory_order_relaxed) && g_slotLevel.load(std::memory_order_relaxed) > 0;
}

auto MapIsMaze(const MapDef& map) -> bool {
	cm::Tileset ts;
	return cm::builtinTileset(map.tileset, ts);
}

auto MazeTilesetProblem(const MapDef& map, const Tables& tables) -> std::string {
	cm::Tileset ts;
	if (!cm::builtinTileset(map.tileset, ts)) {
		return {};
	}
	for (const Slot& slot : tables.slots) {
		if (EqualsNoCase(slot.name, MazeSlot)) {
			return cm::slotTypeProblem(ts, slot.levelType, map.name);
		}
	}
	return {};  // no Maze slot: the "not installed" message says so
}

// `room` is what UnitRoom returned: the player's server room. Its DRLG room (+0x18) carries the position and size the build
// wrote (the server room's own +0x60.. are not a tile rect). Every pointer goes through Read, so a wrong layout logs zeros.
void MazeProbeArrival(void* room, int32_t levelId) {
	if (!g_probe.load() || room == nullptr || levelId != g_slotLevel.load() || g_arrivalLogged.exchange(true)) {
		return;
	}
	Entrance entrance;
	{
		std::lock_guard<std::mutex> lock(g_lock);
		entrance = g_entrance;
	}
	const void* drlg = Read<const void*>(room, ServerRoomDrlg);
	const int32_t x = Read<int32_t>(drlg, RoomX), y = Read<int32_t>(drlg, RoomY), w = Read<int32_t>(drlg, RoomW), h = Read<int32_t>(drlg, RoomH);
	if (drlg == nullptr || w <= 0 || h <= 0) {
		plog::Info("[maze probe] arrival: the player's server room %p has no readable DRLG room (%p)", room, drlg);
		return;
	}
	const bool haveCell = entrance.unit > 0 && entrance.levelId == levelId;
	// Integer division rounds toward zero, so a room left of or above the box would land in cell 0: say it is outside instead.
	const bool outsideBox = haveCell && (x < entrance.minX || y < entrance.minY);
	const bool inCell = haveCell && !outsideBox;
	const int32_t cellX = inCell ? entrance.minX + (x - entrance.minX) / entrance.unit * entrance.unit : 0;
	const int32_t cellY = inCell ? entrance.minY + (y - entrance.minY) / entrance.unit * entrance.unit : 0;
	char cell[64];
	if (outsideBox) {
		std::snprintf(cell, sizeof(cell), "outside the room box");
	} else {
		std::snprintf(cell, sizeof(cell), "(%d,%d)", cellX, cellY);
	}
	g_arrivalResult = entrance.known && inCell ? (cellX == entrance.x && cellY == entrance.y ? 1 : 2) : 3;
	plog::Info("[maze probe] arrival: the player is in DRLG room %p (server room %p) of level %d at (%d,%d) size %d x %d, in plan cell %s; the entrance cell is %s (%d,%d)%s", drlg,
	           room, levelId, x, y, w, h, cell, entrance.known ? "at" : "unknown, last seen at", entrance.x, entrance.y,
	           entrance.known && inCell ? (cellX == entrance.x && cellY == entrance.y ? ": ARRIVAL IS IN THE ENTRANCE CELL" : ": arrival is NOT in the entrance cell") : "");
}

void InstallMazeHooks(const D2RL::PluginContext* context, const std::vector<SignatureOverride>& overrides, const CodeRange* ranges, int count) {
	for (Part& part : g_parts) {
		ResolvePart(context, overrides, ranges, count, part);
	}
	bool allOn = true;
	for (int i = 0; i < MazeSteerParts; ++i) {
		allOn = allOn && g_parts[i].status.on;
	}
	if (!allOn) {
		// Nothing is hooked: one missing part turns only the maze steering off, and a half-installed set could not be taken out.
		for (Part& part : g_parts) {
			if (part.status.on) {
				part.status.on = false;
				part.status.detail = "off: not hooked, because another maze part was not found";
			}
		}
		plog::Warn("maze steering is off: not every maze part was found (see 'cartographer check'); Cave and Crypt maps are refused, the others still work");
		return;
	}
	const auto install = [&](MazePartId id, void* target, void** original) {
		Part& part = g_parts[id];
		if (id == LevelBuild) {
			g_levelBuildTried = true;
		}
		const uint64_t rva = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(part.address) - context->exeBase);
		const bool hooked = context->InstallInlineHook(rva, part.guard, part.hookSize, target, original);
		return HookOutcome(part, hooked, *original != nullptr);
	};
	// Inner hooks first; the level build, which switches the others on, last.
	bool ok = install(MazeRingGrower, reinterpret_cast<void*>(&RingHook), reinterpret_cast<void**>(&g_ringOriginal));
	ok = ok && install(MazeEntranceExit, reinterpret_cast<void*>(&SpecialHook), reinterpret_cast<void**>(&g_specialOriginal));
	ok = ok && install(MazeWalk, reinterpret_cast<void*>(&WalkHook), reinterpret_cast<void**>(&g_walkOriginal));
	ok = ok && install(LevelBuild, reinterpret_cast<void*>(&LevelBuildHook), reinterpret_cast<void**>(&g_levelOriginal));
	if (!ok) {
		plog::Warn("maze steering is off: a hook was refused (see 'cartographer check'); the hooks that went in pass straight through");
		return;
	}
	g_live = true;
}

}  // namespace carto
