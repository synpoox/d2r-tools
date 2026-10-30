// UI probe (ui_probe.h).
#include "ui_probe.h"

#include "log.h"
#include "strip_art.h"

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace ms {

namespace {

const D2RL::SharedEventService* g_events = nullptr;
const D2RL::WidgetService* g_widgets = nullptr;
const D2RL::ThreadService* g_uiThreads = nullptr;
const D2RL::PanelService* g_panels = nullptr;
D2RL::Panels::RegistrationHandle g_probePanel = D2RL::Panels::InvalidHandle;
std::atomic<bool> g_logging {false};
std::atomic<bool> g_dock {false};
std::atomic<uint32_t> g_seen {0};

// Spike 8b test panel (design section 11, "Strip art"): the docked strip with our own art (strip_art.h), sized to its
// slots. Earlier runs: the strip sits at x -1394 - its visible width on $RightPanelAnchor, flush with the tree
// (spikes 5-6); priority -5 draws it under the tree, so the wing's tucked right edge hides under the tree's frame
// (spike 8); a plugin sprite under data/hd/global/ui/d2rloader/multispec/ resolves (spike 8); a button's own target
// never reaches the listener, so the sockets send PanelManager:OpenPanel:multispec/ProbeSwap<n>, which the listener
// logs and consumes (spike 5). The wing's top lines up with the tree's tab row (y 145).
constexpr int32_t ProbeSlots = 3;
constexpr int32_t WingTop = 145;
char g_layout[4096];

auto BuildProbeLayout() noexcept -> size_t {
	const int32_t h = art::WingHeight(ProbeSlots);
	int n = std::snprintf(g_layout, sizeof(g_layout), R"json({
	"type": "Panel", "name": "multispec/ProbePanel",
	"fields": {
		"anchor": "$RightPanelAnchor",
		"rect": { "x": %d, "y": %d, "width": %d, "height": %d },
		"priority": -5
	},
	"children": [
		{ "type": "ClickCatcherWidget", "name": "ClickCatcher",
		  "fields": { "rect": { "x": 0, "y": 0, "width": %d, "height": %d } } },
		{ "type": "ImageWidget", "name": "Wing",
		  "fields": { "rect": { "x": 0, "y": 0 }, "filename": "d2rloader\\multispec\\wing" } },
		{ "type": "TextBoxWidget", "name": "Heading",
		  "fields": { "rect": { "x": %d, "y": %d, "width": %d, "height": %d }, "text": "Specs",
		              "style": { "pointSize": "$MediumPanelFontSize", "fontColor": "$FontColorGold",
		                         "alignment": { "h": "center", "v": "center" } } } })json",
	                      -1394 - art::VisibleWidth, -651 + WingTop, art::WingWidth, h, art::VisibleWidth, h,
	                      art::WingFrame, art::WingFrame, art::VisibleWidth - art::WingFrame, art::WingHeadingHeight);
	for (int32_t i = 0; i < ProbeSlots && n > 0 && static_cast<size_t>(n) < sizeof(g_layout); ++i) {
		n += std::snprintf(g_layout + n, sizeof(g_layout) - static_cast<size_t>(n), R"json(,
		{ "type": "ButtonWidget", "name": "Slot%d",
		  "fields": { "rect": { "x": %d, "y": %d }, "filename": "d2rloader\\multispec\\socket", "hoveredFrame": 1,
		              "onClickMessage": "PanelManager:OpenPanel:multispec/ProbeSwap%d" } },
		{ "type": "TextBoxWidget", "name": "Number%d",
		  "fields": { "rect": { "x": %d, "y": %d, "width": %d, "height": %d }, "text": "%d",
		              "style": { "pointSize": "$XMediumPanelFontSize", "fontColor": "$FontColorLightGold",
		                         "alignment": { "h": "center", "v": "center" } } } })json",
		                   i + 1, art::SocketX, art::SlotY(i), i + 1, i + 1, art::SocketX, art::SlotY(i),
		                   art::SocketSize, art::SocketSize, i + 1);
	}
	if (n > 0 && static_cast<size_t>(n) < sizeof(g_layout)) {
		n += std::snprintf(g_layout + n, sizeof(g_layout) - static_cast<size_t>(n), "\n\t]\n}");
	}
	return n > 0 && static_cast<size_t>(n) < sizeof(g_layout) ? static_cast<size_t>(n) : 0;
}

auto Text(const char* s) noexcept -> const char* {
	return s != nullptr ? s : "(null)";
}

auto Is(const char* a, const char* b) noexcept -> bool {
	return a != nullptr && std::strcmp(a, b) == 0;
}

auto Has(const char* a, const char* part) noexcept -> bool {
	return a != nullptr && std::strstr(a, part) != nullptr;
}

void __cdecl OpenProbePanel(const D2RL::PluginContext* context, void*) noexcept {
	const auto r = g_panels->openPanel(context, g_probePanel);
	plog::Info("uiprobe: dock open ProbePanel -> result %u", static_cast<uint32_t>(r));
}

void __cdecl CloseProbePanel(const D2RL::PluginContext* context, void*) noexcept {
	const auto r = g_panels->closePanel(context, g_probePanel);
	plog::Info("uiprobe: dock close ProbePanel -> result %u", static_cast<uint32_t>(r));
}

auto __cdecl OnUiMessage(const D2RL::PluginContext* context, const D2RL::SharedEvents::UiMessageEvent* event, void*) noexcept
	-> D2RL::SharedEvents::UiMessageAction {
	if (event == nullptr) {
		return D2RL::SharedEvents::UiMessageAction::Continue;
	}
	if (Is(event->target, "PanelManager") && event->text != nullptr &&
	    std::strncmp(event->text, "multispec/ProbeSwap", 19) == 0) {
		char line[200];
		std::snprintf(line, sizeof(line), "uiprobe: slot click '%s' %s (consumed)", event->text, Text(event->command));
		plog::Info("%s", line);
		context->WriteConsoleMessage(line);
		return D2RL::SharedEvents::UiMessageAction::Consume;  // no such panel: keep PanelManager from trying to open it
	}
	// Any message naming the plugin or the probe, whichever field it lands in (spike 5: no MultiSpec target was seen).
	if (Has(event->target, "MultiSpec") || Has(event->command, "MultiSpec") || Has(event->text, "MultiSpec") ||
	    Has(event->target, "Probe") || Has(event->command, "Probe") || Has(event->text, "Probe")) {
		char line[400];
		std::snprintf(line, sizeof(line), "uiprobe: click? target '%s' command '%s' text '%s'", Text(event->target),
		              Text(event->command), Text(event->text));
		plog::Info("%s", line);
		context->WriteConsoleMessage(line);
	}
	// Queued, not called here: opening a panel from inside PanelManager's own message may re-enter it.
	if (g_dock.load() && g_probePanel != D2RL::Panels::InvalidHandle && Is(event->target, "PanelManager") &&
	    Is(event->text, "SkillsTreePanel")) {
		if (Is(event->command, "OpenPanel")) {
			(void)g_uiThreads->runOnUiThread(context, OpenProbePanel, nullptr);
		} else if (Is(event->command, "ClosePanel")) {
			(void)g_uiThreads->runOnUiThread(context, CloseProbePanel, nullptr);
		}
	}
	if (g_logging.load()) {
		const uint32_t n = ++g_seen;
		char line[700];
		std::snprintf(line, sizeof(line), "uiprobe #%u target '%s' command '%s' text '%s'", n, Text(event->target),
		              Text(event->command), Text(event->text));
		plog::Info("%s", line);
		context->WriteConsoleMessage(line);
	}
	return D2RL::SharedEvents::UiMessageAction::Continue;
}

void __cdecl ToggleTree(const D2RL::PluginContext* context, void*) noexcept {
	D2RL::Widgets::UiAction action {};
	action.structSize = D2RL::Widgets::UiActionSize;
	action.target = "PanelManager";
	action.command = "TogglePanel";
	action.text = "SkillsTreePanel";
	const auto r = g_widgets->dispatchUiAction(context, &action);
	char line[200];
	std::snprintf(line, sizeof(line), "uiprobe: dispatch PanelManager:TogglePanel:SkillsTreePanel -> result %u",
	              static_cast<uint32_t>(r));
	plog::Info("%s", line);
	context->WriteConsoleMessage(line);
}

void __cdecl FindTree(const D2RL::PluginContext* context, void*) noexcept {
	D2RL::Widgets::WidgetHandle panel = D2RL::Widgets::InvalidHandle;
	const auto found = g_widgets->findPanel(context, "SkillsTreePanel", &panel);
	char line[300];
	if (found != D2RL::Widgets::Result::Success) {
		std::snprintf(line, sizeof(line), "uiprobe: findPanel SkillsTreePanel -> result %u", static_cast<uint32_t>(found));
	} else {
		D2RL::Widgets::Rect rect {};
		const auto got = g_widgets->getWidgetRect(context, panel, &rect);
		std::snprintf(line, sizeof(line), "uiprobe: findPanel SkillsTreePanel -> found; getWidgetRect -> result %u rect %d %d %d x %d",
		              static_cast<uint32_t>(got), rect.x, rect.y, rect.width, rect.height);
	}
	plog::Info("%s", line);
	context->WriteConsoleMessage(line);
}

void __cdecl ToggleProbePanel(const D2RL::PluginContext* context, void*) noexcept {
	char line[200];
	if (g_panels == nullptr || g_probePanel == D2RL::Panels::InvalidHandle) {
		std::snprintf(line, sizeof(line), "uiprobe: no probe panel (see the uiprobe line in the log)");
	} else {
		std::snprintf(line, sizeof(line), "uiprobe: togglePanel ProbePanel -> result %u",
		              static_cast<uint32_t>(g_panels->togglePanel(context, g_probePanel)));
	}
	plog::Info("%s", line);
	context->WriteConsoleMessage(line);
}

// Our own art (strip_art.h), drawn once at load. D2RLoader copies the bytes, but they are kept for the plugin's life.
uint8_t g_wing[art::SpriteBytes(art::WingWidth, art::WingHeight(ProbeSlots))];
uint8_t g_wingLow[art::SpriteBytes(art::WingWidth / 2, art::WingHeight(ProbeSlots) / 2)];
uint8_t g_socket[art::SpriteBytes(art::SocketSize * 2, art::SocketSize)];
uint8_t g_socketLow[art::SpriteBytes(art::SocketSize, art::SocketSize / 2)];

auto RegisterArt(const D2RL::ResourceService* resources, const D2RL::PluginContext* context) noexcept -> uint32_t {
	const size_t sizes[4] = {
		art::DrawWing(ProbeSlots, 1, g_wing, sizeof(g_wing)),
		art::DrawWing(ProbeSlots, 2, g_wingLow, sizeof(g_wingLow)),
		art::DrawSocket(1, g_socket, sizeof(g_socket)),
		art::DrawSocket(2, g_socketLow, sizeof(g_socketLow)),
	};
	const D2RL::Resources::ResourceRegistration files[4] = {
		{D2RL::Resources::ResourceRegistrationSize, 0, "data/hd/global/ui/d2rloader/multispec/wing.sprite", g_wing, sizes[0]},
		{D2RL::Resources::ResourceRegistrationSize, 0, "data/hd/global/ui/d2rloader/multispec/wing.lowend.sprite", g_wingLow,
		 sizes[1]},
		{D2RL::Resources::ResourceRegistrationSize, 0, "data/hd/global/ui/d2rloader/multispec/socket.sprite", g_socket,
		 sizes[2]},
		{D2RL::Resources::ResourceRegistrationSize, 0, "data/hd/global/ui/d2rloader/multispec/socket.lowend.sprite",
		 g_socketLow, sizes[3]},
	};
	for (const auto& file : files) {
		if (file.byteCount == 0) {
			return 100;  // a sprite did not fit its buffer (a bug here, not D2RLoader)
		}
		D2RL::Resources::RegistrationHandle handle = D2RL::Resources::InvalidHandle;
		const auto r = resources->registerResource(context, &file, &handle);
		if (r != D2RL::Resources::Result::Success) {
			return static_cast<uint32_t>(r);
		}
	}
	return 0;
}
void RegisterProbePanel(const D2RL::PluginContext* context, char* note, size_t size) noexcept {
	const D2RL::ResourceService* resources = nullptr;
	if (context->QueryService(&resources) != D2RL::ServiceQueryResult::Success ||
	    !D2RL::HasResourceServiceField(resources, D2RL::ResourceServiceRequiredSize) ||
	    context->QueryService(&g_panels) != D2RL::ServiceQueryResult::Success ||
	    !D2RL::HasPanelServiceField(g_panels, D2RL::PanelServiceRequiredSize)) {
		g_panels = nullptr;
		std::snprintf(note, size, "no probe panel (no resource or panel service)");
		return;
	}
	const D2RL::Resources::ResourceRegistration resource {
		.structSize = D2RL::Resources::ResourceRegistrationSize,
		.flags      = 0,
		.path       = "data/global/ui/layouts/multispec/ProbePanelhd.json",
		.bytes      = g_layout,
		.byteCount  = BuildProbeLayout(),
	};
	if (resource.byteCount == 0) {
		std::snprintf(note, size, "no probe panel (layout did not fit its buffer)");
		return;
	}
	D2RL::Resources::RegistrationHandle layout = D2RL::Resources::InvalidHandle;
	const auto r = resources->registerResource(context, &resource, &layout);
	if (r != D2RL::Resources::Result::Success) {
		std::snprintf(note, size, "no probe panel (layout resource result %u)", static_cast<uint32_t>(r));
		return;
	}
	const D2RL::Panels::PanelRegistration registration {
		.structSize = D2RL::Panels::PanelRegistrationSize,
		.flags      = D2RL::Panels::PanelFlags::None,  // Escape must reach the tree; dock closes the strip with it
		.localId    = "ProbePanel",
	};
	const auto p = g_panels->registerPanel(context, &registration, &g_probePanel);
	if (p != D2RL::Panels::Result::Success) {
		g_probePanel = D2RL::Panels::InvalidHandle;
		std::snprintf(note, size, "no probe panel (registerPanel result %u)", static_cast<uint32_t>(p));
		return;
	}
	const uint32_t sprites = RegisterArt(resources, context);
	std::snprintf(note, size, "probe panel multispec/ProbePanel (%d slots, %d x %d); art %s (result %u)", ProbeSlots,
	              art::VisibleWidth, art::WingHeight(ProbeSlots), sprites == 0 ? "registered" : "NOT registered", sprites);
}

auto ProbeCommand(D2R::Game::Client*, const D2RL::ConsoleCommandContext* command, void*) noexcept
	-> D2RL::ConsoleCommandResult {
	if (command == nullptr || command->plugin == nullptr) {
		return D2RL::ConsoleCommandResult::Failed;
	}
	const D2RL::PluginContext* ctx = command->plugin;
	const char* args = command->args != nullptr ? command->args : "";
	while (*args == ' ') {
		++args;
	}
	if (std::strncmp(args, "on", 2) == 0 && (args[2] == '\0' || args[2] == ' ')) {
		g_logging.store(true);
		plog::Info("uiprobe: logging UI messages");
		ctx->WriteConsoleMessage("uiprobe: logging UI messages (msprobe off to stop)");
		return D2RL::ConsoleCommandResult::Handled;
	}
	if (std::strncmp(args, "dock", 4) == 0) {
		const bool on = std::strstr(args, "off") == nullptr;
		if (on && (g_probePanel == D2RL::Panels::InvalidHandle || g_uiThreads == nullptr)) {
			ctx->WriteConsoleWarning("uiprobe: no probe panel (see the uiprobe line in the log)");
			return D2RL::ConsoleCommandResult::Failed;
		}
		g_dock.store(on);
		plog::Info("uiprobe: dock %s", on ? "on" : "off");
		ctx->WriteConsoleMessage(on ? "uiprobe: the probe panel follows the skill tree" : "uiprobe: dock off");
		return D2RL::ConsoleCommandResult::Handled;
	}
	if (std::strncmp(args, "off", 3) == 0) {
		g_logging.store(false);
		plog::Info("uiprobe: stopped after %u messages", g_seen.load());
		ctx->WriteConsoleMessage("uiprobe: stopped");
		return D2RL::ConsoleCommandResult::Handled;
	}
	D2RL::Threads::Callback work = nullptr;
	if (std::strncmp(args, "toggle", 6) == 0) {
		work = ToggleTree;
	} else if (std::strncmp(args, "find", 4) == 0) {
		work = FindTree;
	} else if (std::strncmp(args, "panel", 5) == 0) {
		work = ToggleProbePanel;
	}
	if (work == nullptr) {
		ctx->WriteConsoleMessage("usage: msprobe on | off | toggle | find | panel | dock [off]");
		return D2RL::ConsoleCommandResult::InvalidArguments;
	}
	if (g_widgets == nullptr || g_uiThreads == nullptr) {
		ctx->WriteConsoleWarning("uiprobe: D2RLoader has no widget or thread service");
		return D2RL::ConsoleCommandResult::Failed;
	}
	if (g_uiThreads->runOnUiThread(ctx, work, nullptr) != D2RL::Threads::Result::Success) {
		ctx->WriteConsoleWarning("uiprobe: could not queue to the UI thread");
		return D2RL::ConsoleCommandResult::Failed;
	}
	return D2RL::ConsoleCommandResult::Handled;
}

}  // namespace

