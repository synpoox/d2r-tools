#include "engine/map_quality.h"

#include "core/part_signatures.h"
#include "engine/item_stats.h"
#include "engine/scan.h"
#include "log.h"
#include "mapquality.h"
#include "statplan/statplan.h"

#include <atomic>
#include <cstdint>
#include <cstring>
#include <string>
#include <windows.h>
#include <intrin.h>

namespace carto {

namespace {

// Build 93847; evidence in docs/reverse-engineering/cartographer/26-map-quality.md.
constexpr int UnitDataFlagOffset = 0x1bd;  // uint8 on a unit: the first argument of the item record lookup (the game's own getter 0x34a0e0)
constexpr int RecordSpellOffset = 0x94;    // int32: misc.txt pSpell, as in portal.cpp
constexpr int RecordCodeOffset = 0x80;     // uint32: the item code, four letters
constexpr int32_t MapSpell = 16;           // the map item's spell: what makes an item a map for this plugin
constexpr int RequestForcedOffset = 0x40;  // int32 on the item request: a nonzero quality forced by the caller (cheat spawn typ m)
constexpr int RequestLevelOffset = 0x18;   // int32 on the item request: the item level the caller asked for (game thread; the client copy of an item reads 1)
// Return address of the item maker's own quality roll (call at 0x442e7b); the rolls inside the fallbacks are not counted.
constexpr uint64_t MainRollReturnRva = 0x442e80;

// The item quality roll (item, request) -> quality 1..8. 19 bytes of whole instructions, then the first call; the hook cuts the
// first 16 (five whole instructions). Unique in the executable sections. The affix fallbacks call it too and ignore the result.
// MagicFallback is the fallback the item maker takes when the set, rare or unique maker fails: it forces quality 4 and runs the
// magic affix maker. NormalFallback is the same with quality 2. Both are unique from the prologue to the quality constant.
namespace qsig {
inline constexpr const char* ItemQualityRoll = "48 89 54 24 10 41 55 41 57 48 83 EC 58 4C 8B EA 4C 8B F9 E8 ?? ?? ?? ?? 66 83 F8 01 72 ?? 41 8B 45 40 85 C0 0F 85 ?? ?? ?? ?? 48 89 5C 24 70 49 8B CF";
inline constexpr const char* MagicFallback = "48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18 57 48 83 EC 20 49 8B F1 49 8B F8 48 8B EA 33 DB 90 48 8B 0F 33 D2 44 8B C3 E8 ?? ?? ?? ?? 48 8B 0F 33 D2 44 8B C3 E8 ?? ?? ?? ?? FF C3 83 FB 03 7C ?? 48 8B 0F 33 D2 E8 ?? ?? ?? ?? 48 8B 0F 33 D2 E8 ?? ?? ?? ?? 48 8B 0F BA FF FF FF FF E8 ?? ?? ?? ?? 8B 16 48 8B 0F E8 ?? ?? ?? ?? 48 8B 0F 8B 1E E8 ?? ?? ?? ?? 8B D3 48 8B C8 E8 ?? ?? ?? ?? 48 8B 0F 48 8B D5 E8 ?? ?? ?? ?? 48 8B 0F BA 04 00 00 00 E8 ?? ?? ?? ?? C7 45 40 04 00 00 00";
inline constexpr const char* NormalFallback = "48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18 48 89 7C 24 20 41 56 48 83 EC 20 41 8B E9 49 8B F8 48 8B F2 4C 8B F1 33 DB 0F 1F 84 00 00 00 00 00 48 8B 0F 33 D2 44 8B C3 E8 ?? ?? ?? ?? 48 8B 0F 33 D2 44 8B C3 E8 ?? ?? ?? ?? FF C3 83 FB 03 7C ??";
}

struct Unit;

using QualityRollFn = uint64_t (*)(Unit* item, void* request) noexcept;
using FallbackMagicFn = uint64_t (*)(void* a, void* request, Unit** item, int32_t* level) noexcept;
using FallbackNormalFn = uint64_t (*)(void* a, void* request, Unit** item, int32_t level) noexcept;
using ItemRecordFn = const uint8_t* (*)(uint32_t dataFlag, int32_t row) noexcept;
using UnitClassFn = int32_t (*)(Unit*, const char* file, int32_t line) noexcept;

enum PartId { QualityRoll, MagicFallback, NormalFallback, ItemRecord, UnitClassOf, PartCount };

Part g_parts[PartCount] = {
	{"ItemQualityRoll", qsig::ItemQualityRoll, 16, false, nullptr, {}, {}},
	{"MagicFallback", qsig::MagicFallback, 16, false, nullptr, {}, {}},
	{"NormalFallback", qsig::NormalFallback, 0, false, nullptr, {}, {}},
	{"ItemRecord", sig::ItemRecord, 0, false, nullptr, {}, {}},
	{"UnitClassOf", sig::UnitClassOf, 0, false, nullptr, {}, {}},
};

QualityRollFn g_original = nullptr;
FallbackMagicFn g_originalFallback = nullptr;
uint64_t g_base = 0;
std::atomic<bool> g_rollLive{false};
std::atomic<bool> g_fallbackLive{false};
constexpr int MaxQuality = 9;
std::atomic<uint32_t> g_maps[MaxQuality];  // natural map rolls by quality as the item maker gets them (not forced, not a fallback's)
std::atomic<uint32_t> g_seen{0};      // map rolls logged with their item level (the first 20 are written to the log)
std::atomic<uint32_t> g_rolled{0};    // magic rolls of the item maker turned normal
std::atomic<uint32_t> g_stamped{0};    // new map items given the map level stat (issue #422)
std::atomic<uint32_t> g_fallbacks{0};  // magic fallbacks (a failed set, rare or unique maker) sent to the normal one

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

// The map item's code from its misc.txt row (trailing blanks cut); empty when it cannot be read.
auto MapCodeOf(Unit* item) noexcept -> std::string {
	const int32_t row = reinterpret_cast<UnitClassFn>(const_cast<uint8_t*>(g_parts[UnitClassOf].address))(item, __FILE__, __LINE__);
	if (row < 0) {
		return {};
	}
	const uint8_t* record = reinterpret_cast<ItemRecordFn>(const_cast<uint8_t*>(g_parts[ItemRecord].address))(At<uint8_t>(item, UnitDataFlagOffset), row);
	if (record == nullptr) {
		return {};
	}
	char code[5] = {};
	std::memcpy(code, record + RecordCodeOffset, 4);
	std::string out(code);
	while (!out.empty() && (out.back() == ' ' || out.back() == '\t')) {
		out.pop_back();
	}
	return out;
}

// The game-facing read and write, in functions of their own (a function with __try holds no C++ objects): a fault reads as "not done".
auto ReadStatSafe(Unit* item, int stat, int* value, bool* known) noexcept -> bool {
	__try {
		*value = ItemMagicStat(item, stat, known);
		return true;
	} __except (EXCEPTION_EXECUTE_HANDLER) {
		return false;
	}
}

auto WriteStatSafe(Unit* item, int stat, int value) noexcept -> bool {
	__try {
		return SetItemMagicStat(item, stat, value);
	} __except (EXCEPTION_EXECUTE_HANDLER) {
		return false;
	}
}

// Issue #422: a new map item carries its maps.txt Level as the map level stat (itemstatcost.txt), so its tooltip shows `Map Level: N` first
// (the stat's descpriority is the highest). An item that already carries the stat is left alone; affixes the game applies after the roll add
// to it, as the cube does. Fail open: any part or table missing leaves the item as the game made it.
void StampMapLevel(Unit* item) noexcept {
	if (!ItemStatsWritable()) {
		return;
	}
	int statId = -1;
	int mapsLevel = 0;
	if (!MapLevelStatFor(MapCodeOf(item), &statId, &mapsLevel)) {
		return;  // no loaded map names this item, or the mod has no map level stat: nothing to show
	}
	int current = 0;
	bool known = false;
	if (!ReadStatSafe(item, statId, &current, &known)) {
		return;
	}
	const int level = cartographer::statplan::mapLevelToStamp(mapsLevel, current, known);
	if (level < 1) {
		return;
	}
	const bool done = WriteStatSafe(item, statId, level);
	const uint32_t n = done ? g_stamped.fetch_add(1) + 1 : g_stamped.load();
	if (!done || n <= 10) {
		plog::Info("map level line: %s Map Level %d on a new map item (%u so far)", done ? "set" : "could not set", level, n);
	}
}

auto QualityRollHook(Unit* item, void* request) noexcept -> uint64_t {
	const uint64_t caller = reinterpret_cast<uint64_t>(_ReturnAddress());
	const uint64_t rolled = g_original(item, request);
	if (caller - g_base != MainRollReturnRva) {
		return rolled;  // a roll inside a fallback: the game throws its result away
	}
	if (!IsMapItem(item)) {
		return rolled;
	}
	StampMapLevel(item);  // every new map item, whatever its quality
	if (request != nullptr && At<int32_t>(request, RequestForcedOffset) != 0) {
		return rolled;  // a quality forced by the caller (cheat spawn typ m) wins over the roll in the game; leave it
	}
	const int decided = cartographer::mapquality::mapQuality(static_cast<int>(rolled), true);
	if (decided != static_cast<int>(rolled)) {
		const uint32_t n = g_rolled.fetch_add(1) + 1;
		if (n <= 20) {
			plog::Info("map quality: a map rolled magic and was made normal (%u so far)", n);
		}
	}
	if (decided >= 0 && decided < MaxQuality) {
		g_maps[decided].fetch_add(1);  // what the item maker gets for a map, after the change
	}
	const uint32_t seen = g_seen.fetch_add(1) + 1;
	if (seen <= 20 && request != nullptr) {
		plog::Info("map quality: map roll %u: rolled %d, item level %d", seen, static_cast<int>(rolled), At<int32_t>(request, RequestLevelOffset));
	}
	return static_cast<uint64_t>(decided);
}

// The game's magic fallback (a set, rare or unique maker failed): a map takes the normal fallback instead.
auto MagicFallbackHook(void* a, void* request, Unit** item, int32_t* level) noexcept -> uint64_t {
	if (item != nullptr && level != nullptr && cartographer::mapquality::useNormalFallback(IsMapItem(*item))) {
		const uint32_t n = g_fallbacks.fetch_add(1) + 1;
		if (n <= 20) {
			plog::Info("map quality: a map's set, rare or unique roll failed and was made normal, not magic (%u so far)", n);
		}
		return reinterpret_cast<FallbackNormalFn>(const_cast<uint8_t*>(g_parts[NormalFallback].address))(a, request, item, *level);
	}
	return g_originalFallback(a, request, item, level);
}

auto Hook(const D2RL::PluginContext* context, PartId id, void* hook, void** original) -> bool {
	Part& part = g_parts[id];
	const uint64_t rva = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(part.address) - context->exeBase);
	const bool hooked = context->InstallInlineHook(rva, part.guard, part.hookSize, hook, original);
	return HookOutcome(part, hooked, *original != nullptr);
}

}  // namespace

