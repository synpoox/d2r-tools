// Spec panel (spec_panel.h).
//
// Docking (spikes 4-5): the tree sends PanelManager OpenPanel / ClosePanel with text SkillsTreePanel on every route, and a
// panel opened from a queued UI callback stays open beside it. Clicks (spike 5): a button's own target never reaches the
// listener, so the arrows send PanelManager:OpenPanel:multispec/Spec<Up|Down>, which the listener consumes: no such panel
// exists. The panel is not CloseOnEscape: Escape goes to the tree, and the close message closes the panel with it.
// Wheel: the SDK has no mouse input, so the plugin subclasses D2R's window (class OsWindow), as scrollable-waypoints does.
// A notch counts anywhere while the panel is open: the panels' screen position depends on the aspect ratio, and the skill
// tree has no use for the wheel.
#include "spec_panel.h"

#include <D2RLPlugin/core_exports.h>

#include "log.h"
#include "spec_panel_layout.h"

#include <windows.h>

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <utility>

namespace ms {

namespace {

constexpr ULONGLONG SwapTimeoutMs = 5000;  // a swap that never answered (left the game) stops blocking the next step

const D2RL::SharedEventService* g_events = nullptr;
const D2RL::WidgetService* g_widgets = nullptr;
const D2RL::ThreadService* g_threads = nullptr;
const D2RL::PanelService* g_panels = nullptr;
const D2RL::PluginContext* g_context = nullptr;
D2RL::Panels::RegistrationHandle g_panel = D2RL::Panels::InvalidHandle;
D2RL::CoreExports::ExecuteConsoleCommandFn g_execute = nullptr;
int32_t g_specCount = 1;
char g_layout[65536];  // one number per spec, up to MaxSpecs

std::atomic<bool> g_open {false};
std::atomic<int32_t> g_active {0};
std::atomic<ULONGLONG> g_swapStarted {0};  // 0: no swap on its way
std::atomic<HWND> g_window {nullptr};
std::atomic<WNDPROC> g_gameProc {nullptr};

auto Is(const char* a, const char* b) noexcept -> bool {
	return a != nullptr && std::strcmp(a, b) == 0;
}

void RunCommand(const char* command) noexcept {
	if (g_execute == nullptr || !g_execute(command)) {
		plog::Warn("specui: D2RCore did not take '%s'", command);
	}
}

// UI thread. One swap at a time: fast clicks and wheel notches beyond that are dropped.
void __cdecl Step(const D2RL::PluginContext*, void* step) noexcept {
	const ULONGLONG now = GetTickCount64();
	const ULONGLONG started = g_swapStarted.load();
	if (started != 0 && now - started < SwapTimeoutMs) {
		return;
	}
	const int32_t to = panel::NextSpec(g_active.load(), g_specCount, static_cast<int32_t>(reinterpret_cast<intptr_t>(step)));
	if (to == 0) {
		return;
	}
	g_swapStarted.store(now);
	char command[24];
	std::snprintf(command, sizeof(command), "spec-panel %d", to);
	RunCommand(command);
}

auto CALLBACK WheelProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam) -> LRESULT {
	const WNDPROC game = g_gameProc.load();
	if (message == WM_MOUSEWHEEL && g_open.load() && g_threads != nullptr) {
		const int32_t step = GET_WHEEL_DELTA_WPARAM(wParam) > 0 ? 1 : -1;
		(void)g_threads->runOnUiThread(g_context, Step, reinterpret_cast<void*>(static_cast<intptr_t>(step)));
		return 0;
	}
	return game != nullptr ? CallWindowProcW(game, hwnd, message, wParam, lParam) : DefWindowProcW(hwnd, message, wParam, lParam);
}

auto CALLBACK FindGameWindow(HWND window, LPARAM result) -> BOOL {
	DWORD pid = 0;
	GetWindowThreadProcessId(window, &pid);
	char name[32] = "";
	if (pid == GetCurrentProcessId() && GetClassNameA(window, name, sizeof(name)) > 0 && std::strcmp(name, "OsWindow") == 0) {
		*reinterpret_cast<HWND*>(result) = window;
		return FALSE;
	}
	return TRUE;
}

void WatchWheel() noexcept {
	if (g_window.load() != nullptr) {
		return;
	}
	HWND window = nullptr;
	EnumWindows(FindGameWindow, reinterpret_cast<LPARAM>(&window));
	if (window == nullptr) {
		plog::Warn("specui: game window not found, no wheel");
		return;
	}
	g_gameProc.store(reinterpret_cast<WNDPROC>(GetWindowLongPtrW(window, GWLP_WNDPROC)));
	if (SetWindowLongPtrW(window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(&WheelProc)) == 0) {
		plog::Warn("specui: could not watch the wheel (error %lu)", GetLastError());
		return;
	}
	g_window.store(window);
}

void __cdecl OpenPanel(const D2RL::PluginContext* context, void*) noexcept {
	const auto r = g_panels->openPanel(context, g_panel);
	if (r != D2RL::Panels::Result::Success) {
		plog::Warn("specui: open result %u", static_cast<uint32_t>(r));
		return;
	}
	g_open.store(true);
	WatchWheel();
	RunCommand("spec-panel");  // the answer shows the active spec and sets the arrows
}

void __cdecl ClosePanel(const D2RL::PluginContext* context, void*) noexcept {
	g_open.store(false);
	(void)g_panels->closePanel(context, g_panel);
}

auto __cdecl OnUiMessage(const D2RL::PluginContext* context, const D2RL::SharedEvents::UiMessageEvent* event, void*) noexcept
	-> D2RL::SharedEvents::UiMessageAction {
	if (event == nullptr || !Is(event->target, "PanelManager")) {
		return D2RL::SharedEvents::UiMessageAction::Continue;
	}
	const int32_t step = panel::ParseStep(event->text);
	if (step != 0) {
		// Queued, not called here: the command must not run inside PanelManager's own message.
		(void)g_threads->runOnUiThread(context, Step, reinterpret_cast<void*>(static_cast<intptr_t>(step)));
		return D2RL::SharedEvents::UiMessageAction::Consume;  // no such panel: keep PanelManager from trying to open it
	}
	if (Is(event->text, "SkillsTreePanel")) {
		if (Is(event->command, "OpenPanel")) {
			(void)g_threads->runOnUiThread(context, OpenPanel, nullptr);
		} else if (Is(event->command, "ClosePanel")) {
			(void)g_threads->runOnUiThread(context, ClosePanel, nullptr);
		}
	}
	return D2RL::SharedEvents::UiMessageAction::Continue;
}

auto RegisterPanel(const D2RL::PluginContext* context, char* why, size_t size) noexcept -> bool {
	const D2RL::ResourceService* resources = nullptr;
	if (context->QueryService(&resources) != D2RL::ServiceQueryResult::Success ||
	    !D2RL::HasResourceServiceField(resources, D2RL::ResourceServiceRequiredSize) ||
	    context->QueryService(&g_panels) != D2RL::ServiceQueryResult::Success ||
	    !D2RL::HasPanelServiceField(g_panels, D2RL::PanelServiceRequiredSize)) {
		g_panels = nullptr;
		std::snprintf(why, size, "D2RLoader has no resource or panel service");
		return false;
	}
	const D2RL::Resources::ResourceRegistration resource {
		.structSize = D2RL::Resources::ResourceRegistrationSize,
		.flags      = 0,
		.path       = "data/global/ui/layouts/multispec/SpecPanelhd.json",
		.bytes      = g_layout,
		.byteCount  = panel::BuildLayout(g_specCount, g_layout, sizeof(g_layout)),
	};
	if (resource.byteCount == 0) {
		std::snprintf(why, size, "the layout did not fit its buffer");
		return false;
	}
	D2RL::Resources::RegistrationHandle layout = D2RL::Resources::InvalidHandle;
	const auto r = resources->registerResource(context, &resource, &layout);
	if (r != D2RL::Resources::Result::Success) {
		std::snprintf(why, size, "layout resource result %u", static_cast<uint32_t>(r));
		return false;
	}
	const D2RL::Panels::PanelRegistration registration {
		.structSize = D2RL::Panels::PanelRegistrationSize,
		.flags      = D2RL::Panels::PanelFlags::None,  // Escape must reach the tree
		.localId    = "SpecPanel",
	};
	const auto p = g_panels->registerPanel(context, &registration, &g_panel);
	if (p != D2RL::Panels::Result::Success) {
		g_panel = D2RL::Panels::InvalidHandle;
		std::snprintf(why, size, "registerPanel result %u", static_cast<uint32_t>(p));
		return false;
	}
	return true;
}

}  // namespace

