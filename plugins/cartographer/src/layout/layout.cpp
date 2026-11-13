// Cartographer layout graph generator. See layout.h for the contract. Pure integer code.
#include "layout.h"

#include <algorithm>
#include <cstddef>

#include "rng.h"

namespace cartographer::layout {

namespace {

// Directions, in the order of the mask bits: W, E, S, N.
constexpr int kDx[4] = {-1, 1, 0, 0};
constexpr int kDy[4] = {0, 0, 1, -1};
constexpr uint8_t kBit[4] = {kWest, kEast, kSouth, kNorth};
constexpr int kDirW = 0, kDirE = 1, kDirS = 2, kDirN = 3;

Point step(Point p, int d) { return Point{p.x + kDx[d], p.y + kDy[d]}; }
int opposite(int d) { return d ^ 1; }
int manhattan(Point a, Point b) { return (a.x > b.x ? a.x - b.x : b.x - a.x) + (a.y > b.y ? a.y - b.y : b.y - a.y); }

// Direction index from a to the adjacent cell b, or -1.
int dirBetween(Point a, Point b)
{
	for (int d = 0; d < 4; ++d)
		if (step(a, d) == b) return d;
	return -1;
}

int popcount4(uint8_t m) { return (m & 1) + ((m >> 1) & 1) + ((m >> 2) & 1) + ((m >> 3) & 1); }

// The work one generate() may spend, in cells touched. A rule set that is too tight to build used to spin for tens of
// seconds; once the budget is gone every step fails fast and generate() falls back. The cost of a try depends only on
// the rules and the seed, so the result stays deterministic. The presets use a small fraction of it.
constexpr int64_t kWorkBudget = 1500000;

struct Work {
	int64_t used = 0;
	void charge(int64_t n) { used += n; }
	bool over() const { return used > kWorkBudget; }
};

// The working grid of one try: which cells are used and how they are linked.
class Grid {
public:
	Grid(int w, int h) : w_(w), h_(h), used_(static_cast<size_t>(w) * h, 0), mask_(static_cast<size_t>(w) * h, 0) {}

	int width() const { return w_; }
	int height() const { return h_; }
	bool inside(Point p) const { return p.x >= 0 && p.y >= 0 && p.x < w_ && p.y < h_; }
	bool used(Point p) const { return inside(p) && used_[idx(p)] != 0; }
	uint8_t mask(Point p) const { return mask_[idx(p)]; }
	int degree(Point p) const { return popcount4(mask_[idx(p)]); }

	// A new cell at c, hanging off `parent`, may touch no used cell except `parent` and `allowed` (side contact rule).
	bool canPlace(Point c, Point parent, Point allowed) const
	{
		if (!inside(c) || used(c)) return false;
		for (int d = 0; d < 4; ++d) {
			const Point n = step(c, d);
			if (used(n) && n != parent && n != allowed) return false;
		}
		return true;
	}

	void place(Point c) { used_[idx(c)] = 1; }

	void link(Point a, Point b)
	{
		const int d = dirBetween(a, b);
		mask_[idx(a)] = static_cast<uint8_t>(mask_[idx(a)] | kBit[d]);
		mask_[idx(b)] = static_cast<uint8_t>(mask_[idx(b)] | kBit[opposite(d)]);
	}

	void unlink(Point a, Point b)
	{
		const int d = dirBetween(a, b);
		mask_[idx(a)] = static_cast<uint8_t>(mask_[idx(a)] & ~kBit[d]);
		mask_[idx(b)] = static_cast<uint8_t>(mask_[idx(b)] & ~kBit[opposite(d)]);
	}

