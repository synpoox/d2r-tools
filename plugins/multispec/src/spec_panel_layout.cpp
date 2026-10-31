// Spec panel layout (spec_panel_layout.h).
#include "spec_panel_layout.h"

#include <cstdio>
#include <cstring>

namespace ms::panel {

auto BuildLayout(int32_t slots, char* out, size_t size) noexcept -> size_t {
	if (out == nullptr || slots < 1 || slots > MaxSlots) {
		return 0;
	}
	int n = std::snprintf(out, size, R"json({
	"type": "Panel", "name": "multispec/SpecPanel",
	"fields": {
		"anchor": "$RightPanelAnchor",
		"rect": { "x": %d, "y": %d, "width": %d, "height": %d },
		"priority": -5
	},
	"children": [
		{ "type": "ClickCatcherWidget", "name": "ClickCatcher",
		  "fields": { "rect": { "x": 0, "y": 0, "width": %d, "height": %d } } },
		{ "type": "ImageWidget", "name": "Wing",
		  "fields": { "rect": { "x": 0, "y": 0 }, "filename": "PANEL\\Modals\\Modal_No_Title_BG" } },
		{ "type": "TextBoxWidget", "name": "Heading",
		  "fields": { "rect": { "x": %d, "y": %d, "width": %d, "height": %d }, "text": "Specs",
		              "style": { "pointSize": "$MediumPanelFontSize", "fontColor": "$FontColorGold",
		                         "alignment": { "h": "center", "v": "center" } } } })json",
	                      TreeLeft - VisibleWidth, TreeTop + TabRow, WingTotalWidth, WingHeight, VisibleWidth, WingHeight, SocketX,
	                      WingBorder, VisibleWidth - SocketX, HeadingHeight);
	for (int32_t i = 0; i < slots && n > 0 && static_cast<size_t>(n) < size; ++i) {
		const int32_t y = SlotY(slots, i);
		// The gold frame over a slot marks the active spec; it starts hidden and the panel shows the right one.
		n += std::snprintf(out + n, size - static_cast<size_t>(n), R"json(,
		{ "type": "ButtonWidget", "name": "Slot%d",
		  "fields": { "rect": { "x": %d, "y": %d }, "filename": "PANEL\\DropdownButton_Large_Left",
		              "pressedFrame": 1, "disabledFrame": 2, "hoveredFrame": 3,
		              "onClickMessage": "PanelManager:OpenPanel:multispec/Swap%d" } },
		{ "type": "ImageWidget", "name": "Active%d",
		  "fields": { "rect": { "x": %d, "y": %d }, "filename": "PANEL\\DropdownButton_Large_Left", "frame": 3, "visible": false } },
		{ "type": "TextBoxWidget", "name": "Number%d",
		  "fields": { "rect": { "x": %d, "y": %d, "width": %d, "height": %d }, "text": "%d",
		              "style": { "pointSize": "$XMediumPanelFontSize", "fontColor": "$FontColorLightGold",
		                         "alignment": { "h": "center", "v": "center" } } } })json",
		                   i + 1, SocketX, y, i + 1, i + 1, SocketX, y, i + 1, SocketX, y, VisibleWidth - SocketX, SocketHeight, i + 1);
	}
	if (n > 0 && static_cast<size_t>(n) < size) {
		n += std::snprintf(out + n, size - static_cast<size_t>(n), "\n\t]\n}");
	}
	return n > 0 && static_cast<size_t>(n) < size ? static_cast<size_t>(n) : 0;
}

auto ParseSwap(const char* text) noexcept -> int32_t {
	constexpr const char* Prefix = "multispec/Swap";
	constexpr size_t PrefixLength = 14;
	if (text == nullptr || std::strncmp(text, Prefix, PrefixLength) != 0) {
		return 0;
	}
	const char* p = text + PrefixLength;
	if (*p < '1' || *p > '9' || p[1] != '\0') {
		return 0;
	}
	return *p - '0';
}

}  // namespace ms::panel
