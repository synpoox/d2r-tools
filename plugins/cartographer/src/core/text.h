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

// Text tables are UTF-8. True when `s` is strictly valid UTF-8 (no overlong forms, surrogates or values above U+10FFFF), the same
// test as .NET's UTF8Encoding(false, true) that tools/install.ps1 uses.
inline auto IsValidUtf8(std::string_view s) noexcept -> bool {
	size_t i = 0;
	const auto at = [&](size_t k) { return static_cast<unsigned char>(s[k]); };
	while (i < s.size()) {
		const unsigned c = at(i);
		size_t extra = 0;
		unsigned lo = 0x80, hi = 0xBF;  // allowed range of the first continuation byte
		if (c < 0x80) {
			++i;
			continue;
		} else if (c >= 0xC2 && c <= 0xDF) {
			extra = 1;
		} else if (c >= 0xE0 && c <= 0xEF) {
			extra = 2;
			if (c == 0xE0) lo = 0xA0;
			if (c == 0xED) hi = 0x9F;
		} else if (c >= 0xF0 && c <= 0xF4) {
			extra = 3;
			if (c == 0xF0) lo = 0x90;
			if (c == 0xF4) hi = 0x8F;
		} else {
			return false;
		}
		if (i + extra >= s.size()) {
			return false;
		}
		const unsigned first = at(i + 1);
		if (first < lo || first > hi) {
			return false;
		}
		for (size_t k = 2; k <= extra; ++k) {
			if ((at(i + k) & 0xC0) != 0x80) {
				return false;
			}
		}
		i += extra + 1;
	}
	return true;
}

// Each byte read as one Latin-1 character and written as UTF-8 (0xE9 -> C3 A9).
inline auto Latin1ToUtf8(std::string_view s) -> std::string {
	std::string out;
	out.reserve(s.size());
	for (const char ch : s) {
		const auto byte = static_cast<unsigned char>(ch);
		if (byte < 0x80) {
			out += ch;
		} else {
			out += static_cast<char>(0xC0 | (byte >> 6));
			out += static_cast<char>(0x80 | (byte & 0x3F));
		}
	}
	return out;
}

// The text of a table file as UTF-8: a UTF-8 byte order mark is dropped, and text that is not valid UTF-8 is taken as Latin-1
// (what an editor's "ANSI" save gives for Western names) and converted. *fromLatin1 says which. tools/install.ps1 does the same.
inline auto NormalizeTableText(std::string_view raw, bool* fromLatin1 = nullptr) -> std::string {
	if (raw.size() >= 3 && static_cast<unsigned char>(raw[0]) == 0xEF && static_cast<unsigned char>(raw[1]) == 0xBB &&
	    static_cast<unsigned char>(raw[2]) == 0xBF) {
		raw.remove_prefix(3);
	}
	const bool latin = !IsValidUtf8(raw);
	if (fromLatin1 != nullptr) {
		*fromLatin1 = latin;
	}
	return latin ? Latin1ToUtf8(raw) : std::string(raw);
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
