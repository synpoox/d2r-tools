// Cartographer maze steering, the pure part of the hooks (phase 3b). No engine headers, no SDK: the hooks in
// src/engine/maze_hooks.cpp only read and write D2R's memory and ask the functions here what to do, so every decision
// below is covered by tests/maze_tests.cpp.
#pragma once
#include <array>
#include <atomic>
#include <cstdint>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "maze.h"

namespace cartographer::maze {

// The maze tilesets with numbers on record (LvlMaze / LvlPrest, build 93847): Cave and Crypt. Names ignore case.
bool builtinTileset(std::string_view name, Tileset &out);

// The tileset limited to the level the slot row declares (levels.txt SizeX / SizeY, as the game put them in the level
// record). The plan may ask for less than the row declares, never more: maxLevelTiles becomes the smaller declared side
// (and never more than kMaxLevelTiles). ok is false, with a reason, when the row declares less than one room.
struct SlotFit {
	bool ok = false;
	std::string reason;
	Tileset tileset;
};
SlotFit clampToSlot(Tileset tileset, int declaredX, int declaredY);

// The size a slot level declares (levels.txt SizeX / SizeY), recorded once per game, per level id, the first time the
// level is seen and before the hooks write anything to it. Every later plan is limited to this record, never to the
// level record's current size (the hooks overwrite that with the plan's size). Cleared when the game is left.
class DeclaredSizes {
public:
	struct Size {
		int x = 0, y = 0;
	};
	// The recorded size of `levelId`: stores (x, y) if none is recorded yet and returns it. A size with a side below 1 is not
	// recorded (a failed read) and is returned as given.
	Size record(int levelId, int x, int y);
	void clear() { sizes_.clear(); }

private:
	std::map<int, Size> sizes_;
};

// Whether a map can still be opened through the Maze slot. A build that ran while a map was open (a job existed) uses the slot
// up for the game; a build that ran with no map open (the game's lazy world lookup) does not, and neither refuses a later
// open on its own; a build still in progress refuses an open for as long as it runs. Cleared when the game is left.
class SlotBuildState {
public:
	void begin() { ++building_; }
	void end(bool hadJob)
	{
		if (hadJob) built_ = true;
		ran_ = true;
		--building_;
	}
	bool blocked() const { return built_.load() || building_.load() > 0; }
	// A build ran or is running, with a map open or not. A client that learns the host's map after this has the stock layout and takes no plan.
	bool ranAny() const { return ran_.load() || building_.load() > 0; }
	void clear()
	{
		built_ = false;
		ran_ = false;
		building_ = 0;
	}

private:
	std::atomic<bool> built_{false};
	std::atomic<bool> ran_{false};
	std::atomic<int> building_{0};
};

// Empty when a Maze slot level of `slotLevelType` (levels.txt LevelType: 3 Cave, 4 Crypt, -1 unknown) can build a map on
// `tileset`; otherwise the plain-English reason it cannot, naming `mapName`. A Maze slot is one LevelType, so one tileset.
std::string slotTypeProblem(const Tileset &tileset, int slotLevelType, std::string_view mapName);

// The plan for one opened map: the tileset, shape and size from maps.txt, the seed from the opening, and the level size
// the slot row declares. Pure and deterministic in its arguments, so the server and the client build the same maze from
// the same {map, seed}. steer is false, with a plain-English reason, whenever the hooks must leave the level to the game.
struct MapPlan {
	bool steer = false;
	std::string reason;
	Tileset tileset;      // clamped to the slot row
	SeedPlan seedPlan;
	uint64_t hash = 0;    // planHash of the plan: logged so two builds can be compared
};
MapPlan planForMap(std::string_view tileset, const layout::ShapeTable &shapes, std::string_view shape, std::string_view size,
                   uint64_t seed, int declaredX, int declaredY);

// Two numbers mixed into one seed (splitmix64), for the seed of an opening.
uint64_t mixSeed(uint64_t a, uint64_t b);

// --- 0x3eb850 ---------------------------------------------------------------------------------------------------

// One entry of the game's special-room table: {from piece, to piece, file, dir}. A group is the four entries the
// rotation counter walks through (N, E, S, W for Cave Prev); the call passes group[rot] and a pointer to rot.
struct TableEntry {
	int32_t from = 0, to = 0, file = 0, dir = 0;
};
using Group = std::array<TableEntry, 4>;

// The counter after a call of 0x3eb850 (its epilogue stores (rot + 1) mod 4): always 0..3.
inline uint32_t rotationAfter(uint32_t rot) { return (rot + 1u) & 3u; }

// True when `toPiece` is a Prev group piece of the tileset.
bool isPrevPiece(const Tileset &ts, int toPiece);

// What the hook does with one call. Original: pass the call to the game unchanged (the plan does not apply). Redirect:
// call the game with group[index] instead. Suppress: do not call the game; only advance the counter.
struct SpecialAnswer {
	enum class Kind : uint8_t { Original, Redirect, Suppress } kind = Kind::Original;
	int index = 0; // Redirect
};

// Per level build. The hook creates one when the build starts and calls answerSpecial for every call of 0x3eb850.
struct SpecialState {
	int prevCalls = 0;   // Prev group calls seen so far
	bool placed = false; // the entrance call was redirected
	std::string fault;   // plain English, set when a call could not be steered; empty when none
};

// rot: the counter as the call received it. group: the four entries around the call (null: not readable). The called
// entry is group[rot]. Anything that is not the entrance's own call is suppressed (steerSpecial); a Prev call whose
// group cannot be read or does not hold the plan's entry is suppressed too, with state.fault set, so the level is
// reported as a fallback instead of a wrong entrance.
SpecialAnswer answerSpecial(const Tileset &ts, const Plan &plan, SpecialState &state, const Group *group, uint32_t rot);

// After the slot's case returned: why the entrance was not placed, or empty when it was (or the plan places none).
std::string specialProblem(const Plan &plan, const SpecialState &state);

// --- the build --------------------------------------------------------------------------------------------------

// The private LvlMaze row of the slot level (7 ints: Level, Rooms[3], SizeX, SizeY, Merge). The level record keeps the
// pointer, so the hook keeps the storage for good.
std::array<int32_t, 7> lvlMazeRow(int levelId, const Plan &plan);

// The piece a room has after the walk (what 0x3eb960 picks: pieceBase + mask). The placed entrance has its ordinary
// piece then; 0x3eb850 turns it into the special one.
int pieceAfterWalk(const Plan &plan, int roomIndex);

// A reserved room is fixed after the walk (Room::Fix::AfterWalk); the placed entrance is not (ByPlacer).
bool fixAfterWalk(const Plan &plan, int roomIndex);

// --- the arrival room ---------------------------------------------------------------------------------------------

// The game's level-entry finder (0x3da650; docs/reverse-engineering/cartographer/05-maze-slot.md, "Arrival room: why the player
// arrives elsewhere") takes the first room of the level whose flags (room +0x50) hold any of kArrivalFlags. After the
// build the hook gives the room in the middle of the plan's entrance cell these bits and clears them on every other room,
// so the finder's first answer is the entrance whatever the tileset's own pieces carry.
constexpr uint32_t kArrivalFlags = 0x30000;

struct RoomRect {
	int32_t x = 0, y = 0, w = 0, h = 0;  // tiles
	uint32_t flags = 0;                  // room +0x50
};

struct ArrivalMark {
	bool mark = false;       // false: change nothing, `why` says why (the level keeps the game's own arrival)
	std::string why;
	int room = -1;           // index into the rects: the room to give kArrivalFlags
	std::vector<int> clear;  // indexes of the other rooms that carry any of kArrivalFlags
	int cellX = 0, cellY = 0;          // the entrance cell in the plan, cells from the plan's top-left cell
	int32_t originX = 0, originY = 0;  // the entrance cell's top-left corner, tiles
	int32_t minX = 0, minY = 0;        // the built rooms' box corner, tiles
};

// `rects` are the rooms the build made, `unit` the plan cell size in tiles (the tileset's roomTiles). The entrance cell is
// the plan's `entrance.room`, which every plan has, also the loops that place no special entrance piece. The built box
// must be the plan's (in cells) and the cell must hold a room; the room nearest the cell's centre is chosen.
ArrivalMark planArrival(const Plan &plan, int unit, const std::vector<RoomRect> &rects);

// The built room of one plan room (issue #184): the walk's record of a plan room is stale once the build splits the cell (the cell's
// top-left room, or freed memory read as (1,0) 0 x 0), so the room is found by its rect in the level's list, as the arrival is: the
// room with area whose corner is in the plan room's cell and whose centre is nearest the cell's centre. Rooms with no area are
// skipped everywhere, the box check included. room < 0, with `why`, when the built rooms do not span the plan's box or the
// cell holds no room.
struct CellRoom {
	int room = -1;  // index into the rects
	std::string why;
	int cellX = 0, cellY = 0;
	int32_t originX = 0, originY = 0, minX = 0, minY = 0;
};
CellRoom planCellRoom(const Plan &plan, int unit, int planRoom, const std::vector<RoomRect> &rects);

} // namespace cartographer::maze
