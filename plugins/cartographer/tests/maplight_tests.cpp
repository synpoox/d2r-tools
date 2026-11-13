// Host tests for a map's look: the value mapping (exposure and colour temperature scales). No game, no SDK.
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
	CHECK(Near(temperatureScale(look), 1.0f));
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

TEST_CASE("warmth is a percent: a warmer light has a lower colour temperature") {
	Look look;
	look.warmth = 200;
	CHECK(look.any());
	CHECK(Near(temperatureScale(look), 0.5f));
	look.warmth = 50;
	CHECK(Near(temperatureScale(look), 2.0f));
}

TEST_CASE("describe names every column") {
	Look look;
	look.brightness = 80;
	look.warmth = 150;
	CHECK(describe(look) == "brightness 80%, warmth 150%");
}
