// zoom: mouse wheel up zooms in, wheel down zooms out on the full-screen automap overlay.
//
// Two hooks on AutoMapPanel (D2R's own code, not hooked by D2RLoader):
//  - Draw (0x14e3480): while zoomed and in full-screen mode, multiplies the panel's tileScale (+0x15a8, read only by
//    Draw) for the duration of the call and puts the layout value back afterwards. Nothing is left changed.
//  - OnMessage (0x14e4470): takes InputMessage/MouseScroll only when the overlay is up, no UI panel consumed the wheel
//    first (D2R's own "input handled" flag), no side panel is open (inventory, stash, character, skills, ...) and the
//    chat is closed. It then marks the wheel handled, as D2R's arrow-key pan does. Every other message, and the wheel
//    in every other case, goes to D2R untouched.
#include "config.h"
#include "d2r.h"
#include "parts.h"

#include <cstdint>
#include <cstdio>

namespace {

d2r::DrawFn g_draw = nullptr;
d2r::OnMessageFn g_onMessage = nullptr;
bool g_active = false;  // both hooks in

float g_zoom = 1.0f;
int g_wheel = 0;  // wheel delta not yet worth a whole step (high-resolution wheels send less than 120)
long long g_zoomed = 0;  // counters: game main thread only
long long g_passed = 0;

auto Clamp(float z) noexcept -> float {
	const config::Settings& s = config::Get();
	return z < s.zoomMin ? s.zoomMin : z > s.zoomMax ? s.zoomMax : z;
}

auto OverlayUp() noexcept -> bool {
	return d2r::PanelIsOpen(d2r::PanelAutoMap) && !d2r::MiniMapMode();
}

auto WheelIsOurs() noexcept -> bool {
	return OverlayUp() && !d2r::Function<d2r::InputHandledFn>(d2r::RvaInputHandled)() &&
	       *d2r::Global<const uint32_t>(d2r::ScreenShift) == 0 && !d2r::PanelIsOpen(d2r::PanelChat);
}

void Draw(void* panel) noexcept {
	if (!g_active || g_zoom == 1.0f || d2r::MiniMapMode()) {
		g_draw(panel);
		return;
	}
	auto* scale = reinterpret_cast<float*>(static_cast<uint8_t*>(panel) + d2r::AutoMapTileScale);
	const float layout = *scale;
	*scale = layout * g_zoom;
	g_draw(panel);
	*scale = layout;
}

auto OnMessage(void* panel, void* msg) noexcept -> uint64_t {
	const auto* m = static_cast<const uint8_t*>(msg);
	if (m != nullptr && d2r::At<uint64_t>(m, d2r::MsgType) == d2r::HashInputMessage &&
	    d2r::At<uint64_t>(m, d2r::MsgSubtype) == d2r::HashMouseScroll) {
		const auto* payload = d2r::At<const uint8_t*>(m, d2r::MsgPayload);
		if (payload != nullptr && WheelIsOurs()) {
			g_wheel += d2r::At<int32_t>(payload, d2r::ScrollDelta);
			const int steps = g_wheel / d2r::WheelDelta;
			g_wheel -= steps * d2r::WheelDelta;
			if (steps != 0) {
				g_zoom = Clamp(g_zoom + static_cast<float>(steps) * config::Get().zoomStep);
			}
			d2r::Function<d2r::MarkInputHandledFn>(d2r::RvaMarkInputHandled)();
			++g_zoomed;
		} else {
			g_wheel = 0;
			++g_passed;
		}
	}
	return g_onMessage(panel, msg);
}

auto AlreadyHooked(uintptr_t exeBase, uint64_t rva) noexcept -> bool {
	const auto* entry = reinterpret_cast<const uint8_t*>(exeBase + rva);
	return entry[0] == 0xff && entry[1] == 0x25;  // jmp [rip+x]: D2RLoader or another plugin is there
}

}  // namespace

auto InstallOverlayZoom(const D2RL::PluginContext* context, char* report, size_t size) noexcept -> bool {
	g_zoom = Clamp(1.0f);
	for (const uint64_t rva : {d2r::RvaAutoMapDraw, d2r::RvaAutoMapOnMessage}) {
		if (AlreadyHooked(context->exeBase, rva)) {
			std::snprintf(report, size, "off: 0x%llx is already hooked (jmp [rip+x] at its entry)", static_cast<unsigned long long>(rva));
			return false;
		}
	}
	if (!context->InstallInlineHook(d2r::RvaAutoMapDraw, d2r::AutoMapDrawEntry, sizeof(d2r::AutoMapDrawEntry), &Draw, &g_draw)) {
		std::snprintf(report, size, "off: hook on AutoMapPanel::Draw 0x%llx refused (entry bytes differ)",
		              static_cast<unsigned long long>(d2r::RvaAutoMapDraw));
		return false;
	}
	// Draw is in now; until g_active it only calls through, so a refused second hook changes nothing.
	if (!context->InstallInlineHook(d2r::RvaAutoMapOnMessage, d2r::AutoMapOnMessageEntry, sizeof(d2r::AutoMapOnMessageEntry),
	                                &OnMessage, &g_onMessage)) {
		std::snprintf(report, size, "off: hook on AutoMapPanel::OnMessage 0x%llx refused (entry bytes differ); Draw passes through",
		              static_cast<unsigned long long>(d2r::RvaAutoMapOnMessage));
		return false;
	}
	g_active = true;
	const config::Settings& s = config::Get();
	std::snprintf(report, size, "on: wheel zoom %.2f..%.2f step %.2f", s.zoomMin, s.zoomMax, s.zoomStep);
	return true;
}

void StatusOverlayZoom(char* out, size_t size) noexcept {
	std::snprintf(out, size, "zoom %.2f, %lld wheel events used, %lld passed on", g_zoom, g_zoomed, g_passed);
}
