#include "arcane_void.h"

#include "core/part_signatures.h"
#include "game_memory.h"
#include "log.h"
#include "maze_hooks.h"

#include <cstdint>
#include <cstring>

namespace carto {

namespace {
// The fill code's tile words (0x3f3582): main index 30 in bits 20..25, sub index in bits 8..15. Level 74 gets the second.
constexpr uint64_t BlankFillTile = 0x1e00000;
constexpr uint64_t ArcaneFillTile = 0x1e00100;
constexpr int RoomLevel = 0x90;   // DRLG room -> its level
constexpr int LevelId = 0x1f8;    // level -> its levels.txt id (int32)

// A native hook: no PluginSDK service chooses a room's tiles, and the level-74 test is a cmp the SDK cannot widen to the slot level.
using TileLookupFn = uint64_t (*)(void* room, uint64_t orientation, uint64_t tile) noexcept;
TileLookupFn g_lookupOriginal = nullptr;
Part g_lookup = {"RoomTileLookup", sig::RoomTileLookup, 18, false, nullptr, {}, {}};

auto RoomOnArcaneSlot(const void* room) noexcept -> bool {
	if (room == nullptr || !Readable(static_cast<const uint8_t*>(room) + RoomLevel, sizeof(void*))) {
		return false;
	}
	const void* level = nullptr;
	std::memcpy(&level, static_cast<const uint8_t*>(room) + RoomLevel, sizeof(level));
	if (level == nullptr || !Readable(static_cast<const uint8_t*>(level) + LevelId, sizeof(int32_t))) {
		return false;
	}
	int32_t id = 0;
	std::memcpy(&id, static_cast<const uint8_t*>(level) + LevelId, sizeof(id));
	return IsArcaneSlotLevel(id);
}

auto TileLookupHook(void* room, uint64_t orientation, uint64_t tile) noexcept -> uint64_t {
	if (orientation == 0 && tile == BlankFillTile && RoomOnArcaneSlot(room)) {
		tile = ArcaneFillTile;
	}
	return g_lookupOriginal(room, orientation, tile);
}
}  // namespace

void InstallArcaneVoidTile(const D2RL::PluginContext* context, const std::vector<SignatureOverride>& overrides, const CodeRange* ranges, int count) {
	try {
		ResolvePart(context, overrides, ranges, count, g_lookup);
		if (!g_lookup.status.on) {
			plog::Warn("arcane void tile: the room tile lookup was not found; Arcane maps' void blocks missiles");
			return;
		}
		const uint64_t rva = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(g_lookup.address) - context->exeBase);
		const bool hooked = context->InstallInlineHook(rva, g_lookup.guard, g_lookup.hookSize, reinterpret_cast<void*>(&TileLookupHook), reinterpret_cast<void**>(&g_lookupOriginal));
		if (HookOutcome(g_lookup, hooked, g_lookupOriginal != nullptr)) {
			plog::Debug("arcane void tile: room tile lookup hooked; the Arcane slot's blank cells get level 74's void tile");
		} else {
			plog::Warn("arcane void tile: the hook was refused (see 'cartographer check'); Arcane maps' void blocks missiles");
		}
	} catch (...) {
		plog::Error("arcane void tile: installing the hook failed; Arcane maps' void blocks missiles");
	}
}

auto ArcaneVoidTilePartStatus() -> PartStatus {
	return g_lookup.status;
}

}  // namespace carto
