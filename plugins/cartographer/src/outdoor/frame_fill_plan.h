// Cartographer frame-plus-fill plan for maps and arenas. Pure C++: no engine headers, no SDK, no floats, no <random>.
//
// A frame-plus-fill tileset is a frame plus a fill. The frame is a ring of 8 x 8 tile border pieces: one straight piece per side and one per corner,
// each with floor on its inner side and nothing beyond its cliff (read from the DS1 walk flags). Inside the frame the map is a rectangle of 8 x 8 tile
// grid cells packed with the fill pieces of one fill set. The 8 x 8 piece (the last of every set) fits any cell, so the packing always reaches every
// cell. For a map the seed picks the set, the packing and the entrance, boss and event cells; an arena is fixed: one named set, largest piece first, no seed.
//
// Everything a tileset owns (its LevelType, ring pieces, fill sets and largest interior) is one FrameFillTileset row. Mesa (LevelType 27, LvlPrest
// 799..827, notes in docs/cartographer/tilesets/a4-mesa.md) is the first row. A new tileset is a new row in kFrameFillTilesets plus its OutdoorTilesets
// row (see docs/cartographer/adding-a-tileset.md).
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

namespace cartographer::outdoor::framefill {

constexpr int kGridTiles = 8;     // the engine's grid cell, and the border piece size
constexpr int kRingCells = 1;     // the frame is one grid cell deep
constexpr int kRoomCells = 2;     // the entrance, boss and event rectangles are 2 x 2 grid cells (16 x 16 tiles, four rooms)
constexpr int kMinCells = 6;      // the smallest interior either way, in grid cells (Small is 8 x 6)
constexpr int kMaxFillSets = 4;   // the most fill sets and pieces per set a tileset row holds
constexpr int kMaxFillsPerSet = 10;

// One fill piece: a lvlprest.txt row with a size in grid cells and the number of DS1 files the plan may draw.
struct Fill {
	int lvlPrest;
	int w, h;       // grid cells
	int files;      // DS1 files, from the Files column
	int weight;     // chance of being drawn next to the others that fit
};
struct FillSet {
	const char *name;             // the arenas.txt Fill word, no spaces ("Mesa1")
	int count;                    // pieces in use in fills
	Fill fills[kMaxFillsPerSet];  // the last one must be the 8 x 8 piece, which fits any cell
};

// The ring pieces: the straight piece of the ring cell west, north, east or south of the interior, and the corners.
struct EdgePieces {
	int west, north, east, south;
	int nw, ne, se, sw;
};

// One frame-plus-fill tileset: the row the planner reads.
struct FrameFillTileset {
	const char *name;          // the maps.txt / arenas.txt Tileset ("Mesa")
	int levelType;             // the levels.txt LevelType its level must have
	EdgePieces edge;
	int borderFiles;           // DS1 files of every ring piece
	int setCount;
	FillSet sets[kMaxFillSets];
	int maxCellsW, maxCellsH;  // the largest interior (ExtraLarge), in grid cells
};

// Mesa (Act 4 plains, LevelType 27): ring 799..806 (3 files each), sets Mesa1 812..816, Mesa2 817..821, Mesa3 823..827.
extern const FrameFillTileset kMesa;
// Desert (Act 2 sand, LevelType 16): ring 364..371 (1 file each), one set Desert of the ten 8 x 8 fills 398..407.
extern const FrameFillTileset kDesert;
// Every frame-plus-fill tileset, in table order. A new tileset is one row here.
extern const FrameFillTileset *const kFrameFillTilesets[];
extern const int kFrameFillTilesetCount;
// The row for a tileset name (ignoring case), or nullptr.
const FrameFillTileset *findFrameFillTileset(std::string_view name);
// "Mesa", "Mesa or Desert": the names for a message.
std::string frameFillTilesetNames();
// The index of the fill set named `name` (ignoring case, "Mesa1"), or -1.
int findFillSet(const FrameFillTileset &tileset, std::string_view name);
// "Mesa1, Mesa2 or Mesa3": the set names for a message.
std::string fillSetNames(const FrameFillTileset &tileset);

// The ring piece at grid cell (x, y) of a W x H interior: x in -1..W, y in -1..H, not inside it. 0 for a cell inside.
int edgePieceAt(const EdgePieces &edge, int x, int y, int w, int h);

struct FrameFillOptions {
	int originX = 0;       // the level's origin in tiles (the slot row's OffsetX / OffsetY)
	int originY = 0;
	int maxTilesX = 0;     // the slot row's declared size limit in tiles; 0 means no limit
	int maxTilesY = 0;
	bool pinFiles = false; // true: pass a drawn DS1 file index; false: -1, the engine draws it from the level RNG
};

struct FrameFillRequest {
	uint64_t seed = 0;
	int cellsW = 12;       // the interior in 8-tile grid cells, without the ring; Large is 12 x 10
	int cellsH = 10;
	FrameFillOptions options;
	// An arena (planFrameFillArena): no seed, the interior from 1 cell up, one named fill set, and an entry cell instead of the seeded rooms.
	bool arena = false;
	int arenaFill = 2;     // the fill set index (blank in arenas.txt: the last set, Mesa3)
	int entryX = 0;        // the interior cell whose piece is the arrival
	int entryY = 0;
};

// Small, Medium, Large or ExtraLarge (blank means Large), case-insensitive. False for any other word. 8 x 6, 10 x 8, 12 x 10 and 14 x 12 cells.
bool frameFillSizeCells(std::string_view size, int *cellsW, int *cellsH);

struct FrameFillPlan {
	bool ok = false;
	std::string reason;                 // plain English, when !ok: why the game must build its own level
	int fillSet = 0;                    // index into the tileset's sets
	int cellsW = 0;                     // the interior in grid cells
	int cellsH = 0;
	StampPlan stamps;                   // complete only when ok: ring and fill, row-major by their top-left grid cell; grid cell (x, y) of the level is the interior's (x - 1, y - 1)
	uint64_t hash = 0;                  // stampPlanHash of the stamps
};

// Plans a map of `tileset` for a seed: picks the fill set, packs the interior, rings it, picks the rooms and checks the result with checkFrameFillPlan.
// Any failure leaves ok false with the reason. Never throws for bad input.
FrameFillPlan planFrameFillMap(const FrameFillTileset &tileset, const FrameFillRequest &request);

// Plans a fixed arena: the same ring as planFrameFillMap round an interior of request.cellsW x cellsH cells (1 to the tileset's maxCellsW / maxCellsH), packed
// largest piece first from one fill set with no RNG, so the plan is the same in every game. The stamp holding the entry cell is the entrance; there is no boss
// or event rectangle. request.arena is forced on. Checked with checkFrameFillPlan. Any failure leaves ok false with the reason. Never throws for bad input.
FrameFillPlan planFrameFillArena(const FrameFillTileset &tileset, FrameFillRequest request);

// Independent check of a plan against its request (does not use planFrameFillMap). Empty means: the size is within the limits, every grid cell of the level
// is covered by exactly one stamp, the ring holds the right piece on every side and corner, the fill stamps are all of one set and fit inside the
// interior, pinned files are in range, the stamps are in row-major order and the entrance, boss and event rectangles sit inside the interior and
// do not overlap. For an arena request: sizes from 1 cell, one fill set (arenaFill), the entrance rectangle is the piece holding the entry cell and nothing else is planned.
std::vector<std::string> checkFrameFillPlan(const FrameFillTileset &tileset, const FrameFillRequest &request, const FrameFillPlan &plan);

// One character per grid cell of the level for logs and failed tests: the ring is '+', a fill piece is its size class (3 for 24 x 24, 2 for
// 16 x 16, h for 16 x 8, v for 8 x 16, 1 for 8 x 8), with 'E', 'B' and 'V' on the entrance, boss and event rectangles' top-left cell.
std::string frameFillPlanAscii(const FrameFillTileset &tileset, const FrameFillPlan &plan);

} // namespace cartographer::outdoor::framefill
