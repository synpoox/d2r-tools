// Cartographer maze steering core (phase 3a). Pure C++: no engine headers, no SDK, no floats, no <random>.
//
// It turns a layout graph (src/layout) plus a plain description of a maze tileset into everything the steering hooks
// have to feed D2R's own maze code, so the hooks themselves stay thin:
//   * the room walk 0x3e96d0 is replaced by the plan's `adds` (0x3e77a0 per tree edge) and `links` (0x360bb0 per loop
//     edge), then 0x3eb960 on every room; every room's piece is `pieceBase + exit mask`;
//   * the entrance and exit placer 0x3eb850 is steered by `specials`;
//   * the ring grower 0x3e9820 is a no-op for tilesets that call it (`ring`);
//   * the slot level's private LvlMaze row is `lvlMaze`;
//   * when the layout cannot fit the tileset, `steer` is false and `reason` says why in plain English: the hooks then
//     leave the level to D2R's own maze builder.
// Directions: the layout's own (mask bits W 1, E 2, S 4, N 8; x grows east, y grows south) and D2R's room placer order
// (0 W, 1 N, 2 E, 3 S), which `engineDir` converts.
// Determinism: the plan depends on (tileset, layout) only; refits use layout::splitmix64, never <random>.
#pragma once
#include <array>
#include <cstdint>
#include <string>
#include <vector>

#include "layout.h"

