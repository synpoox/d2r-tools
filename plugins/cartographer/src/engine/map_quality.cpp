#include "engine/map_quality.h"

#include "core/part_signatures.h"
#include "engine/scan.h"
#include "log.h"
#include "mapquality.h"

#include <atomic>
#include <cstdint>

namespace carto {

namespace {

// Build 93847; evidence in docs/reverse-engineering/cartographer/23-map-quality.md.
constexpr int UnitDataFlagOffset = 0x1bd;  // uint8 on a unit: the first argument of the item record lookup (the game's own getter 0x34a0e0)
constexpr int RecordSpellOffset = 0x94;    // int32: misc.txt pSpell, as in portal.cpp
constexpr int32_t MapSpell = 16;           // the map item's spell: what makes an item a map for this plugin

// The item quality roll (item, request) -> quality 1..8. 19 bytes of whole instructions, then the first call; the hook cuts the
// first 16 (five whole instructions). Unique in the executable sections. The affix fallbacks call it too and ignore the result.
namespace qsig {
inline constexpr const char* ItemQualityRoll = "48 89 54 24 10 41 55 41 57 48 83 EC 58 4C 8B EA 4C 8B F9 E8 ?? ?? ?? ?? 66 83 F8 01 72 ?? 41 8B 45 40 85 C0 0F 85 ?? ?? ?? ?? 48 89 5C 24 70 49 8B CF";
}

struct Unit;

using QualityRollFn = uint64_t (*)(Unit* item, void* request) noexcept;
using ItemRecordFn = const uint8_t* (*)(uint32_t dataFlag, int32_t row) noexcept;
using UnitClassFn = int32_t (*)(Unit*, const char* file, int32_t line) noexcept;

enum PartId { QualityRoll, ItemRecord, UnitClassOf, PartCount };

Part g_parts[PartCount] = {
	{"ItemQualityRoll", qsig::ItemQualityRoll, 16, false, nullptr, {}, {}},
	{"ItemRecord", sig::ItemRecord, 0, false, nullptr, {}, {}},
	{"UnitClassOf", sig::UnitClassOf, 0, false, nullptr, {}, {}},
};

QualityRollFn g_original = nullptr;
std::atomic<bool> g_live{false};
std::atomic<uint32_t> g_converted{0};

auto On(PartId id) noexcept -> bool {
	return g_parts[id].address != nullptr && g_parts[id].status.on;
}

// True when `item` is a map item: its misc.txt row carries the map spell. False on anything that cannot be read.
auto IsMapItem(Unit* item) noexcept -> bool {
	if (item == nullptr) {
		return false;
	}
	const int32_t row = reinterpret_cast<UnitClassFn>(const_cast<uint8_t*>(g_parts[UnitClassOf].address))(item, __FILE__, __LINE__);
	if (row < 0) {
		return false;
	}
	const uint8_t* record = reinterpret_cast<ItemRecordFn>(const_cast<uint8_t*>(g_parts[ItemRecord].address))(At<uint8_t>(item, UnitDataFlagOffset), row);
	return record != nullptr && At<int32_t>(record, RecordSpellOffset) == MapSpell;
}

auto QualityRollHook(Unit* item, void* request) noexcept -> uint64_t {
	const uint64_t rolled = g_original(item, request);
	if (rolled != static_cast<uint64_t>(cartographer::mapquality::Magic)) {
		return rolled;  // only a magic roll can change; nothing else is read
	}
	const int decided = cartographer::mapquality::mapQuality(static_cast<int>(rolled), IsMapItem(item));
	if (decided != static_cast<int>(rolled)) {
		const uint32_t n = g_converted.fetch_add(1) + 1;
		if (n <= 20) {
			plog::Info("map quality: a map rolled magic and was made normal (%u so far)", n);
		}
	}
	return static_cast<uint64_t>(decided);
}

}  // namespace

auto InstallMapQuality(const D2RL::PluginContext* context, const std::vector<SignatureOverride>& overrides, std::vector<PartStatus>* parts) -> bool {
	CodeRange ranges[8];
	const int count = CodeRanges(context->exeBase, ranges, 8);
	for (Part& part : g_parts) {
		ResolvePart(context, overrides, ranges, count, part);
	}
	// Never installed half-on: the hook needs the two lookups it calls. Without them the game rolls magic maps as before.
	if (On(QualityRoll) && On(ItemRecord) && On(UnitClassOf)) {
		Part& part = g_parts[QualityRoll];
		const uint64_t rva = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(part.address) - context->exeBase);
		const bool hooked = context->InstallInlineHook(rva, part.guard, part.hookSize, reinterpret_cast<void*>(&QualityRollHook), reinterpret_cast<void**>(&g_original));
		g_live = HookOutcome(part, hooked, g_original != nullptr);
	} else if (On(QualityRoll)) {
		g_parts[QualityRoll].status.on = false;
		g_parts[QualityRoll].status.detail = "off: not hooked, because a part it needs was not found";
	}
	if (!g_live.load()) {
		plog::Warn("magic maps are not blocked: the item quality roll is not hooked (see the parts below)");
	}
	plog::Info("%s", MapQualitySummary().c_str());
	for (const Part& part : g_parts) {
		plog::Info("part %s: %s", part.name, part.status.detail.c_str());
	}
	if (parts != nullptr) {
		*parts = MapQualityPartStatuses();
	}
	return g_live.load();
}

auto MapQualityPartStatuses() -> std::vector<PartStatus> {
	std::vector<PartStatus> out;
	for (const Part& part : g_parts) {
		out.push_back(part.status);
	}
	return out;
}

auto MapQualitySummary() -> std::string {
	if (!g_live.load()) {
		return "magic maps are not blocked (the item quality roll is not hooked)";
	}
	return "magic maps are blocked: " + std::to_string(g_converted.load()) + " made normal so far";
}

}  // namespace carto
