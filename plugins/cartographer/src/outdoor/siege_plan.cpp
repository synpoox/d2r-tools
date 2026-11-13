#include "siege_plan.h"

#include <algorithm>
#include <utility>

#include "outdoor_plan.h"

namespace cartographer::outdoor::siege {

namespace {

// Sockets, from the corridor of open rows on each edge of the DS1 files (docs/reverse-engineering/cartographer-siege.md):
//   A rows 5..40   B rows 5..39   C rows 5..37   D rows 7..40   E rows 10..38   (wide seams)
//   F 21..31   G 20..24   H 20..22   I 21..25   J 14..23 (west side of 873 reads 13..23: one wall row apart)   (funnel seams)
const std::vector<Piece> kPieces = {
	{865, "Act 5 - Siege To Town", "Expansion/Siege/striptownsiege.ds1", Role::EastCap, 'A', '\0'},
	{866, "Act 5 - Siege Strip 1", "Expansion/Siege/strip01.ds1", Role::Free, 'B', 'A'},
	{867, "Act 5 - Siege Strip 2", "Expansion/Siege/strip02.ds1", Role::Free, 'A', 'B'},
	{868, "Act 5 - Siege Strip 3", "Expansion/Siege/strip03.ds1", Role::Fixed, 'C', 'A'},
	{869, "Act 5 - Siege Strip 4", "Expansion/Siege/strip04.ds1", Role::Fixed, 'F', 'C'},
	{870, "Act 5 - Siege Strip 5", "Expansion/Siege/strip05.ds1", Role::Fixed, 'G', 'F'},
	{871, "Act 5 - Siege Strip 6", "Expansion/Siege/strip06.ds1", Role::Fixed, 'I', 'G'},
	{872, "Act 5 - Siege Strip 7", "Expansion/Siege/strip07.ds1", Role::Fixed, 'H', 'I'},
	{873, "Act 5 - Siege Strip 8", "Expansion/Siege/strip08.ds1", Role::Fixed, 'J', 'H'},
	{874, "Act 5 - Siege Strip 9", "Expansion/Siege/strip09.ds1", Role::Fixed, 'D', 'J'},
	{875, "Act 5 - Siege Strip 10", "Expansion/Siege/strip10.ds1", Role::Fixed, 'A', 'D'},
	{876, "Act 5 - Siege Strip 11", "Expansion/Siege/strip11.ds1", Role::Free, 'A', 'A'},
	{877, "Act 5 - Siege Strip 12", "Expansion/Siege/strip12.ds1", Role::Free, 'B', 'A'},
	{878, "Act 5 - Siege Strip 13", "Expansion/Siege/strip13.ds1", Role::Fixed, 'E', 'B'},
	{879, "Act 5 - Siege To Barricade", "Expansion/Siege/stripseigebar.ds1", Role::WestCap, '\0', 'E'},
};

const std::vector<int> kFunnel = {875, 874, 873, 872, 871, 870, 869, 868};

constexpr int kWestCap = 879;
constexpr int kStripThirteen = 878;
constexpr int kEastCap = 865;

// A wide strip has a wide seam on both sides: the boss and the event stand on these. That includes 868, 875 and 878 (their seams are wide too: the ends of the funnel block and the west cap's neighbour), not the funnel's narrow middle or the caps.
bool isWide(const Piece &p) {
	const auto wide = [](char c) { return c >= 'A' && c <= 'E'; };
	return p.role != Role::WestCap && p.role != Role::EastCap && wide(p.west) && wide(p.east);
}

TileRect cellRect(const SiegeOptions &o, int index) {
	TileRect r;
	r.known = true;
	r.x = o.originX + index * kStripWidth;
	r.y = o.originY;
	r.w = kStripWidth;
	r.h = kStripHeight;
	return r;
}

// Size problems, shared by the planner and the checker. Empty means the size is acceptable.
std::string sizeProblem(const SiegeRequest &r) {
	if (r.strips < kMinStrips || r.strips > kMaxStrips)
		return "a Siege map has " + std::to_string(kMinStrips) + " to " + std::to_string(kMaxStrips) + " strips, not " + std::to_string(r.strips) + ".";
	const int tilesW = r.strips * kStripWidth, tilesH = kStripHeight;
	if ((r.options.maxTilesX > 0 && tilesW > r.options.maxTilesX) || (r.options.maxTilesY > 0 && tilesH > r.options.maxTilesY)) {
		auto limit = [](int v) { return v > 0 ? std::to_string(v) : std::string("unlimited"); };
		return "the map needs " + std::to_string(tilesW) + " x " + std::to_string(tilesH) + " tiles, more than the slot level's " + limit(r.options.maxTilesX) +
			" x " + limit(r.options.maxTilesY) + ".";
	}
	return {};
}

// The whole line for a left run, a right run and the funnel block between them.
std::vector<int> buildLine(const std::vector<int> &left, const std::vector<int> &right) {
	std::vector<int> line = {kWestCap, kStripThirteen};
	line.insert(line.end(), left.begin(), left.end());
	line.insert(line.end(), kFunnel.begin(), kFunnel.end());
	line.insert(line.end(), right.begin(), right.end());
	line.push_back(kEastCap);
	return line;
}

// True when every neighbouring pair of the line has the same socket on the shared seam.
bool seamsMatch(const std::vector<int> &line) {
	for (size_t i = 0; i + 1 < line.size(); ++i) {
		const Piece *a = findPiece(line[i]), *b = findPiece(line[i + 1]);
		if (a == nullptr || b == nullptr || a->east == '\0' || a->east != b->west) return false;
	}
	return true;
}

// Every placement of `count` distinct free strips on the two sides of the funnel block whose seams all match, in a fixed order (pool order,
// then the split point). The seed picks one of them.
void collectLines(const std::vector<int> &pool, size_t count, std::vector<int> &chosen, std::vector<bool> &used, std::vector<std::vector<int>> &out) {
	if (chosen.size() == count) {
		for (size_t split = 0; split <= chosen.size(); ++split) {
			const std::vector<int> left(chosen.begin(), chosen.begin() + static_cast<std::ptrdiff_t>(split));
			const std::vector<int> right(chosen.begin() + static_cast<std::ptrdiff_t>(split), chosen.end());
			std::vector<int> line = buildLine(left, right);
			if (seamsMatch(line)) out.push_back(std::move(line));
		}
		return;
	}
	for (size_t i = 0; i < pool.size(); ++i) {
		if (used[i]) continue;
		used[i] = true;
		chosen.push_back(pool[i]);
		collectLines(pool, count, chosen, used, out);
		chosen.pop_back();
		used[i] = false;
	}
}

std::vector<int> freePool() {
	std::vector<int> pool;
	for (const Piece &p : kPieces)
		if (p.role == Role::Free) pool.push_back(p.lvlPrest);
	return pool;
}

// The cell of the arrival: the strip next to the town strip, always a wide strip.
int entranceIndex(int strips) { return strips - 2; }

// The boss and the event cells: wide strips, distinct from each other and from the entrance. The boss goes to the wide strip farthest from the
// entrance; the event to the one whose nearer neighbour of the two is farthest. Ties are broken by the seed. `known` stays false when the map
// has no wide strip left for the event (it never lacks one for a boss).
struct RoomCells {
	bool bossKnown = false;
	bool eventKnown = false;
	int boss = 0, event = 0;
};

RoomCells chooseRoomCells(const SiegeRequest &r, const std::vector<int> &line) {
	RoomCells out;
	const int entrance = entranceIndex(static_cast<int>(line.size()));
	std::vector<GridCell> wide;
	for (size_t i = 0; i < line.size(); ++i) {
		const Piece *p = findPiece(line[i]);
		if (p != nullptr && isWide(*p) && static_cast<int>(i) != entrance) wide.push_back({static_cast<int>(i), 0});
	}
	if (wide.empty()) return out;
	layout::Pcg32 rng(layout::splitmix64(r.seed + 0x524F4F4Dull));
	GridCell boss;
	if (!pickFarthestCell(wide, {{entrance, 0}}, rng, boss)) return out;
	out.bossKnown = true;
	out.boss = boss.x;
	std::vector<GridCell> rest;
	for (const GridCell &c : wide)
		if (c != boss) rest.push_back(c);
	GridCell event;
	if (pickFarthestCell(rest, {{entrance, 0}, boss}, rng, event)) {
		out.eventKnown = true;
		out.event = event.x;
	}
	return out;
}

} // namespace

const char *roleName(Role role) {
	switch (role) {
	case Role::WestCap: return "west cap";
	case Role::EastCap: return "east cap";
	case Role::Fixed: return "fixed strip";
	case Role::Free: return "free strip";
	}
	return "?";
}

const std::vector<Piece> &pieces() { return kPieces; }

const Piece *findPiece(int lvlPrest) {
	for (const Piece &p : kPieces)
		if (p.lvlPrest == lvlPrest) return &p;
	return nullptr;
}

const std::vector<int> &funnelBlock() { return kFunnel; }

bool siegeSizeStrips(std::string_view size, int *strips) {
	const std::string s = lowerAscii(size);
	int n = 0;
	if (s.empty() || s == "large") n = 14;
	else if (s == "small") n = 12;
	else if (s == "medium") n = 13;
	else if (s == "extralarge") n = 15;
	else return false;
	if (strips) *strips = n;
	return true;
}

SiegePlan planSiegeMap(const SiegeRequest &request) {
	SiegePlan out;
	const auto fail = [&out](std::string why) {
		out.ok = false;
		out.reason = std::move(why);
		out.strips.clear();
		out.stamps = StampPlan{};
		out.hash = 0;
		return out;
	};
	if (const std::string bad = sizeProblem(request); !bad.empty()) return fail(bad);

	const std::vector<int> pool = freePool();
	std::vector<std::vector<int>> lines;
	std::vector<int> chosen;
	std::vector<bool> used(pool.size(), false);
	collectLines(pool, static_cast<size_t>(request.strips - kFixedStrips), chosen, used, lines);
	if (lines.empty()) return fail("internal: no order of the free strips has matching seams.");
	layout::Pcg32 rng(layout::splitmix64(request.seed + 0x53494547ull));
	out.strips = lines[rng.below(static_cast<uint32_t>(lines.size()))];

	// The file draws have their own stream, so pinning files does not move the layout. Every strip has one file, so the draw is always 0.
	layout::Pcg32 fileRng(layout::splitmix64(request.seed + 0x46494C45ull));

	StampPlan &plan = out.stamps;
	plan.originX = request.options.originX;
	plan.originY = request.options.originY;
	const int n = static_cast<int>(out.strips.size());
	plan.tilesW = n * kStripWidth;
	plan.tilesH = kStripHeight;
	const int perX = kStripWidth / kGridTiles;
	plan.cellsW = n * perX;
	plan.cellsH = kStripHeight / kGridTiles;
	plan.macroMinX = 0;
	plan.macroMinY = 0;
	plan.macroW = n;
	plan.macroH = 1;
	const int entrance = entranceIndex(n);
	for (int i = 0; i < n; ++i) {
		Stamp s;
		s.cx = i * perX;
		s.cy = 0;
		s.lvlPrest = out.strips[static_cast<size_t>(i)];
		s.file = request.options.pinFiles ? static_cast<int>(fileRng.below(1u)) : -1;
		s.macroX = i;
		s.macroY = 0;
		s.mask = 0;
		s.role = i == entrance ? layout::Role::Entrance : layout::Role::Path;
		plan.stamps.push_back(s);
	}
	plan.entrance = cellRect(request.options, entrance);
	const RoomCells rooms = chooseRoomCells(request, out.strips);
	if (rooms.bossKnown) plan.boss = cellRect(request.options, rooms.boss);
	if (rooms.eventKnown) plan.event = cellRect(request.options, rooms.event);

	out.ok = true;
	out.hash = stampPlanHash(plan);
	if (const std::vector<std::string> bad = checkSiegePlan(request, out); !bad.empty()) {
		const std::string why = bad.front() + (bad.size() > 1 ? " (and " + std::to_string(bad.size() - 1) + " more)" : std::string());
		return fail("the Siege plan failed its own check: " + why);
	}
	return out;
}

std::vector<std::string> checkSiegePlan(const SiegeRequest &request, const SiegePlan &plan) {
	std::vector<std::string> bad;
	if (const std::string size = sizeProblem(request); !size.empty()) {
		bad.push_back(size);
		return bad;
	}
	const int n = request.strips;
	if (plan.strips.size() != static_cast<size_t>(n)) {
		bad.push_back("the plan has " + std::to_string(plan.strips.size()) + " strips, the request asks for " + std::to_string(n));
		return bad;
	}
	const StampPlan &sp = plan.stamps;
	const int perX = kStripWidth / kGridTiles;
	if (sp.tilesW != n * kStripWidth || sp.tilesH != kStripHeight || sp.cellsW != n * perX || sp.cellsH != kStripHeight / kGridTiles || sp.macroW != n || sp.macroH != 1 ||
		sp.originX != request.options.originX || sp.originY != request.options.originY)
		bad.push_back("the stamp plan's level box does not match the request");
	if (sp.macroMinX != 0 || sp.macroMinY != 0) bad.push_back("the stamp plan's macro grid does not start at 0, 0");
	if (plan.hash != stampPlanHash(sp)) bad.push_back("the plan's hash is not the hash of its stamps");
	if (sp.stamps.size() != plan.strips.size()) {
		bad.push_back("the plan has " + std::to_string(sp.stamps.size()) + " stamps for " + std::to_string(plan.strips.size()) + " strips");
		return bad;
	}
	if (!sp.voidCells.empty()) bad.push_back("a Siege map is a full line but the plan lists void cells");

	// The line: west cap, 878, free strips, the funnel block, free strips, east cap; the free strips are each used once.
	const std::vector<int> &line = plan.strips;
	if (line.front() != kWestCap) bad.push_back("the line does not start with the barricade strip 879");
	if (line.back() != kEastCap) bad.push_back("the line does not end with the town strip 865");
	if (line.size() > 1 && line[1] != kStripThirteen) bad.push_back("the second strip is not 878");
	const auto at = std::search(line.begin(), line.end(), kFunnel.begin(), kFunnel.end());
	if (at == line.end()) bad.push_back("the funnel block 875..868 is missing or out of order");
	std::vector<int> seen;
	for (size_t i = 0; i < line.size(); ++i) {
		const Piece *p = findPiece(line[i]);
		const std::string where = "strip " + std::to_string(i + 1);
		if (p == nullptr) {
			bad.push_back(where + " has piece " + std::to_string(line[i]) + ", which is not a Siege piece (865..879)");
			continue;
		}
		if (std::find(seen.begin(), seen.end(), line[i]) != seen.end()) bad.push_back(where + " repeats piece " + std::to_string(line[i]));
		seen.push_back(line[i]);
		if ((p->role == Role::WestCap) != (i == 0) || (p->role == Role::EastCap) != (i + 1 == line.size()))
			bad.push_back(where + " has a " + roleName(p->role) + " where it does not belong");
		const Stamp &s = sp.stamps[i];
		const int x = static_cast<int>(i);
		if (s.macroX != x || s.macroY != 0 || s.cx != x * perX || s.cy != 0) bad.push_back(where + " is not stamped at its place in order, aligned to the 8-tile grid");
		if (s.lvlPrest != line[i]) bad.push_back(where + " stamps a different piece than the line holds");
		if (s.file < -1 || s.file >= 1) bad.push_back(where + " has file index " + std::to_string(s.file) + ", out of range for piece " + std::to_string(line[i]));
		if (!request.options.pinFiles && s.file != -1) bad.push_back(where + " pins a file index though pinFiles is off");
		const layout::Role role = x == n - 2 ? layout::Role::Entrance : layout::Role::Path;
		if (s.role != role || s.mask != 0) bad.push_back(where + " has the wrong role or mask");
		if (i + 1 < line.size()) {
			const Piece *next = findPiece(line[i + 1]);
			const char out = p->east, in = next != nullptr ? next->west : '\0';
			if (out == '\0' || in == '\0' || out != in)
				bad.push_back(where + " ends with seam " + (out ? std::string(1, out) : std::string("none")) + " but the next strip starts with " +
					(in ? std::string(1, in) : std::string("none")));
		}
	}
	// The free strips on the line are the ones the size asks for, from the pool.
	size_t freeCount = 0;
	for (int id : line) {
		const Piece *p = findPiece(id);
		if (p != nullptr && p->role == Role::Free) ++freeCount;
	}
	if (static_cast<int>(freeCount) != n - kFixedStrips) bad.push_back("the line holds " + std::to_string(freeCount) + " free strips, the size asks for " + std::to_string(n - kFixedStrips));

	const TileRect want = cellRect(request.options, entranceIndex(n));
	const auto same = [](const TileRect &a, const TileRect &b) { return a.known == b.known && a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h; };
	if (!same(sp.entrance, want)) bad.push_back("the entrance rectangle is not on strip " + std::to_string(entranceIndex(n) + 1));
	const RoomCells rooms = chooseRoomCells(request, line);
	if (sp.boss.known != rooms.bossKnown || sp.event.known != rooms.eventKnown) {
		bad.push_back("the plan's boss or event rectangle is missing or has no room");
	} else {
		if (rooms.bossKnown && !same(sp.boss, cellRect(request.options, rooms.boss))) bad.push_back("the boss rectangle is not on strip " + std::to_string(rooms.boss + 1));
		if (rooms.eventKnown && !same(sp.event, cellRect(request.options, rooms.event))) bad.push_back("the event rectangle is not on strip " + std::to_string(rooms.event + 1));
		if (rooms.bossKnown && same(sp.boss, sp.entrance)) bad.push_back("the boss and the entrance are the same strip");
		if (rooms.eventKnown && (same(sp.event, sp.entrance) || same(sp.event, sp.boss))) bad.push_back("the event is on the entrance or boss strip");
	}
	return bad;
}

std::string siegePlanAscii(const SiegePlan &plan) {
	std::string out;
	for (size_t i = 0; i < plan.strips.size(); ++i) {
		const Piece *p = findPiece(plan.strips[i]);
		if (i) out += ' ';
		if (!p) { out += "??"; continue; }
		out += p->west ? p->west : '-';
		out += p->east ? p->east : '-';
	}
	out += '\n';
	return out;
}

} // namespace cartographer::outdoor::siege
