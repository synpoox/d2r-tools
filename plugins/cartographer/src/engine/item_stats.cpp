#include "engine/item_stats.h"

#include "core/part_signatures.h"
#include "engine/map_quality.h"
#include "engine/scan.h"
#include "log.h"

#include <atomic>
#include <cstdint>
#include <cstdio>

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
// Copies up to `max` entries of four int32 each, {layer, stat, value, 0}, from the list into `out`; returns how many.
using ListCopyFn = int32_t (*)(void* list, int32_t* out, int32_t max) noexcept;

// (owner, item, state, flags) -> the item's list with those flags, made and attached when missing.
using ListMakeFn = void* (*)(Unit* owner, Unit* item, int32_t state, int32_t flags) noexcept;
using SetListStatFn = void (*)(uint32_t dataFlag, void* list, int32_t stat, int32_t value, int32_t layer) noexcept;
constexpr int UnitDataFlagOffset = 0x1bd;  // uint8 (what the game's own data flag helper 0x34a0e0 returns)

// The generic property applier (cube mods, item generation). 12 parameters: four in registers, eight on the stack; the Property is the fifth and the ninth.
using PropApplyFn = uint64_t (*)(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t item, uint64_t p5, uint64_t p6, uint64_t p7, uint64_t p8, uint64_t p9,
                                 uint64_t p10, uint64_t p11, uint64_t p12) noexcept;

// The cube's output step (recipe -> output items): its second argument is the player, whose inventory holds the cube items (the recipe's inputs).
using CubeExecFn = uint64_t (*)(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7, uint64_t a8, uint64_t a9,
                                uint64_t a10) noexcept;
using UnitInventoryFn = void* (*)(void* unit) noexcept;
using InventoryFirstFn = void* (*)(void* inventory) noexcept;
using InventoryNextFn = void* (*)(void* item) noexcept;

enum PartId { FindList, ListCopy, MakeList, SetListStat, PropApply, CubeExec, UnitInventory, InventoryFirst, InventoryNext, PartCount };

Part g_parts[PartCount] = {
	{"ItemListFind", sig::ItemListFind, 0, false, nullptr, {}, {}},
	{"ItemListCopy", sig::ItemListCopy, 0, false, nullptr, {}, {}},
	{"ItemListMake", sig::ItemListMake, 0, false, nullptr, {}, {}},
	{"ItemSetListStat", sig::SetListStat, 0, false, nullptr, {}, {}},
	{"ItemPropApply", sig::ItemPropApply, 14, false, nullptr, {}, {}},
	{"CubeExec", sig::CubeExec, 21, false, nullptr, {}, {}},
	{"UnitInventory", sig::UnitInventory, 0, false, nullptr, {}, {}},
	{"InventoryFirst", sig::InventoryFirst, 0, false, nullptr, {}, {}},
	{"InventoryNext", sig::InventoryNext, 0, false, nullptr, {}, {}},
};

auto On(PartId id) noexcept -> bool {
	return g_parts[id].address != nullptr && g_parts[id].status.on;
}

PropApplyFn g_propOriginal = nullptr;
std::atomic<int> g_propId{-1};  // properties.txt row of 'map-level'; -1: the hook does nothing
std::atomic<int> g_statId{-1};  // itemstatcost.txt row of the map level stat
std::atomic<int> g_logged{0};
// An item's storage page is a byte in its item data; the cube's page is 3 (the game's own cube walk keeps only items on it).
constexpr int CubePageOffset = 0x55;
constexpr uint8_t CubePage = 3;
CubeExecFn g_cubeOriginal = nullptr;
std::atomic<bool> g_inCube{false};            // inside the game's cube output step
std::atomic<uint64_t> g_stampedItem{0};       // the map item just given its maps.txt Level (issue #422); its first map-level property starts from 0
std::atomic<int> g_cubeInput{-1};  // the highest map level among the cube's inputs, read when the output step starts (the game frees the inputs during it)

auto CubeInputsFound() noexcept -> bool {
	return On(CubeExec) && g_cubeOriginal != nullptr && On(UnitInventory) && On(InventoryFirst) && On(InventoryNext);
}

