// Pure logic of player-bars: no Windows, no SDK, so tests/bars_logic_test.cpp runs it on any machine.
#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace pb {

struct Rgb {
	float r, g, b;
};

struct Settings {
	bool hideOrbs = true;
	bool showText = false;
	bool below = false;  // under the character instead of above
	float barWidth = 90.0f;
	float healthHeight = 9.0f;
	float manaHeight = 7.0f;
	float offsetY = 150.0f;
	float offsetBelow = 15.0f;
	Rgb healthColor {0.784f, 0.118f, 0.118f};
	Rgb manaColor {0.157f, 0.314f, 0.824f};
	Rgb backColor {0.078f, 0.078f, 0.078f};
	Rgb borderColor {0.0f, 0.0f, 0.0f};
};

inline auto HexDigit(char c) noexcept -> int {
	if (c >= '0' && c <= '9') return c - '0';
	if (c >= 'a' && c <= 'f') return c - 'a' + 10;
	if (c >= 'A' && c <= 'F') return c - 'A' + 10;
	return -1;
}

// "RRGGBB"; false (and *out unchanged) otherwise.
inline auto ParseColor(const char* text, size_t size, Rgb* out) noexcept -> bool {
	if (size != 6) {
		return false;
	}
	float channels[3];
	for (size_t i = 0; i < 3; ++i) {
		const int hi = HexDigit(text[i * 2]), lo = HexDigit(text[i * 2 + 1]);
		if (hi < 0 || lo < 0) {
			return false;
		}
		channels[i] = static_cast<float>(hi * 16 + lo) / 255.0f;
	}
	*out = {channels[0], channels[1], channels[2]};
	return true;
}

// Sizes must be positive and sane; a bad value keeps the default.
inline void ParseSize(const char* value, float* out, float max) noexcept {
	const float v = std::strtof(value, nullptr);
	if (v > 0.0f && v <= max) {
		*out = v;
	}
}

// The TOML subset the default file uses: `key = value` lines, comments, one section. Unknown keys and bad values keep
// the defaults, so a broken file never turns the bars off.
inline auto ParseSettings(const char* toml) noexcept -> Settings {
	Settings s;
	if (toml == nullptr) {
		return s;
	}
	for (const char* line = toml; *line != '\0';) {
		const char* const end = line + std::strcspn(line, "\r\n");
		const char* p = line;
		while (p < end && (*p == ' ' || *p == '\t')) {
			++p;
		}
		const char* const key = p;
		while (p < end && *p != '=' && *p != ' ' && *p != '\t') {
			++p;
		}
		const size_t keySize = static_cast<size_t>(p - key);
		while (p < end && (*p == ' ' || *p == '\t')) {
			++p;
		}
		if (p < end && *p == '=' && keySize > 0 && *key != '#' && *key != '[') {
			++p;
			while (p < end && (*p == ' ' || *p == '\t')) {
				++p;
			}
			char value[32] {};
			const size_t n = static_cast<size_t>(end - p) < sizeof(value) - 1 ? static_cast<size_t>(end - p) : sizeof(value) - 1;
			std::memcpy(value, p, n);
			const auto is = [&](const char* name) { return keySize == std::strlen(name) && std::memcmp(key, name, keySize) == 0; };
			const auto color = [&](Rgb* out) {
				if (value[0] == '"') {
					(void)ParseColor(value + 1, std::strcspn(value + 1, "\""), out);
				}
			};
			const bool yes = std::strncmp(value, "true", 4) == 0;
			if (is("hide_orbs")) s.hideOrbs = yes;
			else if (is("position")) s.below = std::strncmp(value, "\"bottom\"", 8) == 0;
			else if (is("show_text")) s.showText = yes;
			else if (is("bar_width")) ParseSize(value, &s.barWidth, 1000.0f);
			else if (is("health_height")) ParseSize(value, &s.healthHeight, 200.0f);
			else if (is("mana_height")) ParseSize(value, &s.manaHeight, 200.0f);
			else if (is("offset_y")) ParseSize(value, &s.offsetY, 1000.0f);
			else if (is("offset_below")) ParseSize(value, &s.offsetBelow, 1000.0f);
			else if (is("health_color")) color(&s.healthColor);
			else if (is("mana_color")) color(&s.manaColor);
			else if (is("back_color")) color(&s.backColor);
			else if (is("border_color")) color(&s.borderColor);
		}
		line = end;
		while (*line == '\r' || *line == '\n') {
			++line;
		}
	}
	return s;
}

// 0..1; a missing or zero maximum reads as empty.
inline auto Fraction(int32_t value, int32_t max) noexcept -> float {
	if (max <= 0 || value <= 0) {
		return 0.0f;
	}
	return value >= max ? 1.0f : static_cast<float>(value) / static_cast<float>(max);
}

constexpr int Segments = 50;   // pieces per bar
constexpr int PanelCount = 3;  // centre, left, right

inline auto FilledSegments(int32_t value, int32_t max) noexcept -> int {
	const int n = static_cast<int>(Fraction(value, max) * static_cast<float>(Segments) + 0.5f);
	return value > 0 && n == 0 ? 1 : n;  // never look empty while alive
}

// D2R keeps the player centred and moves the view a quarter of its logical width away from an open side panel. That
// width is 4:3 of the height (0xb9c20), a third of the height in all: 720 layout units.
constexpr int ViewShiftUnits = 720;

// shift: 0 none, 1 right panel, 2 left panel, 3 both. Returns 0 centre, 1 left, 2 right.
inline auto PanelForShift(int32_t shift) noexcept -> int {
	return shift == 1 ? 1 : shift == 2 ? 2 : 0;
}

// --- Byte signatures (the same scheme as scrollable-waypoints; plugins share no code) ----------------------------------

constexpr size_t MaxPattern = 192;

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

}  // namespace pb
