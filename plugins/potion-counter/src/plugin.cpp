// potion-counter: D2RLoader plugin entry point. Client-only and global: it hooks nothing, moves no item and changes no game data.
//
// Console command "potion-counter" prints the live state.
#include <D2RLPlugin/api.h>

#include "counter.h"
#include "crash_guard.h"
#include "log.h"

#include <cstdio>

namespace {

constexpr const char* PluginName = "potion-counter";
constexpr const char* PluginVersion = "1.0.0";  // PluginInfo::version too; CHANGELOG.md

const D2RL::PluginContext* g_context = nullptr;

constexpr D2RL::PluginInfo Info {
	.infoSize    = D2RL::PluginInfoSize,
	.abiVersion  = D2RL_PLUGIN_ABI_VERSION,
	.id          = "potion-counter",
	.name        = "Potion Counter",
	.version     = PluginVersion,
	.author      = "Synpoo",
	.description = "Shows a picture of each belt potion or scroll in a row of squares above the belt.",
	.flags       = D2RL::PluginFlags::Client,
};

auto StatusCommand(D2R::Game::Client* client, const D2RL::ConsoleCommandContext* command, void* userData) noexcept
	-> D2RL::ConsoleCommandResult {
	(void)client;
	(void)userData;
	if (command == nullptr || command->plugin == nullptr) {
		return D2RL::ConsoleCommandResult::Failed;
	}
	char image[200];
	crash_guard::Describe(image, sizeof(image), g_context != nullptr ? D2RL::GetBuildVersion(g_context) : nullptr);
	char state[300];
	CounterStatus(state, sizeof(state));
	char line[600];
	std::snprintf(line, sizeof(line), "%s %s | %s | %s", PluginName, PluginVersion, image, state);
	command->plugin->WriteConsoleMessage(line);
	plog::Info("%s", line);
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
	crash_guard::Install(PluginVersion, context->exeBase);
	char image[200];
	crash_guard::Describe(image, sizeof(image), D2RL::GetBuildVersion(context));
	plog::Info("%s loaded (mod %s, %s)", PluginVersion, context->activeMod != nullptr ? context->activeMod : "none", image);
	char report[300];
	(void)InstallCounter(context, report, sizeof(report));
	plog::Info("counter %s", report);
	// Stay loaded even when the part is off, so the status command can say why.
	return context->RegisterConsoleCommand(PluginName, StatusCommand, "Status of Potion Counter.");
}

D2RL_PLUGIN_EXPORT void D2RLoaderUnloadPlugin() noexcept {
	crash_guard::Remove();
	UninstallCounter();
	g_context = nullptr;
}
