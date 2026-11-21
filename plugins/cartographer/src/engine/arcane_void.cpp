#include "arcane_void.h"

#include "log.h"
#include "void_room.h"

#include <string>
#include <vector>

namespace carto {

namespace {
bool g_served = false;
}  // namespace

auto ArcaneVoidServed() -> bool {
	return g_served;
}

auto InstallArcaneVoid(const D2RL::PluginContext* context) -> bool {
	g_served = false;
	const D2RL::ResourceService* resources = nullptr;
	if (context->QueryService(&resources) != D2RL::ServiceQueryResult::Success || !D2RL::HasResourceServiceField(resources, D2RL::ResourceServiceRequiredSize)) {
		plog::Warn("arcane void room: D2RLoader has no Resource service of the expected version; Arcane maps keep cells that block missiles");
		return false;
	}
	try {
		const std::vector<uint8_t> ds1 = cartographer::maze::voidRoomDs1();
		const std::string preset = cartographer::maze::voidRoomPreset();
		const struct {
			const char* path;
			const void* bytes;
			size_t size;
		} files[] = {{cartographer::maze::kVoidRoomDs1Path, ds1.data(), ds1.size()}, {cartographer::maze::kVoidRoomPresetPath, preset.data(), preset.size()}};
		for (const auto& f : files) {
			D2RL::Resources::ResourceRegistration resource{};
			resource.structSize = D2RL::Resources::ResourceRegistrationSize;
			resource.path = f.path;
			resource.bytes = f.bytes;
			resource.byteCount = f.size;
			D2RL::Resources::RegistrationHandle handle = D2RL::Resources::InvalidHandle;
			const D2RL::Resources::Result result = resources->registerResource(context, &resource, &handle);
			if (result != D2RL::Resources::Result::Success) {
				plog::Warn("arcane void room: D2RLoader refused %s (result %u)", f.path, static_cast<uint32_t>(result));
				return false;
			}
		}
		plog::Debug("arcane void room: registered its DS1 and HD preset");
		g_served = true;
	} catch (...) {
		plog::Warn("arcane void room: building the files failed");
	}
	return g_served;
}

}  // namespace carto
