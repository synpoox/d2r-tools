#include "map_panel.h"

#include "core/part_signatures.h"
#include "log.h"
#include "eventplan.h"
#include "mappanel.h"
#include "scan.h"

#include <windows.h>

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <mutex>

namespace carto {

namespace {

// The panel: a layout the plugin ships as a resource, two TextBoxWidgets (gold headers, blue properties) under a Panel anchored to the top right. Layout units are
// twice the 1080p pixels. The game's info text (area, difficulty, version, mod) ends 40 units from the right edge and takes about
// 260 units from the top, so the panel starts under it.
constexpr const char* PluginId = "cartographer";
constexpr const char* LocalId = "map-panel";
constexpr const char* PanelName = "cartographer/map-panel";
constexpr const char* TextName = "Text";          // the header lines, gold
constexpr const char* BlueTextName = "TextBlue";  // the property lines, blue (item property blue), over the same area
constexpr int PanelWidth = 760;
constexpr int PanelTop = cartographer::mappanel::kPanelTop;
constexpr int PanelRight = 40;
constexpr int PointSize = 30;
constexpr int LineUnits = cartographer::mappanel::kLineUnits;  // one line of the panel text, in layout units (measured live at PointSize 30)
constexpr int MaxPadLines = cartographer::mappanel::kMaxPadLines;
constexpr int PanelHeight = cartographer::mappanel::panelHeight(LineUnits, MaxPadLines);  // room for the padding and the longest panel text (1540: ends at 1810 of 2160)
constexpr size_t ScreenShiftWrite = 0xf;  // the setter's `mov [rip+x], ecx`: its operand is the global
constexpr uint32_t PadTasks = 15;   // the game's terror zone text is measured this often
// The entry banner (issue #357): the Overlay service's drawText returned Success but drew nothing (see below), so the banner is a second panel
// of two centred TextBoxWidgets (event name, event message; the map name is the game's own "Entering" text), 16% down from the top, open for the banner's time.
constexpr const char* BannerLocalId = "entry-banner";
constexpr const char* BannerPanelName = "cartographer/entry-banner";
constexpr int BannerWidth = 1600;
constexpr int BannerTop = 345;  // 16% of the 2160 layout units
constexpr int BannerLines = 2;
constexpr int BannerPoint[BannerLines] = {40, 30};
constexpr int BannerColour[BannerLines][3] = {{235, 82, 66}, {217, 217, 217}};
constexpr uint32_t RefreshTasks = 30;  // while in the map the text is set again this often, in case it was lost

struct StringView {
	const char* text;
	uint64_t size;
};
using FindPanelFn = void* (*)(const char* name) noexcept;
using FindChildFn = void* (*)(void* parent, const char* name) noexcept;
using SetTextFn = void (*)(void* textBox, const StringView* text) noexcept;

// The game's HUDWarnings update (issue #371), hooked below.
using HudUpdateFn = uint64_t (*)(void* self, void* a2, void* a3, void* a4) noexcept;

enum PartId { FindPanel, FindChild, SetText, HudUpdate, WidgetSize, WidgetMove, PartCount };
constexpr int PanelPartCount = HudUpdate;  // the parts the plugin's own panel needs; HudUpdate, WidgetSize and WidgetMove are optional (the game's terror zone field)

Part g_parts[PartCount] = {
	{"UiFindPanel", sig::UiFindPanel, 0, false, nullptr, {}, {}},
	{"UiFindChild", sig::UiFindChild, 0, false, nullptr, {}, {}},
	{"UiSetText", sig::UiSetText, 0, false, nullptr, {}, {}},
	{"HudWarningsUpdate", sig::HudWarningsUpdate, 18, false, nullptr, {}, {}},
	{"WidgetSize", sig::WidgetSize, 0, false, nullptr, {}, {}},
	{"WidgetMove", sig::WidgetMove, 0, false, nullptr, {}, {}},
};
HudUpdateFn g_hudOriginal = nullptr;
std::atomic<bool> g_hudLive{false};     // the HUDWarnings update is hooked: the map text goes into the game's terror zone field in a map, the own panel stays closed
std::atomic<bool> g_hudShown{false};    // the map text is in the game's field (written by the last update): the own panel stays closed

const D2RL::PluginContext* g_context = nullptr;
const D2RL::OverlayService* g_overlay = nullptr;
const D2RL::ThreadService* g_threads = nullptr;
const D2RL::PanelService* g_panels = nullptr;
const D2RL::ResourceService* g_resources = nullptr;
const D2RL::WidgetService* g_widgets = nullptr;  // optional: where the game's terror zone text ends, so the panel can start under it
D2RL::Panels::RegistrationHandle g_panel = D2RL::Panels::InvalidHandle;
D2RL::Resources::RegistrationHandle g_resource = D2RL::Resources::InvalidHandle;
D2RL::Panels::RegistrationHandle g_bannerPanel = D2RL::Panels::InvalidHandle;
D2RL::Resources::RegistrationHandle g_bannerResource = D2RL::Resources::InvalidHandle;
D2RL::Overlay::CallbackHandle g_callback = D2RL::Overlay::InvalidCallbackHandle;
std::atomic<bool> g_on{false};  // the parts and services are there, the panel is registered and the callback is set

// The open map. Written on the game thread, read on the UI thread.
std::mutex g_lock;
std::string g_text;       // the panel header lines; both texts empty: nothing to show
std::string g_textBlue;   // the panel property lines
std::string g_textHud;    // both in one text, for the game's terror zone field (one colour)
bool g_mapOpen = false;   // a map was opened and not left; independent of the text
int g_slotLevel = -1;
std::atomic<bool> g_inMap{false};
std::atomic<bool> g_bannerOn{false};  // the banner panel is registered: ShowEntryBanner can show
std::vector<std::string> g_bannerLines;
uint64_t g_bannerStartMs = 0;
uint64_t g_bannerSeq = 0;  // counts banners; a new one replaces the one showing
bool g_bannerActive = false;

// UI thread only.
bool g_open = false;      // the plugin opened the panel and has not closed it
bool g_bannerOpen = false;
uint64_t g_bannerShownSeq = 0;
bool g_bannerFailedLogged = false;
std::string g_shown;      // what the plugin last put in the text; empty: nothing yet
uint32_t g_tasks = 0;
D2RL::Widgets::WidgetHandle g_hud = D2RL::Widgets::InvalidHandle;      // the game's HUDWarnings panel and its terror zone text wrapper, once found
D2RL::Widgets::WidgetHandle g_terrorWrapper = D2RL::Widgets::InvalidHandle;
int g_padLines = 0;  // blank lines in front of the panel text, so it starts under the game's terror zone text
uint32_t g_padTasks = 0;
bool g_openFailedLogged = false;
bool g_notFoundLogged = false;
std::atomic<bool> g_queued{false};
std::atomic<uint32_t> g_sets{0};
std::atomic<bool> g_faulted{false};
std::atomic<bool> g_bannerFaulted{false};  // the banner's own fault, separate from the map panel's
std::atomic<bool> g_clock{false};  // the frame callback is set: it drives both the map panel and the banner
std::string g_layout;
std::string g_bannerLayout;

template <typename Fn>
auto Call(PartId id) noexcept -> Fn {
	return reinterpret_cast<Fn>(const_cast<uint8_t*>(g_parts[id].address));
}

enum class SetResult { Done, NoWidget, Fault };


auto SetRaw(const std::string& text, const char* panelName = PanelName, const char* textName = TextName) noexcept -> SetResult {
	__try {
		void* const panel = Call<FindPanelFn>(FindPanel)(panelName);
		void* const box = panel != nullptr ? Call<FindChildFn>(FindChild)(panel, textName) : nullptr;
		if (box == nullptr) {
			return SetResult::NoWidget;
		}
		const StringView view{text.c_str(), text.size()};
		Call<SetTextFn>(SetText)(box, &view);
		return SetResult::Done;
	} __except (EXCEPTION_EXECUTE_HANDLER) {
		return SetResult::Fault;
	}
}

// The game's update of the HUDWarnings panel sets the terror zone widget (this+0x180) from its zone string, hides it with no zone, and lays the panel's
// visible children out one under the other (this+0x58 begin, this+0x60 count): each child's scaled height is added to a running y, its x is minus its scaled
// width. While a map is open the hook calls the original, then sets the widget's text to the map text, shows it and replays that layout loop (WidgetSize and
// WidgetMove, the calls the loop makes), so the map text takes the zone lines' place with a zone on or off and the other warnings sit under it. Outside a map
// it passes through. The game's own strings are never written. Kept apart from the lock: __try and objects with destructors do not mix.
constexpr int HudTerrorWidgetOffset = 0x180;
constexpr int HudInfoWidgetOffset = 0x170;
constexpr int HudHostileWidgetOffset = 0x188;
constexpr int HudDurabilityWidgetOffset = 0x190;
constexpr int HudChildrenOffset = 0x58;
constexpr int HudChildCountOffset = 0x60;
constexpr int WidgetVisibleOffset = 0x51;
constexpr int WidgetScaleOffset = 0x80;
constexpr int WidgetParentOffset = 0x30;
constexpr int WidgetSetVisibleSlot = 0x50;
using WidgetSizeFn = void* (*)(void* widget, void* out16) noexcept;
using WidgetMoveFn = void (*)(void* widget, const int* xy) noexcept;

enum class HudResult { Shown, Fault, NoWidget, NotShown };

// The game's loop over the panel's children: a visible child, the hostile warning while the info text is hidden and the durability warning are placed.
// The terror zone widget sits in a wrapper, which is the panel's child (the update shows it with a zone only). Returns the wrapper's scaled height (0: it was not placed or has no size).
auto LayoutHud(void* self, void* widget, void* wrapper) noexcept -> int {
	void* const* const children = At<void* const*>(self, HudChildrenOffset);
	const uint64_t count = At<uint64_t>(self, HudChildCountOffset);
	if (children == nullptr || count > 64) {
		return 0;
	}
	void* const info = At<void*>(self, HudInfoWidgetOffset);
	void* const hostile = At<void*>(self, HudHostileWidgetOffset);
	void* const durability = At<void*>(self, HudDurabilityWidgetOffset);
	int y = 0;
	int shownHeight = 0;
	for (uint64_t i = 0; i < count; ++i) {
		void* const child = children[i];
		if (child == nullptr) {
			continue;
		}
		bool placed = At<uint8_t>(child, WidgetVisibleOffset) != 0;
		if (!placed) {
			placed = child == hostile ? (info != nullptr && At<uint8_t>(info, WidgetVisibleOffset) == 0) : child == durability;
		}
		if (!placed) {
			continue;
		}
		const float scale = At<float>(child, WidgetScaleOffset);
		int rect[4] = {};
		// The game's layout leaves the wrapper's own size at 0 with no zone; the text in it is what fills it.
		Call<WidgetSizeFn>(WidgetSize)(child == wrapper ? widget : child, rect);
		const int width = static_cast<int>(static_cast<float>(rect[2]) * scale);
		const int height = static_cast<int>(static_cast<float>(rect[3]) * scale);
		const int at[2] = {-width, y};
		Call<WidgetMoveFn>(WidgetMove)(child, at);
		y += height;
		if (child == wrapper) {
			shownHeight = height;
		}
	}
	return shownHeight;
}

auto ShowHudText(void* self, const char* text, size_t size) noexcept -> HudResult {
	__try {
		void* const widget = At<void*>(self, HudTerrorWidgetOffset);
		if (widget == nullptr) {
			return HudResult::NoWidget;
		}
		const StringView view{text, size};
		Call<SetTextFn>(SetText)(widget, &view);
		void** const vtable = *static_cast<void***>(widget);
		reinterpret_cast<void (*)(void*, bool)>(vtable[WidgetSetVisibleSlot / sizeof(void*)])(widget, true);
		void* const wrapper = At<void*>(widget, WidgetParentOffset);
		if (wrapper == nullptr) {
			return HudResult::NoWidget;
		}
		reinterpret_cast<void (*)(void*, bool)>((*static_cast<void***>(wrapper))[WidgetSetVisibleSlot / sizeof(void*)])(wrapper, true);
		const int height = LayoutHud(self, widget, wrapper);
		return height > 0 && At<uint8_t>(widget, WidgetVisibleOffset) != 0 ? HudResult::Shown : HudResult::NotShown;
	} __except (EXCEPTION_EXECUTE_HANDLER) {
		return HudResult::Fault;
	}
}

auto HudUpdateHook(void* self, void* a2, void* a3, void* a4) noexcept -> uint64_t {
	if (!g_hudLive.load(std::memory_order_relaxed) || !g_inMap.load(std::memory_order_relaxed) || self == nullptr) {
		g_hudShown = false;
		return g_hudOriginal(self, a2, a3, a4);
	}
	std::string text;
	try {
		std::lock_guard<std::mutex> lock(g_lock);
		text = g_textHud;
	} catch (...) {
	}
	const uint64_t result = g_hudOriginal(self, a2, a3, a4);
	if (text.empty()) {
		g_hudShown = false;
		return result;
	}
	const HudResult shown = ShowHudText(self, text.c_str(), text.size());
	// The plugin's own panel stays closed only while the game's field really shows the text.
	if ((shown == HudResult::Shown) != g_hudShown.exchange(shown == HudResult::Shown)) {
		plog::Info("map panel: %s", shown == HudResult::Shown ? "map text shown in the game's terror zone text" : "the game's terror zone text does not show the map text here; the plugin's own panel is used");
	}
	if (shown == HudResult::Fault) {
		g_hudLive = false;  // the game's text shows again; the plugin's own panel takes over
		plog::Error("map panel: memory fault while writing the terror zone text; back to the plugin's own panel");
	}
	return result;
}

// Something other than this plugin may close the panel (a game left): the next task then opens it again.
void NoticeClosed() noexcept {
	if (!g_open) {
		return;
	}
	D2RL::Panels::PanelInfo info{};
	info.structSize = D2RL::Panels::PanelInfoSize;
	if (g_panels->getPanelInfo(g_context, g_panel, &info) == D2RL::Panels::Result::Success && info.presentationState != D2RL::Panels::PresentationState::Open) {
		g_open = false;
		g_shown.clear();
	}
}

// The game's purple terror zone text (HUDWarnings > TerrorZoneInfoTextWrapper > TerrorZoneInfoText) sits under its area and game info, where
// the panel starts. The plugin could neither hide that text nor write the panel's lines into it from its frame callback (both returned Success and
// changed nothing on screen; the likely cause is that the game sets the text and its visibility itself after the callback, not proven, see
// docs/reverse-engineering/cartographer/22-map-panel.md). Instead the panel starts under it: the wrapper's
// rect is 0 high with no zone and as high as the text (in layout units) in a zone, and the panel text gets blank lines in front to reach its
// bottom. The rect is looked up every PadTasks tasks, a zone can start while the player is in the map. UI thread.
bool g_lookupLogged = false;
auto LookupFailed(const char* what) noexcept -> int {
	if (!g_lookupLogged) {
		g_lookupLogged = true;
		plog::Info("map panel: %s not found; the panel is not moved below the game's terror zone text", what);
	}
	return 0;
}

// The rect math assumes the wrapper is a direct child of HUDWarnings (as in hudwarningshd.json); deeper, the parents' offsets in between are missing.
auto TerrorTextBottom() noexcept -> int {
	if (g_widgets == nullptr) {
		return 0;
	}
	if (g_hud == D2RL::Widgets::InvalidHandle && g_widgets->findPanel(g_context, "HUDWarnings", &g_hud) != D2RL::Widgets::Result::Success) {
		g_hud = D2RL::Widgets::InvalidHandle;
		return LookupFailed("the HUDWarnings panel");
	}
	if (g_terrorWrapper == D2RL::Widgets::InvalidHandle &&
	    g_widgets->findWidget(g_context, g_hud, "TerrorZoneInfoTextWrapper", &g_terrorWrapper) != D2RL::Widgets::Result::Success) {
		g_terrorWrapper = D2RL::Widgets::InvalidHandle;
		g_hud = D2RL::Widgets::InvalidHandle;
		return LookupFailed("TerrorZoneInfoTextWrapper");
	}
	D2RL::Widgets::Rect hud{};
	D2RL::Widgets::Rect wrapper{};
	if (g_widgets->getWidgetRect(g_context, g_hud, &hud) != D2RL::Widgets::Result::Success ||
	    g_widgets->getWidgetRect(g_context, g_terrorWrapper, &wrapper) != D2RL::Widgets::Result::Success) {
		g_hud = D2RL::Widgets::InvalidHandle;  // maybe stale (a new game): find them again next time
		g_terrorWrapper = D2RL::Widgets::InvalidHandle;
		return LookupFailed("the rect of the HUDWarnings text");
	}
	return wrapper.height > 0 ? hud.y + wrapper.y + wrapper.height : 0;
}

// A side panel of the game (the inventory with the equipment slots, the skill tree) covers the right side where the panel text is (issue #431), so
// the panel is closed while one is open. The game keeps "which sides are shifted for an open panel" in one global (0 none, 1 right panel, 2 left
// panel, 3 both; setter 0xb9c20, docs/reverse-engineering/better-automap.md), found here from the setter's signature. Without it the panel stays
// up. UI thread.
const int32_t* g_screenShift = nullptr;
auto SidePanelOpen() noexcept -> bool {
	if (g_screenShift == nullptr) {
		return false;
	}
	__try {
		const int32_t shift = *g_screenShift;
		static int32_t last = -1;
		if (shift != last) {
			last = shift;
			plog::Info("map panel: the game's side panel state is %d (%s)", shift, (shift & 1) != 0 ? "a right panel is open: the map panel is hidden" : "the map panel can show");
		}
		return (shift & 1) != 0;
	} __except (EXCEPTION_EXECUTE_HANDLER) {
		g_screenShift = nullptr;
		return false;
	}
}

void ClosePanel() noexcept {
	if (g_open) {
		(void)g_panels->closePanel(g_context, g_panel);
		g_open = false;
		g_shown.clear();
	}
}

void Apply(const std::string& text, const std::string& blue) noexcept {
	NoticeClosed();
	if ((text.empty() && blue.empty()) || SidePanelOpen()) {
		ClosePanel();
		return;
	}
	if (!g_open) {
		const auto result = g_panels->openPanel(g_context, g_panel);
		if (result != D2RL::Panels::Result::Success) {
			if (!g_openFailedLogged) {
				g_openFailedLogged = true;
				plog::Warn("map panel: the panel did not open (result %u); trying again", static_cast<uint32_t>(result));
			}
			return;
		}
		g_open = true;
		g_shown.clear();
		g_padTasks = 0;
	}
	if (g_padTasks++ % PadTasks == 0) {
		const int bottom = TerrorTextBottom();
		const int lines = cartographer::mappanel::padLines(bottom, PanelTop, LineUnits, MaxPadLines);
		if (lines != g_padLines) {
			g_padLines = lines;
			plog::Info("map panel: the game's terror zone text ends at %d, %d blank line(s) in front of the panel text", bottom, lines);
		}
	}
	const std::string pad(static_cast<size_t>(g_padLines), '\n');
	const std::string want = pad + text;
	const std::string wantBlue = pad + blue;
	const std::string both = want + '\x01' + wantBlue;
	if (both == g_shown && ++g_tasks % RefreshTasks != 0) {
		return;
	}
	SetResult set = SetRaw(want);
	if (set == SetResult::Done) {
		set = SetRaw(wantBlue, PanelName, BlueTextName);
	}
	switch (set) {
	case SetResult::Done:
		g_shown = both;
		g_notFoundLogged = false;
		if (g_sets.fetch_add(1) < 3) {
			plog::Info("map panel: text set (%zu character(s))", want.size());
		}
		break;
	case SetResult::NoWidget:
		g_shown.clear();
		if (!g_notFoundLogged) {
			g_notFoundLogged = true;
			plog::Info("map panel: the panel's text is not there yet; trying again");
		}
		break;
	case SetResult::Fault:
		g_faulted = true;
		g_on = false;
		ClosePanel();
		plog::Error("map panel: memory fault while setting the text; the panel is now off");
		break;
	}
}

auto BannerPending() noexcept -> bool {
	std::lock_guard<std::mutex> lock(g_lock);
	return g_bannerActive;
}

// The entry banner, UI thread: open the panel for a new banner, set its lines, close it when its time is up.
void ApplyBanner() noexcept {
	if (!g_bannerOn.load()) {
		return;
	}
	std::vector<std::string> lines;
	uint64_t seq = 0;
	uint64_t start = 0;
	bool active = false;
	{
		std::lock_guard<std::mutex> lock(g_lock);
		lines = g_bannerLines;
		seq = g_bannerSeq;
		start = g_bannerStartMs;
		active = g_bannerActive;
	}
	if (g_bannerOpen) {  // something other than the plugin may close it (a game left): the next pass opens it again
		D2RL::Panels::PanelInfo info{};
		info.structSize = D2RL::Panels::PanelInfoSize;
		if (g_panels->getPanelInfo(g_context, g_bannerPanel, &info) == D2RL::Panels::Result::Success && info.presentationState != D2RL::Panels::PresentationState::Open) {
			g_bannerOpen = false;
		}
	}
	const bool visible = active && cartographer::eventplan::bannerVisible(static_cast<int64_t>(GetTickCount64() - start));
	if (!visible) {
		if (g_bannerOpen) {
			(void)g_panels->closePanel(g_context, g_bannerPanel);
			g_bannerOpen = false;
			plog::Info("entry banner: closed");
		}
		if (active) {
			std::lock_guard<std::mutex> lock(g_lock);
			if (g_bannerSeq == seq) {
				g_bannerActive = false;
			}
		}
		return;
	}
	if (!g_bannerOpen) {
		const auto result = g_panels->openPanel(g_context, g_bannerPanel);
		if (result != D2RL::Panels::Result::Success) {
			if (!g_bannerFailedLogged) {
				g_bannerFailedLogged = true;
				plog::Warn("entry banner: the panel did not open (result %u); trying again", static_cast<uint32_t>(result));
			}
			return;
		}
		g_bannerOpen = true;
		g_bannerShownSeq = 0;
		plog::Info("entry banner: panel opened");
	}
	if (g_bannerShownSeq == seq) {
		return;
	}
	bool done = true;
	for (int i = 0; i < BannerLines; ++i) {
		char name[16];
		std::snprintf(name, sizeof(name), "Line%d", i + 1);
		const std::string text = i < static_cast<int>(lines.size()) ? lines[static_cast<size_t>(i)] : std::string();
		const SetResult result = SetRaw(text, BannerPanelName, name);
		if (result == SetResult::Fault) {
			g_bannerFaulted = true;
			g_bannerOn = false;
			(void)g_panels->closePanel(g_context, g_bannerPanel);
			g_bannerOpen = false;
			plog::Error("entry banner: memory fault while setting the text; the banner is now off");
			return;
		}
		done = done && result == SetResult::Done;
	}
	if (done) {
		g_bannerShownSeq = seq;
		plog::Info("entry banner: text set (%zu line(s))", lines.size());
	}
}

void __cdecl UiTask(const D2RL::PluginContext*, void*) noexcept {
	g_queued = false;
	if (!g_clock.load()) {
		return;
	}
	try {
		if (g_on.load()) {
			std::string want;
			std::string wantBlue;
			if (g_inMap.load() && !g_hudShown.load()) {  // with the game's own field in use the plugin's panel stays closed
				std::lock_guard<std::mutex> lock(g_lock);
				want = g_text;
				wantBlue = g_textBlue;
			}
			Apply(want, wantBlue);
		}
		ApplyBanner();
	} catch (...) {
		plog::Error("map panel: the panel update failed");
	}
}

void __cdecl OnFrame(const D2RL::PluginContext* context, const D2RL::Overlay::Frame*, void*) noexcept {
	if (!g_clock.load()) {
		return;
	}
	// Idle: no map and no panel of ours open.
	if (!(g_on.load() && ((g_inMap.load() && !g_hudShown.load()) || g_open)) && !(g_bannerOn.load() && BannerPending())) {
		return;
	}
	if (!g_queued.exchange(true) && (g_threads == nullptr || g_threads->runOnUiThread(context, UiTask, nullptr) != D2RL::Threads::Result::Success)) {
		g_queued = false;
	}
}

void ReleaseResource() noexcept {
	if (g_resources != nullptr && g_context != nullptr && g_bannerResource != D2RL::Resources::InvalidHandle) {
		(void)g_resources->unregisterResource(g_context, g_bannerResource);
	}
	g_bannerResource = D2RL::Resources::InvalidHandle;
	if (g_resources != nullptr && g_context != nullptr && g_resource != D2RL::Resources::InvalidHandle) {
		(void)g_resources->unregisterResource(g_context, g_resource);
	}
	g_resource = D2RL::Resources::InvalidHandle;
}

auto BuildLayout() -> std::string {
	char buf[1600];
	std::snprintf(buf, sizeof(buf),
	              "{\n\"type\": \"Panel\", \"name\": \"%s\",\n\"fields\": { \"rect\": { \"x\": %d, \"y\": %d, \"width\": %d, \"height\": %d }, "
	              "\"anchor\": { \"x\": 1, \"y\": 0 } },\n\"children\": [\n"
	              "{ \"type\": \"TextBoxWidget\", \"name\": \"%s\", \"fields\": { \"text\": \"\", \"rect\": { \"x\": 0, \"y\": 0, \"width\": %d, "
	              "\"height\": %d }, \"style\": { \"fontColor\": { \"r\": 220, \"g\": 195, \"b\": 118, \"a\": 255 }, \"pointSize\": %d, "
	              "\"alignment\": { \"h\": \"right\", \"v\": \"top\" }, \"options\": { \"newlineHandling\": \"standard\" } } } },\n"
	              "{ \"type\": \"TextBoxWidget\", \"name\": \"%s\", \"fields\": { \"text\": \"\", \"rect\": { \"x\": 0, \"y\": 0, \"width\": %d, "
	              "\"height\": %d }, \"style\": { \"fontColor\": { \"r\": 105, \"g\": 105, \"b\": 255, \"a\": 255 }, \"pointSize\": %d, "
	              "\"alignment\": { \"h\": \"right\", \"v\": \"top\" }, \"options\": { \"newlineHandling\": \"standard\" } } } }\n]\n}\n",
	              PanelName, -(PanelWidth + PanelRight), PanelTop, PanelWidth, PanelHeight, TextName, PanelWidth, PanelHeight, PointSize,
	              BlueTextName, PanelWidth, PanelHeight, PointSize);
	return buf;
}

auto BuildBannerLayout() -> std::string {
	char buf[900];
	std::snprintf(buf, sizeof(buf),
	              "{\n\"type\": \"Panel\", \"name\": \"%s\",\n\"fields\": { \"rect\": { \"x\": %d, \"y\": %d, \"width\": %d, \"height\": 240 }, "
	              "\"anchor\": { \"x\": 0.5, \"y\": 0 } },\n\"children\": [\n",
	              BannerPanelName, -BannerWidth / 2, BannerTop, BannerWidth);
	std::string out = buf;
	int y = 0;
	for (int i = 0; i < BannerLines; ++i) {
		const int height = BannerPoint[i] + 24;
		std::snprintf(buf, sizeof(buf),
		              "{ \"type\": \"TextBoxWidget\", \"name\": \"Line%d\", \"fields\": { \"text\": \"\", \"rect\": { \"x\": 0, \"y\": %d, \"width\": %d, "
		              "\"height\": %d }, \"style\": { \"fontColor\": { \"r\": %d, \"g\": %d, \"b\": %d, \"a\": 255 }, \"pointSize\": %d, "
		              "\"alignment\": { \"h\": \"center\", \"v\": \"top\" }, \"options\": { \"newlineHandling\": \"standard\" } } } }%s\n",
		              i + 1, y, BannerWidth, height, BannerColour[i][0], BannerColour[i][1], BannerColour[i][2], BannerPoint[i], i + 1 < BannerLines ? "," : "");
		out += buf;
		y += height;
	}
	out += "]\n}\n";
	return out;
}

}  // namespace

// The HUDWarnings update hook (issue #371), after the parts are resolved. Fails open: a part not found or a guard that does not match leaves the
// plugin's own panel (below the game's terror zone text) as it was. Needs UiSetText, WidgetSize and WidgetMove.
void InstallHudHook(const D2RL::PluginContext* context) noexcept {
	Part& part = g_parts[HudUpdate];
	if (part.address == nullptr || !part.status.on || !g_parts[SetText].status.on || !g_parts[WidgetSize].status.on || !g_parts[WidgetMove].status.on) {
		plog::Warn("map panel: the game's terror zone text is not used (%s; the update hook, UiSetText, WidgetSize and WidgetMove are all needed); the panel sits below it", part.status.detail.c_str());
		return;
	}
	const uint64_t rva = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(part.address) - context->exeBase);
	const bool hooked = context->InstallInlineHook(rva, part.guard, part.hookSize, reinterpret_cast<void*>(&HudUpdateHook), reinterpret_cast<void**>(&g_hudOriginal));
	g_hudLive = HookOutcome(part, hooked, g_hudOriginal != nullptr);
	plog::Info("map panel: the game's terror zone text %s", g_hudLive.load() ? "shows the map text in a map" : "is not used; the panel sits below it");
}

