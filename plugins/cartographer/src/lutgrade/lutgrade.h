// Colour math for a map's own Hue and Saturation (issue #442): the grading look-up table the renderer applies to the whole scene is
// replaced, while the player is in the map, by the identity table pushed through one hue/saturation matrix. Pure C++ (no engine, no SDK).
// The hooks that swap the table: src/engine/lut_grade.cpp; notes: docs/reverse-engineering/cartographer/50-colour-grading.md.
#pragma once

#include <cstddef>
#include <cstdint>

namespace cartographer::lutgrade {

// maps.txt columns: Hue in degrees (0 is the tileset's own colours), Saturation in percent (100 is the tileset's own colours, 0 is grey).
inline constexpr int DefaultHue = 0, DefaultSaturation = 100;
inline constexpr int MinHue = -360, MaxHue = 360;
inline constexpr int MinSaturation = 0, MaxSaturation = 300;

struct Grade {
	int hue = DefaultHue;
	int saturation = DefaultSaturation;

	// The hue folded into 0..359, so -90 and 270 (and 360 and 0) are the same grade.
	auto normalisedHue() const -> int { return ((hue % 360) + 360) % 360; }
	// True when the grade differs from the tileset's own colours.
	auto any() const -> bool { return normalisedHue() != 0 || saturation != DefaultSaturation; }
	auto operator==(const Grade& other) const -> bool { return normalisedHue() == other.normalisedHue() && saturation == other.saturation; }
};

// Packs a grade into one non-zero word for an atomic hand-over between threads (0 is "no grade"). Round-trips through unpack.
auto pack(const Grade& grade) -> uint32_t;
auto unpack(uint32_t word) -> Grade;

// The game's grading table: 65 x 65 x 65 texels of four half floats (RGBA), red the fastest index.
inline constexpr size_t LutEdge = 65;
inline constexpr size_t LutTexels = LutEdge * LutEdge * LutEdge;

// A 3x3 colour matrix, row-major: out = m * in.
struct Matrix {
	float m[9];
};

// Luma-preserving: Rec. 709 luma is kept, the colour difference is rotated by the hue and scaled by the saturation. Hue 0 and saturation
// 100 is the identity; saturation 0 is the luma as grey; grey stays grey for any hue.
auto gradeMatrix(const Grade& grade) -> Matrix;

// Applies the matrix to one colour and clamps each channel to 0..1.
void applyMatrix(const Matrix& matrix, const float in[3], float out[3]);

// IEEE half float <-> float (round to nearest even; NaN payloads are not kept).
auto halfToFloat(uint16_t half) -> float;
auto floatToHalf(float value) -> uint16_t;

// Transforms an RGBA half float buffer of `texels` texels in place (alpha untouched).
void transformLut(uint16_t* rgba, size_t texels, const Grade& grade);

}  // namespace cartographer::lutgrade
