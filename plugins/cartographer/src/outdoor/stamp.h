// Cartographer outdoor stamp plan (phase 5, part 1). Pure C++: no engine headers, no SDK, no floats, no <random>.
//
// Turns a layout (src/layout/) and its piece picks (outdoor.h) into the exact list of calls the engine side will make
// for the outdoor slot level: one piece stamp per used macro-cell, the level rectangle, the cells to mark void and
// the rectangles of the entrance, event and boss cells. Nothing here talks to the game; the engine side (a later
// slice) only replays this plan, so every failure happens here, before any engine call.
//
// Units (plan section 1.1 to 1.4, cartographer-outdoor-builder.md):
//   * a macro-cell is `macroTiles` tiles on a side (32 for the jungle blocks); the layout's unused rows and columns
//     are cropped away, so the level covers the used cells' bounding box only;
//   * the engine's grid cell is `gridTiles` tiles (8); a stamp is given in grid cells, relative to the level's
//     top-left corner, and a macro-cell covers (macroTiles / gridTiles) squared of them;
//   * rectangles are in tiles and absolute: the level's origin (the slot row's OffsetX / OffsetY) is added.
//
// Determinism: the plan depends on (layout, picks, options) only. Stamps are in row-major macro-cell order, so the
// room order the engine derives from the stamping order is the same on the host and on a client.
//
// Not proven in game: that the picks' pieces connect the way their sockets say (river or path, issue #22 and #89),
// that a void grid cell (flag 0x100) is empty and unwalkable (RE doc section 8 Q3, inference only) and that the
// engine accepts a file index in place of -1 (RE doc section 8 Q8 does not settle it). The plan is data; those are
// the engine slice's checks.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "layout.h"
#include "outdoor.h"

namespace cartographer::outdoor {

struct StampOptions {
	int macroTiles = 32;    // tiles per macro-cell side; must be a multiple of gridTiles
	int gridTiles = 8;      // tiles per engine grid cell
	int originX = 0;        // the level's origin in tiles (the slot row's OffsetX / OffsetY)
	int originY = 0;
	int maxMacroCells = 12; // longest side of the cropped layout, in macro-cells (plan section 1.1: 12 x 12 jungle blocks)
	int maxTilesX = 0;      // the slot row's declared size limit in tiles; 0 means no limit
	int maxTilesY = 0;
	bool pinFiles = false;  // true: pass each pick's DS1 variant as the file index; false: -1, the engine draws it from the level RNG
	bool listVoid = true;   // true: list the grid cells of unused macro-cells so the engine can mark them void
};

// One piece stamp: the arguments of the engine's 0x3ecfc0 after (dataFlag, level), minus the last constant 0.
struct Stamp {
	int cx = 0;          // grid cell, relative to the level's top-left corner
	int cy = 0;
	int lvlPrest = 0;    // the LvlPrest id
	int file = -1;       // the DS1 file index, or -1 for the engine to draw it
	int macroX = 0;      // the layout cell this stamp is for (layout coordinates, before the crop)
	int macroY = 0;
	uint8_t mask = 0;    // that cell's exit mask
	layout::Role role = layout::Role::Empty;
};

struct TileRect {
	bool known = false;
	int x = 0;           // tiles, absolute (origin included)
	int y = 0;
	int w = 0;
	int h = 0;
	int centreX() const { return x + w / 2; }
	int centreY() const { return y + h / 2; }
};

struct StampPlan {
	int originX = 0;     // tiles, as in the options
	int originY = 0;
	int tilesW = 0;      // the level's size in tiles
	int tilesH = 0;
	int cellsW = 0;      // the same in grid cells
	int cellsH = 0;
	int macroMinX = 0;   // the cropped box in layout coordinates
	int macroMinY = 0;
	int macroW = 0;
	int macroH = 0;
	std::vector<Stamp> stamps;               // row-major by macro-cell
	std::vector<layout::Point> voidCells;    // grid cells (relative) covered by no stamp, row-major; empty unless listVoid
	TileRect entrance;                       // the entrance cell
	TileRect event;                          // the event cell, if the layout has one
	TileRect boss;                           // the boss cell, if the layout has one
	std::vector<std::string> errors;         // plain English; empty means the plan is complete
	bool ok() const { return errors.empty(); }
};

// Builds the plan. On failure `errors` says why in plain English: bad cell sizes, picks with errors or made for another
// layout, no used cell, the cropped layout over the size limits, a cell with no piece or an exit mask over 15, piece 529
// (the Act 3 town), a pinned DS1 variant over 15, a jungle piece that breaks the 0x211 + mask rule, or no entrance /
// more than one entrance, event or boss cell. On failure `stamps`, `voidCells` and the rectangles are empty; the size
// fields (macroMinX/Y, macroW/H, tilesW/H) stay set when the failure was the size check, and the cell fields (cellsW/H)
// are set only past it.
StampPlan planStamps(const layout::Layout &layout, const PickResult &picks, const StampOptions &options);

// Independent check of a plan against the layout it was made from. Empty means every used macro-cell is stamped once,
// aligned to the macro-cell grid, inside the level, with its picked piece; every other grid cell is listed as void
// (when listVoid); the rectangles sit on the right cells. Does not use planStamps.
std::vector<std::string> checkStampPlan(const layout::Layout &layout, const PickResult &picks, const StampOptions &options,
	const StampPlan &plan);

// The LvlPrest id the native jungle builder (0x3f0490) stamps for an exit mask: 0x211 + mask (530..544), or -1 for
// mask 0 (529 is the Act 3 town and must never be emitted) and for a mask over 15.
int jungleLvlPrest(uint8_t mask);

// FNV-1a over the level box, the stamps and the void cells: stable across platforms, for golden tests.
uint64_t stampPlanHash(const StampPlan &plan);

// A short ASCII picture for logs and failed tests: one character per grid cell, '#' stamped, '.' void, row by row.
std::string stampPlanAscii(const StampPlan &plan);

} // namespace cartographer::outdoor