void InstallUiProbe(const D2RL::PluginContext* context, char* report, size_t size) noexcept {
	if (context->QueryService(&g_events) != D2RL::ServiceQueryResult::Success ||
	    !D2RL::HasSharedEventServiceField(g_events, D2RL::SharedEventServiceRequiredSize)) {
		g_events = nullptr;
		std::snprintf(report, size, "off: D2RLoader has no shared event service");
		return;
	}
	if (context->QueryService(&g_widgets) != D2RL::ServiceQueryResult::Success ||
	    !D2RL::HasWidgetServiceField(g_widgets, D2RL::WidgetServiceRequiredSize)) {
		g_widgets = nullptr;
	}
	if (context->QueryService(&g_uiThreads) != D2RL::ServiceQueryResult::Success ||
	    !D2RL::HasThreadServiceField(g_uiThreads, D2RL::ThreadServiceRequiredSize)) {
		g_uiThreads = nullptr;
	}
	D2RL::SharedEvents::UiMessageListener listener {};
	listener.structSize = D2RL::SharedEvents::UiMessageListenerSize;
	listener.priority = 1000;  // first, so it sees messages another listener consumes
	listener.callback = OnUiMessage;
	D2RL::SharedEvents::ListenerHandle handle {};
	const auto r = g_events->registerUiMessageListener(context, &listener, &handle);
	if (r != D2RL::SharedEvents::Result::Success) {
		std::snprintf(report, size, "off: UI message listener not registered (result %u)", static_cast<uint32_t>(r));
		return;
	}
	D2RL::ConsoleCommandRegistration reg = D2RL::MakeConsoleCommand("msprobe", ProbeCommand,
	                                                                 "MultiSpec UI probe: msprobe on | off | toggle | find.");
	reg.usage = "msprobe on|off|toggle|find|panel|dock [off]";
	reg.scope = D2RL::ConsoleCommandScope::LocalOnly;
	if (!context->RegisterConsoleCommand(reg)) {
		std::snprintf(report, size, "off: the msprobe command could not be registered");
		return;
	}
	char panel[160];
	RegisterProbePanel(context, panel, sizeof(panel));
	std::snprintf(report, size, "on: msprobe (listener idle until msprobe on; widgets %s; %s)",
	              g_widgets != nullptr && g_uiThreads != nullptr ? "yes" : "NO", panel);
}

}  // namespace ms
