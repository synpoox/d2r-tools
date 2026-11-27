#include "view.h"
#include "preview.h"
#include <cstdlib>
#include <iostream>
using namespace recap;
#define CHECK(x)                                                                                                                                               \
	do {                                                                                                                                                       \
		if (!(x)) {                                                                                                                                            \
			std::cerr << __LINE__ << ": " << #x << '\n';                                                                                                       \
			std::exit(1);                                                                                                                                      \
		}                                                                                                                                                      \
	} while (false)
int main() {
	const auto preview = PreviewReport();
	const auto graph = view::Life(preview);
	CHECK(graph.maximum == 1000 * 256);
	CHECK(graph.columns.front().known && graph.columns.front().life == 280);
	CHECK(graph.columns.back().known && graph.columns.back().life == 0);
	const auto small = view::Life(preview, 280, 280);
	CHECK(small.columns[279].known && small.columns[279].life == 0 && !small.columns[280].known);
	CHECK(std::count_if(graph.columns.begin(), graph.columns.end(), [](auto c) { return c.healing; }) == 2);
	Report gap;
	gap.windowStartMs = 0;
	gap.deathMs = 1000;
	Event e = Sample({1000, 500, 1000, 500});
	e.sequence = 1;
	gap.timeline.push_back(e);
	e.ms = 1000;
	e.sequence = 2;
	e.life.after.value = 500;
	gap.timeline.push_back(e);
	auto broken = view::Life(gap);
	CHECK(!broken.columns[200].known);
	gap.timeline.back().ms = 400;
	gap.timeline.back().sequence = 3;
	CHECK(!view::Life(gap).columns[80].known);
	gap.timeline.back().sequence = 2;
	gap.timeline.back().life.after = {};
	CHECK(!view::Life(gap).columns[80].known);
	const auto rows = view::Elements(Summarize(preview));
	CHECK(rows.size() == 4 && rows[0].element == 2 && rows[0].fraction > .63 && rows[0].fraction < .65);
	Summary s;
	s.postByElement[0] = {100, 1, 0};
	s.postByElement[2] = {300, 1, 1};
	s.postByElement[4] = {0, 1, 0};
	auto sorted = view::Elements(s);
	CHECK(sorted.size() == 2 && sorted[0].element == 2 && sorted[0].partial && sorted[0].fraction == .75);
	CHECK(view::Number(1842 * 256) == "1,842");
	CHECK(view::Number(0) == "0");
	CHECK(view::Shorten("abc\xc3\xa9z", 4) == "abc...");
	const auto wrapped = view::Wrap({std::string(230, 'x')});
	CHECK(wrapped.size() == 4 && wrapped[0] + wrapped[1] + wrapped[2] + wrapped[3] == std::string(230, 'x'));
	view::Navigation nav;
	CHECK(nav.scroll == 0 && !nav.expanded[0] && !nav.expanded[1] && !nav.expanded[2]);
	nav.Toggle(0);
	CHECK(nav.expanded[0] && nav.scroll == 350);
	nav.Toggle(1);
	nav.Toggle(2);
	nav.Scroll(10000);
	CHECK(nav.scroll == nav.Height() - 1290);
	nav.Toggle(0);
	nav.Toggle(1);
	nav.Toggle(2);
	CHECK(nav.scroll == 0);
	nav.Reset();
	CHECK(nav.focus == 0 && nav.scroll == 0);
	std::cout << "presentation ordering, graph gaps, healing, formatting and expansion tests passed\n";
}
