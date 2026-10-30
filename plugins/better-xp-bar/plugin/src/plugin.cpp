// Better XP Bar: D2RLoader plugin entry point. Based on d2rAI-toolkit's plugin-template.
//
// Target: D2R 3.3.0 build 93847 under D2RLoader 1.3.1 (image stamp 0x6ab3782c), PluginSDK 0.3.0.
//
// What it does at load:
//  - declares itself Client (display only: not recorded in character plugin history, no TCP/IP match needed).
//    Unlike the template it is not ModScopedOnly: it may live in <game>/d2rloader/plugins/ and run with any mod;
//  - log.h: one log line for the plugin (version, D2R version, image stamp, mod, this DLL's path) and one per part;
//  - build_guard.h: checks the game image (PE TimeDateStamp). Every RVA belongs to one image; on any other image the
//    engine parts stay off and say so;
//  - off_switch.h: reads "<plugin>-off.txt" next to the DLL: words naming parts to leave out;
//  - env_guard.h: names a second copy of this DLL and the other plugins (no game address: runs on any image);
//  - installs each part and logs one line per part;
//  - registers the console command "<plugin>" that prints the state of every part.
//
// Adding a part: write Install<Part>(context, report, size) in its own file, add a row to g_parts with a one-word name
// (the word the off-file uses) and whether it needs the expected image. It shows up in the log and the status command.
#include <D2RLPlugin/api.h>

#include "build_guard.h"
#include "display.h"
#include "env_guard.h"
#include "log.h"
#include "off_switch.h"

#include <cstdint>
#include <cstdio>
#include <cstring>

#ifndef BXB_PLUGIN_NAME
#define BXB_PLUGIN_NAME "better-xp-bar"
#endif
#ifndef BXB_PLUGIN_VERSION
#define BXB_PLUGIN_VERSION "0.1.0"
#endif