auto InstallMapPanel(const D2RL::PluginContext* context, const std::vector<SignatureOverride>& overrides, std::vector<PartStatus>* parts) -> bool {
	g_context = context;
	CodeRange ranges[8];
	const int count = CodeRanges(context->exeBase, ranges, 8);
	bool allOn = true;
	for (Part& part : g_parts) {
		ResolvePart(context, overrides, ranges, count, part);
		plog::Info("part %s: %s", part.name, part.status.detail.c_str());
		if (parts != nullptr) {
			parts->push_back(part.status);
		}
		if (&part - g_parts < PanelPartCount) {
			allOn = allOn && part.status.on;
		}
	}
	InstallHudHook(context);
	// Optional: only for hiding the panel while a side panel is open.
	Part shiftPart{"ScreenShift", sig::ScreenShiftSetter, 0, false, nullptr, {}, {}};
	ResolvePart(context, overrides, ranges, count, shiftPart);
	plog::Info("part %s: %s", shiftPart.name, shiftPart.status.detail.c_str());
	if (parts != nullptr) {
		parts->push_back(shiftPart.status);
	}
	if (shiftPart.status.on && shiftPart.address != nullptr) {
		int32_t rel = 0;
		std::memcpy(&rel, shiftPart.address + ScreenShiftWrite + 2, sizeof(rel));
		g_screenShift = reinterpret_cast<const int32_t*>(shiftPart.address + ScreenShiftWrite + 6 + rel);
	} else {
		plog::Info("map panel: the screen shift part was not found; the panel stays up while a side panel is open");
	}
	if (!allOn) {
		plog::Warn("map panel: off (a game part was not found)");
		return false;
	}
	const D2RL::ResourceService* resources = nullptr;
	if (context->QueryService(&g_widgets) != D2RL::ServiceQueryResult::Success || !D2RL::HasWidgetServiceField(g_widgets, D2RL::WidgetServiceRequiredSize)) {
		g_widgets = nullptr;
		plog::Warn("map panel: no Widget service of the expected version; the panel may overlap the game's terror zone text");
	}
	if (context->QueryService(&g_overlay) != D2RL::ServiceQueryResult::Success || !D2RL::HasOverlayServiceField(g_overlay, D2RL::OverlayServiceRequiredSize) ||
	    context->QueryService(&g_threads) != D2RL::ServiceQueryResult::Success || !D2RL::HasThreadServiceField(g_threads, D2RL::ThreadServiceRequiredSize) ||
	    context->QueryService(&g_panels) != D2RL::ServiceQueryResult::Success || !D2RL::HasPanelServiceField(g_panels, D2RL::PanelServiceRequiredSize) ||
	    context->QueryService(&resources) != D2RL::ServiceQueryResult::Success || resources == nullptr || resources->serviceSize < D2RL::ResourceServiceRequiredSize) {
		g_overlay = nullptr;
		g_threads = nullptr;
		g_panels = nullptr;
		plog::Warn("map panel: off (no Overlay, Thread, Panel or Resource service of the expected version)");
		return false;
	}
	g_resources = resources;
	g_layout = BuildLayout();
	char path[96];
	std::snprintf(path, sizeof(path), "data/global/ui/layouts/%s/%shd.json", PluginId, LocalId);
	D2RL::Resources::ResourceRegistration resource{};
	resource.structSize = D2RL::Resources::ResourceRegistrationSize;
	resource.path = path;
	resource.bytes = g_layout.data();
	resource.byteCount = g_layout.size();
	const auto resourceResult = resources->registerResource(context, &resource, &g_resource);
	D2RL::Panels::PanelRegistration registration{};
	registration.structSize = D2RL::Panels::PanelRegistrationSize;
	registration.flags = D2RL::Panels::PanelFlags::None;
	registration.localId = LocalId;
	const auto panelResult = g_panels->registerPanel(context, &registration, &g_panel);
	if (resourceResult != D2RL::Resources::Result::Success || panelResult != D2RL::Panels::Result::Success) {
		if (g_panel != D2RL::Panels::InvalidHandle) {
			(void)g_panels->unregisterPanel(context, g_panel);
		}
		ReleaseResource();
		g_panel = D2RL::Panels::InvalidHandle;
		g_panels = nullptr;
		plog::Warn("map panel: off (the panel was refused: resource %u, panel %u)", static_cast<uint32_t>(resourceResult), static_cast<uint32_t>(panelResult));
		return false;
	}
	D2RL::Overlay::CallbackRegistration callback{};
	callback.structSize = D2RL::Overlay::CallbackRegistrationSize;
	callback.phase = D2RL::Overlay::Phase::AfterGameUi;
	callback.callback = OnFrame;
	const auto result = g_overlay->registerFrameCallback(context, &callback, &g_callback);
	if (result != D2RL::Overlay::Result::Success) {
		g_callback = D2RL::Overlay::InvalidCallbackHandle;
		plog::Warn("map panel: off (the overlay refused the frame callback, result %u)", static_cast<uint32_t>(result));
		return false;
	}
	g_bannerLayout = BuildBannerLayout();
	char bannerPath[96];
	std::snprintf(bannerPath, sizeof(bannerPath), "data/global/ui/layouts/%s/%shd.json", PluginId, BannerLocalId);
	D2RL::Resources::ResourceRegistration bannerResource{};
	bannerResource.structSize = D2RL::Resources::ResourceRegistrationSize;
	bannerResource.path = bannerPath;
	bannerResource.bytes = g_bannerLayout.data();
	bannerResource.byteCount = g_bannerLayout.size();
	const auto bannerResourceResult = resources->registerResource(context, &bannerResource, &g_bannerResource);
	D2RL::Panels::PanelRegistration bannerRegistration{};
	bannerRegistration.structSize = D2RL::Panels::PanelRegistrationSize;
	bannerRegistration.flags = D2RL::Panels::PanelFlags::None;
	bannerRegistration.localId = BannerLocalId;
	const auto bannerPanelResult = g_panels->registerPanel(context, &bannerRegistration, &g_bannerPanel);
	if (bannerResourceResult == D2RL::Resources::Result::Success && bannerPanelResult == D2RL::Panels::Result::Success) {
		g_bannerOn = true;
		plog::Info("entry banner: panel registered");
	} else {
		plog::Warn("entry banner: panel refused (resource %u, panel %u); the entry message goes to the console", static_cast<uint32_t>(bannerResourceResult),
		           static_cast<uint32_t>(bannerPanelResult));
	}
	g_on = true;
	g_clock = true;
	return true;
}

