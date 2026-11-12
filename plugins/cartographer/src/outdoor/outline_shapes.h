// Cartographer outdoor outline shapes (issue #148 desk spike). Pure C++: no engine headers, no SDK, no floats, no <random>.
//
// Shape masks for a non-rectangular outdoor level, and the checks the engine's outline and border pass (0x3efaf0, RE doc
// cartographer-outline.md) puts on them. A mask is a grid of open and void cells; the unit is one cell (the engine's 8 tile
// grid cell for the open-field families, a 32 tile block for Jungle). Nothing here is wired into the build hook: the plan
// (outdoor_plan.*) does not read it, and whether the engine builds and walks such a level is not shown in game.
//
// Units: x grows east, y grows south, as in src/layout. Corner coordinates run 0..w and 0..h.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace cartographer::outdoor::outline {

struct Mask {
	int w = 0;
	int h = 0;
	std::vector<uint8_t> open; // w * h, row-major, 1 = open ground, 0 = void

	bool inside(int x, int y) const { return x >= 0 && y >= 0 && x < w && y < h; }
	bool at(int x, int y) const { return inside(x, y) && open[static_cast<size_t>(y) * w + x] != 0; }
	void set(int x, int y, bool v) { open[static_cast<size_t>(y) * w + x] = v ? 1 : 0; }
	int count() const;
};

Mask makeMask(int w, int h, bool fill);

// --- Generators (each returns an empty mask, w == 0, for sizes it cannot draw) ----------------------------------------

Mask rectangleShape(int w, int h);
// An L: the full-width bottom arm of `armH` rows and the full-height left arm of `armW` columns.
Mask lShape(int w, int h, int armW, int armH);
// A plus: a vertical bar `arm` wide and a horizontal bar `arm` tall, both centred (arm must leave room on both sides when w, h > arm).
Mask crossShape(int w, int h, int arm);
// A rectangle with a rectangular hole: a wall `wall` cells thick all round. The hole is an enclosed void (a ring).
Mask ringShape(int w, int h, int wall);
// A U: a rectangle with a notch of `notchW` x `notchD` cut into the top edge, centred (the notch is open to the outside).
Mask uShape(int w, int h, int notchW, int notchD);
// An irregular blob grown from the middle by random frontier picks, on a grid half the size and drawn at double size (so no feature is
// one cell wide), until `fillPercent` of the half-size cells are open; corner-only contacts are joined and the result is cropped to its
// bounding box, so w and h are at most the arguments and always even. Always one 4-connected region. With `allowHoles` false any
// enclosed void is filled. Needs w, h >= 2. Same arguments, same mask on every platform.
Mask blobShape(int w, int h, int fillPercent, uint64_t seed, bool allowHoles);

// --- Analysis ---------------------------------------------------------------------------------------------------------

// Number of 4-connected open regions (a level must be one).
int openRegions(const Mask &m);
// Number of 4-connected void regions that do not touch the outside of the mask or its border (a ring's hole).
int enclosedVoidRegions(const Mask &m);
// Border cells: open cells with a void cell, or the outside of the mask, among their eight neighbours. These are the cells the engine's
// outline walk stamps border pieces on and marks bit 0 in its flag grid (a polygon through the outermost open cells).
Mask borderCells(const Mask &m);
// Open cells with void (or the outside) on both sides along one axis: a one-cell-wide neck. The outline polygon of such a feature
// doubles back on itself (zero-width), which the border walk has no corner for. Shapes want 0.
int thinCells(const Mask &m);
// Void cells the engine's void pass marks 0x100 (loop at 0x3f0040 in 0x3efaf0): four scans, one from each corner of the mask. A scan
// walks the rows from its corner while the row's edge cell is not a border cell, and in each row marks cells inward until the first
// border cell. 1 = marked. `overrun` (if given) is set when a ray or the row walk left the mask before meeting a border cell: the
// engine reads its grid out of range there. Void it does not reach (a hole, a notch in the middle of an edge) is left to the fill rooms.
Mask engineVoidMarks(const Mask &m, bool *overrun = nullptr);
// Void cells in the mask that engineVoidMarks leaves unmarked: the plan has to mark these itself (VoidCell), all of them.
int unmarkedVoidCells(const Mask &m);

// A corner of the outline in corner coordinates.
struct Vertex {
	int x = 0;
	int y = 0;
	bool operator==(const Vertex &o) const { return x == o.x && y == o.y; }
};
// One closed loop of the boundary, turning points only (no collinear vertex), walked with the open cells on the right hand (clockwise
// on screen for an outer loop, counter-clockwise for a hole). `area` is the signed shoelace area: positive outer, negative hole.
struct Loop {
	std::vector<Vertex> v;
	long long area = 0;
	bool hole() const { return area < 0; }
};
// All boundary loops in corner coordinates (the cell edges between open and void), outer loops and holes, each starting at its top-left
// vertex, ordered by that vertex. Where two open cells touch only at a corner the loop hugs each cell: two loops, never a crossing one.
std::vector<Loop> traceLoops(const Mask &m);
// The same loops as the engine's outline polygon: vertices are cell indices of the outermost open cells (the nodes of the list at block
// +0xb8 in 0x3ee320 run from cell 0,0 to cell w-1,h-1 for a rectangle). A convex corner maps to the corner cell, a reflex corner to the
// open cell diagonally opposite the void cell. `area` is left as the corner loop's. Vertices coincide where a feature is one cell wide.
std::vector<Loop> traceEngineOutlines(const Mask &m);

// Problems the engine's border walk has with a hand-made loop (0x3efaf0 steps one cell at a time along sign(dx), sign(dy) and
// stops on equality): empty means every edge is axis-aligned, non-zero, and the loop has at least 4 vertices. A diagonal edge
// has no entry in the direction table, and a slope other than 45 degrees never reaches its end.
std::vector<std::string> checkLoopForEngine(const std::vector<Vertex> &loop);

// A short ASCII picture for logs and failed tests: '#' open, '.' void, 'x' an enclosed or unmarked void cell.
std::string maskAscii(const Mask &m);

} // namespace cartographer::outdoor::outline