auto Readable(uint64_t p) noexcept -> bool {
	if (p <= 0x10000 || p >= 0x00007fffffffffffull) {
		return false;
	}
	MEMORY_BASIC_INFORMATION mbi{};
	if (VirtualQuery(reinterpret_cast<const void*>(p), &mbi, sizeof(mbi)) == 0 || mbi.State != MEM_COMMIT || (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)) != 0) {
		return false;
	}
	return p + 16 <= reinterpret_cast<uintptr_t>(mbi.BaseAddress) + mbi.RegionSize;
}

// try/catch under /EHsc does not catch an access violation, so the game-facing reads run inside __try/__except, each in a function of its own
// (a function with __try holds no C++ objects). A fault reads as "not read".
auto SafeMagicStat(void* item, int statId, int* value, bool* ok) noexcept -> bool {
	__try {
		*value = ItemMagicStat(item, statId, ok);
		return true;
	} __except (EXCEPTION_EXECUTE_HANDLER) {
		return false;
	}
}

auto SafeSetMagicStat(void* item, int statId, int value) noexcept -> bool {
	__try {
		return SetItemMagicStat(item, statId, value);
	} __except (EXCEPTION_EXECUTE_HANDLER) {
		return false;
	}
}

auto CubeInputLevel(uint64_t player, int statId) noexcept -> int;
void StampCubeOutputs(uint64_t player) noexcept;

auto CubeExecHook(uint64_t a1, uint64_t a2, uint64_t a3, uint64_t a4, uint64_t a5, uint64_t a6, uint64_t a7, uint64_t a8, uint64_t a9, uint64_t a10) noexcept -> uint64_t {
	g_stampedItem.store(0);
	g_inCube.store(true);
	g_cubeInput.store(CubeInputLevel(a2, g_statId.load(std::memory_order_relaxed)));
	const uint64_t result = g_cubeOriginal(a1, a2, a3, a4, a5, a6, a7, a8, a9, a10);
	g_cubeInput.store(-1);
	g_inCube.store(false);
	StampCubeOutputs(a2);
	return result;
}

// The highest map level stat among the items in the cube, read from the player whose inventory the cube step walks: the level the
// recipe's input map carries. The game frees the inputs during the output step, so this runs when the step starts.
auto CubeInputLevelRaw(uint64_t player, int statId) noexcept -> int {
	int best = -1;  // -1: no input map's level was read
	bool trusted = true;
	{
		void* inv = reinterpret_cast<UnitInventoryFn>(const_cast<uint8_t*>(g_parts[UnitInventory].address))(reinterpret_cast<void*>(player));
		if (!Readable(reinterpret_cast<uint64_t>(inv))) {
			return -1;
		}
		void* it = reinterpret_cast<InventoryFirstFn>(const_cast<uint8_t*>(g_parts[InventoryFirst].address))(inv);
		for (int n = 0; n < 40 && Readable(reinterpret_cast<uint64_t>(it)); ++n) {
			const uint64_t data = Readable(reinterpret_cast<uint64_t>(it)) ? At<uint64_t>(it, 0x10) : 0;
			if (At<int32_t>(it, 0) == 4 && Readable(data) && At<uint8_t>(reinterpret_cast<const void*>(data), CubePageOffset) == CubePage) {  // an item in the cube
				bool ok = false;
				const int v = ItemMagicStat(it, statId, &ok);
				trusted = trusted && ok;
				if (v > best) {
					best = v;
				}
			}
			it = reinterpret_cast<InventoryNextFn>(const_cast<uint8_t*>(g_parts[InventoryNext].address))(it);
		}
	}
	return trusted ? best : -1;
}

// Issue #422: a map the cube made (a recipe whose output is a map code, which the game builds without the item maker) has no map level stat; it
// takes its maps.txt Level now. A map that already has the stat (the reused input, or an output whose map-level mod was applied) is left alone.
void StampCubeOutputsRaw(uint64_t player) noexcept {
	void* inv = reinterpret_cast<UnitInventoryFn>(const_cast<uint8_t*>(g_parts[UnitInventory].address))(reinterpret_cast<void*>(player));
	if (!Readable(reinterpret_cast<uint64_t>(inv))) {
		return;
	}
	void* it = reinterpret_cast<InventoryFirstFn>(const_cast<uint8_t*>(g_parts[InventoryFirst].address))(inv);
	for (int n = 0; n < 40 && Readable(reinterpret_cast<uint64_t>(it)); ++n) {
		const uint64_t data = At<uint64_t>(it, 0x10);
		if (At<int32_t>(it, 0) == 4 && Readable(data) && At<uint8_t>(reinterpret_cast<const void*>(data), CubePageOffset) == CubePage) {
			StampMapOutput(it);
		}
		it = reinterpret_cast<InventoryNextFn>(const_cast<uint8_t*>(g_parts[InventoryNext].address))(it);
	}
}

