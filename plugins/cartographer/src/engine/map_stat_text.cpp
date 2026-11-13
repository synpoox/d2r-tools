#include "map_stat_text.h"

#include "log.h"
#include "mapname.h"

#include <mutex>

namespace carto {

namespace {

constexpr const char* StringsPath = "data/local/lng/strings/d2rloader/cartographer/stats.json";

std::mutex g_lock;
const D2RL::PluginContext* g_context = nullptr;
const D2RL::ResourceService* g_resources = nullptr;
D2RL::Resources::RegistrationHandle g_handle = D2RL::Resources::InvalidHandle;
bool g_registered = false;

}  // namespace

void InstallMapStatText(const D2RL::PluginContext* context) {
	std::lock_guard<std::mutex> lock(g_lock);
	g_context = context;
	g_resources = nullptr;
	if (context->QueryService(&g_resources) != D2RL::ServiceQueryResult::Success ||
	    !D2RL::HasResourceServiceField(g_resources, D2RL::ResourceServiceRequiredSize)) {
		g_resources = nullptr;
		plog::Warn("map stat text: D2RLoader has no Resource service of the expected version; the map level shows the game's text for an unknown key");
		return;
	}
	try {
		const std::string json = cartographer::mapname::MapStatStringsJson();
		D2RL::Resources::ResourceRegistration resource{};
		resource.structSize = D2RL::Resources::ResourceRegistrationSize;
		resource.path = StringsPath;
		resource.bytes = json.data();
		resource.byteCount = json.size();
		D2RL::Resources::RegistrationHandle handle = D2RL::Resources::InvalidHandle;
		const D2RL::Resources::Result result = g_resources->registerResource(context, &resource, &handle);
		if (result != D2RL::Resources::Result::Success) {
			plog::Warn("map stat text: D2RLoader refused the strings file (result %u)", static_cast<uint32_t>(result));
			return;
		}
		g_handle = handle;
		g_registered = true;
		plog::Info("map stat text: registered the map level text");
	} catch (...) {
		plog::Warn("map stat text: building the strings file failed");
	}
}

auto MapStatTextSummary() -> std::string {
	std::lock_guard<std::mutex> lock(g_lock);
	return std::string("Map level text: ") + (g_registered ? "registered" : "not registered");
}

void MapStatTextUnload() noexcept {
	std::lock_guard<std::mutex> lock(g_lock);
	try {
		if (g_registered && g_resources != nullptr) {
			(void)g_resources->unregisterResource(g_context, g_handle);
		}
	} catch (...) {
	}
	g_registered = false;
	g_handle = D2RL::Resources::InvalidHandle;
	g_resources = nullptr;
}

}  // namespace carto
