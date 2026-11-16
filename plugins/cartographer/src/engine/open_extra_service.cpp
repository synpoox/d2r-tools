#include "engine/open_extra_service.h"

#include "cartographer_open_extra_v1.h"
#include "engine/portal.h"
#include "log.h"

#include <D2RLPlugin/plugin_communication.h>

namespace carto {

namespace {

auto __cdecl OpenExtra(void* game, void* player, void* nearUnit, int32_t sourceLevel, OpenExtraResultV1* out) noexcept -> bool {
	try {
		if (out == nullptr || out->structSize < sizeof(OpenExtraResultV1)) {
			return false;
		}
		return OpenExtraMap(game, player, nearUnit, sourceLevel, out);
	} catch (...) {
		return false;
	}
}

const OpenExtraServiceV1 g_service = {sizeof(OpenExtraServiceV1), OpenExtraServiceVersion, &OpenExtra};

}  // namespace

void PublishOpenExtraService(const D2RL::PluginContext* context) noexcept {
	try {
		const D2RL::PluginCommunicationService* communication = nullptr;
		if (context == nullptr || context->QueryService(&communication) != D2RL::ServiceQueryResult::Success ||
		    !D2RL::HasPluginCommunicationServiceField(communication, D2RL::PluginCommunicationServiceRequiredSize)) {
			plog::Info("open-extra service: the loader has no communication service; cartographer.open-extra.v1 is not offered");
			return;
		}
		const D2RL::PluginCommunication::PublishServiceRequest request{
		    .structSize = D2RL::PluginCommunication::PublishServiceRequestSize,
		    .name = OpenExtraServiceName,
		    .serviceVersion = OpenExtraServiceVersion,
		    .tableSize = sizeof(g_service),
		    .table = &g_service,
		};
		const D2RL::PluginCommunication::Result result = communication->publishService(context, &request);
		plog::Info("open-extra service: cartographer.open-extra.v1 %s", result == D2RL::PluginCommunication::Result::Success ? "published" : "was refused by the loader");
	} catch (...) {
		plog::Error("open-extra service: publishing failed");
	}
}

}  // namespace carto
