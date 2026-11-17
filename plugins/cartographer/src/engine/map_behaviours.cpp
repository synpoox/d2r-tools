#include "engine/map_behaviours.h"

#include "log.h"

#include <D2RLPlugin/plugin_communication.h>

#include <windows.h>

#include <atomic>
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace carto {

namespace beh = cartographer::behaviours;

namespace {

std::mutex g_lock;
std::map<int, beh::MapBehaviours> g_maps;  // guarded by g_lock; only maps with a behaviour set are kept
std::atomic<bool> g_any{false};

struct Subscriber {
	uint32_t handle;
	std::string pluginId;
	MapDropCallbackV1 callback;
	void* user;
};
std::vector<Subscriber> g_subscribers;  // guarded by g_lock
uint32_t g_nextHandle = 1;

auto __cdecl Register(const char* pluginId, MapDropCallbackV1 callback, void* user, uint32_t* handle) noexcept -> bool {
	try {
		if (pluginId == nullptr || callback == nullptr || handle == nullptr) {
			return false;
		}
		std::lock_guard<std::mutex> lock(g_lock);
		g_subscribers.push_back({g_nextHandle, pluginId, callback, user});
		*handle = g_nextHandle++;
		return true;
	} catch (...) {
		return false;
	}
}

auto __cdecl Unregister(uint32_t handle) noexcept -> bool {
	try {
		std::lock_guard<std::mutex> lock(g_lock);
		for (auto it = g_subscribers.begin(); it != g_subscribers.end(); ++it) {
			if (it->handle == handle) {
				g_subscribers.erase(it);
				return true;
			}
		}
		return false;
	} catch (...) {
		return false;
	}
}

const MapDropServiceV1 g_service = {sizeof(MapDropServiceV1), MapDropServiceVersion, &Register, &Unregister};

}  // namespace

void SetMapBehaviours(int level, const beh::MapBehaviours& b) {
	std::lock_guard<std::mutex> lock(g_lock);
	if (beh::any(b)) {
		g_maps[level] = b;
	} else {
		g_maps.erase(level);
	}
	g_any = !g_maps.empty();
}

auto GetMapBehaviours(int level, beh::MapBehaviours* out) -> bool {
	std::lock_guard<std::mutex> lock(g_lock);
	const auto it = g_maps.find(level);
	if (it == g_maps.end()) {
		return false;
	}
	*out = it->second;
	return true;
}

auto MapBehavioursAny() noexcept -> bool {
	return g_any.load(std::memory_order_relaxed);
}

void MapBehavioursGameLeft() noexcept {
	try {
		std::lock_guard<std::mutex> lock(g_lock);
		g_maps.clear();
		g_any = false;
	} catch (...) {
	}
}

void MapBehavioursTestOpened(int level) {
	char spec[512] = {};
	if (GetEnvironmentVariableA("CARTOGRAPHER_TEST_BEHAVIOURS", spec, sizeof(spec)) == 0) {
		return;
	}
	beh::MapBehaviours b;
	std::string why;
	if (!beh::parseSpec(spec, &b, &why)) {
		plog::Warn("map behaviours test: CARTOGRAPHER_TEST_BEHAVIOURS='%s' is not valid (%s); ignored", spec, why.c_str());
		return;
	}
	SetMapBehaviours(level, b);
	plog::Warn("map behaviours test: level %d set from CARTOGRAPHER_TEST_BEHAVIOURS='%s' (a game check only)", level, spec);
}

void MapDropNotify(void* item, int level, const beh::MapBehaviours& b) noexcept {
	try {
		std::vector<Subscriber> copy;
		{
			std::lock_guard<std::mutex> lock(g_lock);
			copy = g_subscribers;
		}
		const MapDropStatsV1 stats = {sizeof(MapDropStatsV1), level, b.dropBonusPct, b.etherealPct, b.socketedPct, b.extraBoss};
		for (const Subscriber& s : copy) {
			s.callback(item, &stats, s.user);
		}
	} catch (...) {
		plog::Error("map drop: a callback failed");
	}
}

void PublishMapDropService(const D2RL::PluginContext* context) noexcept {
	try {
		const D2RL::PluginCommunicationService* communication = nullptr;
		if (context == nullptr || context->QueryService(&communication) != D2RL::ServiceQueryResult::Success ||
		    !D2RL::HasPluginCommunicationServiceField(communication, D2RL::PluginCommunicationServiceRequiredSize)) {
			plog::Info("map-drop service: the loader has no communication service; cartographer.map-drop.v1 is not offered");
			return;
		}
		const D2RL::PluginCommunication::PublishServiceRequest request{
		    .structSize = D2RL::PluginCommunication::PublishServiceRequestSize,
		    .name = MapDropServiceName,
		    .serviceVersion = MapDropServiceVersion,
		    .tableSize = sizeof(g_service),
		    .table = &g_service,
		};
		const bool ok = communication->publishService(context, &request) == D2RL::PluginCommunication::Result::Success;
		plog::Info(ok ? "map-drop service: cartographer.map-drop.v1 published" : "map-drop service: cartographer.map-drop.v1 was refused by the loader");
	} catch (...) {
		plog::Error("map-drop service: publishing failed");
	}
}

}  // namespace carto
