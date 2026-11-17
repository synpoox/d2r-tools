// A map's own look (issue #380): the maps.txt columns Brightness and Warmth, and what they become: a scale for the level's exposure
// and a scale for the strength of the light around the player. Pure C++ (no engine, no SDK). The hook that applies them: src/engine/map_lighting.cpp;
// notes: docs/reverse-engineering/cartographer/32-level-lighting.md.
#pragma once

#include "lutgrade.h"

#include <string>

namespace cartographer::maplight {

// Percent columns: 100 is the tileset's own look. A blank cell keeps the default.
inline constexpr int DefaultPercent = 100;
inline constexpr int MinBrightness = 10, MaxBrightness = 400;
inline constexpr int MinWarmth = 25, MaxWarmth = 400;
// TintR, TintG, TintB: the percent each light colour channel is multiplied by (0 removes the channel, 100 keeps it). Maps only.
inline constexpr int MinTint = 0, MaxTint = 200;
// Hue (degrees, 0 keeps the tileset's colours) and Saturation (percent, 100 keeps them): the whole picture, through the colour-grading table. Maps only.
inline constexpr int MinHue = lutgrade::MinHue, MaxHue = lutgrade::MaxHue;
inline constexpr int MinSaturation = lutgrade::MinSaturation, MaxSaturation = lutgrade::MaxSaturation;

struct Look {
	int brightness = DefaultPercent;
	int warmth = DefaultPercent;
	int tintR = DefaultPercent, tintG = DefaultPercent, tintB = DefaultPercent;
	int hue = lutgrade::DefaultHue, saturation = lutgrade::DefaultSaturation;

	// True when any column differs from the tileset's own look.
	auto any() const -> bool;
	// The hue and saturation as a grade (any() is false when both are blank or neutral).
	auto grade() const -> lutgrade::Grade;
};

// The factor the level's exposure value is multiplied by for `brightness` percent. The game's exposure value divides the picture
// (a higher value is darker), so this is 100 / brightness.
auto exposureScale(const Look& look) -> float;

// The factor the player light power (component +0x40) is multiplied by for `warmth` percent: it divides the power, so a higher Warmth
// is a dimmer light around the player (100 / warmth). It is not a colour temperature; colour is TintR/G/B.
auto lightPowerScale(const Look& look) -> float;

// The light powers seen working in game (1000 to 20000, note 32): a result outside this range is clamped to it, because
// below it the game's behaviour was not checked.
inline constexpr float MinLightPower = 1000.0f, MaxLightPower = 20000.0f;
auto clampLightPower(float power) -> float;

// False when Warmth is blank or 100: the light power is then not written at all, so a Brightness-only map never clamps
// the tileset's own light power.
auto changesLightPower(const Look& look) -> bool;

// False when TintR, TintG and TintB are all blank or 100: the light colours are then not touched.
auto changesTint(const Look& look) -> bool;

// A short description for the log.
auto describe(const Look& look) -> std::string;

}  // namespace cartographer::maplight