auto InstallMapQuality(const D2RL::PluginContext* context, const std::vector<SignatureOverride>& overrides, std::vector<PartStatus>* parts) -> bool {
	CodeRange ranges[8];
	const int count = CodeRanges(context->exeBase, ranges, 8);
	g_base = context->exeBase;
	for (Part& part : g_parts) {
		ResolvePart(context, overrides, ranges, count, part);
	}
	// Never installed half-on: each hook needs the lookups it calls. Without them the game rolls magic maps as before.
	const bool lookups = On(ItemRecord) && On(UnitClassOf);
	if (On(QualityRoll) && lookups) {
		g_rollLive = Hook(context, QualityRoll, reinterpret_cast<void*>(&QualityRollHook), reinterpret_cast<void**>(&g_original));
	} else if (On(QualityRoll)) {
		g_parts[QualityRoll].status.on = false;
		g_parts[QualityRoll].status.detail = "off: not hooked, because a part it needs was not found";
	}
	if (On(MagicFallback) && On(NormalFallback) && lookups) {
		g_fallbackLive = Hook(context, MagicFallback, reinterpret_cast<void*>(&MagicFallbackHook), reinterpret_cast<void**>(&g_originalFallback));
	} else if (On(MagicFallback)) {
		g_parts[MagicFallback].status.on = false;
		g_parts[MagicFallback].status.detail = "off: not hooked, because a part it needs was not found";
	}
	if (!g_rollLive.load() || !g_fallbackLive.load()) {
		plog::Warn("magic maps are not fully blocked: %s (see the parts below)", MapQualitySummary().c_str());
	}
	plog::Info("%s", MapQualitySummary().c_str());
	for (const Part& part : g_parts) {
		plog::Info("part %s: %s", part.name, part.status.detail.c_str());
	}
	if (parts != nullptr) {
		*parts = MapQualityPartStatuses();
	}
	return g_rollLive.load() && g_fallbackLive.load();
}

auto MapQualityPartStatuses() -> std::vector<PartStatus> {
	std::vector<PartStatus> out;
	for (const Part& part : g_parts) {
		out.push_back(part.status);
	}
	return out;
}

auto MapQualitySummary() -> std::string {
	const bool roll = g_rollLive.load();
	const bool fallback = g_fallbackLive.load();
	if (!roll && !fallback) {
		return "magic maps are not blocked (the item quality roll and its magic fallback are not hooked)";
	}
	namespace mq = cartographer::mapquality;
	std::uint32_t byQuality[10] = {};
	for (int quality = 0; quality < MaxQuality; ++quality) {
		byQuality[quality] = g_maps[quality].load();
	}
	const std::string counts = cartographer::mapquality::rollCounts(byQuality, g_rolled.load()) + "; " + std::to_string(g_fallbacks.load()) +
		" maps whose set, rare or unique maker failed were made normal, not magic";
	if (roll && fallback) {
		return "magic maps are blocked (maps forced magic by a cheat, and maps that already exist, stay magic): " + counts + " so far";
	}
	return std::string("magic maps are only partly blocked (") + (roll ? "the magic fallback is not hooked" : "the item quality roll is not hooked") + "): " + counts + " so far";
}

}  // namespace carto
