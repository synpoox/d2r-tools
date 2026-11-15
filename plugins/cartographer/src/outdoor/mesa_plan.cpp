#include "mesa_plan.h"

#include <algorithm>
#include <cstdlib>
#include <utility>

#include "outdoor_plan.h"

namespace cartographer::outdoor::mesa {

namespace {

// Rows 812..816, 817..821 and 823..827 of lvlprest.txt (822 is Izual's spot and is not used). The Files column is 5 everywhere but Mesa 3's four larger pieces (4).
// The weights make the big pieces common without crowding out the small ones that finish the edges.
const FillSet kSets[kFillSets] = {
	{"Mesa 1", {{812, 3, 3, 5, 4}, {813, 2, 2, 5, 3}, {814, 2, 1, 5, 2}, {815, 1, 2, 5, 2}, {816, 1, 1, 5, 1}}},
	{"Mesa 2", {{817, 3, 3, 5, 4}, {818, 2, 2, 5, 3}, {819, 2, 1, 5, 2}, {820, 1, 2, 5, 2}, {821, 1, 1, 5, 1}}},
	{"Mesa 3", {{823, 3, 3, 4, 4}, {824, 2, 2, 4, 3}, {825, 2, 1, 4, 2}, {826, 1, 2, 4, 2}, {827, 1, 1, 5, 1}}},
};

struct Placed {
	int x, y;     // interior grid cell of the piece's top-left
	int fill;     // index into the set
};

const Fill *findFill(int lvlPrest, int *set) {
	for (int s = 0; s < kFillSets; ++s)
		for (const Fill &f : kSets[s].fills)
			if (f.lvlPrest == lvlPrest) {
				if (set != nullptr) *set = s;
				return &f;
			}
	return nullptr;
}

TileRect roomRect(const MesaOptions &o, int ax, int ay) {
	TileRect r;
	r.known = true;
	r.x = o.originX + (ax + kRingCells) * kGridTiles;
	r.y = o.originY + (ay + kRingCells) * kGridTiles;
	r.w = r.h = kRoomCells * kGridTiles;
	return r;
}

// Size problems, shared by the planner and the checker. Empty means the size is acceptable.
std::string sizeProblem(const MesaRequest &r) {
	if (r.cellsW < kMinCells || r.cellsH < kMinCells || r.cellsW > kMaxCellsW || r.cellsH > kMaxCellsH)
		return "a Mesa map is " + std::to_string(kMinCells) + " to " + std::to_string(kMaxCellsW) + " by " + std::to_string(kMinCells) + " to " + std::to_string(kMaxCellsH) +
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
std::vector<Placed> pack(int w, int h, const FillSet &set, layout::Pcg32 &rng) {
	std::vector<Placed> out;
	std::vector<char> used(static_cast<size_t>(w) * h, 0);
	for (int y = 0; y < h; ++y) {
		for (int x = 0; x < w; ++x) {
			if (used[static_cast<size_t>(y) * w + x]) continue;
			int fits[kFillsPerSet];
			int count = 0, total = 0;
			for (int i = 0; i < kFillsPerSet; ++i) {
				const Fill &f = set.fills[i];
				bool free = x + f.w <= w && y + f.h <= h;
				for (int dy = 0; free && dy < f.h; ++dy)
					for (int dx = 0; free && dx < f.w; ++dx) free = !used[static_cast<size_t>(y + dy) * w + x + dx];
				if (!free) continue;
				fits[count++] = i;
				total += f.weight;
			}
			int roll = static_cast<int>(rng.below(static_cast<uint32_t>(total)));  // total >= 1: the 8 x 8 piece always fits
			int pick = fits[count - 1];
			for (int k = 0; k < count; ++k) {
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

RoomCells chooseRoomCells(const MesaRequest &r) {
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

const FillSet &fillSet(int index) { return kSets[std::clamp(index, 0, kFillSets - 1)]; }

const EdgePieces kEdgePieces = {800, 801, 802, 799, 804, 805, 806, 803};

int edgePieceAt(int x, int y, int w, int h) {
	const bool west = x < 0, east = x >= w, north = y < 0, south = y >= h;
	if (north && west) return kEdgePieces.nw;
	if (north && east) return kEdgePieces.ne;
	if (south && east) return kEdgePieces.se;
	if (south && west) return kEdgePieces.sw;
	if (west) return kEdgePieces.west;
	if (east) return kEdgePieces.east;
	if (north) return kEdgePieces.north;
	if (south) return kEdgePieces.south;
	return 0;
}

bool mesaSizeCells(std::string_view size, int *cellsW, int *cellsH) {
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

MesaPlan planMesaMap(const MesaRequest &request) {
	MesaPlan out;
	const auto fail = [&out](std::string why) {
		out.ok = false;
		out.reason = std::move(why);
		out.stamps = StampPlan{};
		out.hash = 0;
		return out;
	};
	if (const std::string bad = sizeProblem(request); !bad.empty()) return fail(bad);

	const int w = request.cellsW, h = request.cellsH;
	out.cellsW = w;
	out.cellsH = h;
	layout::Pcg32 rng(layout::splitmix64(request.seed + 0x4D455341ull + 1));
	out.fillSet = static_cast<int>(rng.below(kFillSets));
	const FillSet &set = kSets[out.fillSet];
	const std::vector<Placed> placed = pack(w, h, set, rng);

	StampPlan &plan = out.stamps;
	plan.originX = request.options.originX;
	plan.originY = request.options.originY;
	plan.cellsW = w + 2 * kRingCells;
	plan.cellsH = h + 2 * kRingCells;
	plan.tilesW = plan.cellsW * kGridTiles;
	plan.tilesH = plan.cellsH * kGridTiles;
	plan.macroMinX = 0;
	plan.macroMinY = 0;
	plan.macroW = plan.cellsW;
	plan.macroH = plan.cellsH;
	const RoomCells rooms = chooseRoomCells(request);
	if (!rooms.known) return fail("internal: no entrance, boss and event cells fit a " + std::to_string(w) + " x " + std::to_string(h) + " map.");

	// The ring first, then the fill; sorted row-major by the piece's top-left grid cell so the order does not depend on how the pieces were made.
	for (int y = -kRingCells; y < h + kRingCells; ++y) {
		for (int x = -kRingCells; x < w + kRingCells; ++x) {
			if (x >= 0 && y >= 0 && x < w && y < h) continue;
			Stamp s;
			s.cx = x + kRingCells;
			s.cy = y + kRingCells;
			s.lvlPrest = edgePieceAt(x, y, w, h);
			s.macroX = s.cx;
			s.macroY = s.cy;
			plan.stamps.push_back(s);
		}
	}
	for (const Placed &p : placed) {
		Stamp s;
		s.cx = p.x + kRingCells;
		s.cy = p.y + kRingCells;
		s.lvlPrest = set.fills[p.fill].lvlPrest;
		s.macroX = s.cx;
		s.macroY = s.cy;
		const Fill &f = set.fills[p.fill];
		if (rooms.entrance.x >= p.x && rooms.entrance.x < p.x + f.w && rooms.entrance.y >= p.y && rooms.entrance.y < p.y + f.h) s.role = layout::Role::Entrance;
		plan.stamps.push_back(s);
	}
	std::sort(plan.stamps.begin(), plan.stamps.end(), [](const Stamp &a, const Stamp &b) { return a.cy != b.cy ? a.cy < b.cy : a.cx < b.cx; });
	// The file draws have their own stream, so pinning files does not move the layout. The entrance cell's piece is marked above; a ring piece has 3 files.
	layout::Pcg32 fileRng(layout::splitmix64(request.seed + 0x46494C45ull));
	for (Stamp &s : plan.stamps) {
		const Fill *f = findFill(s.lvlPrest, nullptr);
		const int files = f != nullptr ? f->files : kBorderFiles;
		s.file = request.options.pinFiles ? static_cast<int>(fileRng.below(static_cast<uint32_t>(files))) : -1;
	}
	plan.entrance = roomRect(request.options, rooms.entrance.x, rooms.entrance.y);
	plan.boss = roomRect(request.options, rooms.boss.x, rooms.boss.y);
	plan.event = roomRect(request.options, rooms.event.x, rooms.event.y);

	out.ok = true;
	out.hash = stampPlanHash(plan);
	if (const std::vector<std::string> bad = checkMesaPlan(request, out); !bad.empty()) {
		const std::string why = bad.front() + (bad.size() > 1 ? " (and " + std::to_string(bad.size() - 1) + " more)" : std::string());
		return fail("the Mesa plan failed its own check: " + why);
	}
	return out;
}

// The checker's own side-to-piece table (not kEdgePieces). Rows are north, middle, south; columns west, middle, east.
static int checkEdgePiece(int x, int y, int w, int h) {
	static const int table[3][3] = {{804, 801, 805}, {800, 0, 802}, {803, 799, 806}};
	const int col = x < 0 ? 0 : (x >= w ? 2 : 1), row = y < 0 ? 0 : (y >= h ? 2 : 1);
	return table[row][col];
}

// The fill piece's size from its id alone: the checker does not read the planner's table for this. Offsets in a set: 24 x 24, 16 x 16, 16 x 8, 8 x 16, 8 x 8.
static bool checkFillSize(int lvlPrest, int *set, int *w, int *h) {
	static const int base[3] = {812, 817, 823};
	static const int cw[5] = {3, 2, 2, 1, 1}, ch[5] = {3, 2, 1, 2, 1};
	for (int s = 0; s < 3; ++s) {
		const int i = lvlPrest - base[s];
		if (i >= 0 && i < 5) {
			*set = s;
			*w = cw[i];
			*h = ch[i];
			return true;
		}
	}
	return false;
}

std::vector<std::string> checkMesaPlan(const MesaRequest &request, const MesaPlan &plan) {
	std::vector<std::string> bad;
	if (const std::string size = sizeProblem(request); !size.empty()) {
		bad.push_back(size);
		return bad;
	}
	const int w = request.cellsW, h = request.cellsH, lw = w + 2 * kRingCells, lh = h + 2 * kRingCells;
	const StampPlan &sp = plan.stamps;
	if (plan.cellsW != w || plan.cellsH != h) bad.push_back("the plan's interior is not the request's");
	if (sp.tilesW != lw * kGridTiles || sp.tilesH != lh * kGridTiles || sp.cellsW != lw || sp.cellsH != lh || sp.macroW != lw || sp.macroH != lh || sp.macroMinX != 0 ||
		sp.macroMinY != 0 || sp.originX != request.options.originX || sp.originY != request.options.originY)
		bad.push_back("the stamp plan's level box does not match the request");
	if (plan.hash != stampPlanHash(sp)) bad.push_back("the plan's hash is not the hash of its stamps");
	if (!sp.voidCells.empty()) bad.push_back("a Mesa map covers its whole box but the plan lists void cells");
	if (plan.fillSet < 0 || plan.fillSet >= kFillSets) {
		bad.push_back("the plan's fill set is not 0, 1 or 2");
		return bad;
	}
	// Every grid cell of the level is covered by exactly one stamp.
	std::vector<int> cover(static_cast<size_t>(lw) * lh, 0);
	int prevY = -1, prevX = -1;
	for (const Stamp &s : sp.stamps) {
		const std::string where = "the stamp at grid cell (" + std::to_string(s.cx) + "," + std::to_string(s.cy) + ")";
		if (s.cy < prevY || (s.cy == prevY && s.cx <= prevX)) bad.push_back(where + " is out of row-major order");
		prevY = s.cy;
		prevX = s.cx;
		if (s.macroX != s.cx || s.macroY != s.cy || s.mask != 0) bad.push_back(where + " has the wrong macro cell or mask");
		int pw = 1, ph = 1, files = kBorderFiles;
		const int ix = s.cx - kRingCells, iy = s.cy - kRingCells;  // interior coordinates
		const bool ring = ix < 0 || iy < 0 || ix >= w || iy >= h;
		if (ring) {
			if (s.cx < 0 || s.cy < 0 || s.cx >= lw || s.cy >= lh) {
				bad.push_back(where + " is outside the level");
				continue;
			}
			if (s.lvlPrest != checkEdgePiece(ix, iy, w, h)) bad.push_back(where + " holds piece " + std::to_string(s.lvlPrest) + ", not the ring piece for that side");
		} else {
			int set = 0;
			if (!checkFillSize(s.lvlPrest, &set, &pw, &ph)) {
				bad.push_back(where + " holds piece " + std::to_string(s.lvlPrest) + ", which is not a Mesa fill piece");
				continue;
			}
			if (set != plan.fillSet) bad.push_back(where + " is from fill set " + std::to_string(set + 1) + ", not the plan's " + std::to_string(plan.fillSet + 1));
			if (ix + pw > w || iy + ph > h) bad.push_back(where + " reaches past the interior");
			files = findFill(s.lvlPrest, nullptr) != nullptr ? findFill(s.lvlPrest, nullptr)->files : 0;
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

	// The rooms.
	const RoomCells rooms = chooseRoomCells(request);
	const auto same = [](const TileRect &a, const TileRect &b) { return a.known == b.known && a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h; };
	if (!rooms.known) {
		bad.push_back("no entrance, boss and event cells fit the map");
		return bad;
	}
	if (!same(sp.entrance, roomRect(request.options, rooms.entrance.x, rooms.entrance.y))) bad.push_back("the entrance rectangle is not on its cell");
	if (!same(sp.boss, roomRect(request.options, rooms.boss.x, rooms.boss.y))) bad.push_back("the boss rectangle is not on its cell");
	if (!same(sp.event, roomRect(request.options, rooms.event.x, rooms.event.y))) bad.push_back("the event rectangle is not on its cell");
	const TileRect *rects[3] = {&sp.entrance, &sp.boss, &sp.event};
	for (int i = 0; i < 3; ++i) {
		const TileRect &r = *rects[i];
		const int ox = sp.originX + kRingCells * kGridTiles, oy = sp.originY + kRingCells * kGridTiles;
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

std::string mesaPlanAscii(const MesaPlan &plan) {
	const StampPlan &sp = plan.stamps;
	std::vector<std::string> rows(static_cast<size_t>(sp.cellsH), std::string(static_cast<size_t>(sp.cellsW), '?'));
	for (const Stamp &s : sp.stamps) {
		int set = 0, pw = 1, ph = 1;
		char c = '+';
		if (checkFillSize(s.lvlPrest, &set, &pw, &ph)) c = pw == 3 ? '3' : (pw == 2 && ph == 2 ? '2' : (pw == 2 ? 'h' : (ph == 2 ? 'v' : '1')));
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

} // namespace cartographer::outdoor::mesa