namespace cartographer::maze {

// D2R's room placer directions (0x3e77a0): 0 W, 1 N, 2 E, 3 S.
constexpr int kEngineW = 0, kEngineN = 1, kEngineE = 2, kEngineS = 3;
int engineDir(uint8_t maskBit);      // exit bit -> engine direction, or -1 for anything but one of the four bits
uint8_t maskBitOfEngineDir(int dir); // engine direction -> exit bit, or 0

// A tileset's one-exit special room pieces, by the side the room opens to: index 0 W, 1 E, 2 S, 3 N (the order of
// the LvlPrest rows "Prev W, Prev E, Prev S, Prev N", which is also the order of the layout's mask bits).
struct DeadEndFamily {
	bool present = false;
	std::array<int, 4> piece{};
};

// What the core needs to know about one maze tileset (LvlMaze / LvlPrest numbers, written down by the catalogue).
struct Tileset {
	std::string name;
	int levelType = 0;
	int roomTiles = 0;              // LvlMaze SizeX = SizeY: a room is roomTiles x roomTiles tiles
	int pieceBase = 0;              // a room's piece is pieceBase + exit mask (LvlPrest Def of "<tileset> W" is base + 1)
	uint16_t maskPieces = 0xFFFE;   // bit m set: the piece for exit mask m exists (m = 1..15)
	int levelTilesX = 200;          // the slot level's size in tiles; the first room is centred in it
	int levelTilesY = 200;
	bool steerable = true;          // false for tilesets that grow their own rooms (Arcane, Catacombs, tombs ...)
	std::string notSteerableReason; // plain English, shown when steerable is false
	bool callsRingGrower = false;   // the tileset's case in 0x3e69e0 calls 0x3e9820 first
	bool walkFollowsRing = true;    // false: no walk follows the ring grower, so the graph must be built inside it
	DeadEndFamily entrance;         // 0x3eb850's first group (e.g. "Prev"); absent: the entrance is an ordinary room
	DeadEndFamily exit;             // 0x3eb850's second group (e.g. "Down"); absent: the exit is an ordinary room
};

// Where D2R puts the first room: centred in the level. How many rooms fit on each side of it.
struct Capacity {
	int left = 0, right = 0, up = 0, down = 0;
	int across() const { return left + right + 1; }
	int tall() const { return up + down + 1; }
};
Capacity capacity(const Tileset &ts);

// One room of the plan. rooms[0] is the first room (made by D2R before the walk); the rest are in creation order,
// which is also the order of the room list D2R scans for 0x3eb850 (newest last here).
struct Room {
	layout::Point cell;       // layout coordinates
	int dx = 0, dy = 0;       // rooms east / south of the first room
	uint8_t mask = 0;         // exit mask: what 0x3eb960 will read from the room's links
	int piece = 0;            // pieceBase + mask, or the special piece for a placed entrance / exit
	layout::Role role = layout::Role::Empty;
	bool fixed = false;       // reserved (entrance, event, boss / exit): set the fixed flag after the last pick
	int special = -1;         // index into Plan::specials, or -1
};

struct AddStep { int parent = 0; int room = 0; int dir = 0; }; // 0x3e77a0(rooms[parent], dir, merge 0) makes rooms[room]
struct LinkStep { int a = 0; int b = 0; int dir = 0; };        // 0x360bb0: link rooms[a] to rooms[b] (b lies `dir` of a)

enum class SpecialKind : uint8_t { Entrance, Exit };

// One call of 0x3eb850 the tileset's case makes, in call order (entrance first, exit second).
struct SpecialCall {
	SpecialKind kind = SpecialKind::Entrance;
	int room = 0;        // index into Plan::rooms of the cell this call must turn into the special room
	bool place = false;  // false: suppress the call (no family, or the cell is not a dead end); the room stays ordinary
	int fromPiece = 0;   // pieceBase + mask: the table rotation whose entry[0] is this one is the one to use
	int toPiece = 0;     // the special piece (entry[1]) the room becomes
	std::string note;    // plain English, why it is not placed
};

struct LvlMazeRow { int rooms = 0; int sizeX = 0; int sizeY = 0; int merge = 0; }; // Rooms[3] all equal, merge 0

struct RingPlan {
	bool noOp = false;        // make 0x3e9820 do nothing for this level
	bool graphInRing = false; // no walk follows the ring grower: build the graph from inside it instead
};

struct Plan {
	bool steer = false;  // false: fall back, see reason
	std::string reason;  // plain English; empty when steer is true
	std::vector<Room> rooms;
	std::vector<AddStep> adds;   // tree edges, in order
	std::vector<LinkStep> links; // non-tree edges, after all adds
	std::vector<SpecialCall> specials;
	RingPlan ring;
	LvlMazeRow lvlMaze;
	int widthRooms = 0, heightRooms = 0; // bounding box of the layout
	int eventRoom = -1, bossRoom = -1;   // indexes into rooms of the reserved event and boss rooms, or -1
};

// The plan for a layout on a tileset. Always returns a plan: steer is false (with a reason) when the layout cannot
// be built on the tileset. This does not generate anything.
Plan planMaze(const Tileset &ts, const layout::Layout &layout);

// What a hook does with one call of 0x3eb850.
struct SpecialSteer {
	enum class Action : uint8_t { Redirect, Suppress } action = Action::Suppress;
	int fromPiece = 0; // Redirect: use the table entry whose entry[0] is this
};
// callIndex counts the level's calls of 0x3eb850 from 0; toPiece is the chosen group entry's entry[1]. A call the plan
// does not expect (or whose target does not match its family) is suppressed, so it cannot grow a stray room.
SpecialSteer steerSpecial(const Tileset &ts, const Plan &plan, int callIndex, int toPiece);

// The tileset's rules clamped to the rooms the level holds, and then a plan from a layout generated on them. When a
// layout does not fit, up to kRefits further layout seeds are tried (splitmix64(seed ^ (kRefitSalt + k))).
constexpr int kRefits = 8;
constexpr uint64_t kRefitSalt = 0x4D415A45ull; // "MAZE"

layout::Rules fitRules(const Tileset &ts, layout::Rules rules);

struct SeedPlan {
	layout::Layout layout;
	Plan plan;
	int refits = 0; // layouts tried beyond the first
};
SeedPlan planForSeed(const Tileset &ts, const layout::Rules &rules, uint64_t seed);

uint64_t planHash(const Plan &plan); // FNV-1a over the plan: stable across platforms

} // namespace cartographer::maze
