#include "engine/map_lighting.h"

#include "core/part_signatures.h"
#include "engine/arena_service.h"
#include "engine/game_memory.h"
#include "engine/lut_grade.h"
#include "engine/scan.h"
#include "log.h"
#include "maplight.h"

#include <atomic>
#include <cstdint>
#include <cstring>
#include <intrin.h>
#include <map>
#include <mutex>

namespace carto {

namespace {

// Build 93847; evidence in docs/reverse-engineering/cartographer/32-level-lighting.md.
namespace ml = cartographer::maplight;

constexpr size_t ComponentSize = 0x1e8;  // VisualDataComponent, the pool stride of get<VisualDataComponent> (0xf56690)
// The call in VisualDataSystem::Update that passes the current instance's component (call at 0xf5d837, returns 0xf5d83c) is found by the
// InstanceCall signature; the other call (0xf5d9b2, no instance: a component built on the stack) is not a level's look and is left alone.
constexpr size_t ExposureOffset = 0x00;     // component float: the exposure value (a higher value is a darker picture)
constexpr size_t PlayerLightPowerOffset = 0x40;  // component float: the strength of the light around the player (playerUnitLightPower; the apply function copies it to a global)
// The directional lights (DirectionalLightComponent storage, stride 0x60, note 32): a colour vec3 at +0x00 and a second copy at +0x4c. The game rewrites
// them every frame (the level's transition code), so a tint written here lasts one frame and is gone the frame after the player leaves.
constexpr size_t LightStride = 0x60, LightColourOffsets[2] = {0x00, 0x4c};
constexpr size_t MaxLights = 32;
constexpr size_t StorageEntitiesBegin = 0x20, StorageEntitiesEnd = 0x28, StorageData = 0x38;  // the light storage object, from the getter

using ApplyFn = uint64_t (*)(void* system, void* registry, uint8_t* component, uint8_t* frame) noexcept;

Part g_part = {"VisualApply", sig::VisualApply, 19, false, nullptr, {}, {}};
Part g_callPart = {"InstanceCall", sig::InstanceCall, 0, false, nullptr, {}, {}};
uint64_t g_instanceReturnRva = 0;
ApplyFn g_original = nullptr;
uint64_t g_base = 0;
std::atomic<bool> g_live{false};

std::mutex g_lock;  // g_looks and the two scales (no engine call while held)
std::map<int, ml::Look> g_looks;  // slot level -> the look of the map opened there last (maps that share a tileset share the level)
float g_exposure = 1.0f;
float g_lightPower = 1.0f;
bool g_changesLightPower = false;  // false for a Brightness-only look: the tileset's own light power is left alone (not even clamped)
bool g_changesTint = false;
float g_tint[3] = {1.0f, 1.0f, 1.0f};  // TintR, TintG, TintB as multipliers
std::atomic<bool> g_active{false};  // the local player stands in the map's slot level and the map has a look

std::atomic<uint32_t> g_instanceCalls{0}, g_changed{0};

using StorageFn = uint8_t* (*)(void* registry) noexcept;
Part g_lightsPart = {"LightsGetter", sig::LightsGetterCall, 0, false, nullptr, {}, {}};
StorageFn g_lightsStorage = nullptr;  // the get-the-DirectionalLightComponent-storage function, found from the call in VisualDataSystem::Update

// What the game wrote and what the tint wrote, per light and colour copy, so a light the game did not rewrite is neither tinted twice nor left tinted
// (render thread, hook only), keyed by the light's entity id (+0x20) so a light that moved in the storage is not tinted twice or left tinted. `written` is compared to the live value: unchanged means the game left it alone.
struct TintSlot {
	bool valid = false;
	float base[3] = {}, written[3] = {};
};
std::map<uint32_t, TintSlot[2]> g_slots;
std::atomic<bool> g_reset{false};  // MapLightingGameLeft asks the hook to forget g_slots and g_tintWritten (they belong to the render thread)

auto LightData(void* registry, size_t* count, const uint32_t** ids) -> uint8_t* {
	*count = 0;
	if (g_lightsStorage == nullptr) {
		return nullptr;
	}
	uint8_t* storage = g_lightsStorage(registry);
	if (storage == nullptr || !Readable(storage, StorageData + 8)) {
		return nullptr;
	}
	const uint8_t* begin = *reinterpret_cast<uint8_t* const*>(storage + StorageEntitiesBegin);
	const uint8_t* end = *reinterpret_cast<uint8_t* const*>(storage + StorageEntitiesEnd);
	uint8_t* data = *reinterpret_cast<uint8_t**>(storage + StorageData);
	if (begin == nullptr || end < begin || data == nullptr) {
		return nullptr;
	}
	size_t n = static_cast<size_t>(end - begin) / 4;
	n = n < MaxLights ? n : MaxLights;
	if (n == 0 || !Readable(begin, n * 4) || !Readable(data, n * LightStride)) {
		return nullptr;
	}
	*count = n;
	*ids = reinterpret_cast<const uint32_t*>(begin);
	return data;
}

// factor == nullptr puts every light the tint changed back.
void ScaleLights(void* registry, const float* factor) {
	size_t count = 0;
	const uint32_t* ids = nullptr;
	uint8_t* data = LightData(registry, &count, &ids);
	for (size_t i = 0; i < count; i++) {
		for (size_t c = 0; c < 2; c++) {
			float* colour = reinterpret_cast<float*>(data + i * LightStride + LightColourOffsets[c]);
			TintSlot& slot = g_slots[ids[i]][c];
			const bool ours = slot.valid && std::memcmp(colour, slot.written, sizeof(slot.written)) == 0;
			if (!ours) {
				if (factor == nullptr) {
					slot.valid = false;
					continue;
				}
				std::memcpy(slot.base, colour, sizeof(slot.base));
			}
			for (int k = 0; k < 3; k++) {
				colour[k] = factor != nullptr ? slot.base[k] * factor[k] : slot.base[k];
			}
			std::memcpy(slot.written, colour, sizeof(slot.written));
			slot.valid = factor != nullptr;
		}
	}
}

bool g_tintWritten = false;  // a tint was written since the last restore (render thread, hook only)

void TintLights(void* registry, float r, float g, float b) {
	const float factor[3] = {r, g, b};
	ScaleLights(registry, factor);
	g_tintWritten = true;
}

// The frame after the player leaves the map: put back any light the game did not rewrite itself.
void UntintLights(void* registry) {
	if (g_tintWritten) {
		ScaleLights(registry, nullptr);
		g_tintWritten = false;
	}
}

auto ApplyHook(void* system, void* registry, uint8_t* component, uint8_t* frame) noexcept -> uint64_t {
	const bool instance = reinterpret_cast<uint64_t>(_ReturnAddress()) - g_base == g_instanceReturnRva;
	try {
		if (instance && component != nullptr && frame != nullptr) {
			g_instanceCalls.fetch_add(1, std::memory_order_relaxed);
			if (g_reset.exchange(false)) {
				g_slots.clear();
				g_tintWritten = false;
			}
			if (g_active.load(std::memory_order_relaxed) && Readable(component, ComponentSize)) {
				float exposure = 1.0f, lightPower = 1.0f, tintR = 1.0f, tintG = 1.0f, tintB = 1.0f;
				bool changesLightPower = false, tint = false;
				{
					std::lock_guard<std::mutex> lock(g_lock);
					exposure = g_exposure;
					lightPower = g_lightPower;
					changesLightPower = g_changesLightPower;
					tint = g_changesTint;
					tintR = g_tint[0];
					tintG = g_tint[1];
					tintB = g_tint[2];
				}
				const auto scale = [&](size_t offset, float factor, bool clamp) {
					float value = 0;
					std::memcpy(&value, component + offset, 4);
					value *= factor;
					if (clamp) {
						value = ml::clampLightPower(value);
					}
					std::memcpy(component + offset, &value, 4);
				};
				scale(ExposureOffset, exposure, false);
				if (changesLightPower) {
					scale(PlayerLightPowerOffset, lightPower, true);
				}
				g_changed.fetch_add(1, std::memory_order_relaxed);
				if (tint) {
					TintLights(registry, tintR, tintG, tintB);
				} else {
					UntintLights(registry);
				}
			} else {
				UntintLights(registry);
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
	ResolveCallSite(context, overrides, ranges, count, g_callPart, &g_instanceReturnRva);
	uint64_t lightsReturnRva = 0;
	ResolveCallSite(context, overrides, ranges, count, g_lightsPart, &lightsReturnRva);
	if (lightsReturnRva != 0) {
		// The call's target: the rel32 in the four bytes before the return address. The tint is the only user; without it Brightness and Warmth still work.
		int32_t rel = 0;
		std::memcpy(&rel, reinterpret_cast<const void*>(context->exeBase + lightsReturnRva - 4), sizeof(rel));
		g_lightsStorage = reinterpret_cast<StorageFn>(context->exeBase + lightsReturnRva + rel);
	}
	if (g_part.status.on && g_instanceReturnRva == 0) {
		// Never hooked without its caller check: it would run and never match.
		g_part.status.on = false;
		g_part.status.detail = "off: not hooked, because the part InstanceCall was not found";
	}
	if (g_part.address != nullptr && g_part.status.on) {
		const uint64_t rva = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(g_part.address) - context->exeBase);
		const bool hooked = context->InstallInlineHook(rva, g_part.guard, g_part.hookSize, reinterpret_cast<void*>(&ApplyHook), reinterpret_cast<void**>(&g_original));
		g_live = HookOutcome(g_part, hooked, g_original != nullptr);
	}
	plog::Info("part %s: %s", g_callPart.name, g_callPart.status.detail.c_str());
	plog::Info("part %s: %s", g_lightsPart.name, g_lightsPart.status.detail.c_str());
	if (g_lightsStorage == nullptr) {
		plog::Warn("map light tints (TintR, TintG, TintB) are off: the light storage part is not found");
	}
	plog::Info("part %s: %s", g_part.name, g_part.status.detail.c_str());
	if (!g_live.load()) {
		plog::Warn("map looks (Brightness, Warmth, tint) are off: the visual data apply part is not hooked");
	}
	if (parts != nullptr) {
		parts->push_back(g_callPart.status);
		parts->push_back(g_lightsPart.status);
		parts->push_back(g_part.status);
	}
	return g_live.load();
}

auto MapLightingPartStatuses() -> std::vector<PartStatus> {
	return {g_callPart.status, g_lightsPart.status, g_part.status};
}

void MapLightingMapOpened(const MapDef& map, int slotLevel) {
	const bool has = map.look.any() && slotLevel > 0;
	{
		std::lock_guard<std::mutex> lock(g_lock);
		// One look per slot level: a map opened later on another tileset does not replace the look of the first one.
		if (has) {
			g_looks[slotLevel] = map.look;
		} else {
			g_looks.erase(slotLevel);
		}
		g_active = false;
	}
	if (has) {
		ml::Look withoutGrade = map.look;  // hue and saturation have their own hooks: only the rest needs this one
		withoutGrade.hue = cartographer::lutgrade::DefaultHue;
		withoutGrade.saturation = cartographer::lutgrade::DefaultSaturation;
		plog::Debug("map look for '%s' (slot level %d): %s%s", map.name.c_str(), slotLevel, ml::describe(map.look).c_str(), g_live.load() || !withoutGrade.any() ? "" : " (not applied: the hook is off)");
	}
}

void MapLightingPlayerTick(int playerLevel) noexcept {
	if (!g_live.load(std::memory_order_relaxed) && !LutGradeLive()) {
		return;  // neither the apply hook nor the grading hooks are in
	}
	// An arena is not a map open, so its look comes from its arenas.txt row. Read before g_lock: the arena tables have their own lock.
	ml::Look arenaLook;
	bool arenaHasLook = false;
	try {
		const std::shared_ptr<const Tables> tables = ArenaServiceTables();
		if (tables != nullptr) {
			for (const ArenaDef& a : tables->arenas) {
				if (a.level == playerLevel && a.look.any()) {
					arenaLook = a.look;
					arenaHasLook = true;
					break;
				}
			}
		}
	} catch (...) {
	}
	bool active = false;
	cartographer::lutgrade::Grade grade;  // maps only: an arena has no Hue or Saturation
	{
		std::lock_guard<std::mutex> lock(g_lock);
		const auto found = g_looks.find(playerLevel);
		const ml::Look* look = found != g_looks.end() ? &found->second : arenaHasLook ? &arenaLook : nullptr;
		active = look != nullptr;
		if (active) {
			g_exposure = ml::exposureScale(*look);
			g_lightPower = ml::lightPowerScale(*look);
			g_changesLightPower = ml::changesLightPower(*look);
			g_changesTint = ml::changesTint(*look);
			g_tint[0] = static_cast<float>(look->tintR) / 100.0f;
			g_tint[1] = static_cast<float>(look->tintG) / 100.0f;
			g_tint[2] = static_cast<float>(look->tintB) / 100.0f;
			grade = look->grade();
		}
	}
	LutGradeSet(active ? &grade : nullptr);
	if (g_active.exchange(active) != active) {
		plog::Debug("map look: %s", active ? "applied (the player is in the map)" : "off (the player left the map)");
	}
}

void MapLightingGameLeft() noexcept {
	std::lock_guard<std::mutex> lock(g_lock);
	g_looks.clear();
	g_active = false;
	g_reset = true;
	LutGradeSet(nullptr);
}

auto MapLightingSummary() -> std::string {
	if (!g_live.load()) {
		return "map looks (Brightness, Warmth) are off: the visual data apply part is not hooked";
	}
	return "map looks are on: " + std::to_string(g_changed.load()) + " frame(s) changed of " + std::to_string(g_instanceCalls.load()) + " level frame(s) seen";
}

}  // namespace carto
