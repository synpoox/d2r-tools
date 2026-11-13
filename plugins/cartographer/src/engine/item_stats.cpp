#include "engine/item_stats.h"

#include "core/part_signatures.h"
#include "engine/scan.h"
#include "log.h"

#include <cstdint>
#include <cstdio>

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
// Copies up to `max` entries of four int32 each, {layer, stat, value, 0}, from the list into `out`; returns how many.
using ListCopyFn = int32_t (*)(void* list, int32_t* out, int32_t max) noexcept;

enum PartId { FindList, ListCopy, PartCount };

Part g_parts[PartCount] = {
	{"ItemListFind", sig::ItemListFind, 0, false, nullptr, {}, {}},
	{"ItemListCopy", sig::ItemListCopy, 0, false, nullptr, {}, {}},
};

auto On(PartId id) noexcept -> bool {
	return g_parts[id].address != nullptr && g_parts[id].status.on;
}

}  // namespace

auto InstallItemStats(const D2RL::PluginContext* context, const std::vector<SignatureOverride>& overrides, std::vector<PartStatus>* parts) -> bool {
	CodeRange ranges[8];
	const int count = CodeRanges(context->exeBase, ranges, 8);
	for (Part& part : g_parts) {
		ResolvePart(context, overrides, ranges, count, part);
		plog::Info("part %s: %s", part.name, part.status.detail.c_str());
	}
	if (parts != nullptr) {
		*parts = ItemStatsPartStatuses();
	}
	return On(FindList) && On(ListCopy);
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
	int32_t pairs[CopyMax * 4] = {};
	const int32_t n = reinterpret_cast<ListCopyFn>(const_cast<uint8_t*>(g_parts[ListCopy].address))(list, pairs, CopyMax);
	if (n < 0 || n > CopyMax) {
		if (note != nullptr) {
			*note = "the game copied " + std::to_string(n) + " stats, which is not possible; the item's stats are not used";
		}
		return out;
	}
	for (int32_t i = 0; i < n && i < MaxStats; ++i) {
		const int32_t* e = pairs + i * 4;  // D2RLoader's copy writes 16 bytes an entry: {layer, stat, value, 0}
		out.push_back({static_cast<int>(e[1]), static_cast<int>(e[0]), e[2]});
	}
	if (n > MaxStats && note != nullptr) {
		*note = "the item has " + std::to_string(n) + " stats; only the first " + std::to_string(MaxStats) + " are read";
	}
	return out;
}

}  // namespace carto
