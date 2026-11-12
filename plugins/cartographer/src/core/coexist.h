// The notice shown when core-maps (the PD2-Resurrected map plugin) is loaded next to Cartographer. Pure text, no game, no SDK.
// ADR-0001: a mod ships one or the other. Both hook item spell 16, so the second to load fails to install its hook.
#pragma once

#include <string>

namespace carto {

// Empty when core-maps is not loaded.
inline auto CoreMapsNotice(bool coreMapsLoaded) -> std::string {
	if (!coreMapsLoaded) {
		return std::string();
	}
	return "core-maps is loaded next to Cartographer. They cannot be used together: both hook item spell 16 and the same spawn functions, "
	       "so one of the two fails to install its hook and the map items may not work. Remove one of the two plugins from d2rloader\\plugins and restart the game.";
}

}  // namespace carto
