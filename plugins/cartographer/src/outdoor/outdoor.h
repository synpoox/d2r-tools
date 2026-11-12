// Cartographer outdoor piece-picker (phase 4a). Pure C++: no engine headers, no SDK, no floats, no <random>.
//
// Takes a layout from src/layout/ and the tileset catalogue (tools/catalogue.tsv, as plain structs) and picks one
// catalogue piece for every used macro-cell. The piece's four edge sockets must match the cell's facing bits: an exit
// (a set mask bit) needs a walkable path socket 'p' on that side, no exit needs a closed socket 'x'. The #22 spike
// suggested that a jungle exit is a walkable path, so the layout graph would map 1:1 onto the Act 3 jungle mask
// pieces; that is not proven yet and needs an in-game check.
//
// Which pieces a cell may use depends on its role (the first group that has a match wins):
//   Entrance       entry, then path
//   Event          event, then clearing, then path
//   Boss           boss, then clearing, then path
//   Path / Branch  path   (a Branch tip is a dead end; the mask alone makes it one)
// border and filler pieces are never picked, nor are pieces bigger than 1 x 1 macro-cell (the stamper for those comes
// later). Mask 0 is never emitted: an empty cell gets no piece.
//
// Determinism: the picks depend on (layout, catalogue, tileset, seed) only. Each cell has its own PCG32 stream
// (src/layout/rng.h), so a cell's pick does not depend on how many draws other cells made.
#pragma once
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "layout.h"

namespace cartographer::outdoor {

// ASCII lower-casing and the "cell (x,y)" wording every tileset uses in plan checks and errors.
std::string lowerAscii(std::string_view text);
std::string cellName(int x, int y);

enum class PieceRole : uint8_t { Path, Clearing, Entry, Boss, Event, Filler, Border };
const char *pieceRoleName(PieceRole role);

// One catalogue row. Sockets are one character per macro-cell along the side: x closed, p path, c cliff, r ravine.
struct Piece {
	std::string tileset;
	std::string builder;
	int levelType = 0;
	int lvlPrest = 0;
	std::string name;
	int w = 1; // size in macro-cells
	int h = 1;
	std::string edge[4]; // N, E, S, W
	PieceRole role = PieceRole::Path;
	int weight = 1; // 0 or less: never picked
	std::string tags;
	std::string check; // the catalogue's check column, lower case: ok, n/a, variants or empty (other values are skipped)
	std::vector<std::string> ds1; // DS1 variants of the preset, as listed in the catalogue
	uint8_t openMask = 0; // layout::kWest..kNorth bits of the 'p' sockets; only meaningful for a 1 x 1 piece
};

struct Catalogue {
	std::vector<Piece> pieces; // in file order
};

struct LoadResult {
	Catalogue catalogue;
	std::vector<std::string> warnings; // plain English, one per skipped row or problem, with the line
	bool ok() const { return !catalogue.pieces.empty(); }
};

// Header-driven: columns are found by name, so their order does not matter and extra columns are ignored.
// Required: tileset, lvlprest, w, h, edgeN, edgeE, edgeS, edgeW, role, weight. Optional: builder, leveltype, name,
// tags, check, ds1 (';'-separated). Rows that cannot be used are skipped with a warning, and so is a row whose check
// is not ok, n/a or empty; a 'variants' row is kept but gets no DS1 variant. A UTF-8 byte order mark is accepted.
// `fileName` only appears in messages.
LoadResult parseCatalogue(std::string_view text, std::string_view fileName = "catalogue.tsv");
LoadResult loadCatalogueFile(const std::string &path);

// One placed cell of the output grid. lvlPrest is -1 for an empty cell.
struct PickedCell {
	int x = 0;
	int y = 0;
	int lvlPrest = -1;   // the LvlPrest id to stamp
	int ds1Variant = -1; // index into the piece's ds1 list, or -1 when the piece lists none
	int piece = -1;      // index into Catalogue::pieces, or -1
};

struct PickResult {
	int width = 0;
	int height = 0;
	std::vector<PickedCell> cells; // width * height, row-major like Layout::cells
	std::vector<std::string> errors; // plain English; empty means every used cell has a piece
	bool ok() const { return errors.empty(); }
	const PickedCell &at(int x, int y) const { return cells[static_cast<size_t>(y) * width + x]; }
};

// Picks a piece for every used cell. `tileset` is matched without regard to case. On failure the errors say, in plain
// English, which piece the tileset lacks; the grid still holds the cells that did get a piece.
PickResult pickPieces(const layout::Layout &layout, const Catalogue &catalogue, std::string_view tileset, uint64_t seed);

// Independent check of a result: every used cell has a piece of the tileset whose sockets match its facing bits (and so
// its neighbours'), empty cells have none. Empty means all good.
std::vector<std::string> checkPicks(const layout::Layout &layout, const Catalogue &catalogue, std::string_view tileset,
	const PickResult &result);

// "NSEW" order, as in the catalogue names ("SEW"); "no exits" for 0.
std::string maskName(uint8_t mask);

// FNV-1a over the grid of (lvlPrest, ds1Variant): stable across platforms, for golden tests.
uint64_t picksHash(const PickResult &result);

} // namespace cartographer::outdoor
