#include "engine/map_lighting.h"

#include "core/part_signatures.h"
#include "engine/game_memory.h"
#include "engine/scan.h"
#include "log.h"
#include "maplight.h"

#include <atomic>
#include <cstdint>
#include <cstring>
#include <intrin.h>
#include <mutex>

namespace carto {

namespace {

// Build 93847; evidence in docs/reverse-engineering/cartographer/32-level-lighting.md.
namespace ml = cartographer::maplight;

constexpr size_t ComponentSize = 0x1e8;  // VisualDataComponent, the pool stride of get<VisualDataComponent> (0xf56690)
// Return address of the call in VisualDataSystem::Update that passes the current instance's component (call at 0xf5d837). The other
// call (0xf5d9b2, no instance: a component built on the stack) is not a level's look and is left alone.
constexpr uint64_t InstanceReturnRva = 0xf5d83c;
constexpr size_t ExposureOffset = 0x00;     // component float: the exposure value (a higher value is a darker picture)
constexpr size_t TemperatureOffset = 0x40;  // component float: the colour temperature (the apply function copies it to a global)

using ApplyFn = uint64_t (*)(void* system, void* registry, uint8_t* component, uint8_t* frame) noexcept;

Part g_part = {"VisualApply", sig::VisualApply, 19, false, nullptr, {}, {}};
ApplyFn g_original = nullptr;
uint64_t g_base = 0;
std::atomic<bool> g_live{false};

std::mutex g_lock;  // g_look, g_slotLevel and the two scales (no engine call while held)
ml::Look g_look;
float g_exposure = 1.0f;
float g_temperature = 1.0f;
int g_slotLevel = -1;
std::atomic<bool> g_active{false};  // the local player stands in the map's slot level and the map has a look

std::atomic<uint32_t> g_instanceCalls{0}, g_changed{0};

auto ApplyHook(void* system, void* registry, uint8_t* component, uint8_t* frame) noexcept -> uint64_t {
	const bool instance = reinterpret_cast<uint64_t>(_ReturnAddress()) - g_base == InstanceReturnRva;
	try {
		if (instance && component != nullptr && frame != nullptr && Readable(component, ComponentSize)) {
			g_instanceCalls.fetch_add(1, std::memory_order_relaxed);
			if (g_active.load(std::memory_order_relaxed)) {
				float exposure = 1.0f, temperature = 1.0f;
				{
					std::lock_guard<std::mutex> lock(g_lock);
					exposure = g_exposure;
					temperature = g_temperature;
				}
				const auto scale = [&](size_t offset, float factor) {
					float value = 0;
					std::memcpy(&value, component + offset, 4);
					value *= factor;
					std::memcpy(component + offset, &value, 4);
				};
				scale(ExposureOffset, exposure);
				scale(TemperatureOffset, temperature);
				g_changed.fetch_add(1, std::memory_order_relaxed);
			}
		}
	} catch (...) {
	}
	return g_original(system, registry, component, frame);
}

}  // namespace

auto InstallMapLighting(const D2RL::PluginContext* context, const std::vector<SignatureOverride>& overrides, std::vector<PartStatus>* parts) -> bool {
	CodeRange ranges[8];
	const int count = CodeRanges(context->exeBase, ranges, 8);
	g_base = context->exeBase;
	ResolvePart(context, overrides, ranges, count, g_part);
	if (g_part.address != nullptr && g_part.status.on) {
		const uint64_t rva = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(g_part.address) - context->exeBase);
		const bool hooked = context->InstallInlineHook(rva, g_part.guard, g_part.hookSize, reinterpret_cast<void*>(&ApplyHook), reinterpret_cast<void**>(&g_original));
		g_live = HookOutcome(g_part, hooked, g_original != nullptr);
	}
	plog::Info("part %s: %s", g_part.name, g_part.status.detail.c_str());
	if (!g_live.load()) {
		plog::Warn("map looks (Brightness, Warmth) are off: the visual data apply part is not hooked");
	}
	if (parts != nullptr) {
		parts->push_back(g_part.status);
	}
	return g_live.load();
}

auto MapLightingPartStatuses() -> std::vector<PartStatus> {
	return {g_part.status};
}

void MapLightingMapOpened(const MapDef& map, int slotLevel) {
	std::lock_guard<std::mutex> lock(g_lock);
	g_look = map.look;
	g_exposure = ml::exposureScale(g_look);
	g_temperature = ml::temperatureScale(g_look);
	g_slotLevel = g_look.any() ? slotLevel : -1;
	g_active = false;
	if (g_slotLevel > 0) {
		plog::Info("map look for '%s' (slot level %d): %s%s", map.name.c_str(), slotLevel, ml::describe(g_look).c_str(), g_live.load() ? "" : " (not applied: the hook is off)");
	}
}

void MapLightingPlayerTick(int playerLevel) noexcept {
	if (!g_live.load(std::memory_order_relaxed)) {
		return;
	}
	bool active = false;
	{
		std::lock_guard<std::mutex> lock(g_lock);
		active = g_slotLevel > 0 && playerLevel == g_slotLevel;
	}
	if (g_active.exchange(active) != active) {
		plog::Info("map look: %s", active ? "applied (the player is in the map)" : "off (the player left the map)");
	}
}

void MapLightingGameLeft() noexcept {
	std::lock_guard<std::mutex> lock(g_lock);
	g_slotLevel = -1;
	g_look = {};
	g_active = false;
}

auto MapLightingSummary() -> std::string {
	if (!g_live.load()) {
		return "map looks (Brightness, Warmth) are off: the visual data apply part is not hooked";
	}
	return "map looks are on: " + std::to_string(g_changed.load()) + " frame(s) changed of " + std::to_string(g_instanceCalls.load()) + " level frame(s) seen";
}

}  // namespace carto
