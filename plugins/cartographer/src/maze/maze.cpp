// Cartographer maze steering core. See maze.h for the contract. Pure integer code.
#include "maze.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <set>

#include "rng.h"

namespace cartographer::maze {

namespace {

using layout::Point;

// Engine direction order: W, N, E, S.
constexpr int kDx[4] = {-1, 0, 1, 0};
constexpr int kDy[4] = {0, -1, 0, 1};
constexpr uint8_t kBitOfDir[4] = {layout::kWest, layout::kNorth, layout::kEast, layout::kSouth};
// Index into a DeadEndFamily (W, E, S, N) of a single exit bit.
int familyIndex(uint8_t bit)
{
	switch (bit) {
	case layout::kWest: return 0;
	case layout::kEast: return 1;
	case layout::kSouth: return 2;
	case layout::kNorth: return 3;
	default: return -1;
	}
}

int popcount4(uint8_t m) { return (m & 1) + ((m >> 1) & 1) + ((m >> 2) & 1) + ((m >> 3) & 1); }

// Mask letters in D2's piece-name order: N, S, E, W ("NSEW", "SEW", "NEW").
std::string maskLetters(uint8_t m)
{
	std::string s;
	if (m & layout::kNorth) s += 'N';
	if (m & layout::kSouth) s += 'S';
	if (m & layout::kEast) s += 'E';
	if (m & layout::kWest) s += 'W';
	return s;
}

Plan fallbackPlan(std::string reason)
{
	Plan p;
	p.steer = false;
	p.reason = std::move(reason);
	return p;
}

bool familyHas(const DeadEndFamily &f, int piece)
{
	if (!f.present) return false;
	for (int v : f.piece)
		if (v == piece) return true;
	return false;
}

} // namespace

int engineDir(uint8_t maskBit)
{
	for (int d = 0; d < 4; ++d)
		if (kBitOfDir[d] == maskBit) return d;
	return -1;
}

uint8_t maskBitOfEngineDir(int dir) { return dir >= 0 && dir < 4 ? kBitOfDir[dir] : 0; }

Capacity capacityAt(int roomTiles, int levelTilesX, int levelTilesY)
{
	Capacity c;
	if (roomTiles <= 0) return c;
	auto side = [&](int levelTiles, int &before, int &after) {
		if (levelTiles < roomTiles) return;
		const int offset = (levelTiles - roomTiles) / 2; // the first room is centred
		before = offset / roomTiles;
		after = (levelTiles - offset - roomTiles) / roomTiles;
	};
	side(levelTilesX, c.left, c.right);
	side(levelTilesY, c.up, c.down);
	return c;
}

Capacity maxCapacity(const Tileset &ts)
{
	Capacity c = capacityAt(ts.roomTiles, ts.maxLevelTiles, ts.maxLevelTiles);
	const int ring = std::max(ts.ringCells, 0); // the filler ring takes its cells on every side
	c.left = std::max(c.left - ring, 0);
	c.right = std::max(c.right - ring, 0);
	c.up = std::max(c.up - ring, 0);
	c.down = std::max(c.down - ring, 0);
	return c;
}

int levelTilesFor(const Tileset &ts, int roomsPerSide)
{
	return std::min((2 * std::max(roomsPerSide, 1) + 1) * ts.roomTiles, ts.maxLevelTiles);
}

Plan planMaze(const Tileset &ts, const layout::Layout &l)
{
	const std::string who = ts.name.empty() ? std::string("This tileset") : ts.name;

	if (!ts.steerable)
		return fallbackPlan(ts.notSteerableReason.empty() ? who + " grows its own rooms, so Cartographer cannot steer it yet."
		                                                    : ts.notSteerableReason);
	if (ts.roomTiles <= 0 || ts.maxLevelTiles < ts.roomTiles)
		return fallbackPlan(who + " has no usable room size, so it cannot hold a layout.");
	if (l.cells.size() != static_cast<size_t>(l.width) * static_cast<size_t>(l.height) || l.width <= 0 || l.height <= 0)
		return fallbackPlan("The layout is empty.");
	if (l.mainPath.size() < 2) return fallbackPlan("The layout has no main path.");
	if (!l.rulesMet) return fallbackPlan("This shape could not be laid out as asked, so the layout is not used. Use a smaller size or a simpler shape.");

	// Used cells (row-major), bounding box, and the pieces the tileset must have.
	const size_t total = l.cells.size();
	std::vector<int> roomOf(total, -1);
	int usedCount = 0;
	int minX = l.width, maxX = -1, minY = l.height, maxY = -1;
	for (int y = 0; y < l.height; ++y)
		for (int x = 0; x < l.width; ++x) {
			const layout::Cell &c = l.at(x, y);
			if (c.role == layout::Role::Empty) continue;
			++usedCount;
			minX = std::min(minX, x);
			maxX = std::max(maxX, x);
			minY = std::min(minY, y);
			maxY = std::max(maxY, y);
			if (c.mask == 0 || (c.mask & ~0x0F) != 0) return fallbackPlan("The layout has a room with no valid exits.");
			if ((ts.maskPieces & (1u << c.mask)) == 0)
				return fallbackPlan(who + " has no room piece with exits " + maskLetters(c.mask) + ", which this layout needs.");
		}
	if (usedCount < 2) return fallbackPlan("The layout has fewer than two rooms.");

	const int spanX = maxX - minX + 1, spanY = maxY - minY + 1;
	const Capacity maxCap = maxCapacity(ts);
	if (spanX > maxCap.across() || spanY > maxCap.tall())
		return fallbackPlan("This layout is " + std::to_string(spanX) + " rooms across and " + std::to_string(spanY) +
		                    " tall, but " + who + " levels hold at most " + std::to_string(maxCap.across()) + " across and " +
		                    std::to_string(maxCap.tall()) + " tall. Use a smaller size or a simpler shape.");

	// The one special call of 0x3eb850: the entrance. A maze has no exit (plan section 3.6): the far end is ordinary.
	const Point entranceCell = l.mainPath.front();
	bool placeEntrance = false;
	std::string entranceNote;
	{
		const uint8_t m = l.at(entranceCell.x, entranceCell.y).mask;
		if (!ts.prev.present)
			entranceNote = who + " has no special entrance room, so the entrance is an ordinary room.";
		else if (popcount4(m) != 1)
			entranceNote = "The entrance room has " + std::to_string(popcount4(m)) +
			               " exits, but the special entrance rooms have one, so it is an ordinary room.";
		else
			placeEntrance = true;
	}
	auto deferred = [&](Point p) { return placeEntrance && p == entranceCell; };

	// The first room: a used cell, not the deferred entrance, whose neighbourhood fits around the centre of the level.
	// Closest to the middle of the layout's bounding box wins; ties go to the first in row-major order. Each axis of the
	// level has (2 x k + 1) rooms (k starts at half the span, as the first room sits near the middle); the plan tries
	// the smallest total growth first, one axis at a time, and gives up only when even the largest level fails.
	Point root{-1, -1};
	int levelX = 0, levelY = 0;
	const int maxSteps = ts.maxLevelTiles / ts.roomTiles + 1;
	const int ring = std::max(ts.ringCells, 0);
	// The filler ring wants `ring` free cells around the layout. A layout that cannot have them in any level is planned in the largest
	// level with no margin and its ring is cut to what fits (a gap of void at that one side beats the game's own unsteered maze).
	const auto findRoot = [&](int lx, int ly, int margin) {
		levelX = lx;
		levelY = ly;
		const Capacity cap = capacityAt(ts.roomTiles, levelX, levelY);
		int bestScore = 0;
		for (int y = minY; y <= maxY; ++y)
			for (int x = minX; x <= maxX; ++x) {
				if (l.at(x, y).role == layout::Role::Empty || deferred(Point{x, y})) continue;
				if (x - minX + margin > cap.left || maxX - x + margin > cap.right || y - minY + margin > cap.up || maxY - y + margin > cap.down) continue;
				const int score = std::abs(2 * x - (minX + maxX)) + std::abs(2 * y - (minY + maxY));
				if (root.x < 0 || score < bestScore) {
					root = Point{x, y};
					bestScore = score;
				}
			}
	};
	bool searchedLargest = false;
	for (int total = 0; total <= 2 * maxSteps && root.x < 0 && !searchedLargest; ++total)
		for (int sx = 0; sx <= total && root.x < 0 && !searchedLargest; ++sx) {
			findRoot(levelTilesFor(ts, spanX / 2 + sx), levelTilesFor(ts, spanY / 2 + (total - sx)), ring);
			// Both axes at the maximum is the largest level; every later step would repeat it.
			if (root.x < 0 && levelX == ts.maxLevelTiles && levelY == ts.maxLevelTiles) searchedLargest = true;
		}
	bool clipped = false;
	if (root.x < 0 && ring > 0) {
		findRoot(ts.maxLevelTiles, ts.maxLevelTiles, 0);
		clipped = root.x >= 0;
	}
	if (root.x < 0)
		return fallbackPlan("No room of the layout can be the first room while the whole layout stays inside the largest level. Use a smaller size or a simpler shape.");

	// Walk the layout from the first room, breadth-first, in engine direction order. The entrance is a one-exit room
	// at the start of the path; it is made last so that D2R's "newest free room with this piece" scan in
	// 0x3eb850 finds exactly it.
	Plan plan;
	std::vector<int> parent;
	auto addRoom = [&](Point c, int parentIdx) {
		const int idx = static_cast<int>(plan.rooms.size());
		Room r;
		r.cell = c;
		r.dx = c.x - root.x;
		r.dy = c.y - root.y;
		r.mask = l.at(c.x, c.y).mask;
		r.role = l.at(c.x, c.y).role;
		plan.rooms.push_back(r);
		parent.push_back(parentIdx);
		roomOf[static_cast<size_t>(c.y) * l.width + c.x] = idx;
		return idx;
	};
	addRoom(root, -1);
	for (size_t head = 0; head < plan.rooms.size(); ++head) {
		const Point c = plan.rooms[head].cell;
		for (int d = 0; d < 4; ++d) {
			if ((plan.rooms[head].mask & kBitOfDir[d]) == 0) continue;
			const Point n{c.x + kDx[d], c.y + kDy[d]};
			if (!l.inside(n.x, n.y) || roomOf[static_cast<size_t>(n.y) * l.width + n.x] >= 0 || deferred(n)) continue;
			const int idx = addRoom(n, static_cast<int>(head));
			plan.adds.push_back(AddStep{static_cast<int>(head), idx, d});
		}
	}
	if (placeEntrance) { // made last
		const Point c = entranceCell;
		const int d = engineDir(l.at(c.x, c.y).mask);
		const Point p{c.x + kDx[d], c.y + kDy[d]};
		const int pi = l.inside(p.x, p.y) ? roomOf[static_cast<size_t>(p.y) * l.width + p.x] : -1;
		if (pi < 0) return fallbackPlan("The layout's entrance room is not joined to the rest.");
		const int idx = addRoom(c, pi);
		plan.adds.push_back(AddStep{pi, idx, (d + 2) & 3}); // d points from the end room to its neighbour; the add goes the other way
	}
	if (static_cast<int>(plan.rooms.size()) != usedCount) return fallbackPlan("The layout is not one connected set of rooms.");

	// Loop edges: every link between rooms that is not a tree edge.
	for (size_t a = 0; a < plan.rooms.size(); ++a)
		for (int d = 0; d < 4; ++d) {
			if ((plan.rooms[a].mask & kBitOfDir[d]) == 0) continue;
			const Point n{plan.rooms[a].cell.x + kDx[d], plan.rooms[a].cell.y + kDy[d]};
			const int b = roomOf[static_cast<size_t>(n.y) * l.width + n.x];
			if (b < 0) return fallbackPlan("The layout has an exit to nothing.");
			if (static_cast<size_t>(b) < a) continue; // each edge once, from its lower-numbered room
			if (parent[a] == b || parent[static_cast<size_t>(b)] == static_cast<int>(a)) continue;
			plan.links.push_back(LinkStep{static_cast<int>(a), b, d});
		}

	// The filler ring: closed cells around the layout, in the same breadth-first order (each parent is a room that already exists).
	if (ring > 0) {
		auto key = [](int dx, int dy) { return (static_cast<int64_t>(dx) << 32) ^ static_cast<uint32_t>(dy); };
		std::set<int64_t> taken;
		for (const Room &r : plan.rooms) taken.insert(key(r.dx, r.dy));
		std::set<int64_t> want;
		const Capacity box = capacityAt(ts.roomTiles, levelX, levelY); // only cells inside the level can be made
		for (const Room &r : plan.rooms)
			for (int oy = -ring; oy <= ring; ++oy)
				for (int ox = -ring; ox <= ring; ++ox) {
					const int fx = r.dx + ox, fy = r.dy + oy;
					if (taken.count(key(fx, fy)) == 0 && fx >= -box.left && fx <= box.right && fy >= -box.up && fy <= box.down) want.insert(key(fx, fy));
				}
		std::vector<Point> at; // offset of every room made so far, plan rooms first
		for (const Room &r : plan.rooms) at.push_back(Point{r.dx, r.dy});
		for (size_t head = 0; head < at.size(); ++head)
			for (int d = 0; d < 4; ++d) {
				const Point n{at[head].x + kDx[d], at[head].y + kDy[d]};
				const int64_t k = key(n.x, n.y);
				if (want.count(k) == 0 || taken.count(k) != 0) continue;
				taken.insert(k);
				plan.fills.push_back(AddStep{static_cast<int>(head), static_cast<int>(at.size()), d});
				plan.fillOffsets.push_back(n);
				at.push_back(n);
			}
		if (plan.fills.size() != want.size()) return fallbackPlan("The filler ring around the layout could not be joined to it.");
	}

	// Pieces, reserved rooms and the special calls.
	for (size_t i = 0; i < plan.rooms.size(); ++i) {
		Room &r = plan.rooms[i];
		r.piece = ts.pieceBase + r.mask;
		const bool reserved = r.role == layout::Role::Entrance || r.role == layout::Role::Event || r.role == layout::Role::Boss ||
		                      r.cell == entranceCell;
		r.fix = reserved ? Room::Fix::AfterWalk : Room::Fix::None;
		const int ri = static_cast<int>(i);
		if (r.role == layout::Role::Event) plan.eventRoom = ri;
		if (r.role == layout::Role::Boss) plan.bossRoom = ri;
	}
	{
		SpecialCall &sc = plan.entrance;
		sc.room = roomOf[static_cast<size_t>(entranceCell.y) * l.width + entranceCell.x];
		sc.place = placeEntrance;
		sc.note = entranceNote;
		Room &r = plan.rooms[static_cast<size_t>(sc.room)];
		sc.fromPiece = r.piece;
		if (placeEntrance) {
			sc.toPiece = ts.prev.piece[static_cast<size_t>(familyIndex(r.mask))];
			r.piece = sc.toPiece;
			r.special = 0;
			r.fix = Room::Fix::ByPlacer; // 0x3eb850 fixes it itself once its call places it
		}
	}

	plan.lvlMaze = LvlMazeRow{static_cast<int>(plan.rooms.size() + plan.fills.size()), ts.roomTiles, ts.roomTiles, 0};
	plan.ring.noOp = ts.callsRingGrower;
	plan.ring.graphInRing = ts.callsRingGrower && !ts.walkFollowsRing;
	plan.levelTilesX = levelX;
	plan.levelTilesY = levelY;
	plan.widthRooms = spanX;
	plan.heightRooms = spanY;
	plan.ringClipped = clipped;
	plan.steer = true;
	return plan;
}

SpecialSteer steerSpecial(const Tileset &ts, const Plan &plan, int toPiece, int priorPrevCalls)
{
	SpecialSteer s; // Suppress
	if (!plan.steer || priorPrevCalls != 0 || !plan.entrance.place || !familyHas(ts.prev, toPiece)) return s;
	s.action = SpecialSteer::Action::Redirect;
	s.fromPiece = plan.entrance.fromPiece;
	return s;
}

layout::Rules fitRules(const Tileset &ts, layout::Rules rules)
{
	const Capacity c = maxCapacity(ts);
	rules.width = std::min(rules.width, c.across());
	rules.height = std::min(rules.height, c.tall());
	return rules;
}

SeedPlan planForSeed(const Tileset &ts, const layout::Rules &rules, uint64_t seed)
{
	SeedPlan out;
	const layout::Rules fitted = fitRules(ts, rules);
	const std::string problem = layout::validateRules(fitted);
	if (!problem.empty()) {
		(void)problem; // generator field names mean nothing to a map author: say what they can change instead
		out.plan = fallbackPlan((ts.name.empty() ? std::string("The tileset") : ts.name) + " levels hold at most " +
		                        std::to_string(fitted.width) + " by " + std::to_string(fitted.height) +
		                        " rooms, which is too small for this shape. Use a smaller size or a simpler shape.");
		out.generatorUnmet = true;
		return out;
	}
	SeedPlan clippedFirst; // the first layout that fits only with its filler ring cut short; kept if no later layout has a whole ring
	for (int k = 0; k <= kRefits; ++k) {
		const uint64_t s = k == 0 ? seed : layout::splitmix64(seed ^ (kRefitSalt + static_cast<uint64_t>(k)));
		out.layout = layout::generate(fitted, s);
		out.plan = planMaze(ts, out.layout);
		out.refits = k;
		if (out.plan.steer && out.plan.ringClipped) {
			// The first room must be a used cell, and a layout as wide as the level allows has only one cell that centres it (often unused):
			// another layout of the same shape usually has a whole ring.
			if (!clippedFirst.plan.steer) clippedFirst = out;
			if (k < kRefits) continue;
			out = clippedFirst;
			break;
		}
		if (out.plan.steer) break;
		if (!out.layout.rulesMet) {
			// Even the generator's own fallback broke the shape's rules in this many rooms: more seeds will not help.
			out.plan = fallbackPlan((ts.name.empty() ? std::string("The tileset") : ts.name) + " levels hold at most " +
			                        std::to_string(fitted.width) + " by " + std::to_string(fitted.height) +
			                        " rooms, which is too small to build this shape. Use a smaller size or a simpler shape.");
			out.generatorUnmet = true;
			break;
		}
	}
	if (!out.plan.steer && clippedFirst.plan.steer) out = clippedFirst; // a later layout failed outright: the clipped one still beats the game's own maze
	return out;
}

uint64_t planHash(const Plan &p)
{
	uint64_t h = 1469598103934665603ull;
	auto mix = [&](uint64_t v) {
		for (int i = 0; i < 8; ++i) {
			h ^= (v >> (8 * i)) & 0xFF;
			h *= 1099511628211ull;
		}
	};
	auto mixi = [&](int v) { mix(static_cast<uint64_t>(static_cast<int64_t>(v))); };
	mixi(p.steer ? 1 : 0);
	for (const Room &r : p.rooms) {
		mixi(r.dx); mixi(r.dy); mixi(r.mask); mixi(r.piece); mixi(static_cast<int>(r.role)); mixi(static_cast<int>(r.fix)); mixi(r.special);
	}
	for (const AddStep &a : p.adds) { mixi(a.parent); mixi(a.room); mixi(a.dir); }
	for (const LinkStep &a : p.links) { mixi(a.a); mixi(a.b); mixi(a.dir); }
	for (const AddStep &a : p.fills) { mixi(a.parent); mixi(a.room); mixi(a.dir); }
	{ const SpecialCall &s = p.entrance; mixi(s.room); mixi(s.place ? 1 : 0); mixi(s.fromPiece); mixi(s.toPiece); }
	mixi(p.ring.noOp ? 1 : 0); mixi(p.ring.graphInRing ? 1 : 0);
	mixi(p.lvlMaze.rooms); mixi(p.lvlMaze.sizeX); mixi(p.lvlMaze.sizeY); mixi(p.lvlMaze.merge);
	mixi(p.eventRoom); mixi(p.bossRoom);
	mixi(p.levelTilesX); mixi(p.levelTilesY);
	return h;
}

} // namespace cartographer::maze