void UpdateSpecPanel(const D2RL::PluginContext* context, int32_t active, bool inTown) noexcept {
	g_active.store(active);
	g_swapStarted.store(0);
	if (g_widgets == nullptr || g_panel == D2RL::Panels::InvalidHandle) {
		return;
	}
	D2RL::Widgets::WidgetHandle root = D2RL::Widgets::InvalidHandle;
	if (g_widgets->findPanel(context, "multispec/SpecPanel", &root) != D2RL::Widgets::Result::Success) {
		return;  // not built yet: the next open asks again
	}
	D2RL::Widgets::WidgetHandle widget = D2RL::Widgets::InvalidHandle;
	for (int32_t i = 1; i <= g_specCount; ++i) {
		char name[16];
		std::snprintf(name, sizeof(name), "Number%d", i);
		if (g_widgets->findWidget(context, root, name, &widget) == D2RL::Widgets::Result::Success) {
			(void)g_widgets->setWidgetVisible(context, widget, i == active);
		}
	}
	// setWidgetEnabled does not change how a button looks, so a greyed-out copy of each arrow takes its place.
	const bool usable = inTown && g_specCount > 1;
	for (const auto& [name, show] : {std::pair<const char*, bool> {"Up", usable}, {"Down", usable}, {"UpOff", !usable}, {"DownOff", !usable}}) {
		if (g_widgets->findWidget(context, root, name, &widget) == D2RL::Widgets::Result::Success) {
			(void)g_widgets->setWidgetVisible(context, widget, show);
		}
	}
}

