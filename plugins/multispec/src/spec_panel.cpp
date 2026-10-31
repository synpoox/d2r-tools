// Spec panel (spec_panel.h).
//
// Docking (spikes 4-5): the tree sends PanelManager OpenPanel / ClosePanel with text SkillsTreePanel on every route, and a
// panel opened from a queued UI callback stays open beside it. Clicks (spike 5): a button's own target never reaches the
// listener, so the slots send PanelManager:OpenPanel:multispec/Swap<n>, which the listener consumes: no such panel exists.
// The panel is not CloseOnEscape: Escape goes to the tree, and the close message closes the panel with it.
#include "spec_panel.h"

#include <D2RLPlugin/core_exports.h>

#include "log.h"
#include "spec_panel_layout.h"

#include <windows.h>

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace ms {

namespace {

const D2RL::SharedEventService* g_events = nullptr;
const D2RL::WidgetService* g_widgets = nullptr;
const D2RL::ThreadService* g_threads = nullptr;
const D2RL::PanelService* g_panels = nullptr;
D2RL::Panels::RegistrationHandle g_panel = D2RL::Panels::InvalidHandle;
D2RL::CoreExports::ExecuteConsoleCommandFn g_execute = nullptr;
int32_t g_slots = 1;
char g_layout[4096];

auto Is(const char* a, const char* b) noexcept -> bool {
	return a != nullptr && std::strcmp(a, b) == 0;
}

void RunCommand(const char* command) noexcept {
	if (g_execute == nullptr || !g_execute(command)) {
		plog::Warn("specui: D2RCore did not take '%s'", command);
	}
}

void __cdecl OpenPanel(const D2RL::PluginContext* context, void*) noexcept {
	const auto r = g_panels->openPanel(context, g_panel);
	if (r != D2RL::Panels::Result::Success) {
		plog::Warn("specui: open result %u", static_cast<uint32_t>(r));
		return;
	}
	RunCommand("spec");  // the answer marks the active spec and enables the slots
}

void __cdecl ClosePanel(const D2RL::PluginContext* context, void*) noexcept {
	(void)g_panels->closePanel(context, g_panel);
}

void __cdecl SwapTo(const D2RL::PluginContext*, void* spec) noexcept {
	char command[16];
	std::snprintf(command, sizeof(command), "spec %d", static_cast<int>(reinterpret_cast<intptr_t>(spec)));
	RunCommand(command);
}

auto __cdecl OnUiMessage(const D2RL::PluginContext* context, const D2RL::SharedEvents::UiMessageEvent* event, void*) noexcept
	-> D2RL::SharedEvents::UiMessageAction {
	if (event == nullptr || !Is(event->target, "PanelManager")) {
		return D2RL::SharedEvents::UiMessageAction::Continue;
	}
	const int32_t spec = panel::ParseSwap(event->text);
	if (spec != 0) {
		// Queued, not called here: the command must not run inside PanelManager's own message.
		(void)g_threads->runOnUiThread(context, SwapTo, reinterpret_cast<void*>(static_cast<intptr_t>(spec)));
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
		.byteCount  = panel::BuildLayout(g_slots, g_layout, sizeof(g_layout)),
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
	if (g_widgets == nullptr || g_panel == D2RL::Panels::InvalidHandle) {
		return;
	}
	D2RL::Widgets::WidgetHandle root = D2RL::Widgets::InvalidHandle;
	if (g_widgets->findPanel(context, "multispec/SpecPanel", &root) != D2RL::Widgets::Result::Success) {
		return;  // not built yet: the next open asks again
	}
	for (int32_t i = 1; i <= g_slots; ++i) {
		char name[16];
		D2RL::Widgets::WidgetHandle widget = D2RL::Widgets::InvalidHandle;
		std::snprintf(name, sizeof(name), "Active%d", i);
		if (g_widgets->findWidget(context, root, name, &widget) == D2RL::Widgets::Result::Success) {
			(void)g_widgets->setWidgetVisible(context, widget, i == active);
		}
		std::snprintf(name, sizeof(name), "Slot%d", i);
		if (g_widgets->findWidget(context, root, name, &widget) == D2RL::Widgets::Result::Success) {
			(void)g_widgets->setWidgetEnabled(context, widget, inTown);
		}
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
	g_slots = panel::VisibleSlots(specCount);
	char why[120] = "";
	if (!RegisterPanel(context, why, sizeof(why))) {
		std::snprintf(report, size, "off: %s", why);
		return;
	}
	D2RL::SharedEvents::UiMessageListener listener {};
	listener.structSize = D2RL::SharedEvents::UiMessageListenerSize;
	listener.priority = 1000;  // first, so the slot clicks are consumed before PanelManager acts on them
	listener.callback = OnUiMessage;
	D2RL::SharedEvents::ListenerHandle handle {};
	const auto r = g_events->registerUiMessageListener(context, &listener, &handle);
	if (r != D2RL::SharedEvents::Result::Success) {
		std::snprintf(report, size, "off: UI message listener not registered (result %u)", static_cast<uint32_t>(r));
		return;
	}
	std::snprintf(report, size, "on: multispec/SpecPanel, %d slots%s", g_slots, specCount > panel::MaxSlots ? " (more specs: spec N)" : "");
}

}  // namespace ms
