#include "engine/room_populate_service.h"

#include "log.h"
#include "roompopulate.h"

#include <D2RLPlugin/plugin_communication.h>

#include <windows.h>

#include <cstdint>
#include <cstdlib>
#include <string>
#include <vector>

namespace carto {

namespace roompopulate = cartographer::roompopulate;

namespace {

roompopulate::Subscribers g_subscribers;

// The C table a consumer calls. The pointers are copied into the subscriber, so the consumer's struct may go out of scope.
auto __cdecl Subscribe(const char* pluginId, const RoomPopulateCallbacksV1* callbacks, void* user, uint32_t* handle) noexcept -> bool {
	try {
		if (pluginId == nullptr || callbacks == nullptr || handle == nullptr || callbacks->structSize < sizeof(RoomPopulateCallbacksV1)) {
			return false;
		}
		roompopulate::BeforeFn before;
		if (callbacks->before != nullptr) {
			auto fn = callbacks->before;
			before = [fn, user](void* game, void* room, int32_t level) { return fn(game, room, level, user); };
		}
		roompopulate::AfterFn after;
		if (callbacks->after != nullptr) {
			auto fn = callbacks->after;
			after = [fn, user](void* game, void* room, int32_t level, bool skipped) { fn(game, room, level, skipped, user); };
		}
		*handle = g_subscribers.add(pluginId, std::move(before), std::move(after));
		return true;
	} catch (...) {
		return false;
	}
}

auto __cdecl Unsubscribe(uint32_t handle) noexcept -> bool {
	try {
		return g_subscribers.remove(handle);
	} catch (...) {
		return false;
	}
}

const RoomPopulateServiceV1 g_service = {sizeof(RoomPopulateServiceV1), RoomPopulateServiceVersion, &Subscribe, &Unsubscribe};

void LogDropped(const std::vector<std::string>& dropped) {
	for (const std::string& id : dropped) {
		plog::Error("room-populate: subscriber '%s' threw and was dropped", id.c_str());
	}
}

// shortcut: a game-check knob, not a feature; it goes through the same table a consumer uses, so the check covers that path too. Remove it once the consumer PR has its own check.
auto __cdecl TestBefore(void*, void*, int32_t levelId, void* user) noexcept -> uint32_t {
	const int32_t wanted = static_cast<int32_t>(reinterpret_cast<intptr_t>(user));
	if (levelId != wanted) {
		return 0;
	}
	plog::Info("room-populate test: skipped the native populate of a room of level %d", levelId);
	return RoomPopulateSkipNative;
}

// CARTOGRAPHER_TEST_SKIP_NATIVE=<levelId> skips the game's own populate of every room of that level (a game check only).
void InstallTestSkipNative() {
	char knob[32] = {};
	if (GetEnvironmentVariableA("CARTOGRAPHER_TEST_SKIP_NATIVE", knob, sizeof(knob)) == 0) {
		return;
	}
	char* end = nullptr;
	const long level = std::strtol(knob, &end, 10);
	if (end == knob || *end != '\0' || level <= 0) {
		plog::Warn("room-populate test: CARTOGRAPHER_TEST_SKIP_NATIVE=%s is not a level id; ignored", knob);
		return;
	}
	plog::Warn("room-populate test: CARTOGRAPHER_TEST_SKIP_NATIVE is set: the native populate of level %ld is skipped (a game check only)", level);
	const RoomPopulateCallbacksV1 callbacks = {sizeof(RoomPopulateCallbacksV1), &TestBefore, nullptr};
	uint32_t handle = 0;
	(void)Subscribe("cartographer-test", &callbacks, reinterpret_cast<void*>(static_cast<intptr_t>(level)), &handle);
}

}  // namespace

void PublishRoomPopulateService(const D2RL::PluginContext* context, bool hookLive) noexcept {
	try {
		if (!hookLive) {
			plog::Info("room-populate service: the PopulateRoom hook is off; cartographer.room-populate.v1 is not offered");
			return;
		}
		const D2RL::PluginCommunicationService* communication = nullptr;
		if (context == nullptr || context->QueryService(&communication) != D2RL::ServiceQueryResult::Success ||
		    !D2RL::HasPluginCommunicationServiceField(communication, D2RL::PluginCommunicationServiceRequiredSize)) {
			plog::Info("room-populate service: the loader has no communication service; cartographer.room-populate.v1 is not offered");
			return;
		}
		const D2RL::PluginCommunication::PublishServiceRequest request{
		    .structSize = D2RL::PluginCommunication::PublishServiceRequestSize,
		    .name = RoomPopulateServiceName,
		    .serviceVersion = RoomPopulateServiceVersion,
		    .tableSize = sizeof(g_service),
		    .table = &g_service,
		};
		const D2RL::PluginCommunication::Result result = communication->publishService(context, &request);
		if (result != D2RL::PluginCommunication::Result::Success) {
			plog::Info("room-populate service: cartographer.room-populate.v1 was refused by the loader");
			return;
		}
		plog::Info("room-populate service: cartographer.room-populate.v1 published");
		InstallTestSkipNative();
	} catch (...) {
		plog::Error("room-populate service: publishing failed");
	}
}

auto RoomPopulateHasSubscribers() noexcept -> bool {
	try {
		return g_subscribers.size() > 0;
	} catch (...) {
		return false;
	}
}

auto RoomPopulateBefore(void* game, void* room, int32_t levelId) noexcept -> uint32_t {
	try {
		std::vector<std::string> dropped;
		const uint32_t flags = g_subscribers.runBefore(game, room, levelId, &dropped);
		LogDropped(dropped);
		return flags;
	} catch (...) {
		return 0;  // a failure here never skips the game's populate
	}
}

void RoomPopulateAfter(void* game, void* room, int32_t levelId, bool skipped) noexcept {
	try {
		std::vector<std::string> dropped;
		g_subscribers.runAfter(game, room, levelId, skipped, &dropped);
		LogDropped(dropped);
	} catch (...) {
	}
}

}  // namespace carto
