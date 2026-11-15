// Cartographer outdoor Mesa (Act 4 plains) plan. Pure C++: no engine headers, no SDK, no floats, no <random>.
//
// The Mesa tileset (LevelType 27, LvlPrest 799..827) is a frame plus a fill. The frame is a ring of 8 x 8 tile border pieces: one straight piece
// per side and one per corner, each with floor on its inner side and nothing beyond its cliff (read from the DS1 walk flags, see
// docs/cartographer/tilesets/a4-mesa.md). Inside the frame the map is a rectangle of 8 x 8 tile grid cells packed with the fill pieces
// of one fill set (Mesa 1, 2 or 3: each has a 24 x 24, 16 x 16, 16 x 8, 8 x 16 and 8 x 8 tile piece). The 8 x 8 piece fits any cell, so the
// packing always reaches every cell. The seed picks the set, the packing and the entrance, boss and event cells.
//
// The plan is the same StampPlan the Jungle, Snow and Siege plans produce, so the outdoor build hook replays it unchanged. Its macro cell is
// the 8-tile grid cell, so the stamp grid is the macro grid.
//
// Not proven in game: that the stamper accepts these ids, that the ring leaves no black at the edges, the Act 4 palette in an Act 5 level.
#pragma once
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "stamp.h"

namespace cartographer::outdoor::mesa {

constexpr int kLevelType = 27;    // lvltypes.txt "Act 4 - Mesa"
constexpr int kGridTiles = 8;     // the engine's grid cell, and the border piece size
constexpr int kRingCells = 1;     // the frame is one grid cell deep
constexpr int kRoomCells = 2;     // the entrance, boss and event rectangles are 2 x 2 grid cells (16 x 16 tiles, four rooms)
constexpr int kMinCells = 6;      // the smallest interior either way, in grid cells (Small is 8 x 6)
constexpr int kMaxCellsW = 14;    // the largest interior (ExtraLarge)
constexpr int kMaxCellsH = 12;
constexpr int kLevelTilesX = (kMaxCellsW + 2 * kRingCells) * kGridTiles;  // the slot level: 128 x 112 tiles
constexpr int kLevelTilesY = (kMaxCellsH + 2 * kRingCells) * kGridTiles;
constexpr int kFillSets = 3;
constexpr int kFillsPerSet = 5;

// One fill piece: a lvlprest.txt row with a size in grid cells and the number of DS1 files the plan may draw.
struct Fill {
	int lvlPrest;
	int w, h;       // grid cells
	int files;      // DS1 files, from the Files column
	int weight;     // chance of being drawn next to the others that fit
};
struct FillSet {
	const char *name;
	Fill fills[kFillsPerSet];  // 24 x 24, 16 x 16, 16 x 8, 8 x 16, 8 x 8
};
const FillSet &fillSet(int index);  // 0 Mesa 1 (812..816), 1 Mesa 2 (817..821), 2 Mesa 3 (823..827)

// The ring pieces (LvlPrest 799..806, 3 files each): the straight piece of the ring cell west, north, east or south of the interior, and the corners.
struct EdgePieces {
	int west, north, east, south;
	int nw, ne, se, sw;
};
extern const EdgePieces kEdgePieces;
constexpr int kBorderFiles = 3;
// The ring piece at grid cell (x, y) of a W x H interior: x in -1..W, y in -1..H, not inside it. 0 for a cell inside.
int edgePieceAt(int x, int y, int w, int h);

struct MesaOptions {
	int originX = 0;       // the level's origin in tiles (the slot row's OffsetX / OffsetY)
	int originY = 0;
	int maxTilesX = 0;     // the slot row's declared size limit in tiles; 0 means no limit
	int maxTilesY = 0;
	bool pinFiles = false; // true: pass a drawn DS1 file index; false: -1, the engine draws it from the level RNG
};

struct MesaRequest {
	uint64_t seed = 0;
	int cellsW = 12;       // the interior in 8-tile grid cells, without the ring; Large is 12 x 10
	int cellsH = 10;
	MesaOptions options;
};

// Small, Medium, Large or ExtraLarge (blank means Large), case-insensitive. False for any other word. 8 x 6, 10 x 8, 12 x 10 and 14 x 12 cells.
bool mesaSizeCells(std::string_view size, int *cellsW, int *cellsH);

struct MesaPlan {
	bool ok = false;
	std::string reason;                 // plain English, when !ok: why the game must build its own level
	int fillSet = 0;                    // 0..2
	int cellsW = 0;                     // the interior in grid cells
	int cellsH = 0;
	StampPlan stamps;                   // complete only when ok: ring and fill, row-major by their top-left grid cell; grid cell (x, y) of the level is the interior's (x - 1, y - 1)
	uint64_t hash = 0;                  // stampPlanHash of the stamps
};

// Plans a Mesa map for a seed: picks the fill set, packs the interior, rings it, picks the rooms and checks the result with checkMesaPlan. Any failure
// leaves ok false with the reason. Never throws for bad input.
MesaPlan planMesaMap(const MesaRequest &request);

// Independent check of a plan against its request (does not use planMesaMap). Empty means: the size is within the limits, every grid cell of the level
// is covered by exactly one stamp, the ring holds the right piece on every side and corner, the fill stamps are all of one set and fit inside the
// interior, pinned files are in range, the stamps are in row-major order and the entrance, boss and event rectangles sit inside the interior and
// do not overlap.
std::vector<std::string> checkMesaPlan(const MesaRequest &request, const MesaPlan &plan);

// One character per grid cell of the level for logs and failed tests: the ring is '+', a fill piece is its size class (3 for 24 x 24, 2 for
// 16 x 16, h for 16 x 8, v for 8 x 16, 1 for 8 x 8), with 'E', 'B' and 'V' on the entrance, boss and event rectangles' top-left cell.
std::string mesaPlanAscii(const MesaPlan &plan);

} // namespace cartographer::outdoor::mesa
