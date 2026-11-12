// Tab-separated tables with a header row, like D2's own: columns are found by (case-insensitive) header name, so their
// order does not matter and unknown columns can be reported. Blank lines are skipped. Rows keep their line number in
// the file (the header is line 1) so messages can say where the problem is.
#pragma once

#include "text.h"

#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace carto {

struct TsvRow {
	int line = 0;
	std::vector<std::string> cells;
};

struct Tsv {
	std::vector<std::string> header;
	std::vector<TsvRow> rows;

	// Index of the column called `name`, or -1.
	auto Column(std::string_view name) const -> int {
		for (size_t i = 0; i < header.size(); ++i) {
			if (EqualsNoCase(header[i], name)) {
				return static_cast<int>(i);
			}
		}
		return -1;
	}

	// The trimmed cell, or "" when the column is missing, the row is short or the cell is blank.
	auto Cell(const TsvRow& row, int column) const -> std::string {
		return column >= 0 && static_cast<size_t>(column) < row.cells.size() ? row.cells[static_cast<size_t>(column)] : std::string();
	}
};

inline auto ParseTsv(std::string_view text) -> Tsv {
	Tsv out;
	if (text.size() >= 3 && static_cast<unsigned char>(text[0]) == 0xEF && static_cast<unsigned char>(text[1]) == 0xBB &&
	    static_cast<unsigned char>(text[2]) == 0xBF) {
		text.remove_prefix(3);  // UTF-8 byte order mark
	}
	int line = 0;
	bool haveHeader = false;
	size_t start = 0;
	while (start <= text.size()) {
		size_t end = text.find('\n', start);
		if (end == std::string_view::npos) {
			end = text.size();
		}
		++line;
		const std::string_view raw = text.substr(start, end - start);
		start = end + 1;
		std::vector<std::string> cells = Split(raw, '\t');
		const bool blank = std::all_of(cells.begin(), cells.end(), [](const std::string& c) { return c.empty(); });
		if (blank) {
			continue;
		}
		if (!haveHeader) {
			out.header = std::move(cells);
			haveHeader = true;
		} else {
			out.rows.push_back({line, std::move(cells)});
		}
	}
	return out;
}

}  // namespace carto
