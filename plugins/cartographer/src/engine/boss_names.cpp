#include "boss_names.h"

#include "log.h"
#include "mapname.h"

#include <mutex>
#include <string>
#include <vector>

namespace carto {

namespace {

namespace mn = cartographer::mapname;

constexpr const char* StringsPath = "data/local/lng/strings/d2rloader/cartographer/bosses.json";

std::mutex g_lock;
const D2RL::PluginContext* g_context = nullptr;
const D2RL::ResourceService* g_resources = nullptr;
D2RL::Resources::RegistrationHandle g_handle = D2RL::Resources::InvalidHandle;
std::vector<std::string> g_wanted;  // the bosses.txt names of the tables set last
std::vector<std::string> g_held;    // what the loader holds
bool g_registered = false;

// g_lock held.
void ApplyLocked() noexcept {
	try {
		if (g_resources == nullptr || (g_registered && g_held == g_wanted)) {
			return;
		}
		if (g_registered) {
			(void)g_resources->unregisterResource(g_context, g_handle);
			g_handle = D2RL::Resources::InvalidHandle;
			g_registered = false;
			g_held.clear();
		}
		if (g_wanted.empty()) {
			return;
		}
		const std::string json = mn::BossStringsJson(g_wanted);
		D2RL::Resources::ResourceRegistration resource{};
		resource.structSize = D2RL::Resources::ResourceRegistrationSize;
		resource.path = StringsPath;
		resource.bytes = json.data();
		resource.byteCount = json.size();
		D2RL::Resources::RegistrationHandle handle = D2RL::Resources::InvalidHandle;
		const D2RL::Resources::Result result = g_resources->registerResource(g_context, &resource, &handle);
		if (result != D2RL::Resources::Result::Success) {
			plog::Warn("boss names: D2RLoader refused the strings file (result %u); the bosses show the game's text for an unknown key", static_cast<uint32_t>(result));
			return;
		}
		g_handle = handle;
		g_held = g_wanted;
		g_registered = true;
		plog::Info("boss names: registered %zu boss name(s) as strings", g_held.size());
	} catch (...) {
		plog::Warn("boss names: building the strings file failed");
	}
}

}  // namespace

void InstallBossNames(const D2RL::PluginContext* context) {
	std::lock_guard<std::mutex> lock(g_lock);
	g_context = context;
	g_resources = nullptr;
	if (context->QueryService(&g_resources) != D2RL::ServiceQueryResult::Success ||
	    !D2RL::HasResourceServiceField(g_resources, D2RL::ResourceServiceRequiredSize)) {
		g_resources = nullptr;
		plog::Warn("boss names: D2RLoader has no Resource service of the expected version; the bosses show the game's text for an unknown key");
		return;
	}
	ApplyLocked();
}

void BossNamesSetTables(const std::shared_ptr<const Tables>& tables) {
	std::vector<std::string> names;
	if (tables != nullptr) {
		for (const BossDef& boss : tables->bosses) {
			names.push_back(boss.name);
		}
	}
	std::lock_guard<std::mutex> lock(g_lock);
	g_wanted = std::move(names);
	ApplyLocked();  // before InstallBossNames there is no service yet: Install registers what was set
}

auto BossNamesSummary() -> std::string {
	std::lock_guard<std::mutex> lock(g_lock);
	if (g_resources == nullptr) {
		return "Boss names: off (no D2RLoader Resource service)";
	}
	if (g_wanted.empty()) {
		return "Boss names: none (no bosses.txt entries)";
	}
	return g_registered ? "Boss names: " + std::to_string(g_held.size()) + " registered (written, not verified in game)" : "Boss names: not registered";
}

void BossNamesUnload() noexcept {
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
	g_held.clear();
}

}  // namespace carto
