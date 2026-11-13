// Cartographer outdoor Snow (Act 5 Barricade) plan. Pure C++: no engine headers, no SDK, no floats, no <random>.
//
// The Snow tileset is not a mask family like the Jungle: its 16 pieces (LvlPrest 987..1002, 16 x 16 tiles, LevelType 31) are fence-line
// tiles that join by the position of the fence where it crosses an edge, so the jungle picker (one piece per exit mask) cannot use them.
// A Snow map is a rectangle of 16-tile cells. Every line of the rectangle (a row for Horizontal, a column for Vertical) is a west cap,
// a chain of fence runs whose fence positions match their neighbours, and an east cap. The evidence for every rule is in
// docs/reverse-engineering/cartographer-snow.md.
//
// The plan is the same StampPlan the Jungle plan produces, so the outdoor build hook can replay it unchanged once it is wired. This file is
// not wired: no portal, table, installer or hook calls it, and it changes nothing in game.
//
// Not proven in game: that the engine accepts these ids in the outdoor stamper, that the fence positions line up on screen, what the empty
// side of a cap looks like next to a void cell and that the arrival cell is walkable (the doc's "Open" list).
#pragma once
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "stamp.h"

namespace cartographer::outdoor::snow {

constexpr int kLevelType = 31;     // lvltypes.txt "Act 5 - Barricade", shared by the dirt and the snow pieces
constexpr int kLevelDrlgType = 3;  // outdoor
constexpr int kCellTiles = 16;     // every Snow piece is 16 x 16 tiles
constexpr int kGridTiles = 8;      // the engine's grid cell
constexpr int kFirstLvlPrest = 987;
constexpr int kLastLvlPrest = 1002;
constexpr int kDirtOffset = 72;    // the dirt twin of a snow piece is lvlPrest - 72 (915..930); same fence grammar, noisier edges
constexpr int kMaxCells = 12;      // longest side of a map, in 16-tile cells (192 tiles)
constexpr int kMinRunCells = 3;    // a line needs a cap, a run piece and a cap
// The edge ring: every side of the fence rectangle gets kEdgeCells more cells of kEdgePiece, so the screen has ground to draw past the
// outer fence lines and the empty side of a cap, where it was black void (issue #395). docs/reverse-engineering/cartographer-snow.md, section 8.
constexpr int kEdgeCells = 1;
constexpr int kEdgePiece = 964;    // "Act 5 - Barricade Cliff Border 8 Snow": a 16 x 16 piece with a tile in every cell

// How a piece joins: Run pieces have a fence socket on both ends of their axis, Cap pieces on one end only (the other side has no tiles).
enum class Kind : uint8_t {
	RunH,      // fence runs west to east; sockets on W and E (A or B); N and S are open ground
	RunV,      // fence runs north to south; sockets on N and S (C or D); W and E are open ground
	CapEast,   // ends a horizontal line: socket on W (A or B), E side empty (the catalogue's "R" pieces)
	CapWest,   // starts a horizontal line: socket on E, W side empty ("F" pieces)
	CapSouth,  // ends a vertical line: socket on N (C or D), S side empty
	CapNorth,  // starts a vertical line: socket on S, N side empty
};
const char *kindName(Kind kind);

struct Piece {
	int lvlPrest;      // the snow piece, 987..1002
	const char *name;  // the lvlprest.txt name
	const char *ds1;   // first DS1 file, under data/global/tiles
	Kind kind;
	char start;        // socket at the west or north end, or '\0' when the piece has none there
	char end;          // socket at the east or south end, or '\0'
	int files;         // DS1 files the piece can draw: the smaller of the Files column and the files listed
	int dirtLvlPrest() const { return lvlPrest - kDirtOffset; }
};

// The 16 rows, in lvlPrest order.
const std::vector<Piece> &pieces();
const Piece *findPiece(int lvlPrest);

enum class Orientation : uint8_t { Horizontal, Vertical };
const char *orientationName(Orientation orientation);

struct SnowOptions {
	int originX = 0;       // the level's origin in tiles (the slot row's OffsetX / OffsetY)
	int originY = 0;
	int maxTilesX = 0;     // the slot row's declared size limit in tiles; 0 means no limit
	int maxTilesY = 0;
	bool pinFiles = false; // true: pass a drawn DS1 file index; false: -1, the engine draws it from the level RNG
};

struct SnowRequest {
	uint64_t seed = 0;
	int cellsW = 8;        // map size in 16-tile cells; 8 x 5 is the size of the game's own Frozen Tundra (128 x 80 tiles)
	int cellsH = 5;
	bool autoOrientation = true; // true: the seed picks Horizontal or Vertical; false: use `orientation`
	Orientation orientation = Orientation::Horizontal;
	SnowOptions options;
};

// ExtraSmall, Small, Medium or Large (blank means Medium), case-insensitive. False for any other word. 4 x 3, 6 x 4, 8 x 5 and 10 x 7 cells.
bool snowSizeCells(std::string_view size, int *cellsW, int *cellsH);

struct SnowPlan {
	bool ok = false;
	std::string reason;                 // plain English, when !ok: why the game must build its own level
	Orientation orientation = Orientation::Horizontal;
	int cellsW = 0;                     // in 16-tile cells
	int cellsH = 0;
	std::vector<int> grid;              // cellsW * cellsH piece ids, row-major (the fence rectangle, without the edge ring)
	StampPlan stamps;                   // complete only when ok; the same plan the Jungle builder makes. Holds the edge ring too: (cellsW + 2 * kEdgeCells) x (cellsH + 2 * kEdgeCells)
	                                    // stamps, row-major, starting at macro cell (-kEdgeCells, -kEdgeCells)
	uint64_t hash = 0;                  // stampPlanHash of the stamps
};

// Plans a Snow map for a seed: resolves the orientation, builds every line, and checks the result with checkSnowPlan. Any failure leaves ok
// false with the reason. Never throws for bad input.
SnowPlan planSnowMap(const SnowRequest &request);

// Independent check of a plan against its request (does not use planSnowMap). Empty means: the size (edge ring included) is within the limits, every
// cell is stamped once on the 16-tile grid with a Snow piece of the right kind for its position, every ring cell holds the edge piece, every fence
// socket matches the next piece, pinned files are in range, the stamps are in row-major order and the entrance rectangle sits on the entrance cell.
std::vector<std::string> checkSnowPlan(const SnowRequest &request, const SnowPlan &plan);

// Two characters per cell for logs and failed tests, cells separated by a space, row by row. A run piece is its start and end socket letters
// ("ab"), a west or north cap is F and its end socket ("Fb"), an east or south cap is its start socket and R ("bR").
std::string snowPlanAscii(const SnowPlan &plan);

} // namespace cartographer::outdoor::snow
