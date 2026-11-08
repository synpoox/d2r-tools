// Pure logic of dps-counter: no Windows, no SDK, so tests/dps_logic_test.cpp runs it on any machine.
#pragma once

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace dps {

struct Settings {
	bool enabled = true;
	float windowSeconds = 5.0f;  // damage over this many seconds, divided by them
	float offsetX = -495.0f;     // the label's centre from the screen's centre, in pixels at 1080p: over the life globe
	float offsetY = 830.0f;      // the label's top from the top of the screen, just above the "Life:" line
	float textSize = 22.0f;
};

// "true" / "false" (also 1 / 0); anything else keeps the default.
inline void ParseBool(const char* value, bool* out) noexcept {
	if (std::strncmp(value, "true", 4) == 0 || value[0] == '1') {
		*out = true;
	} else if (std::strncmp(value, "false", 5) == 0 || value[0] == '0') {
		*out = false;
	}
}

// A value outside [min, max] keeps the default.
inline void ParseNumber(const char* value, float* out, float min, float max) noexcept {
	char* end = nullptr;
	const float v = std::strtof(value, &end);
	if (end != value && v >= min && v <= max) {
		*out = v;
	}
}

// The TOML subset the default file uses: `key = value` lines, comments, one section. Unknown keys and bad values keep
// the defaults, so a broken file never turns the counter off.
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
			if (is("enabled")) ParseBool(value, &s.enabled);
			else if (is("window_seconds")) ParseNumber(value, &s.windowSeconds, 1.0f, 60.0f);
			else if (is("offset_x")) ParseNumber(value, &s.offsetX, -2000.0f, 2000.0f);
			else if (is("offset_y")) ParseNumber(value, &s.offsetY, 0.0f, 1000.0f);
			else if (is("text_size")) ParseNumber(value, &s.textSize, 8.0f, 100.0f);
		}
		line = end;
		while (*line == '\r' || *line == '\n') {
			++line;
		}
	}
	return s;
}

// Damage in tenth-of-a-second buckets over the last `window` seconds. A fight that started less than a window ago is
// divided by its own length (at least one second), so the number does not climb slowly from zero.
class Window {
public:
	static constexpr int BucketsPerSecond = 10;
	static constexpr int MaxBuckets = 60 * BucketsPerSecond;

	explicit Window(float windowSeconds) noexcept
		: size_(static_cast<int>(windowSeconds * BucketsPerSecond + 0.5f)) {
		size_ = size_ < BucketsPerSecond ? BucketsPerSecond : size_ > MaxBuckets ? MaxBuckets : size_;
	}

	void Add(double seconds, int64_t damage) noexcept {
		Advance(seconds);
		if (damage <= 0) {
			return;
		}
		if (total_ == 0) {
			fightStart_ = now_;
		}
		sums_[now_ % MaxBuckets] += damage;
		total_ += damage;
	}

	// Damage per second at `seconds`; 0 once a whole window passed with no damage.
	auto PerSecond(double seconds) noexcept -> double {
		Advance(seconds);
		if (total_ <= 0) {
			return 0.0;
		}
		int64_t span = now_ - fightStart_ + 1;
		span = span < BucketsPerSecond ? BucketsPerSecond : span > size_ ? size_ : span;
		return static_cast<double>(total_) * BucketsPerSecond / static_cast<double>(span);
	}

private:
	void Advance(double seconds) noexcept {
		const int64_t bucket = static_cast<int64_t>(seconds * BucketsPerSecond);
		if (now_ < 0 || bucket - now_ >= size_) {
			for (int64_t& sum : sums_) {
				sum = 0;
			}
			total_ = 0;
		} else {
			for (int64_t b = now_ + 1; b <= bucket; ++b) {
				int64_t& old = sums_[((b - size_) % MaxBuckets + MaxBuckets) % MaxBuckets];  // the bucket leaving the window
				total_ -= old;
				old = 0;
			}
		}
		if (bucket > now_) {
			now_ = bucket;
		}
	}

	int size_;
	int64_t now_ = -1;
	int64_t fightStart_ = 0;
	int64_t total_ = 0;
	int64_t sums_[MaxBuckets] {};
};

// The hit's total damage in the damage record D2Game's 0x44a9b0 gets, in 1/256 life (0x451fb0 reads it so); 0 if none.
constexpr size_t DamageTotalOffset = 0x134;

// Only a hit that landed: the game reads the total only when result flag bit 0 at +4 is set (0x451fb0), and the
// melee caller also passes a miss's record.
constexpr size_t DamageResultOffset = 0x4;

inline auto DamageTaken(const uint8_t* damage) noexcept -> int64_t {
	uint16_t result = 0;
	std::memcpy(&result, damage + DamageResultOffset, sizeof(result));
	if ((result & 1) == 0) {
		return 0;
	}
	int32_t total = 0;
	std::memcpy(&total, damage + DamageTotalOffset, sizeof(total));
	return total > 0 ? total : 0;
}

// "12,345"; "" for zero.
inline void FormatDps(double perSecond, char* out, size_t size) noexcept {
	const auto value = static_cast<int64_t>(perSecond + 0.5);
	if (value <= 0) {
		if (size > 0) {
			out[0] = '\0';
		}
		return;
	}
	char digits[24];
	const int n = std::snprintf(digits, sizeof(digits), "%lld", static_cast<long long>(value));
	char grouped[40];
	int g = 0;
	for (int i = 0; i < n; ++i) {
		if (i > 0 && (n - i) % 3 == 0) {
			grouped[g++] = ',';
		}
		grouped[g++] = digits[i];
	}
	grouped[g] = '\0';
	std::snprintf(out, size, "DPS: %s", grouped);
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

}  // namespace dps
