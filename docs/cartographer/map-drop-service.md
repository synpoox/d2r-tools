# cartographer.map-drop.v1

The service Cartographer publishes so another plugin can act on every item a map monster drops (issue #538). Cartographer applies the generic map drop behaviours itself (`drop_tc`, `drop_bonus`, `drop_ethereal`, `drop_socketed`, `extra_boss`; [tables.md](tables.md#map-drop-behaviours)); a mod's own rules (a corruption chance, an ear, a tag) register here and run on each item.

- Provider: the loader plugin id `cartographer`. Service name: `cartographer.map-drop.v1`. Version: 1.
- Published once at plugin load through the SDK's `PluginCommunicationService`. Acquire it lazily, never in your own load. No service means no Cartographer or an older one.
- **Game thread only**: the callback runs inside the game's drop code.

The structs and the name and version constants are in `plugins/cartographer/include/cartographer_map_drop_v1.h`, to copy into a consumer.

```cpp
struct MapDropStatsV1 { uint32_t structSize; int32_t levelId, dropBonusPct, etherealPct, socketedPct, extraBoss; };
using MapDropCallbackV1 = void(__cdecl*)(void* item, const MapDropStatsV1* stats, void* user) noexcept;
struct MapDropServiceV1 {
	uint32_t serviceSize, serviceVersion;  // sizeof, 1
	bool(__cdecl* registerCallback)(const char* pluginId, MapDropCallbackV1 callback, void* user, uint32_t* handle) noexcept;
	bool(__cdecl* unregisterCallback)(uint32_t handle) noexcept;
};
```

- The callback runs once per item, for hostile, non-minion monsters killed on a map level that has behaviours set; the item exists and Cartographer's ethereal and socket rolls are done. `item` is the game's item unit, never null; do not free it. A map with no behaviours set never calls back.
- `stats` is what the map was set to (0 = off), not what happened to this item. `levelId` is the map's slot level.
- Several callbacks run in registration order. `registerCallback` returns false for a null id, callback or handle; the handle is for `unregisterCallback`.
- New fields only ever go at the end of a struct; a new version is a new service name.

## Setting the behaviours

`SetMapBehaviours(level, MapBehaviours)` in `src/engine/map_behaviours.h` replaces the behaviours of the map opened on `level`; an empty struct clears it. The stat router (issue #537) calls it once per opened map. `CARTOGRAPHER_TEST_BEHAVIOURS` (a game check only) sets it for every map opened: `tc=<name>:<pct>;bonus=<pct>;eth=<pct>;sock=<pct>;boss=<0..9>`.
