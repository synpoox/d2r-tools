// minimap: puts the mini map on the configured side.
//
// D2R places the mini map with AutoMapPanel's layout (0x14e48e0): it anchors the panel at miniOffsetLeft when
// MinimapOnLeft() (0xd34b0) is true or a right-side panel such as the inventory is open, else at miniOffsetRight.
// MinimapOnLeft() only returns a session flag set from the "Automap Size" option (Mini Map left/right) and flipped by
// the Toggle Minimap key. This part rewrites that 8-byte getter to a constant, so the option and the key no longer
// move the mini map; D2R's own move to the left while a right-side panel is open stays.
//
// A layout file cannot do this: the side comes from code, and a game-wide plugin cannot ship a mod's layout JSON.
#include "config.h"
#include "d2r.h"
#include "parts.h"

#include <cstdio>

auto InstallMinimapSide(const D2RL::PluginContext* context, char* report, size_t size) noexcept -> bool {
	const bool left = config::Get().minimapLeft;
	// mov al,1 / ret (left) or xor al,al / ret (right), then int3 padding: the same 8 bytes, one whole function.
	const uint8_t onLeft[] = {0xb0, 0x01, 0xc3, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc};
	const uint8_t onRight[] = {0x32, 0xc0, 0xc3, 0xcc, 0xcc, 0xcc, 0xcc, 0xcc};
	static_assert(sizeof(onLeft) == sizeof(d2r::MinimapOnLeftBytes) && sizeof(onRight) == sizeof(d2r::MinimapOnLeftBytes));
	const bool ok = context->PatchBytes(d2r::RvaMinimapOnLeft, d2r::MinimapOnLeftBytes, sizeof(d2r::MinimapOnLeftBytes),
	                                    left ? onLeft : onRight, sizeof(onLeft));
	if (ok) {
		std::snprintf(report, size, "on: mini map on the %s (0x%llx patched)", left ? "left" : "right",
		              static_cast<unsigned long long>(d2r::RvaMinimapOnLeft));
	} else {
		std::snprintf(report, size, "off: patch at 0x%llx refused (bytes differ: another plugin or another image)",
		              static_cast<unsigned long long>(d2r::RvaMinimapOnLeft));
	}
	return ok;
}
