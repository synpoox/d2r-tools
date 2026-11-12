// Small text helpers for the table loader: trimming, splitting, case-insensitive compare and spelling suggestions.
// Pure C++ (no Windows, no SDK) so tests/tables_test.cpp runs on any machine.
#pragma once

#include <algorithm>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace carto {

inline auto Lower(char c) noexcept -> char {
	return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c;
}

inline auto Lower(std::string_view s) -> std::string {
	std::string out(s);
	for (char& c : out) {
		c = Lower(c);
	}
	return out;
}

// Keeps A-Z a-z 0-9 and writes every other byte as '_' and two uppercase hex digits ("a b" -> "a_20b"): ASCII only, one-to-one.
// The boss string key and the superuniques row key are built with it; tools/install.ps1 (Get-BossEscaped) does the same.
inline auto EscapeKeyBytes(std::string_view s) -> std::string {
	static const char* Hex = "0123456789ABCDEF";
	std::string out;
	for (const char ch : s) {
		const auto byte = static_cast<unsigned char>(ch);
		if ((byte >= 'A' && byte <= 'Z') || (byte >= 'a' && byte <= 'z') || (byte >= '0' && byte <= '9')) {
			out += ch;
		} else {
			out += '_';
			out += Hex[byte >> 4];
			out += Hex[byte & 15];
		}
	}
	return out;
}

inline auto Trim(std::string_view s) -> std::string {
	size_t a = 0, b = s.size();
	while (a < b && (s[a] == ' ' || s[a] == '\t' || s[a] == '\r' || s[a] == '\n')) {
		++a;
	}
	while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r' || s[b - 1] == '\n')) {
		--b;
	}
	return std::string(s.substr(a, b - a));
}

inline auto EqualsNoCase(std::string_view a, std::string_view b) noexcept -> bool {
	if (a.size() != b.size()) {
		return false;
	}
	for (size_t i = 0; i < a.size(); ++i) {
		if (Lower(a[i]) != Lower(b[i])) {
			return false;
		}
	}
	return true;
}

// Pieces between `sep`, each trimmed. "a, b,,c" with ',' gives {"a", "b", "", "c"}.
inline auto Split(std::string_view s, char sep) -> std::vector<std::string> {
	std::vector<std::string> out;
	size_t start = 0;
	for (;;) {
		const size_t at = s.find(sep, start);
		out.push_back(Trim(s.substr(start, at == std::string_view::npos ? std::string_view::npos : at - start)));
		if (at == std::string_view::npos) {
			return out;
		}
		start = at + 1;
	}
}

// Edit distance ignoring case; a swap of two neighbouring letters counts as one edit ("Jungel" -> "Jungle").
inline constexpr size_t MaxSuggestWord = 64;  // longer words get no suggestion: the work grows with the product of lengths

inline auto EditDistance(std::string_view a, std::string_view b) -> int {
	const size_t n = a.size(), m = b.size();
	std::vector<std::vector<int>> d(n + 1, std::vector<int>(m + 1, 0));
	for (size_t i = 0; i <= n; ++i) {
		d[i][0] = static_cast<int>(i);
	}
	for (size_t j = 0; j <= m; ++j) {
		d[0][j] = static_cast<int>(j);
	}
	for (size_t i = 1; i <= n; ++i) {
		for (size_t j = 1; j <= m; ++j) {
			const int cost = Lower(a[i - 1]) == Lower(b[j - 1]) ? 0 : 1;
			d[i][j] = std::min({d[i - 1][j] + 1, d[i][j - 1] + 1, d[i - 1][j - 1] + cost});
			if (i > 1 && j > 1 && Lower(a[i - 1]) == Lower(b[j - 2]) && Lower(a[i - 2]) == Lower(b[j - 1])) {
				d[i][j] = std::min(d[i][j], d[i - 2][j - 2] + 1);
			}
		}
	}
	return d[n][m];
}

// The name closest to `word` (the same name in another case first), or "" when nothing is close enough to be a guess.
inline auto Suggest(std::string_view word, const std::vector<std::string>& names) -> std::string {
	if (word.size() > MaxSuggestWord) {
		return {};
	}
	const int allowed = std::max(1, std::min(3, static_cast<int>(word.size()) / 3));
	int best = allowed + 1;
	std::string pick;
	for (const std::string& name : names) {
		if (name.size() > MaxSuggestWord) {
			continue;
		}
		const int d = EditDistance(word, name);
		if (d < best) {
			best = d;
			pick = name;
		}
	}
	return pick;
}

// " Did you mean 'X'?" or "" when there is no good guess.
inline auto DidYouMean(std::string_view word, const std::vector<std::string>& names) -> std::string {
	const std::string pick = Suggest(word, names);
	return pick.empty() ? std::string() : " Did you mean '" + pick + "'?";
}

// "'a', 'b' or 'c'" for a list of choices, in the error text.
inline auto OneOf(const std::vector<std::string>& names) -> std::string {
	std::string out;
	for (size_t i = 0; i < names.size(); ++i) {
		out += i == 0 ? "" : i + 1 == names.size() ? " or " : ", ";
		out += "'" + names[i] + "'";
	}
	return out;
}

}  // namespace carto
