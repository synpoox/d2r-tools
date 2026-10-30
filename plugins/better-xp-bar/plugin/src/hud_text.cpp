// Native panel text (hud_text.h). NOT RUN IN GAME YET: needs live check throughout (TESTING.md).
//
// Load: byte checks for the four widget functions, the layout JSON (from the settings) registered at
// data/global/ui/layouts/<id>/XpPanelhd.json, and the panel "XpPanel" registered with no flags.
// UI thread: open the panel when it is closed, find it natively by its full name, find "Line1".."LineN", check each is
// a TextBoxWidget and set its text when it changed. Every native call is SEH-guarded; a fault turns the part off.
#include "hud_text.h"

#include "d2r.h"
#include "log.h"

#define NOMINMAX
#include <windows.h>

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstring>

#ifndef BXB_PLUGIN_NAME
#define BXB_PLUGIN_NAME "better-xp-bar"
#endif

namespace hud_text {

namespace {

constexpr const char* LocalId = "XpPanel";
constexpr const char* PanelName = BXB_PLUGIN_NAME "/XpPanel";
constexpr const char* LayoutPath = "data/global/ui/layouts/" BXB_PLUGIN_NAME "/XpPanelhd.json";
constexpr size_t MaxLines = 8;
constexpr float DefaultPointSize = 30.0f;  // layout units; the HUD's small text is about 22
constexpr int PadX = 20;                   // the in-game tooltip's padding (profile "padding": [20, 12, 20, 12])
constexpr int PadY = 12;

const D2RL::PluginContext* g_context = nullptr;
const D2RL::PanelService* g_panels = nullptr;
D2RL::Panels::RegistrationHandle g_panel = D2RL::Panels::InvalidHandle;
d2r::FindPanelFn g_findPanel = nullptr;
d2r::FindChildFn g_findChild = nullptr;
d2r::TextBoxTypeFn g_textBoxType = nullptr;
d2r::SetTextFn g_setText = nullptr;
d2r::ExpBarTypeFn g_expBarType = nullptr;  // null: hover-to-show is off (byte checks failed or a fault)
d2r::IsHoveredFn g_isHovered = nullptr;
bool g_installed = false;
size_t g_lineCount = 0;

// UI thread
d2r::Widget* g_lastPanel = nullptr;
std::string g_shown[MaxLines];
bool g_open = false;
std::atomic<uint32_t> g_faults{0};
std::atomic<uint64_t> g_sets{0};
std::atomic<uint32_t> g_openFailures{0};
std::atomic<uint32_t> g_lastOpenResult{0};
std::atomic<bool> g_found{false};

auto Clamp255(float v) -> int {
	return static_cast<int>(std::clamp(v, 0.0f, 1.0f) * 255.0f + 0.5f);
}

// The layout: a Panel at the configured anchor, one TextBoxWidget per line aligned by text_align. Inline style only
// (no profile names a mod might not have). Coordinates are layout units relative to the anchor: the box edge named by
// text_align sits at offset_x; its bottom edge (top edge for top anchors) sits at offset_y.
auto BuildLayout(const bxb::Settings& s, size_t lines) -> std::string {
	const float scale = s.scale > 0.0f ? s.scale : 1.0f;
	const float size = (s.textSize > 0.0f ? s.textSize : DefaultPointSize) * scale;
	const int lineHeight = static_cast<int>(size * 1.35f + 0.5f);
	const int textWidth = static_cast<int>(size * 22.0f);  // "XP 124.60M/146.07M  85.30%" measured ~640 units at size 30
	const int padX = static_cast<int>(PadX * scale + 0.5f);
	const int padY = static_cast<int>(PadY * scale + 0.5f);
	const int width = textWidth + 2 * padX;
	const int height = lineHeight * static_cast<int>(lines) + 2 * padY;
	const bool top = s.anchor == bxb::Anchor::TopLeft || s.anchor == bxb::Anchor::TopCenter || s.anchor == bxb::Anchor::TopRight;
	const float ay = top ? 0.0f : 1.0f;
	float ax = 0.5f;
	switch (s.anchor) {
	case bxb::Anchor::TopLeft: case bxb::Anchor::BottomLeft: ax = 0.0f; break;
	case bxb::Anchor::TopCenter: case bxb::Anchor::BottomCenter: ax = 0.5f; break;
	case bxb::Anchor::TopRight: case bxb::Anchor::BottomRight: ax = 1.0f; break;
	}
	const char* align = bxb::TextAlignName(s.textAlign);
	int x = static_cast<int>(s.offsetX);
	if (s.textAlign == bxb::TextAlign::Center) {
		x -= width / 2;
	} else if (s.textAlign == bxb::TextAlign::Right) {
		x -= width;
	}
	const int y = static_cast<int>(s.offsetY) - (top ? 0 : height);
	char buf[512];
	std::string json;
	std::snprintf(buf, sizeof(buf),
	              "{\n\"type\": \"Panel\", \"name\": \"%s\",\n\"fields\": { \"rect\": { \"x\": %d, \"y\": %d, \"width\": %d, \"height\": %d }, "
	              "\"anchor\": { \"x\": %.3f, \"y\": %.3f } },\n\"children\": [\n",
	              PanelName, x, y, width, height, ax, ay);
	json += buf;
	if (s.background > 0.0f) {
		// Like the XP bar's hover popup (in-game tooltip style: black, 0.8 opacity, padding 20 x 12, no border).
		std::snprintf(buf, sizeof(buf),
		              "{ \"type\": \"RectangleWidget\", \"name\": \"Background\", \"fields\": { \"rect\": { \"x\": 0, \"y\": 0, "
		              "\"width\": %d, \"height\": %d }, \"color\": [ 0.0, 0.0, 0.0, %.2f ] } },\n",
		              width, height, s.background);
		json += buf;
	}
	for (size_t i = 0; i < lines; ++i) {
		std::snprintf(buf, sizeof(buf),
		              "{ \"type\": \"TextBoxWidget\", \"name\": \"Line%zu\", \"fields\": { \"text\": \"\", "
		              "\"rect\": { \"x\": %d, \"y\": %d, \"width\": %d, \"height\": %d }, \"style\": { \"fontColor\": { \"r\": %d, \"g\": %d, "
		              "\"b\": %d, \"a\": 255 }, \"pointSize\": %.0f, \"alignment\": { \"h\": \"%s\", \"v\": \"center\" } } } }%s\n",
		              i + 1, padX, padY + lineHeight * static_cast<int>(i), textWidth, lineHeight, Clamp255(s.textColor[0]),
		              Clamp255(s.textColor[1]), Clamp255(s.textColor[2]), size, align, i + 1 < lines ? "," : "");
		json += buf;
	}
	json += "]\n}\n";
	return json;
}

auto IsA(d2r::Widget* w, d2r::TypeDesc* type) noexcept -> bool {
	using GetTypeFn = d2r::TypeDesc* (*)(d2r::Widget*) noexcept;
	const auto vtbl = *reinterpret_cast<GetTypeFn* const*>(w);
	for (auto* t = vtbl[d2r::VtblGetType](w); t != nullptr; t = *reinterpret_cast<d2r::TypeDesc* const*>(reinterpret_cast<const char*>(t) + d2r::TypeDescBase)) {
		if (t == type) {
			return true;
		}
	}
	return false;
}

// The player's (not the hireling's) ExpBarWidget below `w`, depth-first, bounded.
auto FindExpBar(d2r::Widget* w, d2r::TypeDesc* type, int depth) noexcept -> d2r::Widget* {
	if (IsA(w, type) && *(reinterpret_cast<const uint8_t*>(w) + d2r::ExpBarIsHireling) == 0) {
		return w;
	}
	if (depth >= 8) {
		return nullptr;
	}
	const auto children = *reinterpret_cast<d2r::Widget* const* const*>(reinterpret_cast<const char*>(w) + d2r::WidgetChildren);
	const auto count = *reinterpret_cast<const uint64_t*>(reinterpret_cast<const char*>(w) + d2r::WidgetChildCount);
	for (uint64_t i = 0; children != nullptr && i < count && i < 512; ++i) {
		if (children[i] != nullptr) {
			if (d2r::Widget* found = FindExpBar(children[i], type, depth + 1)) {
				return found;
			}
		}
	}
	return nullptr;
}

// Plain C++ (no SEH here). True while the mouse is over the stock XP bar (the same test its popup uses).
auto ExpBarHoveredRaw() noexcept -> bool {
	d2r::Widget* hud = g_findPanel("HUDPanel");
	if (hud == nullptr) {
		return false;
	}
	d2r::Widget* bar = FindExpBar(hud, g_expBarType(), 0);
	if (bar == nullptr) {
		return false;
	}
	const void* target = *reinterpret_cast<void* const*>(reinterpret_cast<const char*>(bar) + d2r::ExpBarHoverTarget);
	return target != nullptr && g_isHovered(target);
}

// Plain C++ (no SEH here). Returns the panel found (null when not up yet); sets changed lines.
auto FillRaw(const std::vector<std::string>* lines, d2r::Widget** panelOut, uint32_t* setCount) noexcept -> bool {
	d2r::Widget* panel = g_findPanel(PanelName);
	*panelOut = panel;
	if (panel == nullptr) {
		return false;
	}
	d2r::TypeDesc* textBox = g_textBoxType();
	for (size_t i = 0; i < g_lineCount; ++i) {
		static const std::string Empty;
		const std::string& text = i < lines->size() ? (*lines)[i] : Empty;
		if (panel == g_lastPanel && text == g_shown[i]) {
			continue;
		}
		char name[16];
		std::snprintf(name, sizeof(name), "Line%zu", i + 1);
		d2r::Widget* box = g_findChild(panel, name);
		if (box == nullptr || !IsA(box, textBox)) {
			continue;
		}
		const d2r::StringView view {text.c_str(), text.size()};
		g_setText(box, &view);
		g_shown[i] = text;
		++*setCount;
	}
	return true;
}

#if defined(_MSC_VER)
#define BXB_TRY __try
#define BXB_EXCEPT __except (EXCEPTION_EXECUTE_HANDLER)
#else
#define BXB_TRY if (true)
#define BXB_EXCEPT else
#endif

auto GuardedHovered(bool* hovered) noexcept -> bool {
	BXB_TRY {
		*hovered = ExpBarHoveredRaw();
		return true;
	}
	BXB_EXCEPT {
		return false;
	}
}

auto GuardedFill(const std::vector<std::string>* lines, d2r::Widget** panel, uint32_t* setCount, bool* found) noexcept -> bool {
	BXB_TRY {
		*found = FillRaw(lines, panel, setCount);
		return true;
	}
	BXB_EXCEPT {
		return false;
	}
}

// True when the primary mouse button went down since the last call and the game window has focus. Uses the
// "pressed since last call" bit as well as the down edge, so a click shorter than the read interval counts.
auto PrimaryClickedSinceLastRead() noexcept -> bool {
	static bool s_wasDown = false;
	const int vk = GetSystemMetrics(SM_SWAPBUTTON) != 0 ? VK_RBUTTON : VK_LBUTTON;
	const SHORT state = GetAsyncKeyState(vk);
	const bool down = (state & 0x8000) != 0;
	const bool edge = (down && !s_wasDown) || (state & 0x0001) != 0;
	s_wasDown = down;
	DWORD pid = 0;
	const HWND fg = GetForegroundWindow();
	return edge && fg != nullptr && GetWindowThreadProcessId(fg, &pid) != 0 && pid == GetCurrentProcessId();
}

void SetOpen(bool open) noexcept {
	D2RL::Panels::PanelInfo info {};
	info.structSize = D2RL::Panels::PanelInfoSize;
	const bool known = g_panels->getPanelInfo(g_context, g_panel, &info) == D2RL::Panels::Result::Success;
	const bool isOpen = known && info.presentationState == D2RL::Panels::PresentationState::Open;
	if (open == isOpen) {
		g_open = isOpen;
		return;
	}
	const auto r = open ? g_panels->openPanel(g_context, g_panel) : g_panels->closePanel(g_context, g_panel);
	if (r == D2RL::Panels::Result::Success) {
		g_open = open;
		if (open) {
			g_lastPanel = nullptr;  // a reopened panel may be a new widget: set every line again
		}
		plog::Info("hud: panel %s", open ? "opened" : "closed");
		return;
	}
	g_lastOpenResult = static_cast<uint32_t>(r);
	static std::atomic<uint32_t> s_logged{0};
	++g_openFailures;
	if (plog::Sample(s_logged)) {
		plog::Warn("hud: %s panel failed with result %u (%u failures so far)", open ? "opening" : "closing", static_cast<uint32_t>(r),
		           g_openFailures.load());
	}
}

}  // namespace

void Install(const D2RL::PluginContext* context, const bxb::Settings& settings, char* report, size_t size) noexcept {
	g_context = context;
	for (const d2r::ExpectedBytes& c : d2r::HudChecks) {
		if (!context->CheckExpectedBytes(c.rva, c.bytes, c.size)) {
			std::snprintf(report, size, "off: game bytes differ at %s (another D2R/D2RLoader build, or patched)", c.what);
			return;
		}
	}
	const D2RL::ResourceService* resources = nullptr;
	if (context->QueryService(&resources) != D2RL::ServiceQueryResult::Success || resources == nullptr ||
	    resources->serviceSize < D2RL::ResourceServiceRequiredSize) {
		std::snprintf(report, size, "off: D2RLoader has no Resource service of the expected version");
		return;
	}
	if (context->QueryService(&g_panels) != D2RL::ServiceQueryResult::Success ||
	    !D2RL::HasPanelServiceField(g_panels, D2RL::PanelServiceRequiredSize)) {
		g_panels = nullptr;
		std::snprintf(report, size, "off: D2RLoader has no Panel service of the expected version");
		return;
	}
	g_lineCount = std::clamp<size_t>(settings.lines.size(), 1, MaxLines);
	const std::string layout = BuildLayout(settings, g_lineCount);
	D2RL::Resources::ResourceRegistration res {};
	res.structSize = D2RL::Resources::ResourceRegistrationSize;
	res.path = LayoutPath;
	res.bytes = layout.data();
	res.byteCount = layout.size();
	D2RL::Resources::RegistrationHandle rh = D2RL::Resources::InvalidHandle;
	const auto rr = resources->registerResource(context, &res, &rh);
	if (rr != D2RL::Resources::Result::Success) {
		std::snprintf(report, size, "off: layout resource %s refused (result %u)", LayoutPath, static_cast<uint32_t>(rr));
		return;
	}
	D2RL::Panels::PanelRegistration reg {};
	reg.structSize = D2RL::Panels::PanelRegistrationSize;
	reg.flags = D2RL::Panels::PanelFlags::None;
	reg.localId = LocalId;
	const auto pr = g_panels->registerPanel(context, &reg, &g_panel);
	if (pr != D2RL::Panels::Result::Success) {
		std::snprintf(report, size, "off: panel %s refused (result %u)", PanelName, static_cast<uint32_t>(pr));
		return;
	}
	const uintptr_t base = context->exeBase;
	g_findPanel = reinterpret_cast<d2r::FindPanelFn>(base + d2r::RvaFindPanel);
	g_findChild = reinterpret_cast<d2r::FindChildFn>(base + d2r::RvaFindChild);
	g_textBoxType = reinterpret_cast<d2r::TextBoxTypeFn>(base + d2r::RvaTextBoxType);
	g_setText = reinterpret_cast<d2r::SetTextFn>(base + d2r::RvaSetText);
	const char* hover = "on";
	for (const d2r::ExpectedBytes& c : d2r::HoverChecks) {
		if (!context->CheckExpectedBytes(c.rva, c.bytes, c.size)) {
			hover = c.what;
			break;
		}
	}
	if (std::strcmp(hover, "on") == 0) {
		g_expBarType = reinterpret_cast<d2r::ExpBarTypeFn>(base + d2r::RvaExpBarType);
		g_isHovered = reinterpret_cast<d2r::IsHoveredFn>(base + d2r::RvaIsHovered);
	} else {
		plog::Warn("hud: game bytes differ at %s: showing on XP bar hover is off", hover);
	}
	g_installed = true;
	plog::Info("hud: layout %s (%zu bytes):\n%s", LayoutPath, layout.size(), layout.c_str());
	std::snprintf(report, size, "on: %zu byte checks passed; panel %s with %zu lines", sizeof(d2r::HudChecks) / sizeof(d2r::HudChecks[0]),
	              PanelName, g_lineCount);
}

auto Installed() noexcept -> bool {
	return g_installed;
}

auto Update(const std::vector<std::string>& lines, bool show) noexcept -> bool {
	if (!g_installed) {
		return false;
	}
	const bool clicked = PrimaryClickedSinceLastRead();  // read every time so an old click never counts later
	bool toggle = false;
	if (g_expBarType != nullptr && !lines.empty()) {
		bool hovered = false;
		if (!GuardedHovered(&hovered)) {
			g_expBarType = nullptr;
			plog::Error("hud: memory fault in the XP bar hover check; hover and click on the XP bar are now off");
		}
		toggle = hovered && clicked;  // a click on the stock XP bar toggles the display
		if (toggle) {
			show = !show;
		}
		show = show || hovered;       // hidden: shown while the stock XP bar is hovered
	}
	if (!show || lines.empty()) {
		if (g_open) {
			SetOpen(false);
		}
		return toggle;
	}
	SetOpen(true);
	if (!g_open) {
		return toggle;
	}
	d2r::Widget* panel = nullptr;
	uint32_t sets = 0;
	bool found = false;
	if (!GuardedFill(&lines, &panel, &sets, &found)) {
		++g_faults;
		g_installed = false;
		plog::Error("hud: memory fault while setting the text; part hud is now off (status shows it)");
		return toggle;
	}
	if (found != g_found.exchange(found)) {
		plog::Info("hud: native panel %s %s", PanelName, found ? "found" : "not found");
	}
	g_lastPanel = panel;
	g_sets += sets;
	return toggle;
}

void Status(char* out, size_t size) noexcept {
	std::snprintf(out, size, "panel %s, native %s, text sets %llu, open failures %u (last %u), faults %u, show on XP bar hover %s",
	              g_open ? "open" : "closed", g_found ? "found" : "not found", static_cast<unsigned long long>(g_sets.load()),
	              g_openFailures.load(), g_lastOpenResult.load(), g_faults.load(), g_expBarType != nullptr ? "on" : "off");
}

}  // namespace hud_text
