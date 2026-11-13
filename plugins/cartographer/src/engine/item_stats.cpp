#include "engine/item_stats.h"

#include "core/part_signatures.h"
#include "engine/scan.h"
#include "log.h"

#include <atomic>
#include <cstdint>

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace carto {

namespace {

namespace sp = cartographer::statplan;

// Build 93847; evidence in docs/reverse-engineering/cartographer/14-item-stats.md.
constexpr int32_t MagicListFlags = 0x40;  // the item's base magic list (created with flags 0x40); state 0 asks for a match by flags
constexpr int MaxStats = 16;              // the game's own sender sends 16 at most
// The original asserts when the list count is at least `max`, so ask for the 0x200 that the game's own callers use and cut to MaxStats here.
constexpr int CopyMax = 0x200;

struct Unit;

using FindListFn = void* (*)(Unit*, int32_t state, int32_t flags) noexcept;
// Copies up to `max` {stat << 16 | layer, value} pairs (two int32 each) from the list into `out`; returns how many.
using ListCopyFn = int32_t (*)(void* list, int32_t* out, int32_t max) noexcept;

// The generic property applier (cube mods, item generation). 12 parameters: four in registers, eight on the stack; the Property is the fifth and the ninth.
using PropApplyFn = uint64_t (*)(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t item, uint64_t p5, uint64_t p6, uint64_t p7, uint64_t p8, uint64_t p9,
                                 uint64_t p10, uint64_t p11, uint64_t p12) noexcept;

enum PartId { FindList, ListCopy, MakeList, SetListStat, PropApply, PartCount };

Part g_parts[PartCount] = {
	{"ItemListFind", sig::ItemListFind, 0, false, nullptr, {}, {}},
	{"ItemListCopy", sig::ItemListCopy, 0, false, nullptr, {}, {}},
	{"ItemListMake", sig::ItemListMake, 0, false, nullptr, {}, {}},
	{"ItemSetListStat", sig::SetListStat, 0, false, nullptr, {}, {}},
	{"ItemPropApply", sig::ItemPropApply, 14, false, nullptr, {}, {}},
};

auto On(PartId id) noexcept -> bool {
	return g_parts[id].address != nullptr && g_parts[id].status.on;
}

PropApplyFn g_propOriginal = nullptr;
std::atomic<int> g_propId{-1};  // properties.txt row of 'map-level'; -1: the hook does nothing
std::atomic<int> g_statId{-1};  // itemstatcost.txt row of the map level stat
std::atomic<int> g_logged{0};

auto Readable(uint64_t p) noexcept -> bool {
	return p > 0x10000 && p < 0x00007fffffffffffull && !IsBadReadPtr(reinterpret_cast<const void*>(p), 16);
}

// The cube's output step applies each `mod` of a recipe through the property applier. When it is the map-level property, the map's stat is
// read before and after the game's own apply and corrected to "add to what the map has" (statplan::cubeMapLevelTarget).
auto PropApplyHook(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t item, uint64_t p5, uint64_t p6, uint64_t p7, uint64_t p8, uint64_t p9, uint64_t p10, uint64_t p11,
                   uint64_t p12) noexcept -> uint64_t {
	const int propId = g_propId.load(std::memory_order_relaxed);
	const int statId = g_statId.load(std::memory_order_relaxed);
	bool match = false;
	uint64_t prop = 0;
	if (propId >= 0 && statId >= 0 && g_propOriginal != nullptr) {
		for (const uint64_t cand : {p5, p9}) {
			if (!match && Readable(cand) && At<int32_t>(reinterpret_cast<const void*>(cand), 0) == propId) {
				match = true;
				prop = cand;
			}
		}
	}
	static std::atomic<int> s_seen{0};
	if (s_seen.fetch_add(1) < 60) {  // experiment: what the applier is called with
		plog::Info("prop apply #%d: cl=%u edx=%u item=%p p5=%p(%d) p9=%p(%d) propId %d", s_seen.load(), static_cast<unsigned>(a1 & 0xff), static_cast<unsigned>(a2), reinterpret_cast<void*>(item),
		           reinterpret_cast<void*>(p5), Readable(p5) ? At<int32_t>(reinterpret_cast<const void*>(p5), 0) : -1, reinterpret_cast<void*>(p9),
		           Readable(p9) ? At<int32_t>(reinterpret_cast<const void*>(p9), 0) : -1, propId);
	}
	if (!match) {
		return g_propOriginal(a1, a2, a3, item, p5, p6, p7, p8, p9, p10, p11, p12);
	}
	int before = 0;
	int after = 0;
	bool ok = false;
	try {
		before = ItemMagicStat(reinterpret_cast<void*>(item), statId, &ok);
	} catch (...) {
	}
	const uint64_t result = g_propOriginal(a1, a2, a3, item, p5, p6, p7, p8, p9, p10, p11, p12);
	try {
		after = ItemMagicStat(reinterpret_cast<void*>(item), statId, &ok);
	} catch (...) {
	}
	if (g_logged.fetch_add(1) < 200) {
		const uint8_t* b = reinterpret_cast<const uint8_t*>(prop);
		plog::Info("cube map level: item %p type %d class %d, property %02x%02x%02x%02x %02x%02x%02x%02x %02x%02x%02x%02x %02x%02x%02x%02x, stat before %d after %d (read %s)",
		           reinterpret_cast<void*>(item), Readable(item) ? At<int32_t>(reinterpret_cast<const void*>(item), 0) : -1,
		           Readable(item) ? At<int32_t>(reinterpret_cast<const void*>(item), 4) : -1, b[0], b[1], b[2], b[3], b[4], b[5], b[6], b[7], b[8], b[9], b[10], b[11], b[12], b[13],
		           b[14], b[15], before, after, ok ? "on" : "off");
	}
	return result;
}

}  // namespace

auto InstallItemStats(const D2RL::PluginContext* context, const std::vector<SignatureOverride>& overrides, std::vector<PartStatus>* parts) -> bool {
	CodeRange ranges[8];
	const int count = CodeRanges(context->exeBase, ranges, 8);
	for (Part& part : g_parts) {
		ResolvePart(context, overrides, ranges, count, part);
		plog::Info("part %s: %s", part.name, part.status.detail.c_str());
	}
	// The cube map level hook goes in at load, only when everything it calls was found. Never installed half-on.
	if (On(PropApply) && ItemStatsWritable()) {
		Part& part = g_parts[PropApply];
		const uint64_t rva = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(part.address) - context->exeBase);
		const bool hooked = context->InstallInlineHook(rva, part.guard, part.hookSize, reinterpret_cast<void*>(&PropApplyHook), reinterpret_cast<void**>(&g_propOriginal));
		(void)HookOutcome(part, hooked, g_propOriginal != nullptr);
		if (!On(PropApply)) {
			g_propOriginal = nullptr;
		}
	} else if (On(PropApply)) {
		g_parts[PropApply].status.on = false;
		g_parts[PropApply].status.detail = "off: not hooked, because a part it needs was not found";
	}
	if (parts != nullptr) {
		*parts = ItemStatsPartStatuses();
	}
	return On(FindList) && On(ListCopy);
}

