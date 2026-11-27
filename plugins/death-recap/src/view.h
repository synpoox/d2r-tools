#pragma once
#include "summary.h"
#include <cmath>
#include <sstream>
#include <iomanip>

namespace recap::view {
inline std::string Number(int64_t raw) {
	const auto whole = static_cast<int64_t>(std::llround(double(raw) / 256.));
	std::string s = std::to_string(whole);
	for (int i = int(s.size()) - 3; i > (whole < 0 ? 1 : 0); i -= 3)
		s.insert(size_t(i), ",");
	return s;
}
inline std::string Value(const Amount &a) { return a.Confirmed() ? Number(a.value) : "Unavailable"; }
inline std::string Seconds(uint64_t ms) {
	std::ostringstream o;
	o << std::fixed << std::setprecision(1) << double(ms) / 1000.;
	return o.str();
}
inline std::string Shorten(const std::string &s, size_t limit) {
	if (s.size() <= limit) return s;
	size_t end = limit;
	while (end && (static_cast<unsigned char>(s[end]) & 0xc0) == 0x80)
		--end;
	return s.substr(0, end) + "...";
}
inline std::vector<std::string> Wrap(const std::vector<std::string> &lines) {
	std::vector<std::string> result;
	for (const auto &line : lines) {
		size_t start = 0;
		while (line.size() - start > 60) {
			size_t end = start + 60;
			while (end > start && (static_cast<unsigned char>(line[end]) & 0xc0) == 0x80)
				--end;
			const size_t space = line.rfind(' ', end);
			if (space > start && space != std::string::npos) end = space;
			result.push_back(line.substr(start, end - start));
			start = end;
			if (line[start] == ' ') ++start;
		}
		result.push_back(line.substr(start));
	}
	return result;
}
struct ElementRow {
	size_t element;
	int64_t amount;
	double fraction;
	bool partial;
};
inline std::vector<ElementRow> Elements(const Summary &s) {
	std::vector<ElementRow> rows;
	int64_t total = 0;
	for (size_t i = 0; i < s.postByElement.size(); ++i) {
		const auto &m = s.postByElement[i];
		if (m.known && m.total > 0) {
			rows.push_back({i, m.total, 0, m.unknown != 0});
			total += m.total;
		}
	}
	std::stable_sort(rows.begin(), rows.end(), [](const auto &a, const auto &b) { return a.amount > b.amount; });
	for (auto &row : rows)
		row.fraction = double(row.amount) / double(total);
	return rows;
}
struct GraphColumn {
	bool known = false, healing = false;
	int low = 0, high = 0, life = 0;
};
struct Graph {
	static constexpr size_t Columns = 420;
	std::array<GraphColumn, Columns> columns{};
	int64_t maximum = 256;
};
// A step trace follows observed endpoints. Never interpolate across a sequence
// gap, an unknown endpoint or more than 500ms without an observation.
inline Graph Life(const Report &r, int height = 280, size_t columns = Graph::Columns) {
	columns = std::clamp<size_t>(columns, 1, Graph::Columns);
	Graph g;
	for (const auto &e : r.timeline) {
		for (const auto *a : {&e.maxLife, &e.life.before, &e.life.after})
			if (a->Confirmed()) g.maximum = std::max<int64_t>(g.maximum, a->value);
	}
	const uint64_t duration = std::max<uint64_t>(1, r.deathMs - r.windowStartMs);
	auto x = [&](uint64_t ms) { return size_t(std::min<uint64_t>(columns - 1, (ms - r.windowStartMs) * (columns - 1) / duration)); };
	auto y = [&](int32_t value) { return int(std::clamp<int64_t>(int64_t(value) * height / g.maximum, 0, height)); };
	const Event *previous = nullptr;
	for (const auto &e : r.timeline) {
		if (e.ms < r.windowStartMs || e.ms > r.deathMs) continue;
		if (!e.life.after.Confirmed()) {
			previous = nullptr;
			continue;
		}
		const size_t at = x(e.ms);
		if (previous && e.sequence == previous->sequence + 1 && e.ms - previous->ms <= 500 &&
			(!e.life.before.Confirmed() || e.life.before.value == previous->life.after.value)) {
			for (size_t i = x(previous->ms) + 1; i < at; ++i) {
				auto &c = g.columns[i];
				c.known = true;
				c.life = c.low = c.high = y(previous->life.after.value);
			}
		}
		auto &c = g.columns[at];
		const int after = y(e.life.after.value), before = e.life.before.Confirmed() ? y(e.life.before.value) : after;
		const int lo = std::min(before, after), hi = std::max(before, after);
		c.low = c.known ? std::min(c.low, lo) : lo;
		c.high = c.known ? std::max(c.high, hi) : hi;
		c.known = true;
		c.life = after;
		c.healing = c.healing || (GrossKnown(e.life) && e.life.restored.value > 0);
		previous = &e;
	}
	return g;
}
struct Navigation {
	std::array<bool, 3> expanded{};
	int focus = 0, scroll = 0;
	void Reset() noexcept {
		expanded = {};
		focus = scroll = 0;
	}
	int Height() const noexcept {
		int h = 1240;
		for (bool e : expanded)
			if (e) h += 400;
		return h;
	}
	void Scroll(int delta) noexcept { scroll = std::clamp(scroll + delta, 0, std::max(0, Height() - 1290)); }
	void Toggle(int section) noexcept {
		if (section < 0 || section > 2) return;
		expanded[size_t(section)] = !expanded[size_t(section)];
		focus = section;
		int top = 870 + section * 130;
		for (int i = 0; i < section; ++i)
			if (expanded[size_t(i)]) top += 400;
		if (expanded[size_t(section)] && top + 510 - scroll > 1290) scroll = std::max(350, top + 510 - 1290);
		Scroll(0);
	}
};
} // namespace recap::view
