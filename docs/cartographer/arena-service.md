# cartographer.arena.v1

The content service Cartographer publishes for other plugins ([ADR 0001](adr-0001-cartographer-standalone.md), 2026-11-15 amendment). It answers questions about the fixed arenas of `arenas.txt` and their named spots from `arenaspots.txt` ([tables.md](tables.md#arenaspotstxt-optional)).

- Provider plugin id: `cartographer`. Service name: `cartographer.arena.v1`. Version: 1.
- Published once at plugin load through the SDK's `PluginCommunicationService`. A consumer acquires it like any service (`D2RL::PluginCommunication::Acquire`) with a minimum table size of `sizeof(ArenaServiceV1)` as below. No service means no Cartographer or an older one: refuse the fight.
- Read-only. The tables behind it are fixed after load (`cartographer check` swaps in a new set whole), so every call is safe on any thread, including the game thread inside a hook. Calls do not touch game memory.

```cpp
struct ArenaSpotV1 {
	uint32_t structSize;                 // the caller sets sizeof(ArenaSpotV1)
	int32_t  subX, subY;                 // the spot: absolute subtiles (5 per tile), the level origin and the ring included
	int32_t  roomX, roomY, roomW, roomH; // the room that holds it: an absolute tile rectangle (the 8 x 8 grid cell)
};

struct ArenaServiceV1 {
	uint32_t serviceSize;     // sizeof(ArenaServiceV1)
	uint32_t serviceVersion;  // 1
	bool(__cdecl* isArena)(int32_t levelId) noexcept;
	bool(__cdecl* findSpot)(int32_t levelId, const char* name, ArenaSpotV1* out) noexcept;
};
```

- `isArena(levelId)`: true when `arenas.txt` has a loaded row for that `levels.txt` Id.
- `findSpot(levelId, name, out)`: true and `out` filled when the arena on that level has a spot of that name (ignoring case). False, with `out` untouched, for an unknown level or name, a null pointer or an `out->structSize` smaller than `sizeof(ArenaSpotV1)`.
- A spot is the centre of its tile: `subX = tile * 5 + 2`. `findSpot("centre")` is what a consumer reads instead of working out a level origin.
- Versions: a newer version keeps these fields and adds new ones only at the end (`serviceSize` grows); a change that cannot keep them gets a new service name (`cartographer.arena.v2`). A consumer that needs a field checks `serviceSize`.
- Cartographer never guesses: a consumer that finds no spot refuses its fight rather than inventing one.
