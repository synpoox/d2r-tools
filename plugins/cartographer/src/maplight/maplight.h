// A map's own look (issue #380): the maps.txt columns Brightness and Warmth, and what they become: a scale for the level's exposure
// and a scale for its colour temperature. Pure C++ (no engine, no SDK). The hook that applies them: src/engine/map_lighting.cpp;
// notes: docs/reverse-engineering/cartographer/32-level-lighting.md.
#pragma once

#include <string>

namespace cartographer::maplight {

// Percent columns: 100 is the tileset's own look. A blank cell keeps the default.
inline constexpr int DefaultPercent = 100;
inline constexpr int MinBrightness = 10, MaxBrightness = 400;
inline constexpr int MinWarmth = 25, MaxWarmth = 400;

struct Look {
	int brightness = DefaultPercent;
	int warmth = DefaultPercent;

	// True when any column differs from the tileset's own look.
	auto any() const -> bool;
};

// The factor the level's exposure value is multiplied by for `brightness` percent. The game's exposure value divides the picture
// (a higher value is darker), so this is 100 / brightness.
auto exposureScale(const Look& look) -> float;

// The factor the level's colour temperature (Kelvin) is multiplied by for `warmth` percent: a warmer light is a lower temperature,
// so this is 100 / warmth.
auto temperatureScale(const Look& look) -> float;

// A short description for the log.
auto describe(const Look& look) -> std::string;

}  // namespace cartographer::maplight
