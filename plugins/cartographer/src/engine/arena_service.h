// cartographer.arena.v1: what other plugins may ask about the fixed arenas of arenas.txt and their named spots (arenaspots.txt, issue #488).
// Contract (fields, threads, versions): docs/cartographer/arena-service.md. Read-only; the tables behind it are fixed once loaded (a reload swaps them whole).
#pragma once

#include "engine/portal.h"

#include <D2RLPlugin/api.h>

#include <cstdint>
#include <memory>

namespace carto {

// What findSpot fills in. `structSize` is set by the caller to sizeof(ArenaSpotV1); new fields only ever go at the end.
struct ArenaSpotV1 {
	uint32_t structSize;
	int32_t subX, subY;                          // the spot, absolute subtiles (level origin and ring included)
	int32_t roomX, roomY, roomW, roomH;          // the room that holds it, absolute tiles
};

// The function table a consumer acquires: provider "cartographer", service "cartographer.arena.v1", version 1.
struct ArenaServiceV1 {
	uint32_t serviceSize;     // sizeof(ArenaServiceV1)
	uint32_t serviceVersion;  // 1
	bool(__cdecl* isArena)(int32_t levelId) noexcept;
	bool(__cdecl* findSpot)(int32_t levelId, const char* name, ArenaSpotV1* out) noexcept;
};

// The loaded tables (SetTables calls this). Any thread.
void ArenaServiceSetTables(std::shared_ptr<const Tables> tables);
auto ArenaServiceTables() -> std::shared_ptr<const Tables>;

// Publishes the service. Best effort: without the SDK's communication service nothing is offered, and nothing else changes.
void PublishArenaService(const D2RL::PluginContext* context) noexcept;

}  // namespace carto
