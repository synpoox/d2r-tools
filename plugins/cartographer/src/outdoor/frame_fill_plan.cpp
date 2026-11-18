#include "frame_fill_plan.h"

#include <algorithm>
#include <cstdlib>
#include <utility>

#include "outdoor_plan.h"

namespace cartographer::outdoor::framefill {

// Rows 812..816, 817..821 and 823..827 of lvlprest.txt (822 is Izual's spot and is not used). The Files column is 5 everywhere but Mesa3's four larger pieces (4).
// The weights make the big pieces common without crowding out the small ones that finish the edges.
const FrameFillTileset kMesa = {
	"Mesa",
	27,  // lvltypes.txt "Act 4 - Mesa"
	{800, 801, 802, 799, 804, 805, 806, 803},
	{3, 3, 3, 3, 3, 3, 3, 3},
	3,
	{
		{"Mesa1", 5, {{812, 3, 3, 5, 4}, {813, 2, 2, 5, 3}, {814, 2, 1, 5, 2}, {815, 1, 2, 5, 2}, {816, 1, 1, 5, 1}}},
		{"Mesa2", 5, {{817, 3, 3, 5, 4}, {818, 2, 2, 5, 3}, {819, 2, 1, 5, 2}, {820, 1, 2, 5, 2}, {821, 1, 1, 5, 1}}},
		{"Mesa3", 5, {{823, 3, 3, 4, 4}, {824, 2, 2, 4, 3}, {825, 2, 1, 4, 2}, {826, 1, 2, 4, 2}, {827, 1, 1, 5, 1}}},
	},
	14,
	12,
	{},
};

// Desert: ring 364..371 of lvlprest.txt (edge1..edge8.ds1, 1 file each; 372..375 are the inner corners, not used), in the same order as Mesa's 799..806: south, west, north, east,
// then the corners SW, NW, NE, SE. One fill set of the ten 8 x 8 pieces 398..407: the open dune and berm ground is common, the props (head, bones, wagon, mesa) are rare.
// The arena packs the first piece everywhere, so the dunes come first. Notes in docs/cartographer/tilesets/a2-desert.md.
const FrameFillTileset kDesert = {
	"Desert",
	16,  // lvltypes.txt "Act 2 - Desert"
	{365, 366, 367, 364, 369, 370, 371, 368},
	{1, 1, 1, 1, 1, 1, 1, 1},
	1,
	{
		{"Desert", 10, {{404, 1, 1, 5, 4}, {405, 1, 1, 5, 4}, {406, 1, 1, 4, 3}, {407, 1, 1, 3, 3}, {399, 1, 1, 1, 1}, {400, 1, 1, 1, 1}, {401, 1, 1, 1, 1}, {402, 1, 1, 1, 1}, {403, 1, 1, 1, 1}, {398, 1, 1, 1, 1}}},
	},
	14,
	12,
	{},
};

// Wilderness: lvlprest.txt 4..7 (Bord1..4, 3 files each) are the straight pieces and 8..11 (Bord5..8, 1 file) the convex corners; each has floor on one side only (DS1 walk
// flags, checked with plugins/cartographer/tools/catalogue.py's reader): Bord1 floor north, so it rings the south; Bord2 east, so west; Bord3 south, so north; Bord4 west,
// so east; Bord5 floor north-east, so the south-west corner; Bord6 south-east (north-west corner); Bord7 south-west (north-east); Bord8 north-west (south-east). Pieces 12..15
// (Bord9..12) are concave corners and stay out. Fills: 38 swamp 8 x 8, 39 swamp 16 x 16, 40 trees 16 x 16, each with one DS1 file in the Files column. The Tree set borrows
// the 8 x 8 swamp piece to fill the cells a 16 x 16 cannot. Stone, corral, fence and ruin pieces are left out: they are light, fenced or have a building.
const FrameFillTileset kWilderness = {
	"Wilderness",
	2,  // lvltypes.txt "Act 1 - Wilderness"
	{5, 6, 7, 4, 9, 10, 11, 8},
	{3, 3, 3, 3, 1, 1, 1, 1},
	2,
	{
		{"Swamp", 2, {{39, 2, 2, 1, 4}, {38, 1, 1, 1, 1}}},
		{"Tree", 2, {{40, 2, 2, 1, 4}, {38, 1, 1, 1, 1}}},
	},
	14,
	12,
	{},
};

// Kurast (Act 3 city, LevelType 22): one district, the Burbs; no gates, bridge, temple, sewer or waypoint piece. Ring 619..626 (Burbs Border N, S, E, W: 2 files each; NE, NW, SE, SW: 1 file;
// floor on the inner side, read from the DS1 walk flags), one fill set "Burbs" 635/634/633/632 (4, 4, 4 and 5 files; the 8 x 8 piece last). Notes in docs/cartographer/tilesets/a3-kurast.md.
// The Burbs ring is a low wall (N, E) or a street with open floor on its outer side (W, S), not a cliff, so the black past the level showed beyond it (#550).
// The outer ring puts a second district's ring back to back with it, as Kurast districts meet in the game: W faces an E piece, N an S piece, and so on.
const FrameFillTileset kKurast = {
	"Kurast",
	22,  // lvltypes.txt "Act 3 - Kurast"
	{622, 619, 621, 620, 624, 623, 625, 626},
	{2, 2, 2, 2, 1, 1, 1, 1},
	1,
	{
		{"Burbs", 4, {{635, 2, 2, 4, 3}, {634, 2, 1, 4, 2}, {633, 1, 2, 4, 2}, {632, 1, 1, 5, 1}}},
	},
	14,
	12,
	{621, 620, 622, 619, 625, 626, 624, 623},
};

const FrameFillTileset *const kFrameFillTilesets[] = {&kMesa, &kDesert, &kWilderness, &kKurast};
const int kFrameFillTilesetCount = static_cast<int>(sizeof(kFrameFillTilesets) / sizeof(kFrameFillTilesets[0]));

const FrameFillTileset *findFrameFillTileset(std::string_view name) {
	for (const FrameFillTileset *t : kFrameFillTilesets)
		if (layout::equalsNoCase(t->name, name)) return t;
	return nullptr;
}

namespace {

std::string joinOr(const std::vector<std::string> &names) {
	std::string out;
	for (size_t i = 0; i < names.size(); ++i) {
		if (i > 0) out += i + 1 == names.size() ? " or " : ", ";
		out += names[i];
	}
	return out;
}

} // namespace

std::string frameFillTilesetNames() {
	std::vector<std::string> names;
	for (const FrameFillTileset *t : kFrameFillTilesets) names.push_back(t->name);
	return joinOr(names);
}

int findFillSet(const FrameFillTileset &tileset, std::string_view name) {
	for (int i = 0; i < tileset.setCount; ++i)
		if (layout::equalsNoCase(tileset.sets[i].name, name)) return i;
	return -1;
}

std::string fillSetNames(const FrameFillTileset &tileset) {
	std::vector<std::string> names;
	for (int i = 0; i < tileset.setCount; ++i) names.push_back(tileset.sets[i].name);
	return joinOr(names);
}

namespace {

struct Placed {
	int x, y;     // interior grid cell of the piece's top-left
	int fill;     // index into the set
};

// DS1 files of a ring piece (0 for an id that is not one).
int ringFiles(const FrameFillTileset &t, int lvlPrest) {
	const EdgePieces &p = t.edge, &f = t.edgeFiles;
	const int ids[8] = {p.west, p.north, p.east, p.south, p.nw, p.ne, p.se, p.sw};
	const int files[8] = {f.west, f.north, f.east, f.south, f.nw, f.ne, f.se, f.sw};
	for (int i = 0; i < 8; ++i)
		if (ids[i] == lvlPrest) return files[i];
	return 0;
}

const Fill *findFill(const FrameFillTileset &t, int lvlPrest, int *set) {
	for (int s = 0; s < t.setCount; ++s)
		for (int i = 0; i < t.sets[s].count; ++i)
			if (t.sets[s].fills[i].lvlPrest == lvlPrest) {
				if (set != nullptr) *set = s;
				return &t.sets[s].fills[i];
			}
	return nullptr;
}

TileRect pieceRect(const FrameFillOptions &o, int ring, int ax, int ay, int cw, int ch) {
	TileRect r;
	r.known = true;
	r.x = o.originX + (ax + ring) * kGridTiles;
	r.y = o.originY + (ay + ring) * kGridTiles;
	r.w = cw * kGridTiles;
	r.h = ch * kGridTiles;
	return r;
}

TileRect roomRect(const FrameFillOptions &o, int ring, int ax, int ay) { return pieceRect(o, ring, ax, ay, kRoomCells, kRoomCells); }

// Size problems, shared by the planner and the checker. Empty means the size is acceptable.
std::string sizeProblem(const FrameFillTileset &t, const FrameFillRequest &r) {
	const std::string name = t.name;
	if (r.arena) {
		if (r.cellsW < 1 || r.cellsH < 1 || r.cellsW > t.maxCellsW || r.cellsH > t.maxCellsH)
			return "a " + name + " arena is 1 to " + std::to_string(t.maxCellsW) + " by 1 to " + std::to_string(t.maxCellsH) + " cells of 8 tiles, not " + std::to_string(r.cellsW) + " x " + std::to_string(r.cellsH) + ".";
		if (r.arenaFill < 0 || r.arenaFill >= t.setCount) return "the arena's fill set is not " + fillSetNames(t) + ".";
		if (r.entryX < 0 || r.entryY < 0 || r.entryX >= r.cellsW || r.entryY >= r.cellsH)
			return "the arena's entry cell (" + std::to_string(r.entryX) + "," + std::to_string(r.entryY) + ") is outside its " + std::to_string(r.cellsW) + " x " + std::to_string(r.cellsH) + " interior.";
	} else if (r.cellsW < kMinCells || r.cellsH < kMinCells || r.cellsW > t.maxCellsW || r.cellsH > t.maxCellsH)
		return "a " + name + " map is " + std::to_string(kMinCells) + " to " + std::to_string(t.maxCellsW) + " by " + std::to_string(kMinCells) + " to " + std::to_string(t.maxCellsH) +
			" cells of 8 tiles, not " + std::to_string(r.cellsW) + " x " + std::to_string(r.cellsH) + ".";
	const int tilesW = (r.cellsW + 2 * kRingCells) * kGridTiles, tilesH = (r.cellsH + 2 * kRingCells) * kGridTiles;
	if ((r.options.maxTilesX > 0 && tilesW > r.options.maxTilesX) || (r.options.maxTilesY > 0 && tilesH > r.options.maxTilesY)) {
		auto limit = [](int v) { return v > 0 ? std::to_string(v) : std::string("unlimited"); };
		return "the map needs " + std::to_string(tilesW) + " x " + std::to_string(tilesH) + " tiles (the frame included), more than the slot level's " + limit(r.options.maxTilesX) +
			" x " + limit(r.options.maxTilesY) + ".";
	}
	return {};
}

// Packs the interior: the first free cell in row-major order takes a piece drawn from those that fit there (inside the interior, on free cells).
// The 8 x 8 piece always fits, so every cell ends up covered; the check at the end of the plan proves it again.
// With a null rng the largest piece that fits is taken (an arena).
std::vector<Placed> pack(int w, int h, const FillSet &set, layout::Pcg32 *rng) {
	std::vector<Placed> out;
	std::vector<char> used(static_cast<size_t>(w) * h, 0);
	for (int y = 0; y < h; ++y) {
		for (int x = 0; x < w; ++x) {
			if (used[static_cast<size_t>(y) * w + x]) continue;
			int fits[kMaxFillsPerSet];
			int count = 0, total = 0;
			for (int i = 0; i < set.count; ++i) {
				const Fill &f = set.fills[i];
				bool free = x + f.w <= w && y + f.h <= h;
				for (int dy = 0; free && dy < f.h; ++dy)
					for (int dx = 0; free && dx < f.w; ++dx) free = !used[static_cast<size_t>(y + dy) * w + x + dx];
				if (!free) continue;
				fits[count++] = i;
				total += f.weight;
			}
			int roll = rng != nullptr ? static_cast<int>(rng->below(static_cast<uint32_t>(total))) : 0;  // total >= 1: the 8 x 8 piece always fits
			int pick = rng != nullptr ? fits[count - 1] : fits[0];
			for (int k = 0; k < count && rng != nullptr; ++k) {
				roll -= set.fills[fits[k]].weight;
				if (roll < 0) {
					pick = fits[k];
					break;
				}
			}
			const Fill &f = set.fills[pick];
			for (int dy = 0; dy < f.h; ++dy)
				for (int dx = 0; dx < f.w; ++dx) used[static_cast<size_t>(y + dy) * w + x + dx] = 1;
			out.push_back({x, y, pick});
		}
	}
	return out;
}

// The entrance, boss and event rectangles, as the interior grid cell of their top-left. The entrance is one of the four inner corners (the seed picks);
// the boss is the candidate farthest from it, the event the one farthest from both. A candidate keeps one cell of the interior free on every side
// and may not overlap a rectangle already chosen. `known` is false only when no event candidate is left (never for the sizes a map has).
struct RoomCells {
	bool known = false;
	GridCell entrance, boss, event;
};

bool apart(const GridCell &a, const GridCell &b) { return std::abs(a.x - b.x) >= kRoomCells || std::abs(a.y - b.y) >= kRoomCells; }

RoomCells chooseRoomCells(const FrameFillRequest &r) {
	RoomCells out;
	layout::Pcg32 rng(layout::splitmix64(r.seed + 0x4D455341ull));
	const int lastX = r.cellsW - kRoomCells - 1, lastY = r.cellsH - kRoomCells - 1;  // the highest anchor that keeps a free cell on the far side
	const uint32_t corner = rng.below(4);
	out.entrance = {(corner & 1) ? lastX : 1, (corner & 2) ? lastY : 1};
	std::vector<GridCell> bossOptions, eventOptions;
	for (int y = 1; y <= lastY; ++y)
		for (int x = 1; x <= lastX; ++x)
			if (apart({x, y}, out.entrance)) bossOptions.push_back({x, y});
	if (!pickFarthestCell(bossOptions, {out.entrance}, rng, out.boss)) return out;
	for (const GridCell &c : bossOptions)
		if (apart(c, out.boss)) eventOptions.push_back(c);
	if (!pickFarthestCell(eventOptions, {out.entrance, out.boss}, rng, out.event)) return out;
	out.known = true;
	return out;
}

} // namespace

int edgePieceAt(const EdgePieces &edge, int x, int y, int w, int h) {
	const bool west = x < 0, east = x >= w, north = y < 0, south = y >= h;
	if (north && west) return edge.nw;
	if (north && east) return edge.ne;
	if (south && east) return edge.se;
	if (south && west) return edge.sw;
	if (west) return edge.west;
	if (east) return edge.east;
	if (north) return edge.north;
	if (south) return edge.south;
	return 0;
}

int ringCellsFor(const FrameFillTileset &tileset, const FrameFillRequest &request) {
	if (request.arena || tileset.outer.west == 0) return kRingCells;
	const int tilesW = (request.cellsW + 4) * kGridTiles, tilesH = (request.cellsH + 4) * kGridTiles;
	const bool fits = (request.options.maxTilesX <= 0 || tilesW <= request.options.maxTilesX) && (request.options.maxTilesY <= 0 || tilesH <= request.options.maxTilesY);
	return fits ? 2 : kRingCells;  // a slot level too small for it keeps the one ring (an ExtraLarge Kurast map in a 128 x 112 slot)
}

bool frameFillSizeCells(std::string_view size, int *cellsW, int *cellsH) {
	const std::string s = lowerAscii(size);
	int w = 0, h = 0;
	if (s.empty() || s == "large") { w = 12; h = 10; }
	else if (s == "small") { w = 8; h = 6; }
	else if (s == "medium") { w = 10; h = 8; }
	else if (s == "extralarge") { w = 14; h = 12; }
	else return false;
	if (cellsW) *cellsW = w;
	if (cellsH) *cellsH = h;
	return true;
}

FrameFillPlan planFrameFillArena(const FrameFillTileset &tileset, FrameFillRequest request) {
	request.arena = true;
	return planFrameFillMap(tileset, request);
}

FrameFillPlan planFrameFillMap(const FrameFillTileset &tileset, const FrameFillRequest &request) {
	FrameFillPlan out;
	const auto fail = [&out](std::string why) {
		out.ok = false;
		out.reason = std::move(why);
		out.stamps = StampPlan{};
		out.hash = 0;
		return out;
	};
	if (const std::string bad = sizeProblem(tileset, request); !bad.empty()) return fail(bad);

	const int w = request.cellsW, h = request.cellsH, ring = ringCellsFor(tileset, request);
	out.cellsW = w;
	out.cellsH = h;
	out.ringCells = ring;
	layout::Pcg32 rng(layout::splitmix64(request.seed + 0x4D455341ull + 1));
	out.fillSet = request.arena ? request.arenaFill : static_cast<int>(rng.below(static_cast<uint32_t>(tileset.setCount)));
	const FillSet &set = tileset.sets[out.fillSet];
	const std::vector<Placed> placed = pack(w, h, set, request.arena ? nullptr : &rng);

	StampPlan &plan = out.stamps;
	plan.originX = request.options.originX;
	plan.originY = request.options.originY;
	plan.cellsW = w + 2 * ring;
	plan.cellsH = h + 2 * ring;
	plan.tilesW = plan.cellsW * kGridTiles;
	plan.tilesH = plan.cellsH * kGridTiles;
	plan.macroMinX = 0;
	plan.macroMinY = 0;
	plan.macroW = plan.cellsW;
	plan.macroH = plan.cellsH;
	const RoomCells rooms = request.arena ? RoomCells{true, {request.entryX, request.entryY}, {}, {}} : chooseRoomCells(request);
	if (!rooms.known) return fail("internal: no entrance, boss and event cells fit a " + std::to_string(w) + " x " + std::to_string(h) + " map.");

	// The rings first, then the fill; sorted row-major by the piece's top-left grid cell so the order does not depend on how the pieces were made.
	for (int y = -ring; y < h + ring; ++y) {
		for (int x = -ring; x < w + ring; ++x) {
			if (x >= 0 && y >= 0 && x < w && y < h) continue;
			const bool inner = x >= -1 && y >= -1 && x <= w && y <= h;
			Stamp s;
			s.cx = x + ring;
			s.cy = y + ring;
			s.lvlPrest = inner ? edgePieceAt(tileset.edge, x, y, w, h) : edgePieceAt(tileset.outer, x + 1, y + 1, w + 2, h + 2);
			s.macroX = s.cx;
			s.macroY = s.cy;
			plan.stamps.push_back(s);
		}
	}
	for (const Placed &p : placed) {
		Stamp s;
		s.cx = p.x + ring;
		s.cy = p.y + ring;
		s.lvlPrest = set.fills[p.fill].lvlPrest;
		s.macroX = s.cx;
		s.macroY = s.cy;
		const Fill &f = set.fills[p.fill];
		if (rooms.entrance.x >= p.x && rooms.entrance.x < p.x + f.w && rooms.entrance.y >= p.y && rooms.entrance.y < p.y + f.h) s.role = layout::Role::Entrance;
		plan.stamps.push_back(s);
	}
	std::sort(plan.stamps.begin(), plan.stamps.end(), [](const Stamp &a, const Stamp &b) { return a.cy != b.cy ? a.cy < b.cy : a.cx < b.cx; });
	// The file draws have their own stream, so pinning files does not move the layout. The entrance cell's piece is marked above; a ring piece has its own file count (edgeFiles).
	layout::Pcg32 fileRng(layout::splitmix64(request.seed + 0x46494C45ull));
	for (Stamp &s : plan.stamps) {
		const Fill *f = findFill(tileset, s.lvlPrest, nullptr);
		const int files = f != nullptr ? f->files : ringFiles(tileset, s.lvlPrest);
		s.file = request.options.pinFiles ? static_cast<int>(fileRng.below(static_cast<uint32_t>(files))) : -1;
	}
	if (request.arena) {
		// The arrival is the piece that holds the entry cell: the engine builds a piece as its own rooms, so no smaller rectangle is safe to promise.
		for (const Placed &p : placed) {
			const Fill &f = set.fills[p.fill];
			if (rooms.entrance.x >= p.x && rooms.entrance.x < p.x + f.w && rooms.entrance.y >= p.y && rooms.entrance.y < p.y + f.h)
				plan.entrance = pieceRect(request.options, ring, p.x, p.y, f.w, f.h);
		}
	} else {
		plan.entrance = roomRect(request.options, ring, rooms.entrance.x, rooms.entrance.y);
		plan.boss = roomRect(request.options, ring, rooms.boss.x, rooms.boss.y);
		plan.event = roomRect(request.options, ring, rooms.event.x, rooms.event.y);
	}

	out.ok = true;
	out.hash = stampPlanHash(plan);
	if (const std::vector<std::string> bad = checkFrameFillPlan(tileset, request, out); !bad.empty()) {
		const std::string why = bad.front() + (bad.size() > 1 ? " (and " + std::to_string(bad.size() - 1) + " more)" : std::string());
		return fail("the " + std::string(tileset.name) + " plan failed its own check: " + why);
	}
	return out;
}

namespace {

// The checker's own side-to-piece lookup (not edgePieceAt). Rows are north, middle, south; columns west, middle, east.
int checkEdgePiece(const EdgePieces &e, int x, int y, int w, int h) {
	const int table[3][3] = {{e.nw, e.north, e.ne}, {e.west, 0, e.east}, {e.sw, e.south, e.se}};
	const int col = x < 0 ? 0 : (x >= w ? 2 : 1), row = y < 0 ? 0 : (y >= h ? 2 : 1);
	return table[row][col];
}

// The fill piece's size and set from its id alone, scanning the row.
bool checkFillSize(const FrameFillTileset &t, int lvlPrest, int *set, int *w, int *h, int preferSet = -1) {
	// a piece two sets share (Wilderness's 8 x 8) counts for the set the plan uses
	for (int s = 0; s < t.setCount; ++s)
		if (preferSet < 0 || s == preferSet)
			for (int i = 0; i < t.sets[s].count; ++i)
			if (t.sets[s].fills[i].lvlPrest == lvlPrest) {
				*set = s;
				*w = t.sets[s].fills[i].w;
				*h = t.sets[s].fills[i].h;
				return true;
			}
	return false;
}

} // namespace

std::vector<std::string> checkFrameFillPlan(const FrameFillTileset &tileset, const FrameFillRequest &request, const FrameFillPlan &plan) {
	std::vector<std::string> bad;
	if (const std::string size = sizeProblem(tileset, request); !size.empty()) {
		bad.push_back(size);
		return bad;
	}
	const std::string name = tileset.name;
	// The checker's own ring rule: a map gets the outer ring when the tileset has one and the box with it is inside the size limit.
	const int outerW = (request.cellsW + 4) * kGridTiles, outerH = (request.cellsH + 4) * kGridTiles;
	const bool outer = !request.arena && tileset.outer.west != 0 && (request.options.maxTilesX <= 0 || outerW <= request.options.maxTilesX) &&
		(request.options.maxTilesY <= 0 || outerH <= request.options.maxTilesY);
	const int ring = outer ? 2 : 1;
	const int w = request.cellsW, h = request.cellsH, lw = w + 2 * ring, lh = h + 2 * ring;
	const StampPlan &sp = plan.stamps;
	if (plan.cellsW != w || plan.cellsH != h) bad.push_back("the plan's interior is not the request's");
	if (plan.ringCells != ring) bad.push_back("the plan has " + std::to_string(plan.ringCells) + " ring(s), not " + std::to_string(ring));
	if (sp.tilesW != lw * kGridTiles || sp.tilesH != lh * kGridTiles || sp.cellsW != lw || sp.cellsH != lh || sp.macroW != lw || sp.macroH != lh || sp.macroMinX != 0 ||
		sp.macroMinY != 0 || sp.originX != request.options.originX || sp.originY != request.options.originY)
		bad.push_back("the stamp plan's level box does not match the request");
	if (plan.hash != stampPlanHash(sp)) bad.push_back("the plan's hash is not the hash of its stamps");
	if (!sp.voidCells.empty()) bad.push_back("a " + name + " map covers its whole box but the plan lists void cells");
	if (plan.fillSet < 0 || plan.fillSet >= tileset.setCount) {
		bad.push_back("the plan's fill set is not one of " + fillSetNames(tileset));
		return bad;
	}
	if (request.arena && plan.fillSet != request.arenaFill) bad.push_back("the plan's fill set is not the arena's");
	// Every grid cell of the level is covered by exactly one stamp.
	std::vector<int> cover(static_cast<size_t>(lw) * lh, 0);
	int prevY = -1, prevX = -1;
	for (const Stamp &s : sp.stamps) {
		const std::string where = "the stamp at grid cell (" + std::to_string(s.cx) + "," + std::to_string(s.cy) + ")";
		if (s.cy < prevY || (s.cy == prevY && s.cx <= prevX)) bad.push_back(where + " is out of row-major order");
		prevY = s.cy;
		prevX = s.cx;
		if (s.macroX != s.cx || s.macroY != s.cy || s.mask != 0) bad.push_back(where + " has the wrong macro cell or mask");
		int pw = 1, ph = 1, files = 0;
		const int ix = s.cx - ring, iy = s.cy - ring;  // interior coordinates
		const bool onRing = ix < 0 || iy < 0 || ix >= w || iy >= h;
		if (onRing) {
			if (s.cx < 0 || s.cy < 0 || s.cx >= lw || s.cy >= lh) {
				bad.push_back(where + " is outside the level");
				continue;
			}
			const bool inner = ix >= -1 && iy >= -1 && ix <= w && iy <= h;
			const int want = inner ? checkEdgePiece(tileset.edge, ix, iy, w, h) : checkEdgePiece(tileset.outer, ix + 1, iy + 1, w + 2, h + 2);
			files = ringFiles(tileset, want);
			if (s.lvlPrest != want) bad.push_back(where + " holds piece " + std::to_string(s.lvlPrest) + ", not the " + (inner ? "ring" : "outer ring") + " piece for that side");
		} else {
			int set = 0;
			if (!checkFillSize(tileset, s.lvlPrest, &set, &pw, &ph, plan.fillSet) && !checkFillSize(tileset, s.lvlPrest, &set, &pw, &ph)) {
				bad.push_back(where + " holds piece " + std::to_string(s.lvlPrest) + ", which is not a " + name + " fill piece");
				continue;
			}
			if (set != plan.fillSet) bad.push_back(where + " is from fill set " + std::to_string(set + 1) + ", not the plan's " + std::to_string(plan.fillSet + 1));
			if (ix + pw > w || iy + ph > h) bad.push_back(where + " reaches past the interior");
			files = findFill(tileset, s.lvlPrest, nullptr) != nullptr ? findFill(tileset, s.lvlPrest, nullptr)->files : 0;
		}
		if (s.file < -1 || s.file >= files) bad.push_back(where + " has file index " + std::to_string(s.file) + ", out of range for piece " + std::to_string(s.lvlPrest));
		if (!request.options.pinFiles && s.file != -1) bad.push_back(where + " pins a file index though pinFiles is off");
		for (int dy = 0; dy < ph; ++dy)
			for (int dx = 0; dx < pw; ++dx) {
				const int x = s.cx + dx, y = s.cy + dy;
				if (x >= 0 && y >= 0 && x < lw && y < lh) ++cover[static_cast<size_t>(y) * lw + x];
			}
	}
	int uncovered = 0, doubled = 0;
	for (int c : cover) {
		uncovered += c == 0 ? 1 : 0;
		doubled += c > 1 ? 1 : 0;
	}
	if (uncovered > 0) bad.push_back(std::to_string(uncovered) + " grid cell(s) of the level are covered by no piece");
	if (doubled > 0) bad.push_back(std::to_string(doubled) + " grid cell(s) of the level are covered by more than one piece");

	if (request.arena) {
		// The arena has the ring (checked above), one entrance stamp that holds the entry cell, and no boss or event rectangle.
		int entranceStamps = 0;
		for (const Stamp &s : sp.stamps) {
			if (s.role != layout::Role::Entrance) continue;
			++entranceStamps;
			int set = 0, pw = 1, ph = 1;
			const int ix = s.cx - ring, iy = s.cy - ring;
			if (!checkFillSize(tileset, s.lvlPrest, &set, &pw, &ph) || request.entryX < ix || request.entryX >= ix + pw || request.entryY < iy || request.entryY >= iy + ph)
				bad.push_back("the entrance stamp does not hold the entry cell");
			else if (sp.entrance.x != sp.originX + s.cx * kGridTiles || sp.entrance.y != sp.originY + s.cy * kGridTiles || sp.entrance.w != pw * kGridTiles ||
			         sp.entrance.h != ph * kGridTiles || !sp.entrance.known)
				bad.push_back("the entrance rectangle is not the entry piece");
		}
		if (entranceStamps != 1) bad.push_back("the arena has " + std::to_string(entranceStamps) + " entrance stamps, not one");
		if (sp.boss.known || sp.event.known) bad.push_back("an arena plans no boss or event rectangle");
		return bad;
	}
	// The rooms.
	const RoomCells rooms = chooseRoomCells(request);
	const auto same = [](const TileRect &a, const TileRect &b) { return a.known == b.known && a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h; };
	if (!rooms.known) {
		bad.push_back("no entrance, boss and event cells fit the map");
		return bad;
	}
	if (!same(sp.entrance, roomRect(request.options, ring, rooms.entrance.x, rooms.entrance.y))) bad.push_back("the entrance rectangle is not on its cell");
	if (!same(sp.boss, roomRect(request.options, ring, rooms.boss.x, rooms.boss.y))) bad.push_back("the boss rectangle is not on its cell");
	if (!same(sp.event, roomRect(request.options, ring, rooms.event.x, rooms.event.y))) bad.push_back("the event rectangle is not on its cell");
	const TileRect *rects[3] = {&sp.entrance, &sp.boss, &sp.event};
	for (int i = 0; i < 3; ++i) {
		const TileRect &r = *rects[i];
		const int ox = sp.originX + ring * kGridTiles, oy = sp.originY + ring * kGridTiles;
		if (!r.known || r.x < ox || r.y < oy || r.x + r.w > ox + w * kGridTiles || r.y + r.h > oy + h * kGridTiles) bad.push_back("a room rectangle is outside the interior");
		for (int j = i + 1; j < 3; ++j) {
			const TileRect &q = *rects[j];
			if (r.x < q.x + q.w && q.x < r.x + r.w && r.y < q.y + q.h && q.y < r.y + r.h) bad.push_back("two of the entrance, boss and event rectangles overlap");
		}
	}
	int entranceStamps = 0;
	for (const Stamp &s : sp.stamps) entranceStamps += s.role == layout::Role::Entrance ? 1 : 0;
	if (entranceStamps != 1) bad.push_back("the plan has " + std::to_string(entranceStamps) + " entrance stamps, not one");
	return bad;
}

std::string frameFillPlanAscii(const FrameFillTileset &tileset, const FrameFillPlan &plan) {
	const StampPlan &sp = plan.stamps;
	std::vector<std::string> rows(static_cast<size_t>(sp.cellsH), std::string(static_cast<size_t>(sp.cellsW), '?'));
	for (const Stamp &s : sp.stamps) {
		int set = 0, pw = 1, ph = 1;
		char c = '+';
		if (checkFillSize(tileset, s.lvlPrest, &set, &pw, &ph)) c = pw == 3 ? '3' : (pw == 2 && ph == 2 ? '2' : (pw == 2 ? 'h' : (ph == 2 ? 'v' : '1')));
		else pw = ph = 1;
		for (int dy = 0; dy < ph; ++dy)
			for (int dx = 0; dx < pw; ++dx)
				if (s.cy + dy < sp.cellsH && s.cx + dx < sp.cellsW) rows[static_cast<size_t>(s.cy + dy)][static_cast<size_t>(s.cx + dx)] = c;
	}
	const auto mark = [&](const TileRect &r, char c) {
		if (!r.known) return;
		const int gx = (r.x - sp.originX) / kGridTiles, gy = (r.y - sp.originY) / kGridTiles;
		if (gy >= 0 && gy < sp.cellsH && gx >= 0 && gx < sp.cellsW) rows[static_cast<size_t>(gy)][static_cast<size_t>(gx)] = c;
	};
	mark(sp.entrance, 'E');
	mark(sp.boss, 'B');
	mark(sp.event, 'V');
	std::string out;
	for (const std::string &r : rows) out += r + '\n';
	return out;
}

} // namespace cartographer::outdoor::framefill
