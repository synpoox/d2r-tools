// Cartographer maze steering, the pure part of the hooks (phase 3b). No engine headers, no SDK: the hooks in
// src/engine/maze_hooks.cpp only read and write D2R's memory and ask the functions here what to do, so every decision
// below is covered by tests/maze_tests.cpp.
#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <string_view>

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

} // namespace cartographer::maze
