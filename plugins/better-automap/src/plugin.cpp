// Better Automap: a D2RLoader plugin for the automap (D2R 3.3.0 build 93847, D2RLoader 1.3.1, PluginSDK 0.3.0).
//
// Game-wide (installed in <game>\d2rloader\plugins\, loads with every mod) and client-only: it changes how the
// automap is shown and controlled, nothing that has to match between players.
//
// What it does at load:
//  - build_guard.h: checks the game image (PE TimeDateStamp). Every address belongs to one image; on any other image
//    every part stays off and says so;
//  - config.h: reads better-automap.toml (each part has its own `enabled` switch);
//  - off_switch.h: reads "better-automap-off.txt" next to the DLL: words naming parts to leave out (bisecting);
//  - installs each part on its own (a refused part never stops the others) and logs one line per part;
//  - registers the console command "better-automap" that prints the state of every part.
#include <D2RLPlugin/api.h>

#include "build_guard.h"
#include "config.h"
#include "d2r.h"
#include "parts.h"
#include "log.h"
#include "off_switch.h"

#include <cstdint>
#include <cstdio>
#include <cstring>

#ifndef BETTER_AUTOMAP_VERSION
#define BETTER_AUTOMAP_VERSION "0.1.0"
#endif

namespace {

constexpr const char* PluginId = "better-automap";
constexpr const char* PluginVersion = BETTER_AUTOMAP_VERSION;

const D2RL::PluginContext* g_context = nullptr;
uint32_t g_stamp = 0;
char g_selfPath[520] = "?";

constexpr D2RL::PluginInfo Info {
	.infoSize    = D2RL::PluginInfoSize,
	.abiVersion  = D2RL_PLUGIN_ABI_VERSION,
	.id          = "better-automap",
	.name        = "Better Automap",
	.version     = BETTER_AUTOMAP_VERSION,
	.author      = "Synpoo",
	.description = "Mini map side, full-screen overlay over the mini map, and mouse wheel zoom on the overlay.",
	.flags       = D2RL::PluginFlags::Client | D2RL::PluginFlags::NativeHooks,
};

// --- Parts --------------------------------------------------------------------------------------------------------
using InstallFn = bool (*)(const D2RL::PluginContext* context, char* report, size_t size) noexcept;
using StatusFn = void (*)(char* out, size_t size) noexcept;

struct Part {
	const char* word;     // off-file word, config table and status label
	bool (*enabled)();    // the part's `enabled` key in better-automap.toml
	InstallFn install;
	StatusFn status;      // live counters, or nullptr
	bool on;
	char report[400];
};

Part g_parts[] = {
	{"minimap", [] { return config::Get().minimapEnabled; }, InstallMinimapSide, nullptr, false, ""},
	{"overlay", [] { return config::Get().overlayEnabled; }, InstallOverlayToggle, StatusOverlayToggle, false, ""},
	{"zoom", [] { return config::Get().zoomEnabled; }, InstallOverlayZoom, StatusOverlayZoom, false, ""},
};

void InstallParts(const D2RL::PluginContext* context, bool imageOk) noexcept {
	for (Part& p : g_parts) {
		if (!imageOk) {
			std::snprintf(p.report, sizeof(p.report), "off: game image differs from the one this plugin was built for");
		} else if (!p.enabled()) {
			std::snprintf(p.report, sizeof(p.report), "off: [%s] enabled = false", p.word);
		} else if (off_switch::Off(p.word)) {
			std::snprintf(p.report, sizeof(p.report), "off: left out by %s-off.txt", PluginId);
		} else {
			p.on = p.install(context, p.report, sizeof(p.report));
		}
		plog::Info("%s %s", p.word, p.report);
	}
}

// --- Console command ----------------------------------------------------------------------------------------------
auto StatusCommand(D2R::Game::Client* client, const D2RL::ConsoleCommandContext* command, void* userData) noexcept
	-> D2RL::ConsoleCommandResult {
	(void)client;
	(void)userData;
	if (command == nullptr || command->plugin == nullptr || g_context == nullptr) {
		return D2RL::ConsoleCommandResult::Failed;
	}
	char line[1100];
	char image[200];
	build_guard::Describe(image, sizeof(image), g_stamp, g_context->buildVersion);
	std::snprintf(line, sizeof(line), "Better Automap %s | %s | file %s", PluginVersion, image, g_selfPath);
	command->plugin->WriteConsoleMessage(line);
	std::snprintf(line, sizeof(line), "config: %s%s%s", config::Source(), config::Problems()[0] != '\0' ? " | " : "",
	              config::Problems());
	command->plugin->WriteConsoleMessage(line);
	if (off_switch::Words()[0] != '\0') {
		std::snprintf(line, sizeof(line), "off-file: %s", off_switch::Words());
		command->plugin->WriteConsoleMessage(line);
	}
	for (const Part& p : g_parts) {
		char live[300] = "";
		if (p.on && p.status != nullptr) {
			p.status(live, sizeof(live));
		}
		std::snprintf(line, sizeof(line), "%s %s%s%s", p.word, p.report, live[0] != '\0' ? " | " : "", live);
		command->plugin->WriteConsoleMessage(line);
		plog::Info("%s", line);  // also in the plugin log, for reports
	}
	return D2RL::ConsoleCommandResult::Handled;
}

}  // namespace

D2RL_PLUGIN_EXPORT auto D2RLoaderGetPluginInfo() noexcept -> const D2RL::PluginInfo* {
	return &Info;
}

D2RL_PLUGIN_EXPORT auto D2RLoaderLoadPlugin(const D2RL::PluginContext* context) noexcept -> bool {
	if (context == nullptr) {
		return false;
	}
	g_context = context;
	plog::Init(context, PluginId);
	g_stamp = build_guard::ImageStamp(context->exeBase);
	const bool imageOk = build_guard::Matches(g_stamp);
	if (context->pluginPath != nullptr) {
		std::snprintf(g_selfPath, sizeof(g_selfPath), "%ls", context->pluginPath);
	}
	char image[200];
	build_guard::Describe(image, sizeof(image), g_stamp, context->buildVersion);
	plog::Info("%s loaded (%s, file %s)", PluginVersion, image, g_selfPath);
	config::Load(context);
	plog::Info("config: %s", config::Source());
	if (config::Problems()[0] != '\0') {
		plog::Warn("config: %s", config::Problems());
	}
	off_switch::Read();
	if (off_switch::Words()[0] != '\0') {
		plog::Info("%s-off.txt leaves out: %s", PluginId, off_switch::Words());
	}
	if (imageOk) {
		d2r::Bind(context->exeBase);
	} else {
		plog::Warn("not the expected game image: every part stays off");
	}
	InstallParts(context, imageOk);
	return context->RegisterConsoleCommand(PluginId, StatusCommand, "Status of Better Automap and its parts.");
}

D2RL_PLUGIN_EXPORT void D2RLoaderUnloadPlugin() noexcept {
	g_context = nullptr;
}