namespace {

constexpr const char* PluginName = BXB_PLUGIN_NAME;
constexpr const char* PluginVersion = BXB_PLUGIN_VERSION;

const D2RL::PluginContext* g_context = nullptr;
uint32_t g_stamp = 0;
char g_selfPath[520] = "?";

constexpr D2RL::PluginInfo Info {
	.infoSize    = D2RL::PluginInfoSize,
	.abiVersion  = D2RL_PLUGIN_ABI_VERSION,
	.id          = BXB_PLUGIN_NAME,
	.name        = "Better XP Bar",
	.version     = BXB_PLUGIN_VERSION,
	.author      = "better-xp-bar contributors",
	.description = "XP display: level progress, session XP, XP/hour and time to next level.",
	.flags       = D2RL::PluginFlags::Client,
};

// --- Parts --------------------------------------------------------------------------------------------------------
using InstallFn = void (*)(const D2RL::PluginContext* context, char* report, size_t size) noexcept;

struct Part {
	const char* word;   // off-file word and status label
	bool engine;        // uses game addresses: installed only on the expected image
	InstallFn install;
	char report[512];
};

void InstallEnvironment(const D2RL::PluginContext* context, char* report, size_t size) noexcept {
	StartEnvironmentGuard(context, "");
	EnvironmentStatus(report, size);
}

// Order matters: overlay needs xp. Hotkeys use no game address (they only queue work).
Part g_parts[] = {
	{"envguard", false, InstallEnvironment, ""},
	{"xp", true, display::InstallTracking, ""},
	{"overlay", true, display::InstallOverlay, ""},
	{"hotkeys", false, display::InstallHotkeys, ""},
};

void InstallParts(const D2RL::PluginContext* context, bool imageOk) noexcept {
	for (Part& p : g_parts) {
		if (off_switch::Off(p.word)) {
			std::snprintf(p.report, sizeof(p.report), "off: left out by %s-off.txt", PluginName);
		} else if (p.engine && !imageOk) {
			std::snprintf(p.report, sizeof(p.report), "off: game image differs from the one this plugin was built for");
		} else {
			p.install(context, p.report, sizeof(p.report));
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
	const char* args = command->args != nullptr ? command->args : "";
	while (*args == ' ') {
		++args;
	}
	if (std::strncmp(args, "reset", 5) == 0) {
		display::RequestReset();
		command->plugin->WriteConsoleMessage("better-xp-bar: session reset");
		return D2RL::ConsoleCommandResult::Handled;
	}
	if (std::strncmp(args, "toggle", 6) == 0) {
		display::RequestToggle();
		command->plugin->WriteConsoleMessage("better-xp-bar: display toggled");
		return D2RL::ConsoleCommandResult::Handled;
	}
	if (std::strncmp(args, "reload", 6) == 0) {
		display::RequestReload();
		command->plugin->WriteConsoleMessage("better-xp-bar: config reload queued (see the plugin log)");
		return D2RL::ConsoleCommandResult::Handled;
	}
	if (std::strncmp(args, "test", 4) == 0) {
		display::RequestTest();
		command->plugin->WriteConsoleMessage("better-xp-bar: drawing a magenta test rectangle in the screen centre for 15 s");
		return D2RL::ConsoleCommandResult::Handled;
	}
	if (std::strncmp(args, "show", 4) == 0) {
		display::Lines(line, sizeof(line));
		command->plugin->WriteConsoleMessage(line);
		plog::Info("show: %s", line);
		return D2RL::ConsoleCommandResult::Handled;
	}
	if (args[0] != '\0' && std::strncmp(args, "status", 6) != 0) {
		command->plugin->WriteConsoleMessage("usage: better-xp-bar [status|show|reset|toggle|reload|test]");
		return D2RL::ConsoleCommandResult::InvalidArguments;
	}
	char image[200];
	build_guard::Describe(image, sizeof(image), g_stamp, g_context->buildVersion);
	std::snprintf(line, sizeof(line), "%s %s | %s | mod %s | file %s", PluginName, PluginVersion, image,
	              g_context->activeMod != nullptr ? g_context->activeMod : "?", g_selfPath);
	command->plugin->WriteConsoleMessage(line);
	if (off_switch::Words()[0] != '\0') {
		std::snprintf(line, sizeof(line), "off-file: %s", off_switch::Words());
		command->plugin->WriteConsoleMessage(line);
	}
	for (const Part& p : g_parts) {
		// live state where the part has one, else its install report
		char state[sizeof(p.report) + 64];
		if (std::strcmp(p.word, "envguard") == 0 && !off_switch::Off(p.word)) {
			EnvironmentStatus(state, sizeof(state));
		} else if (std::strcmp(p.word, "xp") == 0 && std::strncmp(p.report, "on", 2) == 0) {
			char live[900];
			display::Status(live, sizeof(live));
			std::snprintf(state, sizeof(state), "%s | %s", p.report, live);
		} else {
			std::snprintf(state, sizeof(state), "%s", p.report);
		}
		std::snprintf(line, sizeof(line), "%s %s", p.word, state);
		command->plugin->WriteConsoleMessage(line);
	}
	OtherPlugins(line, sizeof(line));
	command->plugin->WriteConsoleMessage(line);
	plog::Info("%s", line);  // also in the plugin log, for crash reports
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
	plog::Init(context, PluginName);
	g_stamp = build_guard::ImageStamp(context->exeBase);
	const bool imageOk = build_guard::Matches(g_stamp);
	if (context->pluginPath != nullptr) {
		std::snprintf(g_selfPath, sizeof(g_selfPath), "%ls", context->pluginPath);
	}
	char image[200];
	build_guard::Describe(image, sizeof(image), g_stamp, context->buildVersion);
	plog::Info("%s loaded (mod %s, %s, file %s)", PluginVersion, context->activeMod != nullptr ? context->activeMod : "?", image, g_selfPath);
	off_switch::Read();
	if (off_switch::Words()[0] != '\0') {
		plog::Info("%s-off.txt leaves out: %s", PluginName, off_switch::Words());
	}
	if (!imageOk) {
		plog::Warn("not the expected game image: every engine part stays off");
	}
	display::LoadConfig(context);
	InstallParts(context, imageOk);
	auto cmd = D2RL::MakeConsoleCommand(PluginName, StatusCommand, "Better XP Bar: status, show, reset, toggle, reload.");
	cmd.usage = "better-xp-bar [status|show|reset|toggle|reload|test]";
	return context->RegisterConsoleCommand(cmd);
}

D2RL_PLUGIN_EXPORT void D2RLoaderUnloadPlugin() noexcept {
	g_context = nullptr;
}
