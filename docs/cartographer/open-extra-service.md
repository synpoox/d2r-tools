# cartographer.open-extra.v1

The service Cartographer publishes so another plugin can open a **second** generated map in a game that already has one open (issue #485). The second map is a red portal beside a unit, to another eligible `maps.txt` map, at the source map's monster level. The first map's state is left alone (per-level state: `level_row`, `event_6b`, `stat_apply`, `map_panel`, `maze_hooks`, `outdoor_hooks`).

- Provider: the loader plugin id `cartographer`. Service name: `cartographer.open-extra.v1`. Version: 1.
- Published once at plugin load through the SDK's `PluginCommunicationService`. Acquire it lazily (at game start or on first use), never in your own load, like `cartographer.arena.v1`. No service means no Cartographer or an older one: refuse the portal.
- **Game thread only** (unlike `arena.v1`): the call opens a portal, reads the game's counters and writes the game's state. Call it from a game-thread hook, such as the consumer's `OnMonsterDeath`.

The two structs and the name and version constants are in `plugins/cartographer/include/cartographer_open_extra_v1.h`, to copy into a consumer.

```cpp
struct OpenExtraResultV1 {
	uint32_t structSize;   // the caller sets sizeof(OpenExtraResultV1)
	int32_t level;         // the destination slot level when the portal opened, else -1
	int32_t monsterLevel;  // the destination's monster level, else 0
	char why[256];         // empty when opened, else the refusal sentence (also logged)
};

struct OpenExtraServiceV1 {
	uint32_t serviceSize;     // sizeof(OpenExtraServiceV1)
	uint32_t serviceVersion;  // 1
	bool(__cdecl* openExtra)(void* game, void* player, void* nearUnit, int32_t sourceLevel, OpenExtraResultV1* out) noexcept;
};
```

- `openExtra(game, player, nearUnit, sourceLevel, out)`: `player` is the player the portal is opened for, `nearUnit` the unit to open it beside (it may be `player`, or a monster: only its room and position are read), `sourceLevel` the level the caller stands in. True when a portal opened, or when the source already has its extra map (see below). `out->level` and `out->monsterLevel` are set either way; `out->why` holds the refusal sentence when false.
- **Refusals** (false, `why` set, and logged): the portal is off or no game is running; `sourceLevel` has no map opened through it in this game; `nearUnit` is not in `sourceLevel`; no eligible map is left (every other slot is built, taken, not installed, refused, or its `MinCharLevel` is above the player's level); the game would not open the portal.
- **One destination per source**: once a portal has opened from `sourceLevel` in this game, the same call returns true with the **same** `level` and opens no new map. A map opened from an extra map can itself open an extra map (the chain takes that map's monster level).
- The pick is random and seeded by the clock, the player's id and the game's map count. It is chosen only from `maps.txt` rows whose slot is free in this game. A row's `Size` and `Shape` stay as written.
- The item and console opens keep one map per game (`'MAPS'`, unchanged). The extra opens count in their own game counter (`'MAPX'`), which numbers each map's opening.
- The destination's return portal leads back to the source level, not to town.

Console (`cartographer open-extra`): the next tick of the local player runs the same call from the player's level, and logs the destination or the refusal. It exists so the service can be checked in game without a consumer.

**Limit (TCP/IP):** a client that joins after an extra open learns only the newest map (map sync tells each connected client each map as it opens; a late joiner gets the last one). The host and single player are unaffected.

Versions: a newer version keeps these fields and adds new ones only at the end (`serviceSize` grows). A change that cannot keep them gets a new service name (`cartographer.open-extra.v2`).
