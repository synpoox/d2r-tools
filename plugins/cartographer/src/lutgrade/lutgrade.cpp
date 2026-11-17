#include "lutgrade.h"

#include <cmath>
#include <cstring>

namespace cartographer::lutgrade {

auto pack(const Grade& grade) -> uint32_t {
	// bit 31 marks "a grade"; hue 0..359 in bits 16..24; saturation 0..300 in bits 0..15.
	return 0x80000000u | (static_cast<uint32_t>(grade.normalisedHue()) << 16) | static_cast<uint32_t>(grade.saturation & 0xffff);
}

auto unpack(uint32_t word) -> Grade {
	Grade grade;
	grade.hue = static_cast<int>((word >> 16) & 0x1ff);
	grade.saturation = static_cast<int>(word & 0xffff);
	return grade;
}

namespace {
constexpr float Kr = 0.2126f, Kg = 0.7152f, Kb = 0.0722f;  // Rec. 709 luma

void gradeColour(const Grade& grade, const float in[3], float out[3]) {
	const float y = Kr * in[0] + Kg * in[1] + Kb * in[2];
	const float cb = (in[2] - y) / (2.0f * (1.0f - Kb)), cr = (in[0] - y) / (2.0f * (1.0f - Kr));
	const float angle = static_cast<float>(grade.normalisedHue()) * 3.14159265f / 180.0f, c = std::cos(angle), s = std::sin(angle);
	const float k = static_cast<float>(grade.saturation) / 100.0f;
	const float cb2 = k * (c * cb - s * cr), cr2 = k * (s * cb + c * cr);
	out[2] = y + 2.0f * (1.0f - Kb) * cb2;
	out[0] = y + 2.0f * (1.0f - Kr) * cr2;
	out[1] = (y - Kr * out[0] - Kb * out[2]) / Kg;
}
}  // namespace

auto gradeMatrix(const Grade& grade) -> Matrix {
	Matrix matrix{};
	for (int col = 0; col < 3; col++) {
		float basis[3] = {0, 0, 0}, out[3];
		basis[col] = 1.0f;
		gradeColour(grade, basis, out);
		for (int row = 0; row < 3; row++) {
			matrix.m[row * 3 + col] = out[row];
		}
	}
	return matrix;
}

void applyMatrix(const Matrix& matrix, const float in[3], float out[3]) {
	for (int r = 0; r < 3; r++) {
		const float v = matrix.m[r * 3] * in[0] + matrix.m[r * 3 + 1] * in[1] + matrix.m[r * 3 + 2] * in[2];
		out[r] = v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
	}
}

auto halfToFloat(uint16_t half) -> float {
	const uint32_t sign = static_cast<uint32_t>(half & 0x8000) << 16;
	uint32_t exponent = (half >> 10) & 0x1f, mantissa = half & 0x3ff;
	uint32_t bits = 0;
	if (exponent == 0) {
		if (mantissa == 0) {
			bits = sign;
		} else {  // subnormal: normalise
			exponent = 127 - 15 + 1;
			while ((mantissa & 0x400) == 0) {
				mantissa <<= 1;
				exponent--;
			}
			bits = sign | (exponent << 23) | ((mantissa & 0x3ff) << 13);
		}
	} else if (exponent == 31) {
		bits = sign | 0x7f800000u | (mantissa << 13);
	} else {
		bits = sign | ((exponent + 127 - 15) << 23) | (mantissa << 13);
	}
	float value;
	std::memcpy(&value, &bits, sizeof(value));
	return value;
}

auto floatToHalf(float value) -> uint16_t {
	uint32_t bits;
	std::memcpy(&bits, &value, sizeof(bits));
	const uint16_t sign = static_cast<uint16_t>((bits >> 16) & 0x8000);
	const int32_t exponent = static_cast<int32_t>((bits >> 23) & 0xff) - 127 + 15;
	uint32_t mantissa = bits & 0x7fffff;
	if (((bits >> 23) & 0xff) == 0xff) {
		return static_cast<uint16_t>(sign | 0x7c00 | (mantissa != 0 ? 0x200 : 0));
	}
	if (exponent >= 31) {
		return static_cast<uint16_t>(sign | 0x7c00);
	}
	if (exponent <= 0) {
		if (exponent < -10) {
			return sign;
		}
		mantissa |= 0x800000;
		const int shift = 14 - exponent;
		uint32_t half = mantissa >> shift;
		const uint32_t rest = mantissa & ((1u << shift) - 1), mid = 1u << (shift - 1);
		if (rest > mid || (rest == mid && (half & 1))) {
			half++;
		}
		return static_cast<uint16_t>(sign | half);
	}
	uint32_t half = (static_cast<uint32_t>(exponent) << 10) | (mantissa >> 13);
	const uint32_t rest = mantissa & 0x1fff;
	if (rest > 0x1000 || (rest == 0x1000 && (half & 1))) {
		half++;  // a carry into the exponent is the correct rounding
	}
	return static_cast<uint16_t>(sign | half);
}

void transformLut(uint16_t* rgba, size_t texels, const Grade& grade) {
	const Matrix matrix = gradeMatrix(grade);
	for (size_t i = 0; i < texels; i++) {
		uint16_t* t = rgba + i * 4;
		const float in[3] = {halfToFloat(t[0]), halfToFloat(t[1]), halfToFloat(t[2])};
		float out[3];
		applyMatrix(matrix, in, out);
		for (int k = 0; k < 3; k++) {
			t[k] = floatToHalf(out[k]);
		}
	}
}

}  // namespace cartographer::lutgrade
