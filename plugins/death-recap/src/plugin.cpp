// Development-only entry point. No native hook is installed until the capture
// contracts in docs/reverse-engineering/death-recap.md are independently validated.
#include <D2RLPlugin/api.h>
#include "crash_guard.h"
#include "log.h"

namespace {
constexpr const char* PluginId = "death-recap";
constexpr const char* Version = "0.1.0-alpha.1";
constexpr const char* Status = "Death Recap: capture unavailable (native contracts not validated).";
constexpr D2RL::PluginInfo Info {
	.infoSize = D2RL::PluginInfoSize,
	.abiVersion = D2RL_PLUGIN_ABI_VERSION,
	.id = PluginId,
	.name = "Death Recap",
	.version = Version,
	.author = "Synpoo",
	.description = "Reports the combat events leading to your character's death.",
	.flags = D2RL::PluginFlags::Client,
};
auto StatusCommand(D2R::Game::Client*, const D2RL::ConsoleCommandContext* command, void*) noexcept
	-> D2RL::ConsoleCommandResult {
	if (command == nullptr || command->plugin == nullptr) return D2RL::ConsoleCommandResult::Failed;
	command->plugin->WriteConsoleMessage(Status);
	return D2RL::ConsoleCommandResult::Handled;
}
}  // namespace

D2RL_PLUGIN_EXPORT auto D2RLoaderGetPluginInfo() noexcept -> const D2RL::PluginInfo* {
	return &Info;
}
D2RL_PLUGIN_EXPORT auto D2RLoaderLoadPlugin(const D2RL::PluginContext* context) noexcept -> bool {
	if (context == nullptr) return false;
	plog::Init(context, PluginId);
	crash_guard::Install(Version, context->exeBase);
	char image[200];
	crash_guard::Describe(image, sizeof(image), D2RL::GetBuildVersion(context));
	plog::Info("%s loaded (%s)", Version, image);
	plog::Warn("%s", Status);
	if (!context->RegisterConsoleCommand(PluginId, StatusCommand, "Death Recap capture status.")) {
		crash_guard::Remove();
		return false;
	}
	return true;
}
D2RL_PLUGIN_EXPORT void D2RLoaderUnloadPlugin() noexcept {
	crash_guard::Remove();
}
