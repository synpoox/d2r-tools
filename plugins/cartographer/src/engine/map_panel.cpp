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

// The panel: a layout the plugin ships as a resource, one TextBoxWidget under a Panel anchored to the top right. Layout units are
// twice the 1080p pixels. The game's info text (area, difficulty, version, mod) ends 40 units from the right edge and takes about
// 260 units from the top, so the panel starts under it.
constexpr const char* PluginId = "cartographer";
constexpr const char* LocalId = "map-panel";
constexpr const char* PanelName = "cartographer/map-panel";
constexpr const char* TextName = "Text";
constexpr int PanelWidth = 760;
constexpr int PanelHeight = 700;
constexpr int PanelTop = 270;
constexpr int PanelRight = 40;
constexpr int PointSize = 30;
constexpr int LineUnits = 44;       // one line of the panel text, in layout units (measured live at PointSize 30)
constexpr int MaxPadLines = 8;
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

enum PartId { FindPanel, FindChild, SetText, PartCount };

Part g_parts[PartCount] = {
	{"UiFindPanel", sig::UiFindPanel, 0, false, nullptr, {}, {}},
	{"UiFindChild", sig::UiFindChild, 0, false, nullptr, {}, {}},
	{"UiSetText", sig::UiSetText, 0, false, nullptr, {}, {}},
};

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
std::string g_text;       // the panel text; empty: no map is open
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
// the panel starts. The game sets that text, and whether it shows, itself every frame, so the plugin can neither hide it nor write the panel's
// lines into it (both tried live, see docs/reverse-engineering/cartographer/22-map-panel.md). Instead the panel starts under it: the wrapper's
// rect is 0 high with no zone and as high as the text (in layout units) in a zone, and the panel text gets blank lines in front to reach its
// bottom. The rect is looked up every PadTasks tasks, a zone can start while the player is in the map. UI thread.
auto TerrorTextBottom() noexcept -> int {
	if (g_widgets == nullptr) {
		return 0;
	}
	if (g_hud == D2RL::Widgets::InvalidHandle && g_widgets->findPanel(g_context, "HUDWarnings", &g_hud) != D2RL::Widgets::Result::Success) {
		g_hud = D2RL::Widgets::InvalidHandle;
		return 0;
	}
	if (g_terrorWrapper == D2RL::Widgets::InvalidHandle &&
	    g_widgets->findWidget(g_context, g_hud, "TerrorZoneInfoTextWrapper", &g_terrorWrapper) != D2RL::Widgets::Result::Success) {
		g_terrorWrapper = D2RL::Widgets::InvalidHandle;
		g_hud = D2RL::Widgets::InvalidHandle;
		return 0;
	}
	D2RL::Widgets::Rect hud{};
	D2RL::Widgets::Rect wrapper{};
	if (g_widgets->getWidgetRect(g_context, g_hud, &hud) != D2RL::Widgets::Result::Success ||
	    g_widgets->getWidgetRect(g_context, g_terrorWrapper, &wrapper) != D2RL::Widgets::Result::Success) {
		g_hud = D2RL::Widgets::InvalidHandle;  // maybe stale (a new game): find them again next time
		g_terrorWrapper = D2RL::Widgets::InvalidHandle;
		return 0;
	}
	return wrapper.height > 0 ? hud.y + wrapper.y + wrapper.height : 0;
}

void ClosePanel() noexcept {
	if (g_open) {
		(void)g_panels->closePanel(g_context, g_panel);
		g_open = false;
		g_shown.clear();
	}
}

void Apply(const std::string& text) noexcept {
	NoticeClosed();
	if (text.empty()) {
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
	const std::string want = std::string(static_cast<size_t>(g_padLines), '\n') + text;
	if (want == g_shown && ++g_tasks % RefreshTasks != 0) {
		return;
	}
	switch (SetRaw(want)) {
	case SetResult::Done:
		g_shown = want;
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
			if (g_inMap.load()) {
				std::lock_guard<std::mutex> lock(g_lock);
				want = g_text;
			}
			Apply(want);
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
	if (!(g_on.load() && (g_inMap.load() || g_open)) && !(g_bannerOn.load() && BannerPending())) {
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
	char buf[900];
	std::snprintf(buf, sizeof(buf),
	              "{\n\"type\": \"Panel\", \"name\": \"%s\",\n\"fields\": { \"rect\": { \"x\": %d, \"y\": %d, \"width\": %d, \"height\": %d }, "
	              "\"anchor\": { \"x\": 1, \"y\": 0 } },\n\"children\": [\n"
	              "{ \"type\": \"TextBoxWidget\", \"name\": \"%s\", \"fields\": { \"text\": \"\", \"rect\": { \"x\": 0, \"y\": 0, \"width\": %d, "
	              "\"height\": %d }, \"style\": { \"fontColor\": { \"r\": 220, \"g\": 195, \"b\": 118, \"a\": 255 }, \"pointSize\": %d, "
	              "\"alignment\": { \"h\": \"right\", \"v\": \"top\" }, \"options\": { \"newlineHandling\": \"standard\" } } } }\n]\n}\n",
	              PanelName, -(PanelWidth + PanelRight), PanelTop, PanelWidth, PanelHeight, TextName, PanelWidth, PanelHeight, PointSize);
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
		allOn = allOn && part.status.on;
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

void MapPanelMapOpened(const std::string& text, int slotLevel) {
	std::lock_guard<std::mutex> lock(g_lock);
	g_text = text;
	g_slotLevel = slotLevel;
	g_inMap = false;  // the player's tick says when it stands in the map
}

void MapPanelPlayerTick(void* player) noexcept {
	if (!g_on.load(std::memory_order_relaxed)) {
		return;
	}
	int slot = -1;
	{
		std::lock_guard<std::mutex> lock(g_lock);
		if (g_text.empty()) {
			return;
		}
		slot = g_slotLevel;
	}
	g_inMap = slot > 0 && PlayerLevelOf(player) == slot;
}

void MapPanelGameLeft() noexcept {
	std::lock_guard<std::mutex> lock(g_lock);
	g_text.clear();
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
	if (g_faulted.load()) {
		return "Map panel: off (a memory fault; see the plugin log)";
	}
	if (!g_on.load()) {
		return "Map panel: off (a game part or D2RLoader service was not found)";
	}
	return "Map panel: on (the host shows it, a TCP/IP client does not know the properties)";
}

}  // namespace carto
