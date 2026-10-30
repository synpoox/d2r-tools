// Quick NPC: a D2RLoader plugin built from d2rAI-toolkit's plugin-template.
//
// Target: D2R 3.3.0 build 93847 under D2RLoader 1.3.1 (image 0x6ab3782c), PluginSDK 0.3.0.
//
// What it does at load (the template's lifecycle):
//  - declares itself Client (a client-side UI convenience: not recorded in the character's plugin history, no
//    multiplayer version match) and NativeHooks (patches game code). It is a global plugin: installed in
//    <game>/d2rloader/plugins/, it loads with every mod and with the unmodded game;
//  - log.h: one log line for the plugin (version, D2R version, image stamp, mod, this DLL's path) and one per part;
//  - build_guard.h: checks the game image (PE TimeDateStamp). On any other image the engine part stays off and says so;
//  - off_switch.h: reads "<plugin>-off.txt" next to the DLL: words naming parts to leave out;
//  - env_guard.h: warns when a second copy of this plugin is loaded and lists the other plugins (no mod check: global);
//  - installs each part and logs one line per part;
//  - registers the console command "<plugin>": status of every part; "<plugin> off" / "<plugin> on" switches the
//    feature at runtime; "<plugin> reload" re-reads <game>/d2rloader/config/<plugin>.toml.
#include <D2RLPlugin/api.h>

#include "build_guard.h"
#include "env_guard.h"
#include "log.h"
#include "off_switch.h"
#include "quick_npc.h"
#include "quick_npc_logic.h"

#include <cstdint>
#include <cstdio>
#include <cstring>

#ifndef MODKIT_PLUGIN_NAME
#define MODKIT_PLUGIN_NAME "quick-npc"
#endif
#ifndef MODKIT_PLUGIN_VERSION
#define MODKIT_PLUGIN_VERSION "1.0.0"
#endif

namespace {

constexpr const char* PluginName = MODKIT_PLUGIN_NAME;
constexpr const char* PluginVersion = MODKIT_PLUGIN_VERSION;

const D2RL::PluginContext* g_context = nullptr;
uint32_t g_stamp = 0;
char g_selfPath[520] = "?";

constexpr D2RL::PluginInfo Info {
	.infoSize    = D2RL::PluginInfoSize,
	.abiVersion  = D2RL_PLUGIN_ABI_VERSION,
	.id          = MODKIT_PLUGIN_NAME,
	.name        = "Quick NPC",
	.version     = MODKIT_PLUGIN_VERSION,
	.author      = "Synpoo",
	.description = "Hold a hotkey and left-click an NPC to skip its dialog: trade, identify, resurrect or travel.",
	.flags       = D2RL::PluginFlags::Client | D2RL::PluginFlags::NativeHooks,
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
	StartEnvironmentGuard(context, "");  // global plugin: no mod to check
	EnvironmentStatus(report, size);
}

void InstallFeature(const D2RL::PluginContext* context, char* report, size_t size) noexcept {
	(void)InstallQuickNpc(context, report, size);
}

Part g_parts[] = {
	{"envguard", false, InstallEnvironment, ""},
	{"quicknpc", true, InstallFeature, ""},
};

void InstallParts(const D2RL::PluginContext* context, bool imageOk) noexcept {
	for (Part& p : g_parts) {
		switch (quick_npc::DecidePart(off_switch::Off(p.word), p.engine, imageOk)) {
		case quick_npc::PartDecision::OffByFile:
			std::snprintf(p.report, sizeof(p.report), "off: left out by %s-off.txt", PluginName);
			break;
		case quick_npc::PartDecision::OffImage:
			std::snprintf(p.report, sizeof(p.report), "off: game image differs from the one this plugin was built for");
			break;
		case quick_npc::PartDecision::Install:
			p.install(context, p.report, sizeof(p.report));
			break;
		}
		plog::Info("%s %s", p.word, p.report);
	}
}

// First word of the command's arguments, lower case ("" when none).
void FirstArg(const D2RL::ConsoleCommandContext* command, char* out, size_t size) noexcept {
	out[0] = '\0';
	if (command->args == nullptr || size == 0) {
		return;
	}
	const char* a = command->args;
	const char* end = a + command->argsLength;
	while (a < end && *a == ' ') {
		++a;
	}
	size_t n = 0;
	while (a < end && *a != ' ' && *a != '\0' && n + 1 < size) {
		const char c = *a++;
		out[n++] = (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
	}
	out[n] = '\0';
}

// --- Console command ----------------------------------------------------------------------------------------------
auto StatusCommand(D2R::Game::Client* client, const D2RL::ConsoleCommandContext* command, void* userData) noexcept
	-> D2RL::ConsoleCommandResult {
	(void)client;
	(void)userData;
	if (command == nullptr || command->plugin == nullptr || g_context == nullptr) {
		return D2RL::ConsoleCommandResult::Failed;
	}
	char arg[16];
	FirstArg(command, arg, sizeof(arg));
	if (std::strcmp(arg, "on") == 0 || std::strcmp(arg, "off") == 0) {
		SetQuickNpcEnabled(arg[1] == 'n');
		plog::Info("quicknpc switched %s from the console", QuickNpcEnabled() ? "on" : "off");
	} else if (std::strcmp(arg, "reload") == 0) {
		command->plugin->WriteConsoleMessage(ReloadQuickNpcConfig() ? "quick-npc.toml reloaded"
		                                                            : "quick-npc.toml could not be read: defaults in use (see the log)");
	}
	char line[1100];
	char image[200];
	build_guard::Describe(image, sizeof(image), g_stamp, g_context->buildVersion);
	std::snprintf(line, sizeof(line), "%s %s | %s | mod %s | file %s", PluginName, PluginVersion, image,
	              g_context->activeMod != nullptr && g_context->activeMod[0] != '\0' ? g_context->activeMod : "(none)", g_selfPath);
	command->plugin->WriteConsoleMessage(line);
	std::snprintf(line, sizeof(line), "config %ls | log %ls", g_context->pluginConfigPath != nullptr ? g_context->pluginConfigPath : L"?",
	              g_context->pluginLogPath != nullptr ? g_context->pluginLogPath : L"?");
	command->plugin->WriteConsoleMessage(line);
	if (off_switch::Words()[0] != '\0') {
		std::snprintf(line, sizeof(line), "off-file: %s", off_switch::Words());
		command->plugin->WriteConsoleMessage(line);
	}
	for (const Part& p : g_parts) {
		// live state where the part has one, else its install report
		char state[sizeof(p.report) + 300];
		if (std::strcmp(p.word, "envguard") == 0 && !off_switch::Off(p.word)) {
			EnvironmentStatus(state, sizeof(state));
		} else if (std::strcmp(p.word, "quicknpc") == 0 && std::strncmp(p.report, "on", 2) == 0) {
			char live[400];
			QuickNpcStatus(live, sizeof(live));
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
	InstallParts(context, imageOk);
	return context->RegisterConsoleCommand(PluginName, StatusCommand,
	                                       "Quick NPC status; 'quick-npc off' / 'on' switches it, 'quick-npc reload' re-reads quick-npc.toml.");
}

D2RL_PLUGIN_EXPORT void D2RLoaderUnloadPlugin() noexcept {
	ShutdownQuickNpc();
	g_context = nullptr;
}
