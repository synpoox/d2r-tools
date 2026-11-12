// Byte signatures ("48 8B ?? 05"), the way the other plugins find engine code: no fixed addresses, so a part that
// finds no single match stays off. Pure C++ (no Windows) so the tests can run it on any machine.
#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace carto {

inline constexpr size_t MaxPattern = 256;

struct Pattern {
	uint8_t bytes[MaxPattern];
	bool wild[MaxPattern];
	size_t size;
};

inline auto HexDigit(char c) noexcept -> int {
	if (c >= '0' && c <= '9') return c - '0';
	if (c >= 'a' && c <= 'f') return c - 'a' + 10;
	if (c >= 'A' && c <= 'F') return c - 'A' + 10;
	return -1;
}

// False on a malformed signature, an empty one, one longer than MaxPattern or one starting with a wildcard.
inline auto ParsePattern(const char* text, Pattern* out) noexcept -> bool {
	out->size = 0;
	for (const char* p = text; *p != '\0';) {
		if (*p == ' ') {
			++p;
			continue;
		}
		if (out->size == MaxPattern || p[1] == '\0' || (p[2] != ' ' && p[2] != '\0')) {
			return false;
		}
		if (p[0] == '?' && p[1] == '?') {
			out->wild[out->size] = true;
			out->bytes[out->size] = 0;
		} else {
			const int hi = HexDigit(p[0]), lo = HexDigit(p[1]);
			if (hi < 0 || lo < 0) {
				return false;
			}
			out->wild[out->size] = false;
			out->bytes[out->size] = static_cast<uint8_t>(hi * 16 + lo);
		}
		++out->size;
		p += 2;
	}
	return out->size > 0 && !out->wild[0];
}

// Matches of `pat` in data[0, size): returns how many (counting stops at 2) and the first one's offset in *first.
inline auto FindPattern(const uint8_t* data, size_t size, const Pattern& pat, size_t* first) noexcept -> int {
	int count = 0;
	if (pat.size == 0 || size < pat.size) {
		return 0;
	}
	const uint8_t* const end = data + size - pat.size + 1;
	for (const uint8_t* p = data; p < end;) {
		p = static_cast<const uint8_t*>(std::memchr(p, pat.bytes[0], static_cast<size_t>(end - p)));
		if (p == nullptr) {
			break;
		}
		size_t i = 1;
		while (i < pat.size && (pat.wild[i] || p[i] == pat.bytes[i])) {
			++i;
		}
		if (i == pat.size) {
			if (count == 0) {
				*first = static_cast<size_t>(p - data);
			}
			if (++count == 2) {
				break;
			}
		}
		++p;
	}
	return count;
}

}  // namespace carto
