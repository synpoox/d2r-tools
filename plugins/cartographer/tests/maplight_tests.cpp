// Host tests for a map's look: the value mapping (exposure and player light power scales). No game, no SDK.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "maplight.h"

using namespace cartographer::maplight;

namespace {
auto Near(float a, float b) -> bool {
	return a - b < 0.002f && b - a < 0.002f;
}
}  // namespace

TEST_CASE("the default look changes nothing") {
	const Look look;
	CHECK_FALSE(look.any());
	CHECK(Near(exposureScale(look), 1.0f));
	CHECK(Near(lightPowerScale(look), 1.0f));
}

TEST_CASE("brightness is a percent of the tileset's brightness: the exposure value goes down as the picture gets brighter") {
	Look look;
	look.brightness = 200;
	CHECK(look.any());
	CHECK(Near(exposureScale(look), 0.5f));
	look.brightness = 50;
	CHECK(Near(exposureScale(look), 2.0f));
	look.brightness = 10;
	CHECK(Near(exposureScale(look), 10.0f));
}

TEST_CASE("warmth is a percent: a higher Warmth gives a lower light power") {
	Look look;
	look.warmth = 200;
	CHECK(look.any());
	CHECK(Near(lightPowerScale(look), 0.5f));
	look.warmth = 50;
	CHECK(Near(lightPowerScale(look), 2.0f));
}

TEST_CASE("the light power is clamped to the range seen working in game") {
	CHECK(Near(clampLightPower(2125.0f), 2125.0f));
	CHECK(Near(clampLightPower(531.0f), 1000.0f));
	CHECK(Near(clampLightPower(50000.0f), 20000.0f));
}

TEST_CASE("a Brightness-only look does not write the light power") {
	Look look;
	look.brightness = 150;
	CHECK(look.any());
	CHECK_FALSE(changesLightPower(look));
	look.warmth = 120;
	CHECK(changesLightPower(look));
	look.warmth = 100;
	CHECK_FALSE(changesLightPower(look));
}

TEST_CASE("a tint counts as a look and is named by describe") {
	Look look;
	CHECK_FALSE(changesTint(look));
	look.tintB = 0;
	CHECK(changesTint(look));
	CHECK(look.any());
	CHECK(describe(look) == "brightness 100%, warmth 100%, tint 100/100/0%");
}

TEST_CASE("describe names every column") {
	Look look;
	look.brightness = 80;
	look.warmth = 150;
	CHECK(describe(look) == "brightness 80%, warmth 150%");
}
