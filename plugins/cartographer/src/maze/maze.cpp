// Cartographer maze steering core. See maze.h for the contract. Pure integer code.
#include "maze.h"

#include <algorithm>
#include <cstddef>
#include <cstdlib>

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

Capacity capacity(const Tileset &ts)
{
	Capacity c;
	if (ts.roomTiles <= 0) return c;
	auto side = [&](int levelTiles, int &before, int &after) {
		if (levelTiles < ts.roomTiles) return;
		const int offset = (levelTiles - ts.roomTiles) / 2; // the first room is centred
		before = offset / ts.roomTiles;
		after = (levelTiles - offset - ts.roomTiles) / ts.roomTiles;
	};
	side(ts.levelTilesX, c.left, c.right);
	side(ts.levelTilesY, c.up, c.down);
	return c;
}

Plan planMaze(const Tileset &ts, const layout::Layout &l)
{
	const std::string who = ts.name.empty() ? std::string("This tileset") : ts.name;

	if (!ts.steerable)
		return fallbackPlan(ts.notSteerableReason.empty() ? who + " grows its own rooms, so Cartographer cannot steer it yet."
		                                                    : ts.notSteerableReason);
	if (ts.roomTiles <= 0 || ts.levelTilesX < ts.roomTiles || ts.levelTilesY < ts.roomTiles)
		return fallbackPlan(who + " has no usable room size, so it cannot hold a layout.");
	if (l.cells.size() != static_cast<size_t>(l.width) * static_cast<size_t>(l.height) || l.width <= 0 || l.height <= 0)
		return fallbackPlan("The layout is empty.");
	if (l.mainPath.size() < 2) return fallbackPlan("The layout has no main path.");
	if (!l.rulesMet) return fallbackPlan("The layout generator could not meet the shape's rules, so the layout is not used.");

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
	const Capacity cap = capacity(ts);
	if (spanX > cap.across() || spanY > cap.tall())
		return fallbackPlan("The layout is " + std::to_string(spanX) + " rooms across and " + std::to_string(spanY) +
		                    " tall, but " + who + " levels hold at most " + std::to_string(cap.across()) + " across and " +
		                    std::to_string(cap.tall()) + " tall.");

	// The two special calls of 0x3eb850: the entrance and the exit (the boss room, or the path's last room).
	const Point entranceCell = l.mainPath.front();
	const Point exitCell = l.bossIdx >= 0 ? l.mainPath[static_cast<size_t>(l.bossIdx)] : l.mainPath.back();
	struct Target { SpecialKind kind; Point cell; const DeadEndFamily *family; bool place; std::string note; };
	Target targets[2] = {{SpecialKind::Entrance, entranceCell, &ts.entrance, false, std::string()},
	                     {SpecialKind::Exit, exitCell, &ts.exit, false, std::string()}};
	for (Target &t : targets) {
		const uint8_t m = l.at(t.cell.x, t.cell.y).mask;
		const char *what = t.kind == SpecialKind::Entrance ? "entrance" : "exit";
		if (!t.family->present)
			t.note = std::string(who) + " has no special " + what + " room, so the " + what + " is an ordinary room.";
		else if (popcount4(m) != 1)
			t.note = std::string("The ") + what + " room has " + std::to_string(popcount4(m)) +
			         " exits, but the special " + what + " rooms have one, so it is an ordinary room.";
		else
			t.place = true;
	}
	auto deferred = [&](Point p) {
		return (targets[0].place && p == targets[0].cell) || (targets[1].place && p == targets[1].cell);
	};

	// The first room: a used cell, not a deferred special, whose neighbourhood fits around the centre of the level.
	// Closest to the middle of the layout's bounding box wins; ties go to the first in row-major order.
	Point root{-1, -1};
	int bestScore = 0;
	for (int y = minY; y <= maxY; ++y)
		for (int x = minX; x <= maxX; ++x) {
			if (l.at(x, y).role == layout::Role::Empty || deferred(Point{x, y})) continue;
			if (x - minX > cap.left || maxX - x > cap.right || y - minY > cap.up || maxY - y > cap.down) continue;
			const int score = std::abs(2 * x - (minX + maxX)) + std::abs(2 * y - (minY + maxY));
			if (root.x < 0 || score < bestScore) {
				root = Point{x, y};
				bestScore = score;
			}
		}
	if (root.x < 0) return fallbackPlan("No room of the layout can be the first room while the whole layout stays inside the level.");

	// Walk the layout from the first room, breadth-first, in engine direction order. Special rooms are one-exit rooms
	// at the ends of the path; they are made last (exit, then entrance) so that D2R's "newest free room with this
	// piece" scan in 0x3eb850 finds exactly them.
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
	for (int k = 1; k >= 0; --k) { // exit first, entrance last
		if (!targets[k].place) continue;
		const Point c = targets[k].cell;
		const uint8_t m = l.at(c.x, c.y).mask;
		const int d = engineDir(static_cast<uint8_t>(m));
		const Point p{c.x + kDx[d], c.y + kDy[d]};
		const int pi = l.inside(p.x, p.y) ? roomOf[static_cast<size_t>(p.y) * l.width + p.x] : -1;
		if (pi < 0) return fallbackPlan("The layout's " + std::string(k == 0 ? "entrance" : "exit") + " room is not joined to the rest.");
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

	// Pieces, reserved rooms and the special calls.
	for (size_t i = 0; i < plan.rooms.size(); ++i) {
		Room &r = plan.rooms[i];
		r.piece = ts.pieceBase + r.mask;
		r.fixed = r.role == layout::Role::Entrance || r.role == layout::Role::Event || r.role == layout::Role::Boss ||
		          r.cell == entranceCell || r.cell == exitCell;
		const int ri = static_cast<int>(i);
		if (r.role == layout::Role::Event) plan.eventRoom = ri;
		if (r.role == layout::Role::Boss) plan.bossRoom = ri;
	}
	for (int k = 0; k < 2; ++k) {
		const Target &t = targets[k];
		SpecialCall sc;
		sc.kind = t.kind;
		sc.room = roomOf[static_cast<size_t>(t.cell.y) * l.width + t.cell.x];
		sc.place = t.place;
		sc.note = t.note;
		Room &r = plan.rooms[static_cast<size_t>(sc.room)];
		sc.fromPiece = r.piece;
		if (t.place) {
			sc.toPiece = t.family->piece[static_cast<size_t>(familyIndex(r.mask))];
			r.piece = sc.toPiece;
			r.special = k;
		}
		plan.specials.push_back(sc);
	}

	plan.lvlMaze = LvlMazeRow{static_cast<int>(plan.rooms.size()), ts.roomTiles, ts.roomTiles, 0};
	plan.ring.noOp = ts.callsRingGrower;
	plan.ring.graphInRing = ts.callsRingGrower && !ts.walkFollowsRing;
	plan.widthRooms = spanX;
	plan.heightRooms = spanY;
	plan.steer = true;
	return plan;
}

SpecialSteer steerSpecial(const Tileset &ts, const Plan &plan, int callIndex, int toPiece)
{
	SpecialSteer s;
	if (!plan.steer || callIndex < 0 || static_cast<size_t>(callIndex) >= plan.specials.size()) return s;
	const SpecialCall &sc = plan.specials[static_cast<size_t>(callIndex)];
	if (!sc.place) return s;
	const DeadEndFamily &fam = sc.kind == SpecialKind::Entrance ? ts.entrance : ts.exit;
	if (!familyHas(fam, toPiece)) return s; // the call is not for the group the plan expects
	s.action = SpecialSteer::Action::Redirect;
	s.fromPiece = sc.fromPiece;
	return s;
}

layout::Rules fitRules(const Tileset &ts, layout::Rules rules)
{
	const Capacity c = capacity(ts);
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
		out.plan = fallbackPlan("The shape does not fit " + (ts.name.empty() ? std::string("the tileset") : ts.name) +
		                        " levels, which hold " + std::to_string(fitted.width) + " by " + std::to_string(fitted.height) +
		                        " rooms: " + problem);
		return out;
	}
	for (int k = 0; k <= kRefits; ++k) {
		const uint64_t s = k == 0 ? seed : layout::splitmix64(seed ^ (kRefitSalt + static_cast<uint64_t>(k)));
		out.layout = layout::generate(fitted, s);
		out.plan = planMaze(ts, out.layout);
		out.refits = k;
		if (out.plan.steer) break;
		if (!out.layout.rulesMet) {
			// Even the generator's own fallback broke the shape's rules in this many rooms: more seeds will not help.
			out.plan = fallbackPlan("The " + std::string(ts.name.empty() ? "tileset" : ts.name) + " level holds only " +
			                        std::to_string(fitted.width) + " by " + std::to_string(fitted.height) +
			                        " rooms, which is too small to build this shape.");
			break;
		}
	}
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
		mixi(r.dx); mixi(r.dy); mixi(r.mask); mixi(r.piece); mixi(static_cast<int>(r.role)); mixi(r.fixed ? 1 : 0); mixi(r.special);
	}
	for (const AddStep &a : p.adds) { mixi(a.parent); mixi(a.room); mixi(a.dir); }
	for (const LinkStep &a : p.links) { mixi(a.a); mixi(a.b); mixi(a.dir); }
	for (const SpecialCall &s : p.specials) { mixi(static_cast<int>(s.kind)); mixi(s.room); mixi(s.place ? 1 : 0); mixi(s.fromPiece); mixi(s.toPiece); }
	mixi(p.ring.noOp ? 1 : 0); mixi(p.ring.graphInRing ? 1 : 0);
	mixi(p.lvlMaze.rooms); mixi(p.lvlMaze.sizeX); mixi(p.lvlMaze.sizeY); mixi(p.lvlMaze.merge);
	mixi(p.eventRoom); mixi(p.bossRoom);
	return h;
}

} // namespace cartographer::maze
