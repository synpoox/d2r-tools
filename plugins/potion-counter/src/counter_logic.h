// Pure logic of potion-counter: no Windows, no SDK, so tests/counter_logic_test.cpp runs it on any machine.
#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace pc {

constexpr int Columns = 4;
constexpr int Rows = 4;  // the largest stock belt

// The stock belt items and their HD inventory pictures, under data/hd/global/ui/items/misc/ (data/hd/items/items.json).
// shortcut: a mod that gives these codes other pictures keeps the stock ones here; upgrade: read items.json at load.
struct Icon {
	char code[4];
	const char* sprite;
};
constexpr Icon Icons[] = {
	{"hp1", "potion/lesser_healing_potion"}, {"hp2", "potion/light_healing_potion"}, {"hp3", "potion/healing_potion2"},
	{"hp4", "potion/strong_healing_potion"}, {"hp5", "potion/greater_healing_potion"}, {"hpo", "potion/healing_potion"},
	{"hpf", "potion/full_healing_potion"},   {"mp1", "potion/lesser_mana_potion"},     {"mp2", "potion/light_mana_potion"},
	{"mp3", "potion/mana_potion2"},          {"mp4", "potion/strong_mana_potion"},     {"mp5", "potion/greater_mana_potion"},
	{"mpo", "potion/mana_potion"},           {"mpf", "potion/full_mana_potion"},       {"rvs", "potion/rejuv_potion"},
	{"rvl", "potion/full_rejuv_potion"},     {"yps", "potion/antidote_potion"},        {"vps", "potion/stamina_potion"},
	{"wms", "potion/thawing_potion"},        {"tsc", "scroll/town_portal_scroll"},     {"isc", "scroll/identify_scroll"},
};
constexpr int IconCount = static_cast<int>(sizeof(Icons) / sizeof(Icons[0]));

// What a belt square shows: Empty, 1 + an index into Icons, or Other (an item with no stock picture, a mod's own).
using Kind = uint8_t;
constexpr Kind Empty = 0;
constexpr Kind Other = IconCount + 1;

// The item code as the SDK gives it: four bytes, first character lowest, short codes padded with spaces.
inline auto KindOf(uint32_t code) noexcept -> Kind {
	for (int i = 0; i < IconCount; ++i) {
		const char* const c = Icons[i].code;
		if (code == (static_cast<uint32_t>(static_cast<uint8_t>(c[0])) | static_cast<uint32_t>(static_cast<uint8_t>(c[1])) << 8 |
		             static_cast<uint32_t>(static_cast<uint8_t>(c[2])) << 16 | static_cast<uint32_t>(' ') << 24)) {
			return static_cast<Kind>(i + 1);
		}
	}
	return Other;
}

// The rows a belt with this many boxes has (belts.txt numboxes / 4); no belt has 4 boxes, so 1 row.
constexpr auto RowsOf(int boxes) noexcept -> int {
	return boxes <= 4 ? 1 : boxes >= Columns * Rows ? Rows : (boxes + Columns - 1) / Columns;
}

struct BeltItem {
	int32_t x;  // the belt slot, row * 4 + column
	uint32_t code;
};

// squares[column][row]: the kind in each belt slot, row 0 at the bottom. A slot outside 0..15 is skipped.
inline void FillSquares(const BeltItem* items, size_t n, Kind squares[Columns][Rows]) noexcept {
	for (int c = 0; c < Columns; ++c) {
		for (int r = 0; r < Rows; ++r) {
			squares[c][r] = Empty;
		}
	}
	for (size_t i = 0; i < n; ++i) {
		if (items[i].x >= 0 && items[i].x < Columns * Rows) {
			squares[items[i].x % Columns][items[i].x / Columns] = KindOf(items[i].code);
		}
	}
}

inline auto CountColumn(const Kind squares[Rows]) noexcept -> int {
	int n = 0;
	for (int r = 0; r < Rows; ++r) {
		n += squares[r] != Empty ? 1 : 0;
	}
	return n;
}

// --- Byte signatures (the same scheme as player-bars; plugins share no code) -----------------------------------------

constexpr size_t MaxPattern = 192;

inline auto HexDigit(char c) noexcept -> int {
	if (c >= '0' && c <= '9') return c - '0';
	if (c >= 'a' && c <= 'f') return c - 'a' + 10;
	if (c >= 'A' && c <= 'F') return c - 'A' + 10;
	return -1;
}

// "48 8B ?? 05": hex bytes and ?? wildcards separated by spaces.
struct Pattern {
	uint8_t bytes[MaxPattern];
	bool wild[MaxPattern];
	size_t size;
};

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
	const uint8_t lead = pat.bytes[0];
	const uint8_t* const end = data + size - pat.size + 1;
	for (const uint8_t* p = data; p < end;) {
		p = static_cast<const uint8_t*>(std::memchr(p, lead, static_cast<size_t>(end - p)));
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

}  // namespace pc
