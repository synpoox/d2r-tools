#include "outline_shapes.h"

#include <algorithm>
#include <map>
#include <utility>

#include "rng.h"

namespace cartographer::outdoor::outline {

int Mask::count() const
{
	int n = 0;
	for (uint8_t c : open) n += c != 0;
	return n;
}

Mask makeMask(int w, int h, bool fill)
{
	Mask m;
	if (w <= 0 || h <= 0) return m;
	m.w = w;
	m.h = h;
	m.open.assign(static_cast<size_t>(w) * h, fill ? 1 : 0);
	return m;
}

Mask rectangleShape(int w, int h)
{
	return makeMask(w, h, true);
}

Mask lShape(int w, int h, int armW, int armH)
{
	if (w < 2 || h < 2 || armW < 1 || armW >= w || armH < 1 || armH >= h) return {};
	Mask m = makeMask(w, h, false);
	for (int y = 0; y < h; ++y)
		for (int x = 0; x < w; ++x) m.set(x, y, x < armW || y >= h - armH);
	return m;
}

Mask crossShape(int w, int h, int arm)
{
	if (w < 1 || h < 1 || arm < 1 || arm > w || arm > h) return {};
	Mask m = makeMask(w, h, false);
	const int x0 = (w - arm) / 2;
	const int y0 = (h - arm) / 2;
	for (int y = 0; y < h; ++y)
		for (int x = 0; x < w; ++x) m.set(x, y, (x >= x0 && x < x0 + arm) || (y >= y0 && y < y0 + arm));
	return m;
}

Mask ringShape(int w, int h, int wall)
{
	if (wall < 1 || w <= 2 * wall || h <= 2 * wall) return {};
	Mask m = makeMask(w, h, true);
	for (int y = wall; y < h - wall; ++y)
		for (int x = wall; x < w - wall; ++x) m.set(x, y, false);
	return m;
}

Mask uShape(int w, int h, int notchW, int notchD)
{
	if (w < 3 || h < 2 || notchW < 1 || notchD < 1 || notchD >= h) return {};
	const int x0 = (w - notchW) / 2;
	if (x0 < 1 || x0 + notchW > w - 1) return {};
	Mask m = makeMask(w, h, true);
	for (int y = 0; y < notchD; ++y)
		for (int x = x0; x < x0 + notchW; ++x) m.set(x, y, false);
	return m;
}

namespace {

// Marks every void cell that touches the outside of the mask through void cells only (4-connected flood from the border).
std::vector<uint8_t> outsideVoid(const Mask &m)
{
	std::vector<uint8_t> seen(m.open.size(), 0);
	std::vector<int> stack;
	auto push = [&](int x, int y) {
		if (!m.inside(x, y) || m.at(x, y)) return;
		const size_t i = static_cast<size_t>(y) * m.w + x;
		if (seen[i]) return;
		seen[i] = 1;
		stack.push_back(static_cast<int>(i));
	};
	for (int x = 0; x < m.w; ++x) {
		push(x, 0);
		push(x, m.h - 1);
	}
	for (int y = 0; y < m.h; ++y) {
		push(0, y);
		push(m.w - 1, y);
	}
	while (!stack.empty()) {
		const int i = stack.back();
		stack.pop_back();
		const int x = i % m.w;
		const int y = i / m.w;
		push(x + 1, y);
		push(x - 1, y);
		push(x, y + 1);
		push(x, y - 1);
	}
	return seen;
}

} // namespace

Mask blobShape(int w, int h, int fillPercent, uint64_t seed, bool allowHoles)
{
	if (w < 2 || h < 2) return {};
	// Grown on a grid half the size and drawn at double size, so no feature is one cell wide.
	const int cw = w / 2;
	const int ch = h / 2;
	const int pct = std::clamp(fillPercent, 1, 100);
	const int target = std::max(1, static_cast<int>(static_cast<long long>(cw) * ch * pct / 100));
	Mask c = makeMask(cw, ch, false);
	layout::Pcg32 rng(seed);
	std::vector<uint8_t> queued(static_cast<size_t>(cw) * ch, 0);
	std::vector<int> frontier;
	auto offer = [&](int x, int y) {
		if (!c.inside(x, y)) return;
		const size_t i = static_cast<size_t>(y) * cw + x;
		if (c.open[i] || queued[i]) return;
		queued[i] = 1;
		frontier.push_back(static_cast<int>(i));
	};
	auto take = [&](int x, int y) {
		c.set(x, y, true);
		offer(x + 1, y);
		offer(x - 1, y);
		offer(x, y + 1);
		offer(x, y - 1);
	};
	take(cw / 2, ch / 2);
	int have = 1;
	while (have < target && !frontier.empty()) {
		const size_t pick = rng.below(static_cast<uint32_t>(frontier.size()));
		const int i = frontier[pick];
		frontier[pick] = frontier.back();
		frontier.pop_back();
		take(i % cw, i / cw);
		++have;
	}
	// Two open cells meeting only at a corner would make the outline touch itself: open the cell that joins them.
	for (bool changed = true; changed;) {
		changed = false;
		for (int y = 0; y + 1 < ch; ++y) {
			for (int x = 0; x + 1 < cw; ++x) {
				if (c.at(x, y) && c.at(x + 1, y + 1) && !c.at(x + 1, y) && !c.at(x, y + 1)) {
					c.set(x + 1, y, true);
					changed = true;
				} else if (c.at(x + 1, y) && c.at(x, y + 1) && !c.at(x, y) && !c.at(x + 1, y + 1)) {
					c.set(x, y, true);
					changed = true;
				}
			}
		}
	}
	if (!allowHoles) {
		const std::vector<uint8_t> outside = outsideVoid(c);
		for (size_t i = 0; i < c.open.size(); ++i)
			if (!c.open[i] && !outside[i]) c.open[i] = 1;
	}
	// Crop to the open cells' bounding box (every row and column then holds open ground) and double.
	int x0 = cw, y0 = ch, x1 = -1, y1 = -1;
	for (int y = 0; y < ch; ++y)
		for (int x = 0; x < cw; ++x)
			if (c.at(x, y)) {
				x0 = std::min(x0, x);
				y0 = std::min(y0, y);
				x1 = std::max(x1, x);
				y1 = std::max(y1, y);
			}
	Mask m = makeMask((x1 - x0 + 1) * 2, (y1 - y0 + 1) * 2, false);
	for (int y = 0; y < m.h; ++y)
		for (int x = 0; x < m.w; ++x) m.set(x, y, c.at(x0 + x / 2, y0 + y / 2));
	return m;
}

int openRegions(const Mask &m)
{
	std::vector<uint8_t> seen(m.open.size(), 0);
	int regions = 0;
	std::vector<int> stack;
	for (int sy = 0; sy < m.h; ++sy) {
		for (int sx = 0; sx < m.w; ++sx) {
			const size_t s = static_cast<size_t>(sy) * m.w + sx;
			if (!m.open[s] || seen[s]) continue;
			++regions;
			seen[s] = 1;
			stack.push_back(static_cast<int>(s));
			while (!stack.empty()) {
				const int i = stack.back();
				stack.pop_back();
				const int x = i % m.w;
				const int y = i / m.w;
				const int nx[4] = {x + 1, x - 1, x, x};
				const int ny[4] = {y, y, y + 1, y - 1};
				for (int k = 0; k < 4; ++k) {
					if (!m.at(nx[k], ny[k])) continue;
					const size_t j = static_cast<size_t>(ny[k]) * m.w + nx[k];
					if (seen[j]) continue;
					seen[j] = 1;
					stack.push_back(static_cast<int>(j));
				}
			}
		}
	}
	return regions;
}

int enclosedVoidRegions(const Mask &m)
{
	const std::vector<uint8_t> outside = outsideVoid(m);
	std::vector<uint8_t> seen(m.open.size(), 0);
	int regions = 0;
	std::vector<int> stack;
	for (size_t s = 0; s < m.open.size(); ++s) {
		if (m.open[s] || outside[s] || seen[s]) continue;
		++regions;
		seen[s] = 1;
		stack.push_back(static_cast<int>(s));
		while (!stack.empty()) {
			const int i = stack.back();
			stack.pop_back();
			const int x = i % m.w;
			const int y = i / m.w;
			const int nx[4] = {x + 1, x - 1, x, x};
			const int ny[4] = {y, y, y + 1, y - 1};
			for (int k = 0; k < 4; ++k) {
				if (!m.inside(nx[k], ny[k]) || m.at(nx[k], ny[k])) continue;
				const size_t j = static_cast<size_t>(ny[k]) * m.w + nx[k];
				if (seen[j]) continue;
				seen[j] = 1;
				stack.push_back(static_cast<int>(j));
			}
		}
	}
	return regions;
}

Mask borderCells(const Mask &m)
{
	Mask b = makeMask(m.w, m.h, false);
	for (int y = 0; y < m.h; ++y) {
		for (int x = 0; x < m.w; ++x) {
			if (!m.at(x, y)) continue;
			bool edge = false;
			for (int dy = -1; dy <= 1 && !edge; ++dy)
				for (int dx = -1; dx <= 1 && !edge; ++dx) edge = !m.at(x + dx, y + dy);
			b.set(x, y, edge);
		}
	}
	return b;
}

int thinCells(const Mask &m)
{
	int n = 0;
	for (int y = 0; y < m.h; ++y)
		for (int x = 0; x < m.w; ++x)
			n += m.at(x, y) && ((!m.at(x - 1, y) && !m.at(x + 1, y)) || (!m.at(x, y - 1) && !m.at(x, y + 1)));
	return n;
}

Mask engineVoidMarks(const Mask &m, bool *overrun)
{
	if (overrun) *overrun = false;
	Mask marks = makeMask(m.w, m.h, false);
	if (m.w <= 0 || m.h <= 0) return marks;
	const Mask border = borderCells(m);
	const int scans[4][4] = {{0, 0, 1, 1}, {1, 0, -1, 1}, {0, 1, 1, -1}, {1, 1, -1, -1}}; // the table at 0x1d111e0: flagX, flagY, stepX, stepY
	for (const auto &sc : scans) {
		const int x0 = sc[0] ? m.w - 1 : 0;
		int y = sc[1] ? m.h - 1 : 0;
		while (border.inside(x0, y) && !border.at(x0, y)) {
			int x = x0;
			while (border.inside(x, y) && !border.at(x, y)) {
				marks.set(x, y, true);
				x += sc[2];
			}
			if (!border.inside(x, y) && overrun) *overrun = true;
			y += sc[3];
		}
		if (!border.inside(x0, y) && overrun) *overrun = true;
	}
	return marks;
}

int unmarkedVoidCells(const Mask &m)
{
	const Mask marks = engineVoidMarks(m);
	int n = 0;
	for (size_t i = 0; i < m.open.size(); ++i) n += !m.open[i] && !marks.open[i];
	return n;
}

namespace {

struct Edge {
	int ax, ay, bx, by;
	bool used = false;
};

} // namespace

std::vector<Loop> traceLoops(const Mask &m)
{
	std::vector<Edge> edges;
	for (int y = 0; y < m.h; ++y) {
		for (int x = 0; x < m.w; ++x) {
			if (!m.at(x, y)) continue;
			if (!m.at(x, y - 1)) edges.push_back({x, y, x + 1, y});
			if (!m.at(x + 1, y)) edges.push_back({x + 1, y, x + 1, y + 1});
			if (!m.at(x, y + 1)) edges.push_back({x + 1, y + 1, x, y + 1});
			if (!m.at(x - 1, y)) edges.push_back({x, y + 1, x, y});
		}
	}
	std::map<std::pair<int, int>, std::vector<int>> outgoing;
	for (size_t i = 0; i < edges.size(); ++i) outgoing[{edges[i].ay, edges[i].ax}].push_back(static_cast<int>(i));

	std::vector<int> order(edges.size());
	for (size_t i = 0; i < order.size(); ++i) order[i] = static_cast<int>(i);
	std::stable_sort(order.begin(), order.end(), [&](int a, int b) {
		return std::make_pair(edges[a].ay, edges[a].ax) < std::make_pair(edges[b].ay, edges[b].ax);
	});

	std::vector<Loop> loops;
	for (int startIdx : order) {
		if (edges[startIdx].used) continue;
		std::vector<Vertex> raw;
		int cur = startIdx;
		for (;;) {
			edges[cur].used = true;
			raw.push_back({edges[cur].ax, edges[cur].ay});
			const int dx = edges[cur].bx - edges[cur].ax;
			const int dy = edges[cur].by - edges[cur].ay;
			// Prefer a right turn, then straight on, then a left turn: the loop hugs the open cell on its right.
			const int prefer[3][2] = {{-dy, dx}, {dx, dy}, {dy, -dx}};
			int next = -1;
			const auto it = outgoing.find({edges[cur].by, edges[cur].bx});
			for (int p = 0; p < 3 && next < 0 && it != outgoing.end(); ++p) {
				for (int cand : it->second) {
					const bool free = !edges[cand].used || cand == startIdx;
					if (free && edges[cand].bx - edges[cand].ax == prefer[p][0] && edges[cand].by - edges[cand].ay == prefer[p][1]) {
						next = cand;
						break;
					}
				}
			}
			if (next < 0 || next == startIdx) break;
			cur = next;
		}
		Loop loop;
		const size_t n = raw.size();
		for (size_t i = 0; i < n; ++i) {
			const Vertex &p = raw[(i + n - 1) % n];
			const Vertex &c = raw[i];
			const Vertex &q = raw[(i + 1) % n];
			const long long cross = static_cast<long long>(c.x - p.x) * (q.y - c.y) - static_cast<long long>(c.y - p.y) * (q.x - c.x);
			if (cross != 0) loop.v.push_back(c);
		}
		long long twice = 0;
		for (size_t i = 0; i < loop.v.size(); ++i) {
			const Vertex &a = loop.v[i];
			const Vertex &b = loop.v[(i + 1) % loop.v.size()];
			twice += static_cast<long long>(a.x) * b.y - static_cast<long long>(b.x) * a.y;
		}
		loop.area = twice / 2;
		loops.push_back(std::move(loop));
	}
	return loops;
}

std::vector<Loop> traceEngineOutlines(const Mask &m)
{
	std::vector<Loop> loops = traceLoops(m);
	for (Loop &l : loops) {
		const size_t n = l.v.size();
		std::vector<Vertex> cells;
		for (size_t i = 0; i < n; ++i) {
			const Vertex &p = l.v[(i + n - 1) % n];
			const Vertex &c = l.v[i];
			const Vertex &q = l.v[(i + 1) % n];
			const int dx1 = (c.x > p.x) - (c.x < p.x), dy1 = (c.y > p.y) - (c.y < p.y);
			const int dx2 = (q.x > c.x) - (q.x < c.x), dy2 = (q.y > c.y) - (q.y < c.y);
			// The open cells are on the right hand (right of d is (-dy, dx)): the cell index is corner + ((rx - 1) / 2, (ry - 1) / 2).
			cells.push_back({c.x + (-dy1 - dy2 - 1) / 2, c.y + (dx1 + dx2 - 1) / 2});
		}
		l.v = std::move(cells);
	}
	return loops;
}

std::vector<std::string> checkLoopForEngine(const std::vector<Vertex> &loop)
{
	std::vector<std::string> problems;
	if (loop.size() < 4) {
		problems.push_back("an outline needs at least 4 corners");
		return problems;
	}
	for (size_t i = 0; i < loop.size(); ++i) {
		const Vertex &a = loop[i];
		const Vertex &b = loop[(i + 1) % loop.size()];
		const std::string at = "edge " + std::to_string(i) + " (" + std::to_string(a.x) + "," + std::to_string(a.y) + " to " +
			std::to_string(b.x) + "," + std::to_string(b.y) + ")";
		if (a == b)
			problems.push_back(at + " has no length");
		else if (a.x != b.x && a.y != b.y)
			problems.push_back(at + " is not axis-aligned; the border walk has no piece for it and may never reach its end");
	}
	return problems;
}

std::string maskAscii(const Mask &m)
{
	const Mask marks = engineVoidMarks(m);
	std::string out;
	for (int y = 0; y < m.h; ++y) {
		for (int x = 0; x < m.w; ++x) {
			const size_t i = static_cast<size_t>(y) * m.w + x;
			out += m.open[i] ? '#' : (marks.open[i] ? '.' : 'x');
		}
		out += '\n';
	}
	return out;
}

} // namespace cartographer::outdoor::outline
