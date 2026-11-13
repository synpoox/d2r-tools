// Cartographer outdoor build plan (phase 5, part 2). Pure C++: no engine headers, no SDK, no floats, no <random>.
//
// Everything the outdoor build hook (src/engine/outdoor_hooks.cpp) needs before it touches the game, in one call: the
// layout for a map's seed, the piece pick for every cell, and the stamp plan (stamps, level rectangle, void cells). The
// hook only replays the result, so every refusal happens here, before any engine call, and the game then builds its own
// level (fail open).
//
// The plan depends on (tileset, shape, size, seed, origin, size limits, options) only, so the host and a client that
// are given the same seed and the same tables build the same level. PlanCache keeps the plans of the games' seeds so the
// server's build and the client's build of one map share one computation.
//
// Not proven in game: that the picked pieces connect the way their sockets say (issue #22 and #89), that a void grid
// cell (0x100) is empty and unwalkable and that the entrance room is where the plan says (issue #142). The plan is data.
#pragma once
#include <cstdint>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <vector>

#include "layout.h"
#include "outdoor.h"
#include "rng.h"
#include "stamp.h"

namespace cartographer::outdoor {

// The Jungle rows of the catalogue, built into the plugin (no file is read at runtime). tests/outdoor_tests.cpp checks it
// against tests/fixtures/jungle_catalogue.tsv, the copy of tools/catalogue.tsv the picker tests use.
std::string builtinJungleCatalogueText();
const Catalogue &builtinJungleCatalogue();

// True for the tilesets in the table (outdoor_tilesets.h). Case-insensitive.
bool outdoorTilesetKnown(std::string_view tileset);

struct MapRequest {
	std::string tileset;  // the maps.txt Tileset, e.g. "Jungle"
	std::string shape;    // a shape name; blank means Winding. Snow has no shapes: it is ignored there.
	std::string size;     // ExtraSmall, Small, Medium or Large; blank means Medium
	uint64_t seed = 0;    // the map's layout seed
	StampOptions options; // origin, size limits (the slot row's), pinFiles, listVoid
	const layout::ShapeTable *shapes = nullptr; // shapes.txt rows; null means the built-in shapes only. Not part of the cache key.
};

struct MapPlan {
	bool ok = false;
	std::string reason;     // plain English, when !ok: why the game must build its own level
	layout::Layout layout;  // empty unless the shape resolved (always empty for Snow)
	PickResult picks;       // empty for Snow
	StampPlan stamps;       // complete only when ok
	uint64_t hash = 0;      // over the layout, the picks and the stamp plan: stable across platforms
	std::string cacheKey;   // what PlanCache keyed it by
};

// Plans a map: resolves the shape and size, generates the layout for the seed, picks the pieces, plans the stamps and
// checks the stamp plan with checkStampPlan. Any failure leaves ok false with the reason (the first error, with the count
// of the others). Never throws for bad input; allocation failures propagate.
MapPlan planOutdoorMap(const MapRequest &request, const Catalogue &catalogue);

// The cache key of a request: seed, lower-cased tileset, shape and size, origin, limits and options. shapes.txt is not part of
// it, so the owner clears the cache when the tables are reloaded.
std::string planCacheKey(const MapRequest &request);

// Plans keyed by planCacheKey, newest kept, oldest dropped past `capacity`. Failed plans are cached too (the same seed fails the
// same way). Thread safe.
class PlanCache {
public:
	explicit PlanCache(size_t capacity = 4) : capacity_(capacity == 0 ? 1 : capacity) {}
	// The cached plan for the request, or a fresh one (stored). `built` (if given) says whether this call computed it.
	std::shared_ptr<const MapPlan> get(const MapRequest &request, const Catalogue &catalogue, bool *built = nullptr);
	void clear();
	size_t size() const;

private:
	size_t capacity_;
	mutable std::mutex lock_;
	std::map<std::string, std::shared_ptr<const MapPlan>> plans_;
	std::vector<std::string> order_; // oldest first
};

// --- Pure helpers the hook uses ---------------------------------------------------------------------------------------

// The level rectangle to write before the build: origin (the slot row's) and the plan's size in tiles.
struct LevelRect {
	int x = 0;
	int y = 0;
	int w = 0;
	int h = 0;
};
LevelRect levelRectOf(const StampPlan &plan);

// What the grid probe counts in the built level's flag grid: cells with a piece, covered cells (0x200) and void cells (0x100).
struct GridCounts {
	int cells = 0;
	int pieces = 0;
	int covered = 0;
	int voided = 0;
	int other = 0; // cells with none of the three
};
// Compares what the plan asked for with what the grid holds. Empty means they agree (stamped cells all carry a piece or are covered,
// every listed void cell is void, no stamped macro-cell holds a void cell); otherwise plain-English differences. The sample is the
// engine's piece grid (+0x08) and flag grid (+0x58) read cell by cell, row-major from the level's top-left corner.
struct GridSample {
	std::vector<uint64_t> piece; // cellsW * cellsH, row-major
	std::vector<uint64_t> flags;
};
std::vector<std::string> compareGrid(const StampPlan &plan, const GridSample &grid, GridCounts *counts = nullptr);

// --- Picking a room cell on a grid of cells (shared by the tilesets) ---------------------------------------------------

// A cell of a tileset's cell grid.
struct GridCell {
	int x = 0;
	int y = 0;
	bool operator==(const GridCell &o) const { return x == o.x && y == o.y; }
	bool operator!=(const GridCell &o) const { return !(*this == o); }
};

// The candidate whose squared distance (in cells) to the nearest anchor is greatest. Among equal scores the pick is the rng's: a reservoir over the
// ties, one draw per further tie, in candidate order. With one anchor this is "farthest from it"; with two (entrance and boss) it maximises the
// distance to the nearer one. A tileset supplies only its candidate cells and anchors. Returns false (and leaves `out`, with no rng draw) when there are no candidates or no anchors.
bool pickFarthestCell(const std::vector<GridCell> &candidates, const std::vector<GridCell> &anchors, layout::Pcg32 &rng, GridCell &out);

// --- Rooms of the built level: the entry, event and boss rooms --------------------------------------------------------

// A cell of the plan is 32 x 32 tiles and the engine builds it as rooms of 8 x 8 tiles. The map's Boss and event need one exact room (the
// event tracker compares the room's rect), and the arrival needs one room to carry the level-entry mark, so each cell is reduced to the one
// room nearest its centre. Rooms are the built level's, in absolute tiles, in the game's room list order.
struct BuiltRoom {
	int32_t x = 0;
	int32_t y = 0;
	int32_t w = 0;
	int32_t h = 0;
	uint32_t flags = 0; // room +0x50
};

// The bits the game's level-entry finder (0x3da650) looks for in a room's flags: bits 16 and 17.
constexpr uint32_t kArrivalFlags = 0x30000;

// The index of the room that lies wholly inside `cell` and whose centre is nearest the cell's centre (squared straight distance, then the
// smaller y, then the smaller x), or -1 when the cell is not known or holds no room.
int chooseCellRoom(const TileRect &cell, const std::vector<BuiltRoom> &rooms);

struct ArrivalMark {
	bool mark = false;       // false: change nothing, `why` says why (the game keeps its own arrival)
	std::string why;
	int room = -1;           // index into the rooms: the room to give kArrivalFlags
	std::vector<int> clear;  // indexes of the other rooms that carry any of kArrivalFlags
};
// The room that becomes the level entry: the room of the plan's entrance cell (chooseCellRoom). Refuses, with the reason, when the plan has no
// entrance, a room lies outside the plan's level rectangle (the build is not the plan) or the entrance cell holds no room.
ArrivalMark planArrival(const StampPlan &plan, const std::vector<BuiltRoom> &rooms);

} // namespace cartographer::outdoor