auto EntryBannerAvailable() noexcept -> bool {
	return g_clock.load() && g_bannerOn.load();
}

auto ShowEntryBanner(const std::vector<std::string>& lines) noexcept -> bool {
	if (!g_clock.load() || !g_bannerOn.load() || lines.empty()) {
		return false;
	}
	try {
		std::lock_guard<std::mutex> lock(g_lock);
		g_bannerLines = lines;
		g_bannerStartMs = GetTickCount64();
		g_bannerActive = true;
		++g_bannerSeq;
	} catch (...) {
		return false;
	}
	return true;
}

void UnloadMapPanel() noexcept {
	g_hudLive = false;  // the hook stays in the game but writes nothing
	g_on = false;
	g_clock = false;
	g_bannerOn = false;
	if (g_panels != nullptr && g_context != nullptr && g_bannerPanel != D2RL::Panels::InvalidHandle) {
		(void)g_panels->closePanel(g_context, g_bannerPanel);
		(void)g_panels->unregisterPanel(g_context, g_bannerPanel);
		g_bannerPanel = D2RL::Panels::InvalidHandle;
	}
	if (g_overlay != nullptr && g_context != nullptr && g_callback != D2RL::Overlay::InvalidCallbackHandle) {
		(void)g_overlay->unregisterFrameCallback(g_context, g_callback);
		g_callback = D2RL::Overlay::InvalidCallbackHandle;
	}
	if (g_panels != nullptr && g_context != nullptr && g_panel != D2RL::Panels::InvalidHandle) {
		(void)g_panels->closePanel(g_context, g_panel);
		(void)g_panels->unregisterPanel(g_context, g_panel);
		g_panel = D2RL::Panels::InvalidHandle;
	}
	ReleaseResource();
}

