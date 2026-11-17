#include "maplight.h"

namespace cartographer::maplight {

auto Look::any() const -> bool {
	return brightness != DefaultPercent || warmth != DefaultPercent || changesTint(*this);
}

auto exposureScale(const Look& look) -> float {
	return 100.0f / static_cast<float>(look.brightness);
}

auto lightPowerScale(const Look& look) -> float {
	return 100.0f / static_cast<float>(look.warmth);
}

auto clampLightPower(float power) -> float {
	return power < MinLightPower ? MinLightPower : (power > MaxLightPower ? MaxLightPower : power);
}

auto changesLightPower(const Look& look) -> bool {
	return look.warmth != DefaultPercent;
}

auto changesTint(const Look& look) -> bool {
	return look.tintR != DefaultPercent || look.tintG != DefaultPercent || look.tintB != DefaultPercent;
}

auto describe(const Look& look) -> std::string {
	std::string text = "brightness " + std::to_string(look.brightness) + "%, warmth " + std::to_string(look.warmth) + "%";
	if (changesTint(look)) {
		text += ", tint " + std::to_string(look.tintR) + "/" + std::to_string(look.tintG) + "/" + std::to_string(look.tintB) + "%";
	}
	return text;
}

}  // namespace cartographer::maplight
