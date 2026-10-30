// Spec strip art (strip_art.h).
#include "strip_art.h"

#include <cstring>

namespace ms::art {

namespace {

struct Rgba {
	int r, g, b, a;
};

auto Clamp(int v) noexcept -> uint8_t {
	return static_cast<uint8_t>(v < 0 ? 0 : (v > 255 ? 255 : v));
}

// Integer hash -> 0..255, the same everywhere (no rand), so the art never changes between runs.
auto Hash(int32_t x, int32_t y, uint32_t seed) noexcept -> int {
	uint32_t h = static_cast<uint32_t>(x) * 374761393U + static_cast<uint32_t>(y) * 668265263U + seed * 2246822519U;
	h = (h ^ (h >> 13)) * 1274126177U;
	return static_cast<int>((h ^ (h >> 16)) & 0xFF);
}

// Smooth value noise on a `cell`-px grid, 0..255.
auto Noise(int32_t x, int32_t y, int32_t cell, uint32_t seed) noexcept -> int {
	const int32_t gx = x / cell;
	const int32_t gy = y / cell;
	const int fx = (x % cell) * 256 / cell;
	const int fy = (y % cell) * 256 / cell;
	const int a = Hash(gx, gy, seed);
	const int b = Hash(gx + 1, gy, seed);
	const int c = Hash(gx, gy + 1, seed);
	const int d = Hash(gx + 1, gy + 1, seed);
	const int top = a + (b - a) * fx / 256;
	const int bottom = c + (d - c) * fx / 256;
	return top + (bottom - top) * fy / 256;
}

// Dark grey stone like the stock panels' fill: two octaves of noise plus a little grain.
auto Stone(int32_t x, int32_t y, int32_t scale) noexcept -> Rgba {
	const int32_t hx = x * scale;  // sample in HD units, so the lowend twin looks the same
	const int32_t hy = y * scale;
	const int n = (Noise(hx, hy, 48, 1) * 5 + Noise(hx, hy, 12, 2) * 3 + Hash(hx, hy, 3)) / 9;
	const int v = 38 + n * 30 / 255;
	return {v + 2, v, v - 3, 255};
}

void Put(uint8_t* px, Rgba c) noexcept {
	px[0] = Clamp(c.r);
	px[1] = Clamp(c.g);
	px[2] = Clamp(c.b);
	px[3] = Clamp(c.a);
}

auto WriteHeader(uint8_t* out, int32_t frameWidth, int32_t width, int32_t height, int32_t frames) noexcept -> uint8_t* {
	auto put32 = [&](size_t at, uint32_t v) { std::memcpy(out + at, &v, 4); };
	const uint16_t version = 31;
	const auto fw = static_cast<uint16_t>(frameWidth);
	std::memcpy(out, "SpA1", 4);
	std::memcpy(out + 4, &version, 2);
	std::memcpy(out + 6, &fw, 2);
	put32(8, static_cast<uint32_t>(width));
	put32(12, static_cast<uint32_t>(height));
	put32(16, 0);
	put32(20, static_cast<uint32_t>(frames));
	put32(24, 0);
	put32(28, 0);
	put32(32, static_cast<uint32_t>(width) * static_cast<uint32_t>(height) * 4);
	put32(36, 4);
	return out + SpriteHeaderSize;
}

// Metal frame band of `band` px: a dark outer line, a lit edge on the top and left, a shaded edge on the bottom and
// right, and a dark inner line where it meets the stone. d = distance from the outer edge (0 = outermost).
auto Frame(int32_t d, int32_t band, bool litSide, int32_t x, int32_t y, int32_t scale) noexcept -> Rgba {
	if (d < 2) {
		return {14, 13, 12, 255};
	}
	if (d >= band - 2) {
		return {18, 16, 14, 255};
	}
	const int grain = (Hash(x * scale, y * scale, 7) - 128) / 16;
	const int ramp = (band - d) * 30 / band;  // brighter towards the outside
	const int base = (litSide ? 92 : 62) + ramp + grain;
	return {base + 4, base + 2, base - 4, 255};
}

}  // namespace

auto DrawWing(int32_t slots, int32_t scale, uint8_t* out, size_t size) noexcept -> size_t {
	if (slots < 1 || scale < 1) {
		return 0;
	}
	const int32_t w = WingWidth / scale;
	const int32_t h = WingHeight(slots) / scale;
	const size_t need = SpriteBytes(w, h);
	if (out == nullptr || size < need) {
		return 0;
	}
	uint8_t* px = WriteHeader(out, w, w, h, 1);
	const int32_t band = WingFrame / scale;
	const int32_t rivet = 10 / scale;  // rivet centre inset from each corner, inside the band
	for (int32_t y = 0; y < h; ++y) {
		for (int32_t x = 0; x < w; ++x, px += 4) {
			const int32_t dl = x;
			const int32_t dt = y;
			const int32_t dr = w - 1 - x;
			const int32_t db = h - 1 - y;
			const int32_t d = dl < dt ? (dl < dr ? (dl < db ? dl : db) : (dr < db ? dr : db))
			                          : (dt < dr ? (dt < db ? dt : db) : (dr < db ? dr : db));
			Rgba c = d < band ? Frame(d, band, d == dl || d == dt, x, y, scale) : Stone(x, y, scale);
			// Rivets on the four corners of the visible wing (the right ones sit left of the tucked part).
			const int32_t rx[2] = {rivet, (w - WingTuck / scale) - rivet};
			const int32_t ry[2] = {rivet, h - 1 - rivet};
			for (int32_t i = 0; i < 2; ++i) {
				for (int32_t j = 0; j < 2; ++j) {
					const int32_t ddx = x - rx[i];
					const int32_t ddy = y - ry[j];
					const int32_t r = 4 / scale + 1;
					if (ddx * ddx + ddy * ddy <= r * r) {
						const int lit = (ddx + ddy) < 0 ? 40 : 0;
						c = {120 + lit, 98 + lit, 52 + lit / 2, 255};
					}
				}
			}
			Put(px, c);
		}
	}
	return need;
}

auto DrawSocket(int32_t scale, uint8_t* out, size_t size) noexcept -> size_t {
	if (scale < 1) {
		return 0;
	}
	const int32_t s = SocketSize / scale;
	const size_t need = SpriteBytes(s * 2, s);
	if (out == nullptr || size < need) {
		return 0;
	}
	uint8_t* px = WriteHeader(out, s, s * 2, s, 2);
	const int32_t bevel = 6 / scale + 1;
	for (int32_t y = 0; y < s; ++y) {
		for (int32_t x = 0; x < s * 2; ++x, px += 4) {
			const bool hovered = x >= s;
			const int32_t fx = hovered ? x - s : x;
			const int32_t d = [&] {
				const int32_t a = fx < y ? fx : y;
				const int32_t b = (s - 1 - fx) < (s - 1 - y) ? (s - 1 - fx) : (s - 1 - y);
				return a < b ? a : b;
			}();
			Rgba c {};
			if (d == 0) {
				c = {10, 9, 8, 255};  // outline
			} else if (d < bevel) {
				const bool shadowSide = fx + y < s;  // recessed: the top-left bevel is in shadow, bottom-right lit
				const int v = shadowSide ? 30 : 84;
				c = {v + 3, v + 1, v - 3, 255};
			} else if (d < bevel + 2) {
				c = hovered ? Rgba {214, 186, 112, 255} : Rgba {120, 98, 52, 255};  // gold rim, bright when hovered
			} else {
				const int n = (Hash(fx * scale, y * scale, 11) - 128) / 24;
				const int depth = (d - bevel) * 10 / (s / 2);  // a little lighter towards the centre
				const int v = 20 + depth + n + (hovered ? 6 : 0);
				c = {v + 2, v, v - 2, 255};
			}
			Put(px, c);
		}
	}
	return need;
}

}  // namespace ms::art