void RemoveSpecPanel() noexcept {
	const HWND window = g_window.exchange(nullptr);
	if (window != nullptr && GetWindowLongPtrW(window, GWLP_WNDPROC) == reinterpret_cast<LONG_PTR>(&WheelProc)) {
		SetWindowLongPtrW(window, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(g_gameProc.load()));
	}
}

void InstallSpecPanel(const D2RL::PluginContext* context, int32_t specCount, char* report, size_t size) noexcept {
	if (context->QueryService(&g_events) != D2RL::ServiceQueryResult::Success ||
	    !D2RL::HasSharedEventServiceField(g_events, D2RL::SharedEventServiceRequiredSize) ||
	    context->QueryService(&g_widgets) != D2RL::ServiceQueryResult::Success ||
	    !D2RL::HasWidgetServiceField(g_widgets, D2RL::WidgetServiceRequiredSize) ||
	    context->QueryService(&g_threads) != D2RL::ServiceQueryResult::Success ||
	    !D2RL::HasThreadServiceField(g_threads, D2RL::ThreadServiceRequiredSize)) {
		g_widgets = nullptr;
		std::snprintf(report, size, "off: D2RLoader has no event, widget or thread service");
		return;
	}
	const HMODULE core = GetModuleHandleW(L"D2RCore.dll");
	if (core != nullptr) {
		g_execute = reinterpret_cast<D2RL::CoreExports::ExecuteConsoleCommandFn>(
			GetProcAddress(core, MAKEINTRESOURCEA(D2RL::CoreExports::ExecuteConsoleCommandInfo.ordinal)));
	}
	if (g_execute == nullptr) {
		std::snprintf(report, size, "off: D2RCore has no ExecuteConsoleCommand");
		return;
	}
	g_context = context;
	g_specCount = specCount;
	char why[120] = "";
	if (!RegisterPanel(context, why, sizeof(why))) {
		std::snprintf(report, size, "off: %s", why);
		return;
	}
	D2RL::SharedEvents::UiMessageListener listener {};
	listener.structSize = D2RL::SharedEvents::UiMessageListenerSize;
	listener.priority = 1000;  // first, so the arrow clicks are consumed before PanelManager acts on them
	listener.callback = OnUiMessage;
	D2RL::SharedEvents::ListenerHandle handle {};
	const auto r = g_events->registerUiMessageListener(context, &listener, &handle);
	if (r != D2RL::SharedEvents::Result::Success) {
		std::snprintf(report, size, "off: UI message listener not registered (result %u)", static_cast<uint32_t>(r));
		return;
	}
	std::snprintf(report, size, "on: multispec/SpecPanel, %d specs", g_specCount);
}

}  // namespace ms