void StampCubeOutputs(uint64_t player) noexcept {
	if (player == 0 || g_statId.load() < 0 || !CubeInputsFound() || !ItemStatsWritable() || !Readable(player)) {
		return;
	}
	__try {
		StampCubeOutputsRaw(player);
	} __except (EXCEPTION_EXECUTE_HANDLER) {
	}
}

auto CubeInputLevel(uint64_t player, int statId) noexcept -> int {
	if (player == 0 || statId < 0 || !CubeInputsFound() || !Readable(player)) {
		return -1;
	}
	__try {
		return CubeInputLevelRaw(player, statId);
	} __except (EXCEPTION_EXECUTE_HANDLER) {
		return -1;
	}
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
	if (!match) {
		return g_propOriginal(a1, a2, a3, item, p5, p6, p7, p8, p9, p10, p11, p12);
	}
	if (g_cubeOriginal != nullptr && !g_inCube.load() && g_stampedItem.load() == item) {  // replace only with the cube hook in place, so a missing CubeExec part cannot turn a cube add into a replace
		// A map just made with its maps.txt Level: a modder's own map-level (an affix or a unique's property) is the level, not an addition to it.
		g_stampedItem.store(0);
		const bool cleared = SafeSetMagicStat(reinterpret_cast<void*>(item), statId, 0);
		const uint64_t applied = g_propOriginal(a1, a2, a3, item, p5, p6, p7, p8, p9, p10, p11, p12);
		int now = 0;
		bool okNow = false;
		if (!SafeMagicStat(reinterpret_cast<void*>(item), statId, &now, &okNow)) {
			okNow = false;
		}
		plog::Info("map level line: the item's own map-level property gives Area Level %d, replacing its maps.txt Level (%s)", okNow ? now : -1, cleared ? "set" : "reset failed");
		return applied;
	}
	int before = 0;
	int after = 0;
	bool okBefore = false;
	bool okAfter = false;
	if (!SafeMagicStat(reinterpret_cast<void*>(item), statId, &before, &okBefore)) {
		okBefore = false;
	}
	const uint64_t result = g_propOriginal(a1, a2, a3, item, p5, p6, p7, p8, p9, p10, p11, p12);
	if (!SafeMagicStat(reinterpret_cast<void*>(item), statId, &after, &okAfter)) {
		okAfter = false;
	}
	// The recipe's range is the Property's min and max dwords; a range rolls once per transmute.
	const int pmin = At<int32_t>(reinterpret_cast<const void*>(prop), 8);
	const int pmax = At<int32_t>(reinterpret_cast<const void*>(prop), 12);
	// A game result inside the range is the game's own roll and stands; otherwise roll here with the plugin's mixer (statplan::cubeRoll), not a clock modulo.
	const int roll = (after >= pmin && after <= pmax) ? after : sp::cubeRoll(pmin, pmax, sp::mixSeed(GetTickCount64(), static_cast<uint32_t>(item >> 4), static_cast<uint32_t>(g_logged.load())));
	const int inputLevel = g_cubeInput.load();
	// Fail open: both reads of the item and the cube inputs walk must have worked. A level read as 0 (a fresh map) counts; an unread one does not.
	// A map with no stat yet (one made before #422, or a fresh cube output) is at its maps.txt Level, which the recipe's value is added to.
	int base = before > inputLevel ? before : inputLevel;
	if (base < 1) {
		base = MapsLevelOfItem(reinterpret_cast<void*>(item));
	}
	const bool ok = okBefore && okAfter;
	const bool known = sp::cubeLevelKnown(ok, inputLevel, On(CubeExec));
	const int target = sp::cubeMapLevelTarget(base, after == before ? base : after, pmin, pmax, roll, known);
	bool fixed = false;
	if (target != after) {
		fixed = SafeSetMagicStat(reinterpret_cast<void*>(item), statId, target);
	}
	int readback = after;
	bool okReadback = false;
	if (fixed && !SafeMagicStat(reinterpret_cast<void*>(item), statId, &readback, &okReadback)) {
		readback = -1;
	}
	if (g_logged.fetch_add(1) < 200) {
		plog::Info("cube map level: item %p class %d, range %d..%d: stat before %d after %d, cube input %d, target %d, now %d (%s; read %s)", reinterpret_cast<void*>(item),
		           Readable(item) ? At<int32_t>(reinterpret_cast<const void*>(item), 4) : -1, pmin, pmax, before, after, inputLevel, target, readback,
		           fixed ? "corrected" : "left as the game set it", ok ? "on" : "off");
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
	// The cube's output step is hooked only to learn the cube (the recipe's inputs); without it, or the three walkers, the level adds to the output's own stat only.
	if (On(PropApply) && On(CubeExec) && On(UnitInventory) && On(InventoryFirst) && On(InventoryNext)) {
		Part& part = g_parts[CubeExec];
		const uint64_t rva = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(part.address) - context->exeBase);
		const bool hooked = context->InstallInlineHook(rva, part.guard, part.hookSize, reinterpret_cast<void*>(&CubeExecHook), reinterpret_cast<void**>(&g_cubeOriginal));
		(void)HookOutcome(part, hooked, g_cubeOriginal != nullptr);
		if (!On(CubeExec)) {
			g_cubeOriginal = nullptr;
		}
	}
	plog::Info("part %s: %s; part %s: %s", g_parts[PropApply].name, g_parts[PropApply].status.detail.c_str(), g_parts[CubeExec].name, g_parts[CubeExec].status.detail.c_str());
	if (parts != nullptr) {
		*parts = ItemStatsPartStatuses();
	}
	return On(FindList) && On(ListCopy);
}

