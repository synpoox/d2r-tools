#include "engine/arena_service.h"

#include "engine/outdoor_hooks.h"
#include "log.h"

#include <D2RLPlugin/plugin_communication.h>

#include <mutex>

namespace carto {

namespace {

std::mutex g_lock;
std::shared_ptr<const Tables> g_tables;

auto __cdecl IsArena(int32_t levelId) noexcept -> bool {
	try {
		const std::shared_ptr<const Tables> tables = ArenaServiceTables();
		if (tables == nullptr) {
			return false;
		}
		if (!ArenaBuilt(levelId)) {
			return false;  // the game builds that level itself: it is no arena
		}
		for (const ArenaDef& a : tables->arenas) {
			if (a.level == levelId) {
				return true;
			}
		}
	} catch (...) {
	}
	return false;
}

auto __cdecl FindSpot(int32_t levelId, const char* name, ArenaSpotV1* out) noexcept -> bool {
	try {
		if (name == nullptr || out == nullptr || out->structSize < sizeof(ArenaSpotV1)) {
			return false;
		}
		const std::shared_ptr<const Tables> tables = ArenaServiceTables();
		const ArenaSpotDef* spot = tables != nullptr && ArenaBuilt(levelId) ? FindArenaSpot(tables->arenas, levelId, name) : nullptr;
		if (spot == nullptr) {
			return false;
		}
		out->subX = spot->subX;
		out->subY = spot->subY;
		out->roomX = spot->roomX;
		out->roomY = spot->roomY;
		out->roomW = spot->roomW;
		out->roomH = spot->roomH;
		return true;
	} catch (...) {
		return false;
	}
}

const ArenaServiceV1 g_service = {sizeof(ArenaServiceV1), ArenaServiceVersion, &IsArena, &FindSpot};

}  // namespace

void ArenaServiceSetTables(std::shared_ptr<const Tables> tables) {
	std::lock_guard<std::mutex> lock(g_lock);
	g_tables = std::move(tables);
}

auto ArenaServiceTables() -> std::shared_ptr<const Tables> {
	std::lock_guard<std::mutex> lock(g_lock);
	return g_tables;
}

void PublishArenaService(const D2RL::PluginContext* context) noexcept {
	try {
		const std::shared_ptr<const Tables> tables = ArenaServiceTables();
		if (tables == nullptr || tables->arenas.empty()) {
			return;  // no arena row: nothing to ask about, so nothing is offered (a consumer finds no service)
		}
		const D2RL::PluginCommunicationService* communication = nullptr;
		if (context == nullptr || context->QueryService(&communication) != D2RL::ServiceQueryResult::Success ||
		    !D2RL::HasPluginCommunicationServiceField(communication, D2RL::PluginCommunicationServiceRequiredSize)) {
			plog::Info("arena service: the loader has no communication service; cartographer.arena.v1 is not offered");
			return;
		}
		const D2RL::PluginCommunication::PublishServiceRequest request{
		    .structSize = D2RL::PluginCommunication::PublishServiceRequestSize,
		    .name = ArenaServiceName,
		    .serviceVersion = ArenaServiceVersion,
		    .tableSize = sizeof(g_service),
		    .table = &g_service,
		};
		const D2RL::PluginCommunication::Result result = communication->publishService(context, &request);
		plog::Info("arena service: cartographer.arena.v1 %s", result == D2RL::PluginCommunication::Result::Success ? "published" : "was refused by the loader");
	} catch (...) {
		plog::Error("arena service: publishing failed");
	}
}

}  // namespace carto
