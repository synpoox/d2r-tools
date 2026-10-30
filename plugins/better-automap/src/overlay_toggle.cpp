// overlay: while the mini map is shown, the automap key (Tab) opens the full-screen overlay in its place, and the next
// press brings the mini map back as it was.
//
// D2R draws the mini map and the full-screen map with the same panel (AutoMapPanel, panel id 10); the "Automap Size"
// mode decides which one. So the mini map is hidden whenever the overlay is up, and the overlay is turned off by
// restoring the mode. The mode is changed with the setting's own change callback (0xd3d80), the same call D2R makes
// when the option changes, so only the session changes: the saved option stays as the player set it.
//
// The automap key's handler is a D2RLoader thunk (D2RCore already replaces it). This part does not patch it: it
// swaps the key table slot for a wrapper (byte-checked against the thunk's address) and calls the thunk for every
// press it does not handle. With the part off, the slot is never touched.
#include "config.h"
#include "d2r.h"
#include "parts.h"

#include <cstdint>
#include <cstdio>

namespace {

d2r::KeyHandlerFn g_original = nullptr;
bool g_overlayUp = false;  // the overlay replaced a mini map; its mode is in g_savedMode
int32_t g_savedMode = 2;   // 1 = mini map left, 2 = mini map right
long long g_opened = 0;  // counters: game main thread only
long long g_restored = 0;
long long g_passed = 0;

void ApplyMode(int32_t mode) noexcept {
	d2r::Function<d2r::ApplyAutoMapModeFn>(d2r::RvaApplyAutoMapMode)(mode);
}

void AutoMapKey(uint64_t a, uint64_t b, uint64_t c, uint64_t d) noexcept {
	const bool open = d2r::PanelIsOpen(d2r::PanelAutoMap);
	if (g_overlayUp && d2r::MiniMapMode()) {
		g_overlayUp = false;  // the mode was changed elsewhere (option, Automap Center key, new game): theirs wins
	}
	if (g_overlayUp) {
		g_overlayUp = false;
		ApplyMode(g_savedMode);
		++g_restored;
		if (open) {
			return;  // overlay off, mini map back
		}
		// The panel was closed some other way (Escape): the mode is restored, the key opens the mini map as usual.
	} else if (open && d2r::MiniMapMode()) {
		g_savedMode = *d2r::Global<const uint8_t>(d2r::MinimapLeftFlag) != 0 ? 1 : 2;
		ApplyMode(0);
		g_overlayUp = true;
		++g_opened;
		return;
	}
	++g_passed;
	g_original(a, b, c, d);
}

}  // namespace

auto InstallOverlayToggle(const D2RL::PluginContext* context, char* report, size_t size) noexcept -> bool {
	const uint64_t thunk = context->exeBase + d2r::RvaAutoMapKeyThunk;
	const uint64_t current = *d2r::Global<const uint64_t>(d2r::RvaAutoMapKeySlot);
	if (current != thunk) {
		std::snprintf(report, size, "off: automap key slot 0x%llx holds 0x%llx, expected the D2RLoader thunk exe+0x%llx",
		              static_cast<unsigned long long>(d2r::RvaAutoMapKeySlot),
		              static_cast<unsigned long long>(current - context->exeBase),
		              static_cast<unsigned long long>(d2r::RvaAutoMapKeyThunk));
		return false;
	}
	g_original = reinterpret_cast<d2r::KeyHandlerFn>(thunk);
	const bool ok = context->PatchWriteU64(d2r::RvaAutoMapKeySlot, &thunk, sizeof(thunk),
	                                       reinterpret_cast<uint64_t>(&AutoMapKey));
	if (ok) {
		std::snprintf(report, size, "on: automap key slot 0x%llx wrapped", static_cast<unsigned long long>(d2r::RvaAutoMapKeySlot));
	} else {
		g_original = nullptr;
		std::snprintf(report, size, "off: write to the automap key slot 0x%llx refused",
		              static_cast<unsigned long long>(d2r::RvaAutoMapKeySlot));
	}
	return ok;
}

void StatusOverlayToggle(char* out, size_t size) noexcept {
	std::snprintf(out, size, "overlay %s, %lld opened over the mini map, %lld mini map restored, %lld presses passed on",
	              g_overlayUp ? "up" : "down", g_opened, g_restored, g_passed);
}
