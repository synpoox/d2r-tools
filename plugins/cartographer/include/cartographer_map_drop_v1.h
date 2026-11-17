// cartographer.map-drop.v1, the part another plugin copies: the structs and the names (issue #538). Nothing else is needed to acquire the service;
// contract (threads, lifetime, versions): docs/cartographer/map-drop-service.md. New fields only ever go at the end.
#pragma once

#include <cstdint>

namespace carto {

inline constexpr const char* MapDropServiceProvider = "cartographer";  // the loader plugin id
inline constexpr const char* MapDropServiceName = "cartographer.map-drop.v1";
inline constexpr uint32_t MapDropServiceVersion = 1;

// The map's drop behaviours as they were when the item dropped (the values Cartographer applied; 0 = off).
struct MapDropStatsV1 {
	uint32_t structSize;  // sizeof(MapDropStatsV1)
	int32_t levelId;      // the map level the monster died on
	int32_t dropBonusPct;
	int32_t etherealPct;
	int32_t socketedPct;
	int32_t extraBoss;
};

// Called once for every item a hostile, non-minion monster of a map level drops, after the item exists and after Cartographer's own
// ethereal and socket rolls. `item` is the game's item unit (never null). Game thread; must not block and must not free the item.
using MapDropCallbackV1 = void(__cdecl*)(void* item, const MapDropStatsV1* stats, void* user) noexcept;

// The function table a consumer acquires: provider "cartographer", service "cartographer.map-drop.v1", version 1.
struct MapDropServiceV1 {
	uint32_t serviceSize;     // sizeof(MapDropServiceV1)
	uint32_t serviceVersion;  // 1
	// Returns false (handle untouched) for a null pluginId, callback or handle.
	bool(__cdecl* registerCallback)(const char* pluginId, MapDropCallbackV1 callback, void* user, uint32_t* handle) noexcept;
	bool(__cdecl* unregisterCallback)(uint32_t handle) noexcept;  // false for an unknown handle
};

}  // namespace carto