void CubeMapLevelConfigure(int propertyId, int statId) {
	g_statId.store(statId);
	g_propId.store(propertyId);
	plog::Info("cube map level: %s (property row %d, stat row %d)", On(PropApply) ? "on" : "off", propertyId, statId);
}

auto ItemStatsWritable() -> bool {
	return On(FindList) && On(ListCopy) && On(MakeList) && On(SetListStat);
}

auto ItemMagicStat(void* item, int stat, bool* ok) noexcept -> int {
	std::string note;
	if (ok != nullptr) {
		*ok = On(FindList) && On(ListCopy);
	}
	for (const sp::ItemStat& s : ReadMapItemStats(item, &note)) {
		if (s.stat == stat && s.layer == 0) {
			return s.value;
		}
	}
	return 0;
}

auto SetItemMagicStat(void* rawItem, int stat, int value) noexcept -> bool {
	if (!ItemStatsWritable() || rawItem == nullptr) {
		return false;
	}
	auto* item = static_cast<Unit*>(rawItem);
	void* list = reinterpret_cast<ListMakeFn>(const_cast<uint8_t*>(g_parts[MakeList].address))(item, item, 0, MagicListFlags);
	if (list == nullptr) {
		return false;
	}
	reinterpret_cast<SetListStatFn>(const_cast<uint8_t*>(g_parts[SetListStat].address))(At<uint8_t>(item, UnitDataFlagOffset), list, stat, value, 0);
	return true;
}

auto ItemStatsPartStatuses() -> std::vector<PartStatus> {
	std::vector<PartStatus> out;
	for (const Part& part : g_parts) {
		out.push_back(part.status);
	}
	return out;
}

auto ReadMapItemStats(void* rawItem, std::string* note) noexcept -> std::vector<sp::ItemStat> {
	std::vector<sp::ItemStat> out;
	if (!On(FindList) || !On(ListCopy)) {
		if (note != nullptr) {
			*note = "the item stat parts were not found";
		}
		return out;
	}
	auto* item = static_cast<Unit*>(rawItem);
	if (item == nullptr) {
		return out;
	}
	void* list = reinterpret_cast<FindListFn>(const_cast<uint8_t*>(g_parts[FindList].address))(item, 0, MagicListFlags);
	if (list == nullptr) {
		return out;  // the item has no magic list: no stats
	}
	int32_t pairs[CopyMax * 2] = {};
	const int32_t n = reinterpret_cast<ListCopyFn>(const_cast<uint8_t*>(g_parts[ListCopy].address))(list, pairs, CopyMax);
	if (n < 0 || n > CopyMax) {
		if (note != nullptr) {
			*note = "the game copied " + std::to_string(n) + " stats, which is not possible; the item's stats are not used";
		}
		return out;
	}
	for (int32_t i = 0; i < n && i < MaxStats; ++i) {
		const int32_t key = pairs[i * 2];
		out.push_back({static_cast<int>((static_cast<uint32_t>(key) >> 16) & 0xffff), static_cast<int>(key & 0xffff), pairs[i * 2 + 1]});
	}
	if (n > MaxStats && note != nullptr) {
		*note = "the item has " + std::to_string(n) + " stats; only the first " + std::to_string(MaxStats) + " are read";
	}
	return out;
}

}  // namespace carto