void MapPanelNoMap() {
	std::lock_guard<std::mutex> lock(g_lock);
	g_mapOpen = false;
	g_text.clear();
	g_textBlue.clear();
	g_textHud.clear();
	g_slotLevel = -1;
	g_inMap = false;
}

void MapPanelMapOpened(const std::string& gold, const std::string& blue, int slotLevel) {
	std::lock_guard<std::mutex> lock(g_lock);
	g_mapOpen = true;
	g_text = gold;
	g_textBlue = blue;
	g_textHud = cartographer::mappanel::mergeLines(gold, blue);
	g_slotLevel = slotLevel;
	g_inMap = false;  // the player's tick says when it stands in the map
}

void MapPanelPlayerTick(void* player) noexcept {
	if (!g_on.load(std::memory_order_relaxed) && !g_hudLive.load(std::memory_order_relaxed)) {
		return;
	}
	std::lock_guard<std::mutex> lock(g_lock);
	if (!g_mapOpen) {
		return;
	}
	g_inMap = g_slotLevel > 0 && PlayerLevelOf(player) == g_slotLevel;
}

void MapPanelGameLeft() noexcept {
	std::lock_guard<std::mutex> lock(g_lock);
	g_mapOpen = false;
	g_text.clear();
	g_textBlue.clear();
	g_textHud.clear();
	g_slotLevel = -1;
	g_inMap = false;
	g_bannerStartMs = 0;  // a banner still open is out of time: the next pass closes it (bannerVisible of the whole tick count is false)
}

auto MapPanelPartStatuses() -> std::vector<PartStatus> {
	std::vector<PartStatus> out;
	for (const Part& part : g_parts) {
		out.push_back(part.status);
	}
	return out;
}

auto MapPanelSummary() -> std::string {
	if (g_bannerFaulted.load()) {
		return std::string("Entry banner: off (a memory fault; see the plugin log). ") + (g_on.load() ? "Map panel: on" : "Map panel: off");
	}
	if (g_faulted.load() && !g_hudLive.load()) {
		return "Map panel: off (a memory fault; see the plugin log)";
	}
	if (g_hudLive.load()) {
		return "Map text: on, in the game's terror zone text field inside a map";
	}
	if (!g_on.load()) {
		return "Map panel: off (a game part or D2RLoader service was not found)";
	}
	return "Map panel: on (the host shows it, a TCP/IP client does not know the properties)";
}

}  // namespace carto
