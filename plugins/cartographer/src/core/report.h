// What the table loader found, in plain English. The same list is written to cartographer-report.txt at load and
// printed by the console command "cartographer check".
#pragma once

#include <string>
#include <vector>

namespace carto {

enum class Severity { Error, Note };

struct Problem {
	Severity severity = Severity::Error;
	std::string file;  // "maps.txt"
	int line = 0;      // 0: the problem is with the whole file
	std::string text;
};

struct Report {
	std::vector<Problem> problems;

	static constexpr int MaxPerFile = 200;  // a hostile or broken table cannot make the report huge

	void Error(const std::string& file, int line, const std::string& text) {
		int same = 0;
		for (const Problem& p : problems) {
			same += p.file == file ? 1 : 0;
		}
		if (same < MaxPerFile) {
			problems.push_back({Severity::Error, file, line, text});
		} else if (same == MaxPerFile) {
			problems.push_back({Severity::Note, file, 0, "more than 200 problems; the rest are not listed."});
		}
	}
	void Note(const std::string& file, int line, const std::string& text) { problems.push_back({Severity::Note, file, line, text}); }

	auto Errors() const -> int {
		int n = 0;
		for (const Problem& p : problems) {
			n += p.severity == Severity::Error ? 1 : 0;
		}
		return n;
	}

	// "maps.txt row 3: tileset 'Jungel' is not known. Did you mean 'Jungle'?"
	static auto Line(const Problem& p) -> std::string {
		std::string out = p.severity == Severity::Note ? "note: " : "";
		out += p.file;
		if (p.line > 0) {
			out += " row " + std::to_string(p.line);
		}
		return out + ": " + p.text;
	}
};

}  // namespace carto
