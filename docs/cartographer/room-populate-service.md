# cartographer.room-populate.v1

The service Cartographer publishes so another plugin can run code around every room the game populates, and skip the game's own monsters in one room ([ADR 0001](adr-0001-cartographer-standalone.md), 2026-11-15 amendment; issue #487). It is needed where the game's room populate is already hooked by Cartographer, which the loader lets only one plugin hook.

- Provider: the loader plugin id `cartographer`. Service name: `cartographer.room-populate.v1`. Version: 1.
- Published at plugin load through the SDK's `PluginCommunicationService`, and only when Cartographer's `PopulateRoom` hook is live (no hook, no service). A consumer acquires it lazily, at game start or on first use, never in its own load (plugin load order is not defined). With no service, the consumer keeps its own fallback.
- It covers **every** room the hook sees, of every level, not only Cartographer's own maps.
- Game thread only: callbacks run inside the room populate hook, on the thread that populates the room.

The structs and the name and version constants are in `plugins/cartographer/include/cartographer_room_populate_v1.h`, to copy into a consumer.

```cpp
struct RoomPopulateCallbacksV1 {
	uint32_t structSize;  // sizeof(RoomPopulateCallbacksV1), set by the caller
	uint32_t(__cdecl* before)(void* game, void* room, int32_t levelId, void* user) noexcept;          // before the game's populate; returns flags
	void(__cdecl* after)(void* game, void* room, int32_t levelId, bool skipped, void* user) noexcept; // after it
};

struct RoomPopulateServiceV1 {
	uint32_t serviceSize;     // sizeof(RoomPopulateServiceV1)
	uint32_t serviceVersion;  // 1
	bool(__cdecl* subscribe)(const char* pluginId, const RoomPopulateCallbacksV1* callbacks, void* user, uint32_t* handle) noexcept;
	bool(__cdecl* unsubscribe)(uint32_t handle) noexcept;
};
```

- `subscribe(pluginId, callbacks, user, &handle)`: true and `handle` set on success. False, with `handle` untouched, for a null `pluginId`, `callbacks` or `handle`, or a `callbacks->structSize` below `sizeof(RoomPopulateCallbacksV1)`. Either callback may be null. `user` is passed back to both.
- `unsubscribe(handle)`: true when the handle was live; false for an unknown or already removed handle. Allowed from inside a callback: it takes effect at once. From another thread it returns only after the current populate has finished.
- `before` returns flags. `RoomPopulateSkipNative` (1) skips the game's own populate of that room, so the room has none of the game's monsters. Cartographer's own work for the room still runs (its arena spots, its map events and Boss). The flags of all subscribers are ORed; one `SkipNative` is enough.
- Subscribers run in subscribe order. `before` runs before the game's populate; `after` runs after Cartographer's own work for the room, with `skipped` telling whether the game's populate was skipped.
- A C++ exception from a callback drops that subscriber: its plugin id is logged once as an error, and the other subscribers still run. An access violation inside a callback is **not** caught by that; the crash guard logs it and the game may end. Keep callbacks small and safe.
- A consumer unsubscribes in its own unload, or the callbacks run after the consumer's code is gone.
- Versions: a newer version keeps these fields and adds new ones only at the end (`serviceSize` grows); a change that cannot keep them gets a new service name. A consumer that needs a field checks `serviceSize`.
- Game check knob (not for players): `CARTOGRAPHER_TEST_SKIP_NATIVE=<levelId>` set before the game starts makes Cartographer skip the native populate of every room of that level, through this same service. It logs a warning when set and one `room-populate test` line per skipped room.
