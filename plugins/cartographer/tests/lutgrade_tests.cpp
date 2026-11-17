// Host tests for the hue/saturation grade: the colour matrix, the half float conversion and the LUT transform. No game, no SDK.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "lutgrade.h"

#include <cmath>
#include <vector>

using namespace cartographer::lutgrade;

namespace {
auto Near(float a, float b, float eps = 0.002f) -> bool {
	return std::fabs(a - b) < eps;
}
auto Luma(const float c[3]) -> float {
	return 0.2126f * c[0] + 0.7152f * c[1] + 0.0722f * c[2];
}
void Apply(const Grade& g, float r, float gr, float b, float out[3]) {
	const float in[3] = {r, gr, b};
	applyMatrix(gradeMatrix(g), in, out);
}
}  // namespace

TEST_CASE("the default grade changes nothing and is not a grade") {
	const Grade grade;
	CHECK_FALSE(grade.any());
	float out[3];
	Apply(grade, 0.8f, 0.4f, 0.1f, out);
	CHECK(Near(out[0], 0.8f));
	CHECK(Near(out[1], 0.4f));
	CHECK(Near(out[2], 0.1f));
}

TEST_CASE("hue is folded into one turn: 360 is no grade, -90 equals 270") {
	Grade a;
	a.hue = 360;
	CHECK_FALSE(a.any());
	Grade b, c;
	b.hue = -90;
	c.hue = 270;
	CHECK(b.any());
	CHECK(b == c);
	c.saturation = 50;
	CHECK_FALSE(b == c);
}

TEST_CASE("saturation 0 is the Rec. 709 luma as grey") {
	Grade grade;
	grade.saturation = 0;
	float out[3];
	Apply(grade, 0.8f, 0.4f, 0.1f, out);
	const float in[3] = {0.8f, 0.4f, 0.1f};
	CHECK(Near(out[0], Luma(in)));
	CHECK(Near(out[1], Luma(in)));
	CHECK(Near(out[2], Luma(in)));
}

TEST_CASE("saturation 50 is halfway between grey and the colour") {
	Grade grade;
	grade.saturation = 50;
	float out[3];
	Apply(grade, 0.8f, 0.4f, 0.1f, out);
	const float in[3] = {0.8f, 0.4f, 0.1f};
	const float y = Luma(in);
	CHECK(Near(out[0], y + 0.5f * (0.8f - y)));
	CHECK(Near(out[2], y + 0.5f * (0.1f - y)));
}

TEST_CASE("grey stays grey for every hue and saturation") {
	for (int hue : {0, 45, 120, 200, -70}) {
		for (int sat : {0, 100, 250}) {
			Grade grade;
			grade.hue = hue;
			grade.saturation = sat;
			float out[3];
			Apply(grade, 0.5f, 0.5f, 0.5f, out);
			CHECK(Near(out[0], 0.5f));
			CHECK(Near(out[1], 0.5f));
			CHECK(Near(out[2], 0.5f));
		}
	}
}

TEST_CASE("a hue rotation keeps the luma (no brightness swing) for colours that stay in range") {
	const float in[3] = {0.45f, 0.35f, 0.3f};  // a warm brown
	for (int hue : {30, 90, 180, 270, 330}) {
		Grade grade;
		grade.hue = hue;
		float out[3];
		Apply(grade, in[0], in[1], in[2], out);
		CHECK(Near(Luma(out), Luma(in), 0.01f));
	}
}

TEST_CASE("a hue rotation moves the colour") {
	Grade grade;
	grade.hue = 120;
	float out[3];
	Apply(grade, 0.45f, 0.35f, 0.3f, out);
	CHECK(std::fabs(out[0] - 0.45f) > 0.03f);
	CHECK(std::fabs(out[1] - 0.35f) > 0.03f);
}

TEST_CASE("three 120 degree turns come back to the start") {
	Grade grade;
	grade.hue = 120;
	float a[3], b[3], c[3];
	Apply(grade, 0.45f, 0.35f, 0.3f, a);
	Apply(grade, a[0], a[1], a[2], b);
	Apply(grade, b[0], b[1], b[2], c);
	CHECK(Near(c[0], 0.45f));
	CHECK(Near(c[1], 0.35f));
	CHECK(Near(c[2], 0.3f));
}

TEST_CASE("a boosted saturation is clamped to the table's 0..1 range") {
	Grade grade;
	grade.saturation = 300;
	float out[3];
	Apply(grade, 0.9f, 0.1f, 0.1f, out);
	for (int k = 0; k < 3; k++) {
		CHECK(out[k] >= 0.0f);
		CHECK(out[k] <= 1.0f);
	}
}

TEST_CASE("half floats round-trip the values a table holds") {
	for (float v : {0.0f, 1.0f, 0.5f, 0.015625f, 0.984375f, 0.3332f}) {
		CHECK(Near(halfToFloat(floatToHalf(v)), v, 0.0005f));
	}
	CHECK(floatToHalf(1.0f) == 0x3c00);
	CHECK(floatToHalf(0.5f) == 0x3800);
	CHECK(floatToHalf(0.0f) == 0);
	CHECK(halfToFloat(0x3c00) == 1.0f);
	CHECK(Near(halfToFloat(0x0001), 5.96e-8f, 1e-8f));  // smallest subnormal
}

TEST_CASE("packing a grade round-trips and is never zero") {
	Grade grade;
	CHECK(pack(grade) != 0);
	grade.hue = -90;
	grade.saturation = 175;
	const Grade back = unpack(pack(grade));
	CHECK(back == grade);
	CHECK(back.hue == 270);
}

TEST_CASE("transforming an identity table keeps its size and alpha and greys the colours") {
	std::vector<uint16_t> lut(LutTexels * 4);
	for (size_t i = 0; i < LutTexels; i++) {
		lut[i * 4 + 0] = floatToHalf(static_cast<float>(i % LutEdge) / 64.0f);
		lut[i * 4 + 1] = floatToHalf(static_cast<float>((i / LutEdge) % LutEdge) / 64.0f);
		lut[i * 4 + 2] = floatToHalf(static_cast<float>(i / (LutEdge * LutEdge)) / 64.0f);
		lut[i * 4 + 3] = floatToHalf(1.0f);
	}
	Grade grade;
	grade.saturation = 0;
	transformLut(lut.data(), LutTexels, grade);
	for (size_t i = 0; i < LutTexels; i++) {
		CHECK(lut[i * 4 + 3] == 0x3c00);
	}
	// texel (64, 64, 64) is white and stays white; texel (64, 0, 0) is red, now grey at its luma 0.2126
	const size_t white = 64 + 64 * LutEdge + 64 * LutEdge * LutEdge, red = 64;
	CHECK(Near(halfToFloat(lut[white * 4]), 1.0f));
	CHECK(Near(halfToFloat(lut[red * 4]), 0.2126f));
	CHECK(Near(halfToFloat(lut[red * 4 + 1]), 0.2126f));
	CHECK(Near(halfToFloat(lut[red * 4 + 2]), 0.2126f));
}