	// Removes a cell and every link to it.
	void remove(Point c)
	{
		for (int d = 0; d < 4; ++d) {
			const Point n = step(c, d);
			if (used(n)) mask_[idx(n)] = static_cast<uint8_t>(mask_[idx(n)] & ~kBit[opposite(d)]);
		}
		mask_[idx(c)] = 0;
		used_[idx(c)] = 0;
	}

private:
	size_t idx(Point p) const { return static_cast<size_t>(p.y) * w_ + p.x; }
	int w_, h_;
	std::vector<uint8_t> used_;
	std::vector<uint8_t> mask_;
};

// Everything a try builds, before it becomes a Layout.
struct Draft {
	std::vector<Point> path;
	std::vector<Branch> branches;
	std::vector<Branch> loops;
	bool ring = false;
	int eventIdx = -1;
	int bossIdx = -1;
};

void reserveRooms(const Rules &r, Draft &d)
{
	const int n = static_cast<int>(d.path.size());
	d.eventIdx = -1;
	d.bossIdx = -1;
	if (n < 3) return;
	if (d.ring) {
		if (r.bossRoom) d.bossIdx = n / 2;
		int e = n * r.eventPct / 200; // a ring has two halves: the event room is partway round the first one
		if (e < 1) e = 1;
		if (e == d.bossIdx) e = d.bossIdx - 1;
		d.eventIdx = e;
	} else {
		if (r.bossRoom) d.bossIdx = n - 1;
		int e = (n - 1) * r.eventPct / 100;
		if (e < 1) e = 1;
		if (e > n - 2) e = n - 2;
		d.eventIdx = e;
	}
}

bool isReserved(const Draft &d, Point p)
{
	if (d.path.empty()) return false;
	if (p == d.path[0]) return true;
	if (d.eventIdx >= 0 && p == d.path[static_cast<size_t>(d.eventIdx)]) return true;
	if (d.bossIdx >= 0 && p == d.path[static_cast<size_t>(d.bossIdx)]) return true;
	return false;
}

// Every used cell that a side branch may hang from, in row-major order.
std::vector<Point> eligibleRoots(const Grid &g, const Draft &d, Work &work)
{
	work.charge(static_cast<int64_t>(g.width()) * g.height() + static_cast<int64_t>(d.branches.size()));
	// The last cell of a side room stays a dead end: nothing hangs from it or loops into it.
	std::vector<uint8_t> deadEnd(static_cast<size_t>(g.width()) * g.height(), 0);
	for (const Branch &b : d.branches)
		if (!b.cells.empty()) deadEnd[static_cast<size_t>(b.cells.back().y) * g.width() + b.cells.back().x] = 1;
	std::vector<Point> out;
	for (int y = 0; y < g.height(); ++y)
		for (int x = 0; x < g.width(); ++x) {
			const Point p{x, y};
			if (g.used(p) && !isReserved(d, p) && g.degree(p) < 4 && !deadEnd[static_cast<size_t>(y) * g.width() + x]) out.push_back(p);
		}
	return out;
}

// --- main path as a walk ------------------------------------------------------------------------------------------

bool walkMain(Grid &g, Pcg32 &rng, const Rules &r, Draft &d, Work &work)
{
	const int w = g.width(), h = g.height();
	work.charge(static_cast<int64_t>(w) * h);
	const int len = rng.range(r.pathMin, std::min(r.pathMax, w * h));

	// The entrance is on the border of the grid, looking inward along the roomier directions.
	std::vector<Point> border;
	for (int y = 0; y < h; ++y)
		for (int x = 0; x < w; ++x)
			if (x == 0 || y == 0 || x == w - 1 || y == h - 1) border.push_back(Point{x, y});
	const Point start = border[rng.below(static_cast<uint32_t>(border.size()))];

	int room[4];
	uint32_t total = 0;
	for (int dir = 0; dir < 4; ++dir) {
		int n = 0;
		for (Point p = step(start, dir); g.inside(p); p = step(p, dir)) ++n;
		room[dir] = n;
		total += static_cast<uint32_t>(n);
	}
	uint32_t pick = rng.below(total);
	int dir = 0;
	for (; dir < 4; ++dir) {
		if (pick < static_cast<uint32_t>(room[dir])) break;
		pick -= static_cast<uint32_t>(room[dir]);
	}

	g.place(start);
	d.path.push_back(start);
	Point cur = start;
	int turns = 0;
	for (int i = 1; i < len; ++i) {
		int next = dir;
		if (i > 1) {
			const int stepsLeft = len - i; // steps from here on that can still turn
			const bool mustTurn = r.turnsMin - turns >= stepsLeft;
			const bool mustStraight = turns >= r.turnsMax;
			const bool straightOk = g.canPlace(step(cur, dir), cur, Point{-9, -9});
			int turnDirs[2];
			int turnCount = 0;
			for (int t = 0; t < 4; ++t) {
				if (t == dir || t == opposite(dir)) continue;
				if (g.canPlace(step(cur, t), cur, Point{-9, -9})) turnDirs[turnCount++] = t;
			}
			if (mustTurn) {
				if (turnCount == 0) return false;
				next = turnDirs[rng.below(static_cast<uint32_t>(turnCount))];
			} else if (mustStraight) {
				if (!straightOk) return false;
			} else if (straightOk && (turnCount == 0 || rng.percent(r.straightPct))) {
				// straight
			} else if (turnCount > 0) {
				next = turnDirs[rng.below(static_cast<uint32_t>(turnCount))];
			} else {
				return false;
			}
			if (next != dir) ++turns;
		}
		const Point n = step(cur, next);
		if (!g.canPlace(n, cur, Point{-9, -9})) return false;
		g.place(n);
		g.link(cur, n);
		d.path.push_back(n);
		cur = n;
		dir = next;
	}
	return true;
}

// --- main path as a ring ------------------------------------------------------------------------------------------

bool ringValid(const std::vector<Point> &ring, int w, int h, std::vector<int> &scratch)
{
	const int n = static_cast<int>(ring.size());
	if (n < 4) return false;
	scratch.assign(static_cast<size_t>(w) * h, -1);
	for (int i = 0; i < n; ++i) {
		const Point p = ring[static_cast<size_t>(i)];
		if (p.x < 0 || p.y < 0 || p.x >= w || p.y >= h) return false;
		int &slot = scratch[static_cast<size_t>(p.y) * w + p.x];
		if (slot != -1) return false;
		slot = i;
	}
	for (int i = 0; i < n; ++i) {
		const Point p = ring[static_cast<size_t>(i)];
		if (manhattan(p, ring[static_cast<size_t>((i + 1) % n)]) != 1) return false;
		for (int dir = 0; dir < 4; ++dir) {
			const Point q = step(p, dir);
			if (q.x < 0 || q.y < 0 || q.x >= w || q.y >= h) continue;
			const int j = scratch[static_cast<size_t>(q.y) * w + q.x];
			if (j < 0) continue;
			const int diff = (j - i + n) % n;
			if (diff != 1 && diff != n - 1) return false; // side contact between cells that are not ring neighbours
		}
	}
	return true;
}

std::vector<Point> rectRing(int ox, int oy, int w, int h)
{
	std::vector<Point> ring;
	for (int x = 0; x < w; ++x) ring.push_back(Point{ox + x, oy});
	for (int y = 1; y < h; ++y) ring.push_back(Point{ox + w - 1, oy + y});
	for (int x = w - 2; x >= 0; --x) ring.push_back(Point{ox + x, oy + h - 1});
	for (int y = h - 2; y >= 1; --y) ring.push_back(Point{ox, oy + y});
	return ring;
}

// Pushes a straight run of three cells ring[i], ring[i+1], ring[i+2] sideways by one cell: the middle cell is replaced
// by the detour a, b, c (the ring grows by two cells). The result starts at ring[i]. Returns an empty ring when the
// three cells are not in a straight line. (A one-cell detour would leave its two ends touching, which is not allowed.)
std::vector<Point> bumpRing(const std::vector<Point> &ring, size_t i, int side)
{
	const size_t n = ring.size();
	const Point p = ring[i], q = ring[(i + 1) % n], r = ring[(i + 2) % n];
	const int dx = q.x - p.x, dy = q.y - p.y;
	if (r.x - q.x != dx || r.y - q.y != dy) return {};
	const int nx = side ? -dy : dy, ny = side ? dx : -dx;
	std::vector<Point> out;
	out.reserve(n + 2);
	out.push_back(p);
	out.push_back(Point{p.x + nx, p.y + ny});
	out.push_back(Point{q.x + nx, q.y + ny});
	out.push_back(Point{r.x + nx, r.y + ny});
	for (size_t k = 2; k < n; ++k) out.push_back(ring[(i + k) % n]);
	return out;
}

// Moves a corner cell to the opposite corner of its unit square (the ring keeps its length).
bool flipRing(std::vector<Point> &ring, size_t i)
{
	const size_t n = ring.size();
	const Point a = ring[(i + n - 1) % n], b = ring[(i + 1) % n], c = ring[i];
	if (a.x == b.x || a.y == b.y) return false;
	ring[i] = Point{a.x + b.x - c.x, a.y + b.y - c.y};
	return true;
}

// The cells of a ring that is known to be valid, kept so a trial move can be judged by the few cells it touches. A ring is
// valid when its cells are distinct, in the grid, each next to the following one, and every cell has exactly two used
// neighbours (the ring neighbours): that is the same test ringValid makes, without walking the whole ring.
class RingCells {
public:
	RingCells(int w, int h, const std::vector<Point> &ring) : w_(w), h_(h), cell_(static_cast<size_t>(w) * h, 0)
	{
		for (const Point &p : ring) cell_[idx(p)] = 1;
	}
	bool inside(Point p) const { return p.x >= 0 && p.y >= 0 && p.x < w_ && p.y < h_; }
	bool used(Point p) const { return inside(p) && cell_[idx(p)]; }
	void set(Point p, bool v) { cell_[idx(p)] = v ? 1 : 0; }
	int degree(Point p) const
	{
		int n = 0;
		for (int dir = 0; dir < 4; ++dir) n += used(step(p, dir)) ? 1 : 0;
		return n;
	}
	// Every cell in `at` and every used cell next to one of them must have exactly two used neighbours.
	bool settled(const Point *at, int count) const
	{
		for (int k = 0; k < count; ++k) {
			if (used(at[k]) && degree(at[k]) != 2) return false;
			for (int dir = 0; dir < 4; ++dir) {
				const Point q = step(at[k], dir);
				if (used(q) && degree(q) != 2) return false;
			}
		}
		return true;
	}

private:
	size_t idx(Point p) const { return static_cast<size_t>(p.y) * w_ + p.x; }
	int w_, h_;
	std::vector<uint8_t> cell_;
};

// Would bumpRing(ring, i, side) give a valid ring? Answers from the cells it changes; the ring is left as it was.
bool bumpFits(RingCells &cells, const std::vector<Point> &ring, size_t i, int side)
{
	const size_t n = ring.size();
	const Point p = ring[i], q = ring[(i + 1) % n], r = ring[(i + 2) % n];
	const int dx = q.x - p.x, dy = q.y - p.y;
	if (r.x - q.x != dx || r.y - q.y != dy) return false;
	const int nx = side ? -dy : dy, ny = side ? dx : -dx;
	const Point a{p.x + nx, p.y + ny}, b{q.x + nx, q.y + ny}, c{r.x + nx, r.y + ny};
	if (!cells.inside(a) || !cells.inside(b) || !cells.inside(c)) return false;
	if (cells.used(a) || cells.used(b) || cells.used(c)) return false;
	cells.set(q, false);
	cells.set(a, true);
	cells.set(b, true);
	cells.set(c, true);
	const Point touched[] = {p, r, a, b, c};
	const bool ok = cells.settled(touched, 5);
	if (!ok) {
		cells.set(a, false);
		cells.set(b, false);
		cells.set(c, false);
		cells.set(q, true);
	}
	return ok;
}

// Would flipRing(ring, i) give a valid ring? As bumpFits.
bool flipFits(RingCells &cells, const std::vector<Point> &ring, size_t i)
{
	const size_t n = ring.size();
	const Point a = ring[(i + n - 1) % n], b = ring[(i + 1) % n], c = ring[i];
	if (a.x == b.x || a.y == b.y) return false;
	const Point f{a.x + b.x - c.x, a.y + b.y - c.y};
	if (!cells.inside(f) || cells.used(f)) return false;
	cells.set(c, false);
	cells.set(f, true);
	const Point touched[] = {f, a, b};
	const bool ok = cells.settled(touched, 3);
	if (!ok) {
		cells.set(f, false);
		cells.set(c, true);
	}
	return ok;
}

void commitPath(Grid &g, Draft &d, const std::vector<Point> &path, bool ring)
{
	d.path = path;
	d.ring = ring;
	for (const Point &p : path) g.place(p);
	for (size_t i = 0; i + 1 < path.size(); ++i) g.link(path[i], path[i + 1]);
	if (ring) g.link(path.back(), path.front());
}

bool buildRing(Grid &g, Pcg32 &rng, const Rules &r, Draft &d, Work &work)
{
	const int w = g.width(), h = g.height();
	int len = rng.range(r.pathMin, r.pathMax);
	if (len & 1) len = (len + 1 <= r.pathMax) ? len + 1 : len - 1;
	if (len < 4) return false;

	int rw = 0, rh = 0;
	for (int tries = 0; tries < 16; ++tries) {
		const int cw = rng.range(std::min(3, w), w), ch = rng.range(std::min(3, h), h);
		if (2 * (cw + ch) - 4 <= len) { rw = cw; rh = ch; break; }
	}
	if (rw == 0) { rw = std::min(3, w); rh = std::min(3, h); }
	if (2 * (rw + rh) - 4 > len) return false;

	// Two draws never share one expression: C++ leaves the order of arguments unspecified. y is drawn first, which is
	// the order the golden layouts were made with.
	const int oy = rng.range(0, h - rh);
	const int ox = rng.range(0, w - rw);
	std::vector<Point> ring = rectRing(ox, oy, rw, rh);
	std::vector<int> scratch;
	// rectRing is valid unless the grid is only two cells wide or high. generate() never gets there (validateRules asks
	// a Loop grid for at least 3x3, so local is always true); only rules that skip validateRules take the full-ring path.
	work.charge(static_cast<int64_t>(w) * h + 4 * static_cast<int64_t>(ring.size()));
	const bool local = ringValid(ring, w, h, scratch);
	RingCells cells(w, h, ring);
	while (static_cast<int>(ring.size()) < len) {
		// Push out the first segment that still fits, starting from a random one, so the ring only gets stuck when
		// no segment can grow at all.
		const size_t count = ring.size();
		const size_t start = rng.below(static_cast<uint32_t>(count));
		const int sideFirst = static_cast<int>(rng.below(2));
		bool grown = false;
		for (size_t k = 0; k < count * 2 && !grown && !work.over(); ++k) {
			const size_t i = (start + k / 2) % count;
			const int side = (sideFirst + static_cast<int>(k & 1)) & 1;
			if (local) {
				work.charge(40); // a trial touches a handful of cells
				if (!bumpFits(cells, ring, i, side)) continue;
				ring = bumpRing(ring, i, side);
				work.charge(static_cast<int64_t>(ring.size()));
				grown = true;
			} else {
				work.charge(static_cast<int64_t>(w) * h + 4 * static_cast<int64_t>(ring.size()));
				std::vector<Point> trial = bumpRing(ring, i, side);
				if (ringValid(trial, w, h, scratch)) { ring.swap(trial); grown = true; }
			}
		}
		if (!grown) return false;
	}
	if (static_cast<int>(ring.size()) != len) return false;

	const int flips = rng.range(0, 24);
	for (int t = 0; t < flips; ++t) {
		const size_t i = rng.below(static_cast<uint32_t>(ring.size()));
		if (local) {
			work.charge(40);
			if (flipFits(cells, ring, i)) flipRing(ring, i);
		} else {
			work.charge(static_cast<int64_t>(w) * h + 4 * static_cast<int64_t>(ring.size()));
			std::vector<Point> trial(ring);
			if (flipRing(trial, i) && ringValid(trial, w, h, scratch)) ring.swap(trial);
		}
	}

	// Start anywhere on the ring, walking either way round.
	const size_t n = ring.size();
	const size_t s = rng.below(static_cast<uint32_t>(n));
	const bool reverse = rng.below(2) != 0;
	std::vector<Point> path;
	for (size_t k = 0; k < n; ++k) path.push_back(reverse ? ring[(s + n - k) % n] : ring[(s + k) % n]);
	commitPath(g, d, path, true);
	return true;
}

// --- side branches and extra loops --------------------------------------------------------------------------------

const Point kNoPoint{-9, -9};

// Grows a dead-end branch from `root`. Returns the cells placed (they are in the grid and linked).
std::vector<Point> growBranch(Grid &g, Pcg32 &rng, Point root, int len)
{
	std::vector<Point> cells;
	Point cur = root;
	int prev = -1;
	for (int k = 0; k < len; ++k) {
		int legal[4];
		int count = 0;
		for (int dir = 0; dir < 4; ++dir)
			if (g.canPlace(step(cur, dir), cur, kNoPoint)) legal[count++] = dir;
		if (count == 0) break;
		int pick = -1;
		if (prev >= 0 && rng.percent(60))
			for (int i = 0; i < count; ++i)
				if (legal[i] == prev) pick = prev;
		if (pick < 0) pick = legal[rng.below(static_cast<uint32_t>(count))];
		const Point n = step(cur, pick);
		g.place(n);
		g.link(cur, n);
		cells.push_back(n);
		cur = n;
		prev = pick;
	}
	return cells;
}

void rollback(Grid &g, const std::vector<Point> &cells)
{
	for (size_t i = cells.size(); i-- > 0;) g.remove(cells[i]);
}

bool addBranches(Grid &g, Pcg32 &rng, const Rules &r, Draft &d, Work &work)
{
	const int want = rng.range(r.branchesMin, r.branchesMax);
	for (int b = 0; b < want; ++b) {
		// A failed try is rolled back, so the roots only change when a room is placed: list them once per room.
		const std::vector<Point> roots = eligibleRoots(g, d, work);
		for (int tries = 0; tries < 30; ++tries) {
			if (work.over()) return false;
			if (roots.empty()) return static_cast<int>(d.branches.size()) >= r.branchesMin; // nothing to hang a room on
			const Point root = roots[rng.below(static_cast<uint32_t>(roots.size()))];
			const int len = rng.range(r.branchLenMin, r.branchLenMax);
			std::vector<Point> cells = growBranch(g, rng, root, len);
			work.charge(static_cast<int64_t>(cells.size()) * 4 + 4);
			if (static_cast<int>(cells.size()) >= r.branchLenMin) {
				Branch br;
				br.root = root;
				br.cells = std::move(cells);
				d.branches.push_back(std::move(br));
				break;
			}
			rollback(g, cells);
		}
	}
	return static_cast<int>(d.branches.size()) >= r.branchesMin;
}

// One extra loop: a branch that walks toward a target cell and links into it.
bool growLoop(Grid &g, Pcg32 &rng, Draft &d, const std::vector<Point> &roots, Work &work)
{
	if (roots.size() < 2) return false;
	const Point root = roots[rng.below(static_cast<uint32_t>(roots.size()))];
	const Point target = roots[rng.below(static_cast<uint32_t>(roots.size()))];
	if (target == root) return false;

	std::vector<Point> cells;
	Point cur = root;
	for (;;) {
		work.charge(8);
		int legal[4];
		int count = 0;
		for (int dir = 0; dir < 4; ++dir) {
			const Point n = step(cur, dir);
			if (!g.canPlace(n, cur, target)) continue;
			if (cells.empty() && manhattan(n, target) == 1) continue; // a loop needs at least two new cells
			legal[count++] = dir;
		}
		if (count == 0 || static_cast<int>(cells.size()) >= kLoopBranchMax) break;
		int pick = legal[0];
		if (rng.percent(75)) {
			for (int i = 1; i < count; ++i)
				if (manhattan(step(cur, legal[i]), target) < manhattan(step(cur, pick), target)) pick = legal[i];
		} else {
			pick = legal[rng.below(static_cast<uint32_t>(count))];
		}
		const Point n = step(cur, pick);
		g.place(n);
		g.link(cur, n);
		cells.push_back(n);
		cur = n;
		if (manhattan(cur, target) == 1) {
			g.link(cur, target);
			Branch br;
			br.root = root;
			br.cells = std::move(cells);
			br.closes = true;
			br.closeTo = target;
			d.loops.push_back(std::move(br));
			return true;
		}
	}
	rollback(g, cells);
	return false;
}

bool addLoops(Grid &g, Pcg32 &rng, const Rules &r, Draft &d, Work &work)
{
	const int want = rng.range(r.loopsMin, r.loopsMax);
	for (int l = 0; l < want; ++l) {
		// As with side rooms: a failed try is rolled back, so the roots are listed once per loop.
		const std::vector<Point> roots = eligibleRoots(g, d, work);
		bool grown = false;
		for (int tries = 0; tries < 40 && !grown && !work.over(); ++tries) grown = growLoop(g, rng, d, roots, work);
		if (!grown) break; // a whole round of tries failed: more loops won't fit either
	}
	return static_cast<int>(d.loops.size()) >= r.loopsMin;
}

// --- an open map --------------------------------------------------------------------------------------------------

// Fills the whole grid with rooms linked to every neighbour, then closes a few links as walls. A link is closed only when its
// two rooms keep at least two links each (no dead ends) and are still joined round one of the two squares beside it, so the
// layout stays connected after every step. The main path is the shortest walk from an entrance on the border to the farthest room.
bool buildOpen(Grid &g, Pcg32 &rng, const Rules &r, Draft &d, Work &work)
{
	const int w = g.width(), h = g.height();
	work.charge(static_cast<int64_t>(w) * h * 8);
	for (int y = 0; y < h; ++y)
		for (int x = 0; x < w; ++x) g.place(Point{x, y});
	std::vector<std::pair<Point, int>> edges; // (cell, kDirE or kDirS)
	for (int y = 0; y < h; ++y)
		for (int x = 0; x < w; ++x) {
			if (x + 1 < w) { g.link(Point{x, y}, Point{x + 1, y}); edges.push_back({Point{x, y}, kDirE}); }
			if (y + 1 < h) { g.link(Point{x, y}, Point{x, y + 1}); edges.push_back({Point{x, y}, kDirS}); }
		}
	for (size_t i = edges.size(); i > 1; --i) std::swap(edges[i - 1], edges[rng.below(static_cast<uint32_t>(i))]);
	auto linked = [&](Point a, int dir) { return g.inside(step(a, dir)) && (g.mask(a) & kBit[dir]) != 0; };
	int walls = static_cast<int>(edges.size()) * r.wallPct / 100;
	for (const auto &e : edges) {
		if (walls <= 0) break;
		const Point a = e.first;
		const int dir = e.second;
		const Point b = step(a, dir);
		if (g.degree(a) < 3 || g.degree(b) < 3) continue;
		bool detour = false;
		const int perp[2] = {dir == kDirE ? kDirS : kDirW, dir == kDirE ? kDirN : kDirE};
		for (int p : perp) detour = detour || (linked(a, p) && linked(b, p) && linked(step(a, p), dir));
		if (!detour) continue;
		g.unlink(a, b);
		--walls;
	}

	// Entrance on the border; boss at the farthest room (first in row-major order among equals); the path between.
	std::vector<Point> border;
	for (int y = 0; y < h; ++y)
		for (int x = 0; x < w; ++x)
			if (x == 0 || y == 0 || x == w - 1 || y == h - 1) border.push_back(Point{x, y});
	const Point start = border[rng.below(static_cast<uint32_t>(border.size()))];
	std::vector<int> dist(static_cast<size_t>(w) * h, -1);
	std::vector<Point> parent(static_cast<size_t>(w) * h, kNoPoint), queue;
	queue.push_back(start);
	dist[static_cast<size_t>(start.y) * w + start.x] = 0;
	for (size_t head = 0; head < queue.size(); ++head) {
		const Point c = queue[head];
		for (int dir = 0; dir < 4; ++dir) {
			if (!(g.mask(c) & kBit[dir])) continue;
			const Point n = step(c, dir);
			int &dn = dist[static_cast<size_t>(n.y) * w + n.x];
			if (dn >= 0) continue;
			dn = dist[static_cast<size_t>(c.y) * w + c.x] + 1;
			parent[static_cast<size_t>(n.y) * w + n.x] = c;
			queue.push_back(n);
		}
	}
	if (static_cast<int>(queue.size()) != w * h) return false;
	Point far = start;
	for (const Point &p : queue)
		if (dist[static_cast<size_t>(p.y) * w + p.x] > dist[static_cast<size_t>(far.y) * w + far.x]) far = p;
	std::vector<Point> path;
	for (Point p = far; p != kNoPoint; p = parent[static_cast<size_t>(p.y) * w + p.x]) path.push_back(p);
	std::reverse(path.begin(), path.end());
	d.path = path;
	d.ring = false;
	return true;
}

// --- assembling a layout ------------------------------------------------------------------------------------------

Layout assemble(const Grid &g, const Draft &d)
{
	Layout l;
	l.width = g.width();
	l.height = g.height();
	l.cells.assign(static_cast<size_t>(l.width) * l.height, Cell{});
	for (int y = 0; y < l.height; ++y)
		for (int x = 0; x < l.width; ++x)
			if (g.used(Point{x, y})) l.at(x, y).mask = g.mask(Point{x, y});
	for (size_t i = 0; i < d.path.size(); ++i) {
		const Point p = d.path[i];
		Role role = Role::Path;
		if (i == 0) role = Role::Entrance;
		else if (static_cast<int>(i) == d.eventIdx) role = Role::Event;
		else if (static_cast<int>(i) == d.bossIdx) role = Role::Boss;
		l.at(p.x, p.y).role = role;
	}
	for (const Branch &b : d.branches)
		for (const Point &p : b.cells) l.at(p.x, p.y).role = Role::Branch;
	for (const Branch &b : d.loops)
		for (const Point &p : b.cells) l.at(p.x, p.y).role = Role::Branch;
	l.mainPath = d.path;
	l.eventIdx = d.eventIdx;
	l.bossIdx = d.bossIdx;
	l.ring = d.ring;
	l.branches = d.branches;
	l.loops = d.loops;
	return l;
}

// The rooms off the main path of an open map are side rooms (every room of the grid is used).
void markOpenRooms(Layout &l)
{
	for (Cell &c : l.cells)
		if (c.role == Role::Empty && c.mask != 0) c.role = Role::Branch;
}

// --- the fallback -------------------------------------------------------------------------------------------------

// Smallest run of rows between two rows of a serpentine so that side rooms of branchLenMin cells fit between them.
int rowGap(const Rules &r) { return r.branchesMin > 0 ? std::max(2, r.branchLenMin + 2) : 2; }

bool fallbackRing(Grid &g, const Rules &r, Draft &d)
{
	const int w = g.width(), h = g.height();
	int len = r.pathMin + (r.pathMin & 1);
	if (len > r.pathMax) return false;
	int rw = std::min(3, w), rh = std::min(3, h);
	for (;;) {
		if (rw < w && 2 * (rw + 1 + rh) - 4 <= len) ++rw;
		else if (rh < h && 2 * (rw + rh + 1) - 4 <= len) ++rh;
		else break;
	}
	std::vector<Point> ring = rectRing(0, 0, rw, rh);
	std::vector<int> scratch;
	while (static_cast<int>(ring.size()) < len) {
		bool found = false;
		for (size_t i = 0; i < ring.size() && !found; ++i)
			for (int side = 0; side < 2 && !found; ++side) {
				std::vector<Point> trial = bumpRing(ring, i, side);
				if (ringValid(trial, w, h, scratch)) { ring.swap(trial); found = true; }
			}
		if (!found) break;
	}
	if (!ringValid(ring, w, h, scratch)) return false;
	commitPath(g, d, ring, true);
	return true;
}

// A staircase until there are enough turns, then a serpentine of rows that fills the grid top to bottom.
void fallbackWalk(Grid &g, const Rules &r, Draft &d)
{
	const int len = std::min(r.pathMin, g.width() * g.height());
	Point cur{0, 0};
	g.place(cur);
	d.path.push_back(cur);
	int last = -1, turns = 0;
	auto advance = [&](int dir) {
		const Point n = step(cur, dir);
		if (!g.canPlace(n, cur, kNoPoint)) return false;
		g.place(n);
		g.link(cur, n);
		d.path.push_back(n);
		if (last >= 0 && dir != last) ++turns;
		last = dir;
		cur = n;
		return true;
	};
	for (int alt = 0; turns < r.turnsMin && static_cast<int>(d.path.size()) < len; alt ^= 1)
		if (!advance(alt ? kDirS : kDirE)) break;
	int heading = kDirE;
	const int gap = rowGap(r);
	while (static_cast<int>(d.path.size()) < len) {
		if (advance(heading)) continue;
		bool moved = true;
		for (int k = 0; k < gap && moved && static_cast<int>(d.path.size()) < len; ++k) moved = advance(kDirS);
		if (!moved) break;
		heading = opposite(heading);
	}
}

void fallbackBranches(Grid &g, const Rules &r, Draft &d)
{
	const std::vector<int> order = {kDirS, kDirN, kDirE, kDirW};
	std::vector<Point> roots = d.path; // main path cells first, in order
	for (size_t i = 0; i < roots.size() && static_cast<int>(d.branches.size()) < r.branchesMin; ++i) {
		const Point root = roots[i];
		if (isReserved(d, root)) continue;
		for (size_t o = 0; o < order.size() && static_cast<int>(d.branches.size()) < r.branchesMin; ++o) {
			std::vector<Point> cells;
			Point cur = root;
			bool ok = true;
			for (int k = 0; k < r.branchLenMin && ok; ++k) {
				const Point n = step(cur, order[o]);
				if (!g.canPlace(n, cur, kNoPoint)) { ok = false; break; }
				g.place(n);
				g.link(cur, n);
				cells.push_back(n);
				cur = n;
			}
			if (ok) {
				Branch br;
				br.root = root;
				br.cells = std::move(cells);
				d.branches.push_back(std::move(br));
			} else {
				rollback(g, cells);
			}
		}
	}
}

} // namespace

// --- Layout ---------------------------------------------------------------------------------------------------------

int Layout::cellCount() const
{
	int n = 0;
	for (const Cell &c : cells) n += c.role != Role::Empty ? 1 : 0;
	return n;
}

int Layout::turns() const
{
	int t = 0;
	for (size_t i = 2; i < mainPath.size(); ++i) {
		const int a = dirBetween(mainPath[i - 2], mainPath[i - 1]);
		const int b = dirBetween(mainPath[i - 1], mainPath[i]);
		if (a != b) ++t;
	}
	return t;
}

// --- names and presets ---------------------------------------------------------------------------------------------

namespace {

bool equalsNoCase(std::string_view a, std::string_view b)
{
	if (a.size() != b.size()) return false;
	for (size_t i = 0; i < a.size(); ++i) {
		char x = a[i], y = b[i];
		if (x >= 'A' && x <= 'Z') x = static_cast<char>(x - 'A' + 'a');
		if (y >= 'A' && y <= 'Z') y = static_cast<char>(y - 'A' + 'a');
		if (x != y) return false;
	}
	return true;
}

struct Preset {
	int pathMin, pathMax, straight, turnsMin, turnsMax, branchesMin, branchesMax, lenMin, lenMax, loopsMin, loopsMax;
	bool loop;
};

// [shape][size]: ExtraSmall, Small, Medium, Large. The grid is 4x4, 6x6, 9x9 and 12x12 macro-cells.
const Preset kPresets[kShapeCount][kSizeCount] = {
	// Corridor: one long, mostly straight path, few dead ends.
	{{5, 6, 90, 0, 1, 0, 0, 1, 2, 0, 0, false},
	 {8, 10, 90, 0, 2, 0, 1, 1, 2, 0, 0, false},
	 {14, 18, 90, 0, 3, 0, 1, 1, 2, 0, 0, false},
	 {22, 28, 90, 0, 4, 0, 1, 1, 2, 0, 0, false}},
	// Winding: one long path with many turns, few dead ends.
	{{6, 8, 40, 3, 99, 0, 1, 1, 2, 0, 0, false},
	 {10, 14, 40, 5, 99, 0, 1, 1, 2, 0, 0, false},
	 {18, 24, 40, 8, 99, 0, 1, 1, 2, 0, 0, false},
	 {28, 38, 40, 12, 99, 0, 1, 1, 2, 0, 0, false}},
	// Branching: a main path with several side rooms and dead ends.
	{{6, 8, 70, 1, 99, 1, 2, 1, 2, 0, 0, false},
	 {8, 10, 70, 1, 99, 2, 3, 1, 3, 0, 0, false},
	 {12, 16, 70, 1, 99, 3, 5, 2, 4, 0, 0, false},
	 {18, 24, 70, 1, 99, 4, 7, 2, 5, 0, 0, false}},
	// Loop: the path runs in a ring and returns to its start, with a few short side rooms.
	{{6, 8, 0, 3, 99, 0, 1, 1, 2, 0, 0, true},
	 {8, 12, 0, 3, 99, 0, 2, 1, 2, 0, 0, true},
	 {12, 20, 0, 3, 99, 0, 2, 1, 2, 0, 0, true},
	 {20, 32, 0, 3, 99, 0, 3, 1, 2, 0, 0, true}},
	// Sprawl: wide and open, with loops and many branches.
	{{6, 8, 0, 3, 99, 1, 2, 1, 2, 0, 0, true},
	 {12, 16, 0, 3, 99, 2, 4, 1, 2, 0, 1, true},
	 {20, 28, 0, 3, 99, 4, 7, 1, 3, 0, 2, true},
	 {32, 40, 0, 3, 99, 6, 10, 2, 4, 0, 2, true}},
	// Open: an open room grid, every cell a room. Only the main path's length is used (entrance to the farthest room).
	{{4, 16, 0, 0, 99, 0, 0, 1, 1, 0, 0, false},
	 {4, 36, 0, 0, 99, 0, 0, 1, 1, 0, 0, false},
	 {4, 81, 0, 0, 99, 0, 0, 1, 1, 0, 0, false},
	 {4, 144, 0, 0, 99, 0, 0, 1, 1, 0, 0, false}},
};

const int kGridSize[kSizeCount] = {4, 6, 9, 12};

} // namespace

const char *shapeName(Shape shape)
{
	static const char *const names[kShapeCount] = {"Corridor", "Winding", "Branching", "Loop", "Sprawl", "Open"};
	return names[static_cast<int>(shape)];
}

const char *sizeName(Size size)
{
	static const char *const names[kSizeCount] = {"ExtraSmall", "Small", "Medium", "Large"};
	return names[static_cast<int>(size)];
}

std::optional<Shape> parseShapeName(std::string_view name)
{
	for (int i = 0; i < kShapeCount; ++i)
		if (equalsNoCase(name, shapeName(static_cast<Shape>(i)))) return static_cast<Shape>(i);
	return std::nullopt;
}

std::optional<Size> parseSizeName(std::string_view name)
{
	for (int i = 0; i < kSizeCount; ++i)
		if (equalsNoCase(name, sizeName(static_cast<Size>(i)))) return static_cast<Size>(i);
	return std::nullopt;
}

Rules presetRules(Shape shape, Size size)
{
	const Preset &p = kPresets[static_cast<int>(shape)][static_cast<int>(size)];
	Rules r;
	r.width = r.height = kGridSize[static_cast<int>(size)];
	r.pathMin = p.pathMin;
	r.pathMax = p.pathMax;
	r.straightPct = p.straight;
	r.turnsMin = p.turnsMin;
	r.turnsMax = p.turnsMax;
	r.branchesMin = p.branchesMin;
	r.branchesMax = p.branchesMax;
	r.branchLenMin = p.lenMin;
	r.branchLenMax = p.lenMax;
	r.loopsMin = p.loopsMin;
	r.loopsMax = p.loopsMax;
	r.loop = p.loop;
	r.open = shape == Shape::Open;
	return r;
}

std::string validateRules(const Rules &r)
{
	if (r.width < 2 || r.height < 2 || r.width > 32 || r.height > 32)
		return "Width and Height must each be between 2 and 32 cells.";
	if (r.pathMin < 4) return "MinPath must be at least 4: the path needs an entrance, an event room, a boss room and one more.";
	if (r.pathMin > r.pathMax) return "MinPath can't be more than MaxPath.";
	if (r.pathMax > r.width * r.height && !r.open) return "MaxPath is more cells than fit in the Width x Height grid.";
	if (r.straightPct < 0 || r.straightPct > 100) return "Straight must be a percent between 0 and 100.";
	if (r.turnsMin < 0) return "MinTurns can't be negative.";
	if (r.turnsMin > r.turnsMax) return "MinTurns can't be more than MaxTurns.";
	if (r.branchesMin < 0 || r.branchesMin > r.branchesMax) return "MinBranches can't be negative or more than MaxBranches.";
	if (r.branchLenMin < 1 || r.branchLenMin > r.branchLenMax) return "MinBranchLen must be at least 1 and no more than MaxBranchLen.";
	if (r.loopsMin < 0 || r.loopsMin > r.loopsMax) return "MinLoops can't be negative or more than MaxLoops.";
	// Counts far beyond what the grid can hold only make the generator spin, so they are refused up front.
	const int cellCap = r.width * r.height / 2;
	if (r.branchesMax > cellCap) return "MaxBranches is more than the Width x Height grid can hold (at most " + std::to_string(cellCap) + ").";
	if (r.loopsMax > cellCap) return "MaxLoops is more than the Width x Height grid can hold (at most " + std::to_string(cellCap) + ").";
	if (r.branchLenMax > cellCap) return "MaxBranchLen is more than the Width x Height grid can hold (at most " + std::to_string(cellCap) + ").";
	// The side rooms and loops can't need more cells than the grid has beside the main path (loops take two or more).
	const int freeCells = r.width * r.height - r.pathMin;
	if (static_cast<int64_t>(r.branchesMin) * r.branchLenMin + static_cast<int64_t>(r.loopsMin) * 2 > freeCells)
		return "MinBranches x MinBranchLen (plus 2 cells per MinLoops) is more cells than the grid has beside the main path (" + std::to_string(freeCells) + ").";
	if (r.eventPct < 1 || r.eventPct > 99) return "The event room position must be between 1 and 99 percent.";
	if (r.open) {
		if (r.loop) return "An Open map is not a Loop: set Loop to 0.";
		if (r.branchesMax > 0 || r.loopsMax > 0) return "An Open map has no side rooms or extra loops: set MaxBranches and MaxLoops to 0.";
		if (r.width < 3 || r.height < 3) return "An Open map needs a grid at least 3 cells wide and 3 high.";
		if (r.wallPct < 0 || r.wallPct > 40) return "The wall percent of an Open map must be between 0 and 40.";
	}
	if (r.loop) {
		bool evenFits = false;
		for (int n = r.pathMin; n <= r.pathMax; ++n)
			if ((n & 1) == 0) evenFits = true;
		if (!evenFits) return "A Loop path needs an even number of cells: MinPath to MaxPath holds none.";
		if (r.width < 3 || r.height < 3) return "A Loop needs a grid at least 3 cells wide and 3 high.";
		// Growing a long ring is slow (the time grows with the square of its length), so a ring is capped.
		const int ringCap = std::min(cellCap, 200);
		if (r.pathMax > ringCap) return "A Loop's MaxPath is too long: at most " + std::to_string(ringCap) + " cells for this grid.";
	}
	return std::string();
}

// --- generation ----------------------------------------------------------------------------------------------------

constexpr int kRestarts = 4; // restarts inside one try when it gets stuck

namespace {

std::optional<Layout> attemptWithin(const Rules &r, uint64_t attemptSeed, Work &work)
{
	// A walk or a side room can paint itself into a corner; a few quick restarts from the same stream are cheaper than
	// spending one of the kRetries on it.
	Pcg32 rng(attemptSeed);
	for (int restart = 0; restart < kRestarts; ++restart) {
		if (work.over()) break;
		Grid g(r.width, r.height);
		work.charge(static_cast<int64_t>(r.width) * r.height);
		Draft d;
		if (r.open) {
			if (!buildOpen(g, rng, r, d, work)) continue;
			reserveRooms(r, d);
			Layout l = assemble(g, d);
			markOpenRooms(l);
			return l;
		}
		if (r.loop) {
			if (!buildRing(g, rng, r, d, work)) continue;
		} else {
			if (!walkMain(g, rng, r, d, work)) continue;
			d.ring = false;
		}
		reserveRooms(r, d);
		if (!addBranches(g, rng, r, d, work)) continue;
		if (!addLoops(g, rng, r, d, work)) continue;
		work.charge(static_cast<int64_t>(r.width) * r.height);
		return assemble(g, d);
	}
	return std::nullopt;
}

} // namespace

std::optional<Layout> generateAttempt(const Rules &r, uint64_t attemptSeed)
{
	Work work;
	return attemptWithin(r, attemptSeed, work);
}

Layout generateFallback(const Rules &r, uint64_t seed)
{
	Grid g(r.width, r.height);
	Draft d;
	if (r.open) {
		Pcg32 rng(seed);
		Work work;
		if (buildOpen(g, rng, r, d, work)) {
			reserveRooms(r, d);
			Layout l = assemble(g, d);
			markOpenRooms(l);
			l.seed = seed;
			l.attempt = kRetries;
			l.fallback = true;
			l.rulesMet = checkLayout(l, r).empty();
			return l;
		}
	}
	// An Open map that buildOpen could not build (rules validateRules refuses) stays an empty layout and reports rulesMet false.
	if (!r.open) {
		if (r.loop) fallbackRing(g, r, d);
		else fallbackWalk(g, r, d);
	}
	reserveRooms(r, d);
	fallbackBranches(g, r, d);
	Layout l = assemble(g, d);
	l.seed = seed;
	l.attempt = kRetries;
	l.fallback = true;
	l.rulesMet = checkLayout(l, r).empty();
	return l;
}

Layout generate(const Rules &r, uint64_t seed)
{
	if (validateRules(r).empty()) {
		Work work;
		for (int k = 0; k < kRetries; ++k) {
			if (work.over()) break;
			std::optional<Layout> l = attemptWithin(r, splitmix64(seed ^ static_cast<uint64_t>(k)), work);
			work.charge(static_cast<int64_t>(r.width) * r.height * 4);
			if (l && checkLayout(*l, r).empty()) {
				l->seed = seed;
				l->attempt = k;
				l->fallback = false;
				l->rulesMet = true;
				return std::move(*l);
			}
		}
	}
	return generateFallback(r, seed);
}

Layout generate(Shape shape, Size size, uint64_t seed) { return generate(presetRules(shape, size), seed); }

// --- checking ------------------------------------------------------------------------------------------------------

std::vector<std::string> checkLayout(const Layout &l, const Rules &r)
{
	std::vector<std::string> bad;
	auto fail = [&](std::string s) { bad.push_back(std::move(s)); };
	auto at = [](int x, int y) { return "(" + std::to_string(x) + "," + std::to_string(y) + ")"; };
	auto pt = [&](Point p) { return at(p.x, p.y); };

	if (l.width != r.width || l.height != r.height || l.cells.size() != static_cast<size_t>(r.width) * r.height) {
		fail("the grid is not the width and height of the rules");
		return bad;
	}
	auto used = [&](Point p) { return l.inside(p.x, p.y) && l.at(p.x, p.y).role != Role::Empty; };
	auto linked = [&](Point a, Point b) {
		const int d = dirBetween(a, b);
		return d >= 0 && used(a) && used(b) && (l.at(a.x, a.y).mask & kBit[d]) != 0 &&
		       (l.at(b.x, b.y).mask & kBit[opposite(d)]) != 0;
	};

	// Cells: masks, edges and side contact.
	int usedCount = 0, bitCount = 0;
	for (int y = 0; y < l.height; ++y)
		for (int x = 0; x < l.width; ++x) {
			const Cell &c = l.at(x, y);
			const Point p{x, y};
			if (c.role == Role::Empty) {
				if (c.mask != 0) fail("empty cell " + at(x, y) + " has exits");
				continue;
			}
			++usedCount;
			if (c.mask == 0) fail("cell " + at(x, y) + " has mask 0");
			if (c.mask & ~0x0F) fail("cell " + at(x, y) + " has a bad mask");
			bitCount += popcount4(c.mask & 0x0F);
			for (int d = 0; d < 4; ++d) {
				const Point n = step(p, d);
				const bool bit = (c.mask & kBit[d]) != 0;
				if (bit && !used(n)) fail("cell " + at(x, y) + " has an exit to nothing");
				else if (bit && !(l.at(n.x, n.y).mask & kBit[opposite(d)])) fail("cell " + at(x, y) + " and its neighbour disagree on a facing bit");
				else if (!bit && used(n) && !r.open) fail("cells " + at(x, y) + " and " + pt(n) + " touch without a link");
			}
		}
	if (usedCount == 0) { fail("the layout is empty"); return bad; }

	// Connectivity.
	{
		std::vector<uint8_t> seen(l.cells.size(), 0);
		std::vector<Point> stack;
		for (int y = 0; y < l.height && stack.empty(); ++y)
			for (int x = 0; x < l.width && stack.empty(); ++x)
				if (l.at(x, y).role != Role::Empty) { stack.push_back(Point{x, y}); seen[static_cast<size_t>(y) * l.width + x] = 1; }
		int reached = 0;
		while (!stack.empty()) {
			const Point p = stack.back();
			stack.pop_back();
			++reached;
			for (int d = 0; d < 4; ++d) {
				const Point n = step(p, d);
				if (!(l.at(p.x, p.y).mask & kBit[d]) || !l.inside(n.x, n.y)) continue;
				uint8_t &s = seen[static_cast<size_t>(n.y) * l.width + n.x];
				if (!s) { s = 1; stack.push_back(n); }
			}
		}
		if (reached != usedCount) fail("the layout is not connected");
	}

	// Main path.
	const int n = static_cast<int>(l.mainPath.size());
	if (n < r.pathMin || n > r.pathMax) fail("the main path has " + std::to_string(n) + " cells, outside " + std::to_string(r.pathMin) + ".." + std::to_string(r.pathMax));
	if (l.ring != r.loop) fail(r.loop ? "the path is not a ring" : "the path is a ring");
	int accounted = 0;
	std::vector<uint8_t> claimed(l.cells.size(), 0);
	auto claim = [&](Point p) {
		if (!l.inside(p.x, p.y)) { fail("a room lies outside the bounds"); return; }
		uint8_t &c = claimed[static_cast<size_t>(p.y) * l.width + p.x];
		if (c) fail("cell " + pt(p) + " is listed twice");
		c = 1;
		++accounted;
	};
	for (int i = 0; i < n; ++i) {
		const Point p = l.mainPath[static_cast<size_t>(i)];
		claim(p);
		if (!used(p)) { fail("main path cell " + pt(p) + " is empty"); continue; }
		if (i > 0 && !linked(l.mainPath[static_cast<size_t>(i - 1)], p)) fail("main path cells " + std::to_string(i - 1) + " and " + std::to_string(i) + " are not linked");
	}
	if (n > 0) {
		if (used(l.mainPath[0]) && l.at(l.mainPath[0].x, l.mainPath[0].y).role != Role::Entrance) fail("the first path cell is not the entrance");
		if (l.ring && n > 1 && !linked(l.mainPath.back(), l.mainPath.front())) fail("the ring does not close onto the entrance");
	}
	if (l.turns() < r.turnsMin || l.turns() > r.turnsMax) fail("the main path has " + std::to_string(l.turns()) + " turns, outside " + std::to_string(r.turnsMin) + ".." + std::to_string(r.turnsMax));

	// Reserved rooms.
	if (n >= 3) {
		if (l.eventIdx < 1 || l.eventIdx >= n) {
			fail("the event room is not on the main path");
		} else {
			const Point e = l.mainPath[static_cast<size_t>(l.eventIdx)];
			if (!l.ring && l.eventIdx >= n - 1) fail("the event room is at the end of the path");
			if (used(e) && l.at(e.x, e.y).role != Role::Event) fail("the event room cell has the wrong role");
			if (!r.open && used(e) && popcount4(l.at(e.x, e.y).mask) != 2) fail("the event room is not partway along (it has a branch)");
		}
		if (r.bossRoom) {
			if (l.bossIdx < 1 || l.bossIdx >= n || l.bossIdx == l.eventIdx) {
				fail("there is no boss room");
			} else {
				const Point b = l.mainPath[static_cast<size_t>(l.bossIdx)];
				if (!l.ring && l.bossIdx != n - 1) fail("the boss room is not at the end of the path");
				if (used(b) && l.at(b.x, b.y).role != Role::Boss) fail("the boss room cell has the wrong role");
				if (!r.open && used(b) && popcount4(l.at(b.x, b.y).mask) != (l.ring ? 2 : 1)) fail("the boss room has the wrong number of exits");
			}
		} else if (l.bossIdx != -1) {
			fail("a boss room was reserved but the rules have none");
		}
	}
	auto reserved = [&](Point p) {
		return (n > 0 && p == l.mainPath[0]) || (l.eventIdx >= 0 && l.eventIdx < n && p == l.mainPath[static_cast<size_t>(l.eventIdx)]) ||
		       (l.bossIdx >= 0 && l.bossIdx < n && p == l.mainPath[static_cast<size_t>(l.bossIdx)]);
	};

	// Side branches and loops.
	auto checkChain = [&](const Branch &b, const char *what) {
		if (!used(b.root) || reserved(b.root)) fail(std::string(what) + " hangs from a missing or reserved room");
		Point prev = b.root;
		for (const Point &p : b.cells) {
			claim(p);
			if (used(p) && l.at(p.x, p.y).role != Role::Branch) fail(std::string(what) + " cell " + pt(p) + " has the wrong role");
			if (!linked(prev, p)) fail(std::string(what) + " cell " + pt(p) + " is not linked to the cell before it");
			prev = p;
		}
	};
	if (static_cast<int>(l.branches.size()) < r.branchesMin || static_cast<int>(l.branches.size()) > r.branchesMax)
		fail("there are " + std::to_string(l.branches.size()) + " side rooms, outside " + std::to_string(r.branchesMin) + ".." + std::to_string(r.branchesMax));
	for (const Branch &b : l.branches) {
		checkChain(b, "a side room");
		const int len = static_cast<int>(b.cells.size());
		if (len < r.branchLenMin || len > r.branchLenMax) fail("a side room has " + std::to_string(len) + " cells, outside " + std::to_string(r.branchLenMin) + ".." + std::to_string(r.branchLenMax));
		if (len > 0 && used(b.cells.back()) && popcount4(l.at(b.cells.back().x, b.cells.back().y).mask) != 1) fail("a side room does not end in a dead end");
	}
	if (static_cast<int>(l.loops.size()) < r.loopsMin || static_cast<int>(l.loops.size()) > r.loopsMax)
		fail("there are " + std::to_string(l.loops.size()) + " extra loops, outside " + std::to_string(r.loopsMin) + ".." + std::to_string(r.loopsMax));
	for (const Branch &b : l.loops) {
		checkChain(b, "an extra loop");
		const int len = static_cast<int>(b.cells.size());
		if (len < 2 || len > kLoopBranchMax) fail("an extra loop has " + std::to_string(len) + " cells");
		if (!b.closes || len == 0 || !linked(b.cells.back(), b.closeTo) || reserved(b.closeTo)) fail("an extra loop does not rejoin the layout");
	}
	if (r.open) {
		// Every cell of the grid is a room, none is a dead end, and the walls are a few of the links (the grid has
		// w*(h-1) + h*(w-1) of them; connectivity was checked above).
		if (usedCount != l.width * l.height) fail("an open layout has an empty cell");
		for (int y = 0; y < l.height; ++y)
			for (int x = 0; x < l.width; ++x)
				if (l.at(x, y).role != Role::Empty && popcount4(l.at(x, y).mask) < 2) fail("cell " + at(x, y) + " of an open layout is a dead end");
		const int gridLinks = l.width * (l.height - 1) + l.height * (l.width - 1);
		if (gridLinks - bitCount / 2 > gridLinks * r.wallPct / 100) fail("an open layout has more walls than " + std::to_string(r.wallPct) + " percent of its links");
		return bad;
	}
	if (accounted != usedCount) fail("some cells belong to no path, side room or loop");

	// Graph shape: a tree plus one link per loop.
	const int expectLinks = usedCount - 1 + (l.ring ? 1 : 0) + static_cast<int>(l.loops.size());
	if (bitCount / 2 != expectLinks) fail("the number of links does not match the rooms and loops");
	return bad;
}

// --- hash and dump -------------------------------------------------------------------------------------------------

uint64_t layoutHash(const Layout &l)
{
	uint64_t h = 0xcbf29ce484222325ull;
	auto byte = [&](uint8_t b) { h = (h ^ b) * 0x100000001b3ull; };
	auto word = [&](uint32_t v) { for (int i = 0; i < 4; ++i) byte(static_cast<uint8_t>(v >> (8 * i))); };
	word(static_cast<uint32_t>(l.width));
	word(static_cast<uint32_t>(l.height));
	for (const Cell &c : l.cells) { byte(c.mask); byte(static_cast<uint8_t>(c.role)); }
	word(static_cast<uint32_t>(l.mainPath.size()));
	for (const Point &p : l.mainPath) { word(static_cast<uint32_t>(p.x)); word(static_cast<uint32_t>(p.y)); }
	word(static_cast<uint32_t>(l.eventIdx + 1));
	word(static_cast<uint32_t>(l.bossIdx + 1));
	byte(l.ring ? 1 : 0);
	return h;
}

std::string toAscii(const Layout &l)
{
	const int cw = 2 * l.width - 1, ch = 2 * l.height - 1;
	if (cw <= 0 || ch <= 0) return std::string();
	std::vector<std::string> rows(static_cast<size_t>(ch), std::string(static_cast<size_t>(cw), ' '));
	for (int y = 0; y < l.height; ++y)
		for (int x = 0; x < l.width; ++x) {
			const Cell &c = l.at(x, y);
			char glyph = '.';
			switch (c.role) {
			case Role::Entrance: glyph = '@'; break;
			case Role::Event: glyph = '!'; break;
			case Role::Boss: glyph = 'B'; break;
			case Role::Path: glyph = '#'; break;
			case Role::Branch: glyph = 'o'; break;
			case Role::Empty: break;
			}
			rows[static_cast<size_t>(2 * y)][static_cast<size_t>(2 * x)] = glyph;
			if (c.mask & kEast) rows[static_cast<size_t>(2 * y)][static_cast<size_t>(2 * x + 1)] = '-';
			if (c.mask & kSouth) rows[static_cast<size_t>(2 * y + 1)][static_cast<size_t>(2 * x)] = '|';
		}
	std::string out;
	for (std::string &row : rows) {
		while (!row.empty() && row.back() == ' ') row.pop_back();
		out += row;
		out += '\n';
	}
	return out;
}

// --- shapes.txt ----------------------------------------------------------------------------------------------------

namespace {

std::string_view trim(std::string_view s)
{
	while (!s.empty() && (s.front() == ' ' || s.front() == '\r')) s.remove_prefix(1);
	while (!s.empty() && (s.back() == ' ' || s.back() == '\r')) s.remove_suffix(1);
	return s;
}

std::vector<std::string_view> splitTabs(std::string_view line)
{
	std::vector<std::string_view> out;
	size_t start = 0;
	for (;;) {
		const size_t t = line.find('\t', start);
		if (t == std::string_view::npos) { out.push_back(trim(line.substr(start))); break; }
		out.push_back(trim(line.substr(start, t - start)));
		start = t + 1;
	}
	return out;
}

// tooLarge is set when the text is a whole number with too many digits to use.
bool parseInt(std::string_view s, int &out, bool &tooLarge)
{
	tooLarge = false;
	if (s.empty()) return false;
	size_t i = 0;
	bool neg = false;
	if (s[0] == '-') { neg = true; i = 1; }
	if (i >= s.size()) return false;
	for (size_t k = i; k < s.size(); ++k)
		if (s[k] < '0' || s[k] > '9') return false;
	while (i + 1 < s.size() && s[i] == '0') ++i; // leading zeros don't count: "0000005" is 5
	if (s.size() - i > 6) { tooLarge = true; return false; }
	int v = 0;
	for (; i < s.size(); ++i) {
		if (s[i] < '0' || s[i] > '9') return false;
		v = v * 10 + (s[i] - '0');
	}
	out = neg ? -v : v;
	return true;
}

bool parseBool(std::string_view s, bool &out)
{
	if (equalsNoCase(s, "1") || equalsNoCase(s, "yes") || equalsNoCase(s, "true")) { out = true; return true; }
	if (equalsNoCase(s, "0") || equalsNoCase(s, "no") || equalsNoCase(s, "false")) { out = false; return true; }
	return false;
}

enum Column { cName, cBase, cMinPath, cMaxPath, cStraight, cMinTurns, cMaxTurns, cMinBranches, cMaxBranches, cMinBranchLen,
	cMaxBranchLen, cLoop, cMinLoops, cMaxLoops, cWidth, cHeight, cBossRoom, cColumnCount };

// The column names of shapes.txt (listed in docs/cartographer/tables.md); they match the names validateRules() uses in
// its messages.
const char *const kColumnNames[cColumnCount] = {"Name", "Base", "MinPath", "MaxPath", "Straight", "MinTurns", "MaxTurns",
	"MinBranches", "MaxBranches", "MinBranchLen", "MaxBranchLen", "Loop", "MinLoops", "MaxLoops", "Width", "Height", "BossRoom"};

const ShapeDef *findCustom(const ShapeTable &t, std::string_view name)
{
	for (const ShapeDef &d : t.shapes)
		if (equalsNoCase(d.name, name)) return &d;
	return nullptr;
}

void applyDef(const ShapeDef &d, Rules &r)
{
	auto set = [](const std::optional<int> &v, int &dst) { if (v) dst = *v; };
	set(d.pathMin, r.pathMin);
	set(d.pathMax, r.pathMax);
	set(d.straightPct, r.straightPct);
	set(d.turnsMin, r.turnsMin);
	set(d.turnsMax, r.turnsMax);
	set(d.branchesMin, r.branchesMin);
	set(d.branchesMax, r.branchesMax);
	set(d.branchLenMin, r.branchLenMin);
	set(d.branchLenMax, r.branchLenMax);
	set(d.loopsMin, r.loopsMin);
	set(d.loopsMax, r.loopsMax);
	set(d.width, r.width);
	set(d.height, r.height);
	if (d.loop) r.loop = *d.loop;
	if (d.bossRoom) r.bossRoom = *d.bossRoom;
}

bool resolveDepth(const ShapeTable &table, std::string_view name, Size size, Rules &out, std::string &error, int depth)
{
	if (const std::optional<Shape> preset = parseShapeName(name)) {
		out = presetRules(*preset, size);
		return true;
	}
	const ShapeDef *def = findCustom(table, name);
	if (!def) {
		error = "There is no shape called \"" + std::string(name) + "\". Use Corridor, Winding, Branching, Loop, Sprawl, Open, or a Name from shapes.txt.";
		return false;
	}
	if (depth > 16) {
		error = "Shape \"" + std::string(name) + "\" is based on itself.";
		return false;
	}
	const std::string_view base = def->base.empty() ? std::string_view("Winding") : std::string_view(def->base);
	if (!resolveDepth(table, base, size, out, error, depth + 1)) return false;
	applyDef(*def, out);
	return true;
}

} // namespace

bool resolveRules(const ShapeTable &table, std::string_view name, Size size, Rules &out, std::string &error)
{
	if (!resolveDepth(table, name, size, out, error, 0)) return false;
	error = validateRules(out);
	if (!error.empty()) {
		error = "Shape \"" + std::string(name) + "\" at size " + sizeName(size) + ": " + error;
		return false;
	}
	return true;
}

std::string checkShapeAtSize(const ShapeTable &table, std::string_view shapeName, Size size)
{
	Rules r;
	std::string e;
	if (!resolveRules(table, shapeName, size, r, e)) return e;
	for (uint64_t seed = 1; seed <= 16; ++seed)
		if (!generate(r, seed).rulesMet)
			return "Shape \"" + std::string(shapeName) + "\" at size " + sizeName(size) +
				" is too tight: the generator could not meet its rules for seed " + std::to_string(seed) +
				". Loosen the numbers or enlarge Width and Height.";
	return std::string();
}

ParseResult parseShapesTxt(std::string_view text, std::string_view fileName)
{
	ParseResult res;
	auto err = [&](int line, const std::string &msg) {
		res.errors.push_back(std::string(fileName) + (line > 0 ? " line " + std::to_string(line) : std::string()) + ": " + msg);
	};

	int col[cColumnCount];
	for (int &c : col) c = -1;
	bool haveHeader = false;
	int lineNo = 0;
	size_t pos = 0;
	while (pos <= text.size()) {
		size_t eol = text.find('\n', pos);
		if (eol == std::string_view::npos) eol = text.size();
		std::string_view line = text.substr(pos, eol - pos);
		pos = eol + 1;
		++lineNo;
		if (lineNo == 1 && line.size() >= 3 && line.substr(0, 3) == "\xEF\xBB\xBF") line.remove_prefix(3);
		if (trim(line).empty()) continue;

		const std::vector<std::string_view> cells = splitTabs(line);
		if (!haveHeader) {
			haveHeader = true;
			for (size_t i = 0; i < cells.size(); ++i) {
				if (cells[i].empty()) continue;
				int found = -1;
				for (int c = 0; c < cColumnCount; ++c)
					if (equalsNoCase(cells[i], kColumnNames[c])) found = c;
				if (found < 0) { err(lineNo, "unknown column \"" + std::string(cells[i]) + "\"."); continue; }
				if (col[found] >= 0) { err(lineNo, "column \"" + std::string(kColumnNames[found]) + "\" appears twice."); continue; }
				col[found] = static_cast<int>(i);
			}
			if (col[cName] < 0) err(lineNo, "the header row needs a Name column.");
			continue;
		}
		if (col[cName] < 0) break;

		auto cell = [&](int c) -> std::string_view {
			if (col[c] < 0 || static_cast<size_t>(col[c]) >= cells.size()) return std::string_view();
			return cells[static_cast<size_t>(col[c])];
		};
		ShapeDef def;
		def.name = std::string(cell(cName));
		def.base = std::string(cell(cBase));
		const size_t errorsBefore = res.errors.size();
		if (def.name.empty()) { err(lineNo, "Name is empty."); continue; }
		if (parseShapeName(def.name)) err(lineNo, "\"" + def.name + "\" is a built-in shape; pick another Name.");
		if (findCustom(res.table, def.name)) err(lineNo, "Name \"" + def.name + "\" is used twice.");
		if (!def.base.empty() && !parseShapeName(def.base) && !findCustom(res.table, def.base))
			err(lineNo, "Base \"" + def.base + "\" is not a built-in shape or an earlier row.");

		auto readInt = [&](int c, std::optional<int> &dst) {
			const std::string_view v = cell(c);
			if (v.empty()) return;
			int n = 0;
			bool tooLarge = false;
			if (!parseInt(v, n, tooLarge)) {
				if (tooLarge) err(lineNo, std::string(kColumnNames[c]) + " is too large: \"" + std::string(v) + "\".");
				else err(lineNo, std::string(kColumnNames[c]) + " must be a whole number, not \"" + std::string(v) + "\".");
				return;
			}
			dst = n;
		};
		auto readBool = [&](int c, std::optional<bool> &dst) {
			const std::string_view v = cell(c);
			if (v.empty()) return;
			bool b = false;
			if (!parseBool(v, b)) { err(lineNo, std::string(kColumnNames[c]) + " must be 0 or 1 (or yes/no), not \"" + std::string(v) + "\"."); return; }
			dst = b;
		};
		readInt(cMinPath, def.pathMin);
		readInt(cMaxPath, def.pathMax);
		readInt(cStraight, def.straightPct);
		readInt(cMinTurns, def.turnsMin);
		readInt(cMaxTurns, def.turnsMax);
		readInt(cMinBranches, def.branchesMin);
		readInt(cMaxBranches, def.branchesMax);
		readInt(cMinBranchLen, def.branchLenMin);
		readInt(cMaxBranchLen, def.branchLenMax);
		readInt(cMinLoops, def.loopsMin);
		readInt(cMaxLoops, def.loopsMax);
		readInt(cWidth, def.width);
		readInt(cHeight, def.height);
		readBool(cLoop, def.loop);
		readBool(cBossRoom, def.bossRoom);
		if (res.errors.size() != errorsBefore) continue;

		res.table.shapes.push_back(def);
		// Every size must resolve to usable rules that the generator can build. ExtraSmall is checked only when a map uses it
		// (resolveRules), so a shape written before it existed is not rejected for a size it never asked for.
		for (int s = static_cast<int>(Size::Small); s < kSizeCount; ++s) {
			const std::string e = checkShapeAtSize(res.table, def.name, static_cast<Size>(s));
			if (!e.empty()) { err(lineNo, e); break; }
		}
		if (res.errors.size() != errorsBefore) res.table.shapes.pop_back();
	}
	if (!haveHeader && !trim(text).empty()) err(0, "no header row.");
	return res;
}

} // namespace cartographer::layout
