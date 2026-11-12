#include "snow_plan.h"

#include <algorithm>
#include <cctype>
#include <utility>

#include "rng.h"

namespace cartographer::outdoor::snow {

namespace {

std::string lower(std::string_view text) {
	std::string out(text);
	for (char &c : out) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
	return out;
}

bool isHorizontal(Orientation o) { return o == Orientation::Horizontal; }

// Socket letters of an axis: A and B for a horizontal line (fence rows 23 and 58 of 80 on the W and E edges), C and D for a vertical one
// (fence columns 25 and 55 on the N and S edges).
char firstLetter(Orientation o) { return isHorizontal(o) ? 'a' : 'c'; }

Kind runKind(Orientation o) { return isHorizontal(o) ? Kind::RunH : Kind::RunV; }
Kind startCapKind(Orientation o) { return isHorizontal(o) ? Kind::CapWest : Kind::CapNorth; }
Kind endCapKind(Orientation o) { return isHorizontal(o) ? Kind::CapEast : Kind::CapSouth; }

const std::vector<Piece> kPieces = {
	{987, "Act 5 - Barricade 1 Snow", "Expansion/Siege/snowbarrAA1.ds1", Kind::RunH, 'a', 'a', 2},
	{988, "Act 5 - Barricade 2 Snow", "Expansion/Siege/snowbarrBB1.ds1", Kind::RunH, 'b', 'b', 1},
	{989, "Act 5 - Barricade 3 Snow", "Expansion/Siege/snowbarrCC1.ds1", Kind::RunV, 'c', 'c', 1},
	{990, "Act 5 - Barricade 4 Snow", "Expansion/Siege/snowbarrDD1.ds1", Kind::RunV, 'd', 'd', 1},
	{991, "Act 5 - Barricade 5 Snow", "Expansion/Siege/snowbarrAB1.ds1", Kind::RunH, 'a', 'b', 1},
	{992, "Act 5 - Barricade 6 Snow", "Expansion/Siege/snowbarrBA1.ds1", Kind::RunH, 'b', 'a', 1},
	{993, "Act 5 - Barricade 7 Snow", "Expansion/Siege/snowbarrCD1.ds1", Kind::RunV, 'c', 'd', 1},
	{994, "Act 5 - Barricade 8 Snow", "Expansion/Siege/snowbarrDC1.ds1", Kind::RunV, 'd', 'c', 1},
	{995, "Act 5 - Barricade 9 Snow", "Expansion/Siege/snowbarrBR1.ds1", Kind::CapEast, 'b', '\0', 1},
	{996, "Act 5 - Barricade 10 Snow", "Expansion/Siege/snowbarrAR1.ds1", Kind::CapEast, 'a', '\0', 1},
	{997, "Act 5 - Barricade 11 Snow", "Expansion/Siege/snowbarrFB1.ds1", Kind::CapWest, '\0', 'b', 1},
	{998, "Act 5 - Barricade 12 Snow", "Expansion/Siege/snowbarrFA1.ds1", Kind::CapWest, '\0', 'a', 1},
	{999, "Act 5 - Barricade 13 Snow", "Expansion/Siege/snowbarrCR1.ds1", Kind::CapSouth, 'c', '\0', 1},
	{1000, "Act 5 - Barricade 14 Snow", "Expansion/Siege/snowbarrDR1.ds1", Kind::CapSouth, 'd', '\0', 1},
	{1001, "Act 5 - Barricade 15 Snow", "Expansion/Siege/snowbarrFC1.ds1", Kind::CapNorth, '\0', 'c', 1},
	{1002, "Act 5 - Barricade 16 Snow", "Expansion/Siege/snowbarrFD1.ds1", Kind::CapNorth, '\0', 'd', 1},
};

// The pieces of one kind whose joining socket is `letter`: for a run or an end cap the start socket, for a start cap the end socket.
std::vector<const Piece *> candidates(Kind kind, char letter) {
	std::vector<const Piece *> out;
	const bool startCap = kind == Kind::CapWest || kind == Kind::CapNorth;
	for (const Piece &p : kPieces)
		if (p.kind == kind && (startCap ? p.end : p.start) == letter) out.push_back(&p);
	return out;
}

std::string cellName(int x, int y) {
	return "cell (" + std::to_string(x) + "," + std::to_string(y) + ")";
}

TileRect cellRect(const SnowOptions &o, int x, int y) {
	TileRect r;
	r.known = true;
	r.x = o.originX + x * kCellTiles;
	r.y = o.originY + y * kCellTiles;
	r.w = kCellTiles;
	r.h = kCellTiles;
	return r;
}

// The cell the arrival goes to: the first run piece of the middle line, never a cap (a cap has an empty side).
void entranceCell(Orientation o, int cellsW, int cellsH, int *x, int *y) {
	if (isHorizontal(o)) {
		*x = 1;
		*y = cellsH / 2;
	} else {
		*x = cellsW / 2;
		*y = 1;
	}
}

Orientation resolveOrientation(const SnowRequest &r) {
	if (!r.autoOrientation) return r.orientation;
	return (layout::splitmix64(r.seed ^ 0x4F52494Eull) & 1u) ? Orientation::Vertical : Orientation::Horizontal;
}

// Size problems, shared by the planner and the checker. Empty means the size is acceptable.
std::string sizeProblem(const SnowRequest &r, Orientation o) {
	if (r.cellsW < 1 || r.cellsH < 1) return "the map needs at least one cell on each side.";
	if (r.cellsW > kMaxCells || r.cellsH > kMaxCells)
		return "the map is " + std::to_string(r.cellsW) + " x " + std::to_string(r.cellsH) + " cells, more than the " + std::to_string(kMaxCells) + " x " +
			std::to_string(kMaxCells) + " the Snow builder allows.";
	const int along = isHorizontal(o) ? r.cellsW : r.cellsH;
	if (along < kMinRunCells)
		return std::string("a ") + orientationName(o) + " Snow map needs at least " + std::to_string(kMinRunCells) + " cells along its lines (a cap, a run and a cap).";
	const int tilesW = r.cellsW * kCellTiles, tilesH = r.cellsH * kCellTiles;
	if ((r.options.maxTilesX > 0 && tilesW > r.options.maxTilesX) || (r.options.maxTilesY > 0 && tilesH > r.options.maxTilesY)) {
		auto limit = [](int v) { return v > 0 ? std::to_string(v) : std::string("unlimited"); };
		return "the map needs " + std::to_string(tilesW) + " x " + std::to_string(tilesH) + " tiles, more than the slot level's " + limit(r.options.maxTilesX) +
			" x " + limit(r.options.maxTilesY) + ".";
	}
	return {};
}

} // namespace

const char *kindName(Kind kind) {
	switch (kind) {
	case Kind::RunH: return "run-h";
	case Kind::RunV: return "run-v";
	case Kind::CapEast: return "cap-east";
	case Kind::CapWest: return "cap-west";
	case Kind::CapSouth: return "cap-south";
	case Kind::CapNorth: return "cap-north";
	}
	return "?";
}

const char *orientationName(Orientation orientation) {
	return orientation == Orientation::Horizontal ? "horizontal" : "vertical";
}

const std::vector<Piece> &pieces() { return kPieces; }

const Piece *findPiece(int lvlPrest) {
	for (const Piece &p : kPieces)
		if (p.lvlPrest == lvlPrest) return &p;
	return nullptr;
}

bool snowSizeCells(std::string_view size, int *cellsW, int *cellsH) {
	const std::string s = lower(size);
	int w = 0, h = 0;
	if (s.empty() || s == "medium") { w = 8; h = 5; }
	else if (s == "small") { w = 6; h = 4; }
	else if (s == "large") { w = 10; h = 7; }
	else return false;
	if (cellsW) *cellsW = w;
	if (cellsH) *cellsH = h;
	return true;
}

SnowPlan planSnowMap(const SnowRequest &request) {
	SnowPlan out;
	const Orientation o = resolveOrientation(request);
	out.orientation = o;
	const auto fail = [&out](std::string why) {
		out.ok = false;
		out.reason = std::move(why);
		out.grid.clear();
		out.stamps = StampPlan{};
		out.hash = 0;
		return out;
	};
	if (const std::string bad = sizeProblem(request, o); !bad.empty()) return fail(bad);

	const bool horizontal = isHorizontal(o);
	const int lines = horizontal ? request.cellsH : request.cellsW;
	const int along = horizontal ? request.cellsW : request.cellsH;
	out.cellsW = request.cellsW;
	out.cellsH = request.cellsH;
	out.grid.assign(static_cast<size_t>(request.cellsW) * request.cellsH, 0);
	std::vector<int> files(out.grid.size(), -1);

	// One random stream per line, so a line does not depend on how many draws the others made.
	for (int line = 0; line < lines; ++line) {
		layout::Pcg32 rng(layout::splitmix64(request.seed + 0x534E4F57ull) ^ (static_cast<uint64_t>(line + 1) * 0x9E3779B97F4A7C15ull));
		// The file draws have their own stream, so pinning files does not move the layout.
		layout::Pcg32 fileRng(layout::splitmix64(request.seed + 0x46494C45ull) ^ (static_cast<uint64_t>(line + 1) * 0x9E3779B97F4A7C15ull));
		const char first = firstLetter(o);
		const auto pick = [&](Kind kind, char letter, int index) -> bool {
			const std::vector<const Piece *> options = candidates(kind, letter);
			if (options.empty()) return false;
			const Piece *p = options[rng.below(static_cast<uint32_t>(options.size()))];
			const int x = horizontal ? index : line;
			const int y = horizontal ? line : index;
			const size_t at = static_cast<size_t>(y) * request.cellsW + x;
			out.grid[at] = p->lvlPrest;
			files[at] = request.options.pinFiles ? static_cast<int>(fileRng.below(static_cast<uint32_t>(p->files))) : -1;
			return true;
		};
		// The start cap's letter is drawn from the two letters of the axis; each cap kind has one piece per letter.
		const char startLetter = static_cast<char>(first + static_cast<int>(rng.below(2)));
		if (!pick(startCapKind(o), startLetter, 0)) return fail("internal: no start cap for socket " + std::string(1, startLetter) + ".");
		char carry = startLetter;
		for (int i = 1; i < along - 1; ++i) {
			if (!pick(runKind(o), carry, i)) return fail("internal: no run piece for socket " + std::string(1, carry) + ".");
			const int x = horizontal ? i : line;
			const int y = horizontal ? line : i;
			const Piece *placed = findPiece(out.grid[static_cast<size_t>(y) * request.cellsW + x]);
			carry = placed->end;
		}
		if (!pick(endCapKind(o), carry, along - 1)) return fail("internal: no end cap for socket " + std::string(1, carry) + ".");
	}

	StampPlan &plan = out.stamps;
	plan.originX = request.options.originX;
	plan.originY = request.options.originY;
	plan.tilesW = request.cellsW * kCellTiles;
	plan.tilesH = request.cellsH * kCellTiles;
	const int per = kCellTiles / kGridTiles;
	plan.cellsW = request.cellsW * per;
	plan.cellsH = request.cellsH * per;
	plan.macroMinX = 0;
	plan.macroMinY = 0;
	plan.macroW = request.cellsW;
	plan.macroH = request.cellsH;
	int ex = 0, ey = 0;
	entranceCell(o, request.cellsW, request.cellsH, &ex, &ey);
	for (int y = 0; y < request.cellsH; ++y) {
		for (int x = 0; x < request.cellsW; ++x) {
			const size_t at = static_cast<size_t>(y) * request.cellsW + x;
			Stamp s;
			s.cx = x * per;
			s.cy = y * per;
			s.lvlPrest = out.grid[at];
			s.file = files[at];
			s.macroX = x;
			s.macroY = y;
			s.mask = 0;
			s.role = (x == ex && y == ey) ? layout::Role::Entrance : layout::Role::Path;
			plan.stamps.push_back(s);
		}
	}
	plan.entrance = cellRect(request.options, ex, ey);

	out.ok = true;
	out.hash = stampPlanHash(plan);
	if (const std::vector<std::string> bad = checkSnowPlan(request, out); !bad.empty()) {
		const std::string why = bad.front() + (bad.size() > 1 ? " (and " + std::to_string(bad.size() - 1) + " more)" : std::string());
		return fail("the Snow plan failed its own check: " + why);
	}
	return out;
}

std::vector<std::string> checkSnowPlan(const SnowRequest &request, const SnowPlan &plan) {
	std::vector<std::string> bad;
	const Orientation o = plan.orientation;
	if (!request.autoOrientation && o != request.orientation) bad.push_back("the plan's orientation is not the requested one");
	if (const std::string size = sizeProblem(request, o); !size.empty()) {
		bad.push_back(size);
		return bad;
	}
	const int W = request.cellsW, H = request.cellsH;
	if (plan.cellsW != W || plan.cellsH != H || plan.grid.size() != static_cast<size_t>(W) * H) {
		bad.push_back("the plan's grid does not match the requested size");
		return bad;
	}
	const StampPlan &sp = plan.stamps;
	const int per = kCellTiles / kGridTiles;
	if (sp.tilesW != W * kCellTiles || sp.tilesH != H * kCellTiles || sp.cellsW != W * per || sp.cellsH != H * per || sp.macroW != W || sp.macroH != H ||
		sp.originX != request.options.originX || sp.originY != request.options.originY)
		bad.push_back("the stamp plan's level box does not match the request");
	if (sp.stamps.size() != plan.grid.size()) {
		bad.push_back("the plan has " + std::to_string(sp.stamps.size()) + " stamps for " + std::to_string(plan.grid.size()) + " cells");
		return bad;
	}
	if (!sp.voidCells.empty()) bad.push_back("a Snow map is a full rectangle but the plan lists void cells");

	const bool horizontal = isHorizontal(o);
	const int along = horizontal ? W : H;
	int ex = 0, ey = 0;
	entranceCell(o, W, H, &ex, &ey);
	for (int y = 0; y < H; ++y) {
		for (int x = 0; x < W; ++x) {
			const size_t at = static_cast<size_t>(y) * W + x;
			const std::string where = cellName(x, y);
			const Stamp &s = sp.stamps[at];
			if (s.macroX != x || s.macroY != y || s.cx != x * per || s.cy != y * per) bad.push_back(where + " is not stamped at its place in row-major order, aligned to the 16-tile grid");
			if (s.lvlPrest != plan.grid[at]) bad.push_back(where + " stamps a different piece than the grid holds");
			const Piece *p = findPiece(plan.grid[at]);
			if (!p) {
				bad.push_back(where + " has piece " + std::to_string(plan.grid[at]) + ", which is not a Snow piece (987..1002)");
				continue;
			}
			if (s.file < -1 || s.file >= p->files) bad.push_back(where + " has file index " + std::to_string(s.file) + ", out of range for piece " + std::to_string(p->lvlPrest));
			if (!request.options.pinFiles && s.file != -1) bad.push_back(where + " pins a file index though pinFiles is off");
			const int index = horizontal ? x : y;
			const Kind want = index == 0 ? startCapKind(o) : (index == along - 1 ? endCapKind(o) : runKind(o));
			if (p->kind != want) bad.push_back(where + " has a " + kindName(p->kind) + " piece where a " + kindName(want) + " piece belongs");
			const layout::Role role = (x == ex && y == ey) ? layout::Role::Entrance : layout::Role::Path;
			if (s.role != role || s.mask != 0) bad.push_back(where + " has the wrong role or mask");
			// Fence sockets: this cell's end socket must equal the next cell's start socket along the line.
			if (index < along - 1) {
				const Piece *next = findPiece(plan.grid[horizontal ? at + 1 : at + static_cast<size_t>(W)]);
				const char out = p->end, in = next ? next->start : '\0';
				if (out == '\0' || in == '\0' || out != in)
					bad.push_back(where + " ends with fence socket " + (out ? std::string(1, out) : std::string("none")) + " but the next cell starts with " +
						(in ? std::string(1, in) : std::string("none")));
			}
		}
	}
	const TileRect want = cellRect(request.options, ex, ey);
	if (!sp.entrance.known || sp.entrance.x != want.x || sp.entrance.y != want.y || sp.entrance.w != want.w || sp.entrance.h != want.h)
		bad.push_back("the entrance rectangle is not on " + cellName(ex, ey));
	if (sp.event.known || sp.boss.known) bad.push_back("a Snow plan has no event or boss rectangle yet");
	return bad;
}

std::string snowPlanAscii(const SnowPlan &plan) {
	std::string out;
	for (int y = 0; y < plan.cellsH; ++y) {
		for (int x = 0; x < plan.cellsW; ++x) {
			const Piece *p = findPiece(plan.grid[static_cast<size_t>(y) * plan.cellsW + x]);
			if (x) out += ' ';
			if (!p) { out += "??"; continue; }
			switch (p->kind) {
			case Kind::RunH:
			case Kind::RunV: out += p->start; out += p->end; break;
			case Kind::CapWest:
			case Kind::CapNorth: out += 'F'; out += p->end; break;
			case Kind::CapEast:
			case Kind::CapSouth: out += p->start; out += 'R'; break;
			}
		}
		out += '\n';
	}
	return out;
}

} // namespace cartographer::outdoor::snow