void ClearStampedMap() {
	g_stampedItem.store(0);
}

void NoteStampedMap(void* item) {
	g_stampedItem.store(reinterpret_cast<uint64_t>(item));
}

void CubeMapLevelConfigure(int propertyId, int statId) {
	g_statId.store(statId);
	g_propId.store(propertyId);
	plog::Info("cube map level: property id %d, stat row %d (the hook status is in the part lines)", propertyId, statId);
}

auto ItemStatsWritable() -> bool {
	return On(FindList) && On(ListCopy) && On(MakeList) && On(SetListStat);
}

auto ItemMagicStat(void* item, int stat, bool* ok) noexcept -> int {
	std::string note;
	const std::vector<sp::ItemStat> stats = ReadMapItemStats(item, &note);
	for (const sp::ItemStat& s : stats) {
		if (s.stat == stat && s.layer == 0) {
			if (ok != nullptr) {
				*ok = On(FindList) && On(ListCopy);
			}
			return s.value;
		}
	}
	// Not found: 0 only counts as the item's level when the list was read whole (no note: parts present, layout recognised, at most MaxStats stats).
	if (ok != nullptr) {
		*ok = On(FindList) && On(ListCopy) && note.empty();
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
	int32_t pairs[CopyMax * 4] = {};
	const int32_t n = reinterpret_cast<ListCopyFn>(const_cast<uint8_t*>(g_parts[ListCopy].address))(list, pairs, CopyMax);
	if (n < 0 || n > CopyMax) {
		if (note != nullptr) {
			*note = "the game copied " + std::to_string(n) + " stats, which is not possible; the item's stats are not used";
		}
		return out;
	}
	std::string problem;
	out = sp::decodeItemStatList(pairs, n, MaxStats, &problem);
	if (!problem.empty()) {
		plog::Warn("%s", problem.c_str());
		if (note != nullptr) {
			*note = problem;
		}
		return out;
	}
	if (n > MaxStats && note != nullptr) {
		*note = "the item has " + std::to_string(n) + " stats; only the first " + std::to_string(MaxStats) + " are read";
	}
	return out;
}

}  // namespace carto
