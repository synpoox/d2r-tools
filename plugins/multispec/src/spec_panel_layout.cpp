// Spec panel layout (spec_panel_layout.h).
#include "spec_panel_layout.h"

#include <cstdio>
#include <cstring>

namespace ms::panel {

auto BuildLayout(int32_t specCount, char* out, size_t size) noexcept -> size_t {
	if (out == nullptr || specCount < 1) {
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
		  "fields": { "rect": { "x": %d, "y": %d, "width": %d, "height": %d }, "text": "Spec",
		              "style": { "fontColor": "$PanelTitleColor", "pointSize": "$MediumLargeFontSize", "dropShadow": "$DefaultDropShadow",
		                         "alignment": { "h": "center", "v": "center" }, "spacing": "$ReducedSpacing" } } },
		{ "type": "ImageWidget", "name": "Slot",
		  "fields": { "rect": { "x": %d, "y": %d }, "filename": "PANEL\\HUD_02\\belt_popped_row" } },
		{ "type": "ButtonWidget", "name": "Up",
		  "fields": { "rect": { "x": %d, "y": %d }, "filename": "FrontEnd\\HD\\Final\\FrontEnd_UpButton",
		              "pressedFrame": 1, "disabledFrame": 2, "hoveredFrame": 3,
		              "onClickMessage": "PanelManager:OpenPanel:multispec/SpecUp" } },
		{ "type": "ButtonWidget", "name": "Down",
		  "fields": { "rect": { "x": %d, "y": %d }, "filename": "FrontEnd\\HD\\Final\\FrontEnd_DownButton",
		              "pressedFrame": 1, "disabledFrame": 2, "hoveredFrame": 3,
		              "onClickMessage": "PanelManager:OpenPanel:multispec/SpecDown" } },
		{ "type": "ImageWidget", "name": "UpOff",
		  "fields": { "rect": { "x": %d, "y": %d }, "filename": "FrontEnd\\HD\\Final\\FrontEnd_UpButton", "frame": 2, "visible": false } },
		{ "type": "ImageWidget", "name": "DownOff",
		  "fields": { "rect": { "x": %d, "y": %d }, "filename": "FrontEnd\\HD\\Final\\FrontEnd_DownButton", "frame": 2, "visible": false } })json",
	                      TreeLeft - VisibleWidth, TreeTop + TabRow, WingTotalWidth, WingHeight, VisibleWidth, WingHeight, SlotX,
	                      HeadingTop, VisibleWidth - SlotX, HeadingHeight, SlotX, SlotY, ArrowX, UpY, ArrowX, DownY, ArrowX, UpY, ArrowX, DownY);
	for (int32_t i = 1; i <= specCount && n > 0 && static_cast<size_t>(n) < size; ++i) {
		n += std::snprintf(out + n, size - static_cast<size_t>(n), R"json(,
		{ "type": "TextBoxWidget", "name": "Number%d",
		  "fields": { "rect": { "x": %d, "y": %d, "width": %d, "height": %d }, "text": "%d", "visible": false,
		              "style": { "pointSize": "$XMediumPanelFontSize", "fontColor": "$FontColorGold",
		                         "alignment": { "h": "center", "v": "center" } } } })json",
		                   i, SlotX, SlotY, SlotWidth, SlotHeight, i);
	}
	if (n > 0 && static_cast<size_t>(n) < size) {
		n += std::snprintf(out + n, size - static_cast<size_t>(n), "\n\t]\n}");
	}
	return n > 0 && static_cast<size_t>(n) < size ? static_cast<size_t>(n) : 0;
}

auto ParseStep(const char* text) noexcept -> int32_t {
	if (text == nullptr) {
		return 0;
	}
	if (std::strcmp(text, "multispec/SpecUp") == 0) {
		return 1;
	}
	return std::strcmp(text, "multispec/SpecDown") == 0 ? -1 : 0;
}

auto NextSpec(int32_t active, int32_t specCount, int32_t step) noexcept -> int32_t {
	if (specCount < 2 || active < 1 || active > specCount || (step != 1 && step != -1)) {
		return 0;
	}
	const int32_t next = active + step;
	return next < 1 ? specCount : (next > specCount ? 1 : next);
}

}  // namespace ms::panel
