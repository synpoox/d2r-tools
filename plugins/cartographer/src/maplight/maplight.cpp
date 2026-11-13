#include "maplight.h"

namespace cartographer::maplight {

auto Look::any() const -> bool {
	return brightness != DefaultPercent || warmth != DefaultPercent;
}

auto exposureScale(const Look& look) -> float {
	return 100.0f / static_cast<float>(look.brightness);
}

auto temperatureScale(const Look& look) -> float {
	return 100.0f / static_cast<float>(look.warmth);
}

auto clampKelvin(float kelvin) -> float {
	return kelvin < MinKelvin ? MinKelvin : (kelvin > MaxKelvin ? MaxKelvin : kelvin);
}

auto changesTemperature(const Look& look) -> bool {
	return look.warmth != DefaultPercent;
}

auto describe(const Look& look) -> std::string {
	return "brightness " + std::to_string(look.brightness) + "%, warmth " + std::to_string(look.warmth) + "%";
}

}  // namespace cartographer::maplight
