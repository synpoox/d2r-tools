## 12. Multiplayer map sync engine checks (issue #98)

Desk research for phase 7 (multiplayer): four questions about syncing a drawn map to a TCP/IP or open-game client. Read from the build 93847 dump
with `dump_dis.py` (Capstone) and from the pinned SDK (`third_party/PluginSDK` at 717f727); nothing was run in a game and no code changed. Each claim is
marked **static** (read from dump bytes or tables), **SDK** (read from the pinned headers), **inference** (follows from those, not observed) or
**needs game** (with the check). `D2RCore.dll` is not in the dump, so anything the loader or core does on the wire is not covered. Battle.net and the
outdoor builder are out of scope.

| Field | Record |
|---|---|
| Date | 2026-11-12 |
| Consumer | Cartographer phase 7 (multiplayer map sync); part 1 (the host tells clients the open map) is built, see "What was built (issue #125)" at the end of this section |
| Module | `D2R.exe`, D2R 3.3 build 93847, stamp `0x6ab3782c`, under D2RLoader 1.3.1 |
| Location | The table below. Final addresses are found by byte signature |
| Guard | Every prefix in the table below was re-run with an overlapping search over the dump's executable sections (`??` is a relative operand) and matches exactly once. The first draft's `0x86bc0` prefix matched 4 times and `0x490300` 3 times; both are now the longer unique forms. `0x4475c0` is not a hook target here (see Q4) |
| Evidence | `dump_dis.py <dump> --stamp 0x6ab3782c dis <rva> [count]` and `pat "<hex>"`; callers found by scanning `E8`/`E9` targets. SDK headers `network.h`, `lifecycle.h`, `lifecycle_events.h`, `threads.h` and `examples/network-ping` |
| Validation | None in a game. Static and SDK reading only |
| Failure mode | If a check below fails, multiplayer stays off for Cartographer: the plugin refuses to open a map for a remote client and logs why |
| Coexistence | The packet table entries that are D2RLoader thunks into D2RCore (ids 6, 24, 96, 130, 135, 149, 150) are not touched. `NetworkService` is the loader's own route and needs no hook. `0x44db20` is already hooked by Monster Count and Better Auras, so any Q4 fallback hook there must chain, not replace |

| RVA | Role | Prefix |
|---|---|---|
| `0x58f680` | Portal operate (object operate slot 15, server side) | `48 89 4C 24 08 55 53 56 57 41 56 41 57` |
| `0x592c40` | Object operate dispatcher (table `0x23a51c0`) | `48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18 57 48 83 EC 50 48 8B F1` |
| `0x129930` | Client packet `0x03` (LoadAct) handler | `48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18 57 48 83 EC 30 8B 79 02` |
| `0x129b80` | Client packet `0x07` handler (lazy room build trigger) | `48 89 5C 24 10 48 89 6C 24 18 56 57 41 56 48 83 EC 30 0F B7 69 01` |
| `0x86bc0` | Client act load (`isClient = 1`, seed from the packet) | `48 89 5C 24 20 55 56 57 41 54 41 55 41 56 41 57 48 81 EC A0 00 00 00 48 8B 05 ?? ?? ?? ?? 48 33 C4 48 89 84 24 98 00 00 00 44 8B A4 24 00 01 00 00` |
| `0x86e20` | Client unit placement (second lazy room build trigger) | `48 89 5C 24 18 55 57 41 54 41 56 41 57 48 83 EC 40 48 8B D9` |
| `0x2ef1c0` | Act creation (runs the DRLG for the act) | `48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18 48 89 7C 24 20 41 56 48 83 EC 50 44 0F B6 F1` |
| `0x48aa50` | Server act creation (`isClient = 0`, seed from `game + 0x12c`) | `48 89 5C 24 18 48 89 6C 24 20 56 57 41 56 48 83 EC 60 0F B6 EA` |
| `0x2ef320` | World room lookup stub, tail-jumps to `0x328680` (callers `0x86ebe`, `0x86f98`, `0x129bf1`) | `48 8B 44 24 30 48 85 C0 ?? ?? 48 8B 40 18 48 8B 52 70 48 89 44 24 30 ?? ?? ?? ?? ?? CC CC CC CC 48 89 5C 24 08` |
| `0x4817f0` | Server send `(clientCtx, buf, len)` | `40 55 56 57 48 83 EC 40 49 8B E8 48 8B F2` |
| `0x44db20` | Unit death (`D2Game\src\Unit\SUnitDmg.cpp`) | `48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18 57 41 56 41 57 48 83 EC 50 48 8B E9` |
| `0x4475c0` | `(game, info, flag)`, called from the death path and 46 other places; returns 0 unless the unit at `[info + 8]` (the victim) has type 1 (a monster; a player is 0) | `40 53 55 56 41 55 48 83 EC 38 48 8B 5A 08` |
| `0x490300` | Missile owner resolver | `48 89 74 24 18 57 48 83 EC 20 48 8B FA 48 8B F1 48 85 D2 75 20 48 8D 4C 24 38 88 54 24 38 E8 ?? ?? ?? ?? 84 C0 74 1F CC` (the longer form recorded in [dps-counter.md](../dps-counter.md)) |

Already recorded elsewhere and relied on here: maze level build `0x3271c0` and room lookup `0x327400` (section 5), spawn calls `0x492cf0` and `0x499ba0` and the room
populator `0x50acf0` (section 9), the once-per-game superunique bitmap at `game + 0x1d8` (section 6).

### Q1. How can a plugin send a small message host to client and back?

- **SDK (primary route).** `NetworkService` (`network.h`) is the supported route for private plugin messages. A plugin registers a channel with a 64-bit
  compatibility token, connects to the host (`connectToHost`), sends to the host (`sendToHost`) or to one client peer (`sendToClient`), and receives on host and
  client callbacks. A payload is at most 224 bytes. `getPeerPlayer` maps a peer to a player and must be called on the game thread. The `network-ping` example is
  the pattern to copy. No engine address or hook is needed for it.
- **SDK limits.** Not available on Battle.net (out of scope). Plugin roles are Client, Server or Shared; Cartographer is Shared with native hooks, and Shared
  plugins must match between host and client. `runOnGameThread` returns Unavailable on a remote TCP/IP client (`threads.h`: a remote client has no local
  authoritative game thread), so client side work has to run from a hook or a tick the plugin already owns, not from a hop the SDK cannot give. `connectToHost` is called only after entering a game and may return `Busy` on a TCP client until the host's transport packet has arrived, so the client retries; the plan cannot reach the client earlier than that. Per the SDK README, Shared plugins must match in TCP/IP before a client can join at all, so a client without the plugin never gets as far as a map (this mostly answers Q3.3; the rest is the match check itself, **SDK**).
- **Static (fallback, the game's own packet path).** The client packet handler table is at RVA `0x22bbc40`, stride `0x18`, entries `{handler, size, third}`,
  index = classic packet id. Most entries have the handler first and a zero third field, but ids 12 to 16, 23, 36, 76, 77, 103 to 111, 113 and 114 hold the stub `0x78d20` in the first field and the real handler in the third; ids 110 and 111 (size 1) have the stub in the third field too, so they have no real handler (**static**). Seven entries are D2RLoader thunks into D2RCore (ids 6, 24, 96, 130, 135, 149, 150); the loader's own transport packet id was not
  located (D2RCore is not in the dump). The server sends with `0x4817f0(clientCtx, buf, len)`. A plugin-made packet would need a free packet id, a handler table
  entry and a size, which collides with the loader's thunks and with other plugins. **Not recommended**; consider it only if the SDK route fails a check below.
- **Needs game.**
  1. A Shared plugin with a channel on a host and one remote TCP/IP client: does the connection complete, and how many `Busy` retries does `connectToHost` take on a TCP client, and what does the connection event say when the client
     has a different compatibility token (reject reason)? (A client without the plugin is expected to be refused at join by the SDK's Shared rule; confirm.)
  2. A host to client message of 224 bytes: is it delivered intact, and on which thread does the client callback run?
  3. Does the host see the client's reply on its server thread, and can `getPeerPlayer` be called there?
- **Recommended route.** `NetworkService`, one channel, a token derived from the plugin's wire version. The host sends the map plan (seed, map id, a few bytes)
  before the client builds the level; the client acks. Fall back to "map off" for a client whose connection is rejected.

### Q2. Where does a TCP/IP or open-game client build the slot level, and do the maze hooks run on the client, the server, or both?

- **Static: both sides run their own DRLG.** Each act is created by `0x2ef1c0`. The server calls it from `0x48aa50` with `isClient = 0` and the seed at `game + 0x12c`.
  The client calls it from the packet `0x03` handler `0x129930` through `0x86bc0` with `isClient = 1` and the seed taken from the packet. Level seeds are
  deterministic from the act seed: `drlg + 0x860 + level id` through `0x367110`. The same level id and act seed give the same layout on both sides, which is the
  property the plan cache in `maze_hooks.cpp` ("per game, so the server's build and the client's build get the same plan") relies on.
- **Static: client rooms are built lazily.** On the client a room is built through `0x2ef320` -> `0x328680` -> `0x327400` -> `0x3271c0` when the client needs it. Two
  triggers were found: the packet `0x07` handler `0x129b80` and the unit placement function `0x86e20` (the stub's callers are `0x129bf1`, `0x86ebe`, `0x86f98`).
  `0x3271c0` itself has exactly 5 direct callers (`0x32705e`, `0x327525`, `0x3609cf`, `0x3da6c3`, `0x3dd5f6`; E8/E9 scan, **static**). Which of them can run on a client was not worked out beyond the two
  triggers above, so "other triggers" is bounded by those 5 plus any indirect call; classify the other three before relying on "plan before the first lazy build".
- **Static consequence.** `LevelBuild` (`0x3271c0`) and the maze hooks run on **both** the server and the client, each against its own DRLG instance. In single player
  both DRLGs live in one process, so the hook runs twice for one level (already true today). In a TCP/IP game it runs once per DRLG: the host has both, a remote client
  has only the client DRLG. A remote client therefore has the plan only if the plugin put it there before the first lazy build, i.e. before the packet `0x03` / `0x07`
  handlers or the unit placement run for the slot level.
- **Not located.** The server side sender of packet `0x03` (LoadAct) and of packet `0x07`: callers of `0x4817f0` with those ids were not found (only unrelated ids
  `0x21` and `0x1d` to `0x1f`); they may go through D2RCore or a table. The receiving handlers are static, so the answer above stands, but the exact host moment at
  which a client is told to load the act is unknown, which matters for when the plan message must be sent.
- **Inference.** The host must send the plan for the destination map when a client is about to enter a map portal (Q3 gives the moment), and the client's plan cache
  must be keyed like the host's. The host's own build is unchanged.
- **Needs game.**
  1. Log `0x3271c0` entry (level id, thread) on a host and on a remote client while the client enters the slot level; expect host server DRLG, host client DRLG and remote
     client DRLG.
  2. Does the remote client reach `0x3271c0` for the slot level before or after the plugin message arrives? If before, it builds the stock slot layout.
  3. Does the lazy build ever run on a thread other than the client main thread?
  4. Log `0x4817f0` ids when a client enters an act and when it enters the slot level, to find the LoadAct sender.
  5. Log which of the 5 callers of `0x3271c0` fire on a remote client for the slot level.

### Q3. How can the portal entry be held until a client confirms?

- **Static: the gate point is the server portal operate.** Objects 59 and 60 (the red portal) use OperateFn 15, slot 15 of the table at `0x23a51c0`, which is the
  server function `0x58f680`, reached through the dispatcher `0x592c40`. It runs where the object is operated (the host). It takes one pointer, the operate context the
  dispatcher builds on its stack (`lea rcx, [rsp+0x20]; call rdi` at `0x592cbf`): `[ctx + 8]` is the object (its `[unit + 0xc]`, read through `0x34ab60`, must be 2, `0x58f69c`)
  and `[ctx + 0x10]` is the operating player (`0x34b9d0` must return 0, a player, `0x58f6b5`; that check is an assert, an `int3` at `0x58f6cf` when the type is not a player and `0x592580` is true, not a refusal). Only the object check (`[unit + 0xc]` not 2, `0x58f6a7`) takes the refusal path at `0x58fcd4`, which calls `0x491960(player, 0x14, player)` (the third argument is also the player) and
  returns 0 (`r15d`), the existing way the game declines an operate; that call is probably what the client sees (**inference**, callee unread).
- **Inference (route).** Hook `0x58f680` (the 13-byte prefix above is unique). For the red portal and a client the plugin has not marked as synced: send the plan over
  `NetworkService` (Q1), return 0, and when the client acks, mark it synced and let the next click through (or call the original). Hooking the dispatcher `0x592c40` sees only the skill path, never a click (clicks go via `0x593630`; [section 18](18-chest-operate.md)), so it is not a click gate.
- **Route: player to peer.** The gate sees a player unit, but `sendToClient` needs a `PeerHandle`, and `getPeerPlayer` resolves peer to player only from a scheduled host
  game-thread callback (**SDK**). So on each Connected event the host queues `getPeerPlayer` and keeps a player id to peer map; a player with no entry is treated as not synced
  (the gate refuses, or the host's own player skips it).
- **Needs game.**
  1. The context layout above (object `[ctx + 8]`, player `[ctx + 0x10]`) is **static**; confirm in a game by logging both at the hook.
  2. Does a refused operate (the `0x491960(player, 0x14)` call) show the player anything, and does the client retry or must the player click again? A "waiting for client" line through section 9's chat
     path (`0x477a30`) is the likely feedback.
  3. A client that never acks (not running the plugin): timeout, then stock behaviour or refusal? A Shared plugin must match to join (Q1), so this is
     mostly a client that connected but never acks (version skew or a lost message).
  4. The host's own player can skip the wait (same process, the plan is already there).
- **Recommended route.** Gate at `0x58f680` with an ack from `NetworkService`, one pending entry per portal and client, with a timeout.

### Q4. What does kill credit need for monsters spawned by map events in multiplayer?

Result: mostly **needs game**. The death path was read far enough to say which unit is which, not far enough to say who is credited.

- **Static: the death function.** `0x44db20(game, victim, killer, flag)` (`rbp = game`, `rbx = victim`, `rdi = killer`, `r14d = flag`). In the block `0x44dc01` to `0x44dd01`:
  `0x34ab60(victim)` returns `[unit + 0xc]`, which is not the unit type (probably the mode); the block is skipped when it is 0 or 0x11 (`0x44dc09`, `0x44dc19`). Then
  `0x588b00(killer, victim, game)` when the killer is non-null, and a check that `0x34b9d0(victim)` returns 1 (`0x44dc3d`; `0x34b9d0` returns `[unit + 0]`, the unit type, and
  type 0 is a player, type 1 a monster, as `0x58f680` requires a type 0 operator). So the rest of the block runs for **monster victims only**.
- **Static: the death info record.** `0x4471e0(victim, 0, &out)` fills `out` (`dword +0 = 0`, `qword +8 = victim`, written at `0x447276`); `[rsp + 0x30]` (`out + 0x10`) is set to the
  killer at `0x44dcf4`. The killer owner fields (`[killer + 0x38]`, `0x341a30`, `0x341a20`, `0x349b70`, `0x34ae80`, `0x341950`) are resolved just before.
  The call at `0x44dcfc` is `0x4475c0(game, &out, 1)`, where the first argument is the game, not a player.
- **Static: `0x4475c0`.** It loads the unit at `[info + 8]` (the victim) and returns 0 unless its type is 1 (a monster) (`0x4475df`, `0x447600`); the rest of the body (stat `0x36` is read at `0x447630`)
  was not read, and it has 47 direct call sites, so it is a general routine, not something specific to this path. It is **not** shown to select a player, and nothing here says
  who is credited with experience or drops. The earlier "find the player the kill counts for" reading did not follow from the bytes and is withdrawn.
- **SDK.** The `MonsterDeath` listener (server thread) gives the killer identity only, with no owner resolution (a missile or minion shows as itself), so it is not a credit
  mechanism.
- **Static: spawn path.** The map events spawn through `0x492cf0` / `0x499ba0` (section 9). The victim is an ordinary server monster made by the game's spawn core; nothing in
  the death block was found that checks how it was spawned.
- **Static: bosses.** The superunique spawn is gated by the once-per-game bitmap at `game + 0x1d8` (per game, not per player), so one superunique can be spawned once per game: a
  design limit for several maps in one game, not a credit item.
- **Inference (unverified).** Monsters spawned on the host are real server units and replicate through the normal unit packets, so credit, experience, drops and quest flags are probably decided
  server side and need no plugin message. What is open is display: the plugin's monster stats (section 7) are applied on the server unit, and whether a client shows them depends
  on whether those stats replicate.
- **Needs game.**
  1. Kill a map event monster as a non-host client, directly, with a minion and with a missile: experience, kill count and drop owner (this is the only way to settle credit).
  2. Do the monster stats from section 7 reach a remote client's display (health bar, hover text), or only the server's numbers?
  3. Does the monster level override (section 6) change the experience award for a client far from the monster?
- **Recommended route.** Spawn on the host only through the section 9 calls and leave credit to the engine, verified by the three checks. If credit is wrong for minions or
  missiles, read the rest of `0x4475c0` and the owner chain of the block above (and `0x490300` for missiles) before touching anything; a hook on `0x44db20` is the fallback and must chain with Monster Count and Better Auras.

### Needs game (summary)

1. Q1: channel connect and reject behaviour with a remote TCP/IP client; 224 byte delivery; callback threads.
2. Q2: `0x3271c0` hit order on host and remote client for the slot level; does the plan arrive before the remote client's first lazy build; the LoadAct sender.
3. Q3: `0x58f680` context fields (confirm `[ctx + 8]` and `[ctx + 0x10]`); refusal feedback and retry; a client that never acks; the host's own player; the player to peer map.
4. Q4: credit and drops for a remote client (own attack, minion, missile); stat replication to clients' displays; range effects.
5. Ack content: the client solves the layout from its own tables, so its ack should carry a hash of the solved plan and the host can refuse a client whose `maps.txt` or pieces differ (the phase 7 done check, two clients show the same layout).
6. A client that joins, or rejoins, after the map was opened.
7. The client's plan cache surviving the level rebuild on a later visit (plan section 1, pd2-resurrected#930).
8. Which of the 5 callers of `0x3271c0` run on a remote client.

### Recommended route (all four)

`NetworkService` for the plan message (Q1); keep the maze hooks as they are on both sides (Q2) with the plan in the per-game cache before the client's first build; gate the
portal entry at `0x58f680` until the client acks (Q3); spawn events on the host only and leave credit to the engine, verified by the Q4 checks. The unlocated pieces (LoadAct sender,
retry behaviour, stat replication) are needs-game items and do not block writing the plugin code.

### What was built (issue #125, phase 7 part 1): written, not verified in game

The smallest slice section 12 supports: the host tells joined clients which map is open. Nothing here was run in a game.

- **Route.** The SDK `NetworkService` only (Q1). One channel (`localChannelId` 1), compatibility token `"CART"` plus the wire version, so a host and a client on different message versions never connect. **No new native address, no hook, no new byte signature**: nothing was added to `part_signatures.h` or `tools/check_portal_signatures.py`. The gate `0x58f680`, the LoadAct sender and everything else marked unproven above is untouched.
- **Messages** (little endian, `src/mapsync/mapsync.h`). `Hello` (client to host, 2 bytes: wire version, 0). `MapOpen` (host to client, 20 bytes): wire version, flags (bit 0 = maze map), `maps.txt` row (u16), hash of the map's name (u32, FNV-1a, case folded), slot level (u16), opening count (u16), layout seed (u64). Decoding refuses a wrong size or version, an unknown flag, a slot level or opening of 0, and a preset map that carries a seed. A preset map has no layout seed, so it sends 0; the per-player stat and event seeds are drawn on the host and are not sent.
- **Host** (`src/engine/map_sync.cpp`). After a map opens through either path (item or console) the identity is stored and sent to every client that said hello. A client is known only from its `Hello`, not from the host's connection event: whether the host's `connectionState` callback fires per peer is not in the SDK text, so the code logs it and does not rely on it. A `Hello` queues a send through `runOnGameThread` (the pattern in the SDK `network-ping` sample), so a client that joins after the map opened is told too (late join). A peer that answers `NotConnected`, `StaleHandle` or `NotFound` is dropped. At most 16 clients are kept; when the list is full the oldest entry is dropped for a new client. The host's own process never connects to itself (the connect loop, the hello and an incoming `MapOpen` each re-check the server tick, so a late tick still wins).
- **Client.** At `LocalPlayerReady` a connect loop starts on the UI thread (`runOnUiThread`, queued again each frame, since `runOnGameThread` is Unavailable on a remote client). It waits 3 s, then calls `connectToHost` every 1 s, up to 30 times, and stops at once if the server tick part has run (a host or single player: no connection is made), if the host rejects it for any reason but `HostUnavailable` (the reason is logged; `HostUnavailable` keeps the schedule running, as the SDK README says to retry), or on `Incompatible`, `WrongRole`, `Unsupported` or `OwnerInactive`. It also asks `getChannelInfo` before each attempt, so a missed connection event does not stall it. On connect it sends `Hello` once. A `MapOpen` is decoded, then checked against the client's own `maps.txt`: the row must exist and its name hash must match (else the table differs between host and client and the message is logged and dropped), and an older opening than the one kept is ignored. The kept identity (`MapSyncRemoteMap()`) is what later parts will read. Nothing else uses it yet.
- **Fail open.** Every callback and the install are wrapped; a missing or short Network or Thread service, or a refused channel, turns only the sync off with a log line (`cartographer check` prints it). The portal and the map open do not depend on it. A client without the `PlayerTick` part never gets `LocalPlayerReady` from Cartographer and so does not start (the same part the console open needs).
- **Single player and a host with no client.** `MapSyncMapOpened` finds no client and sends nothing (one log line); no connection is made because the server tick shows within a frame, long before the 3 s delay.
- **Evidence.** `tools\build.ps1 -Plugin cartographer -Test`: build clean, 11 of 11 tests pass, including the new `mapsync_tests` (round trip, fixed wire bytes, every truncation and single-byte change, hello, the client's check with its refusals leaving the kept map unchanged, the client list limit (oldest dropped when full), the rejection rule, the connect schedule). `check_portal_signatures.py` and `check_maze_signatures.py` against `D2RLoader_6ab3782c_dump.exe` (`--stamp 0x6ab3782c`): all signatures ok, as before.
- **Reading of the SDK that differs from the first draft of this section.** The README says local games and the TCP/IP host also connect "inside the same process", so `connectToHost` would succeed on a host and in single player; the code avoids that by connecting only when no server tick has run, which is an inference (a process that has the server ticks at once), not something the SDK states. Q3's "queue `getPeerPlayer` on each Connected event" was not built (the portal hold is out of scope here); the host's peer to player map stays a later item.

#### Needs game (part 1)

1. A remote TCP/IP client with Cartographer connects: how many `Busy` attempts, and the log line for it. A client with a different token, and a client without the plugin: the rejection reason shown, and that the join itself is refused.
2. The host's `connectionState` callback: does it fire per client, with a peer handle (logged as `connection event`), and on which thread do the host and client message callbacks run?
3. The `Hello` reaches the host, the reply reaches the client, and the log lines on both sides match (`told client`, `the host's open map`).
4. A client that joins after the map was opened is told (late join); a client that leaves and rejoins.
5. Whether the identity reaches a client before that client first builds the slot level (Q2, needs-game item 2).
6. Whether `runOnUiThread` runs on a remote client at a rate that makes the 1 s retry work, and that the loop ends when the game is left.
7. A host with two clients: both are told, and one leaving does not stop the other.
8. Single player (also paused in the first seconds) and a host with no client are unchanged: no `map sync: ... connectToHost` line, no extra delay.

### What was built (issue #130, phase 7 part 2): written, not verified in game

The client applies the identity part 1 keeps. Scope is what the sections above show the client must do itself; nothing here was run in a game. **No new native address, no new hook, no new signature** (nothing added to `part_signatures.h` or the checkers).

- **Why only these two.** Each process has its own tables (section 10, "Multiplayer"), so the host's write of the slot row's `LevelName` and `LevelEntry` does not reach a remote client: the client must write its own (a). Both sides run their own DRLG and the client builds the slot level lazily (Q2), so the client's build needs the same `{maps.txt row, seed}` as the host's to give the same plan (b). Map stats and events are drawn and spawned by the host (sections 7 and 9) and reach the client as ordinary units; kill credit (Q4) and the portal hold (Q3) are separate.
- **When.** An accepted `MapOpen` (a new identity, not a resend or an older opening) queues one apply step on the UI thread (`runOnUiThread`, the route the connect loop uses; `runOnGameThread` is Unavailable on a remote client). The step re-checks the server tick (a host or single player process applies nothing), the game generation (a step from a left game does nothing), and that the row still has the sent name hash. It runs once per opening (`AppliedOpening`).
- **Decision** (pure, `src/mapsync/mapsync.cpp` `decideApply`, unit tested). Facts the glue reads from the client's own side: the slot level its `slots.txt` gives for the map's kind equals the sent slot level; its row is a maze map exactly when the host's flag says so; `MapNames`; maze steering live; the Maze slot's tileset fits; the slot build state. A slot level or kind mismatch skips both halves (the client's tables do not describe the host's map). Otherwise the name half runs when `MapNames=1` here, and the layout half runs for a maze map with a non-zero seed, live steering, a fitting tileset and no slot build yet. Every skip is logged with its reason and shown in `cartographer check`.
- **(a) Name.** `MapNamesBeforeOpen(map, tables, slotLevel, dataFlag)`, the same code the host path uses (same keys, same saved originals, same restore on `GameLeft` and unload). It needs a levels-table data flag, and a remote client has no `game + 0x106`; the flag is taken from the arguments of the level build the existing `LevelBuild` hook sees (`MazeLevelDataFlag()`, last value, cleared on leaving a game; on a remote client every build is its own). With no build seen yet, the name is skipped and logged; when the maze parts are not all hooked the `LevelBuild` hook is off and no flag ever exists, so the name half logs that as its own reason ("the data flag is not available because the LevelBuild hook is off") instead of "no level build has been seen yet".
- **(b) Layout.** `MazeRemoteMapOpened(map, tables, hostSeed)` installs the maze job through the same `SetJob` as `MazeMapOpened`, but with the host's seed as it is (the host mixes a salt with the job id; the client does not repeat that). The existing plan cache, `LevelBuild` hook and steering then run unchanged. It refuses, changing nothing, when steering is off or the slot level was already built or is being built here, **with or without a map open** (the new `SlotBuildState::ranAny()`; `blocked()` ignores a build with no job, which is right for the host's later open but not for a client that must not swap plans under an existing level). That case is logged and the client keeps the stock layout; no rebuild is attempted.
- **Single player and the host** are unchanged: the message callback and the apply step both return when the server tick has been seen, and a host never connects. The `LevelBuild` hook only stores its data flag argument (one atomic store).
- **What this does not check.** The plan is built from the client's own `maps.txt` and `shapes.txt` row; only the row index and the name are compared with the host's, so a client whose tileset, shape, size or `shapes.txt` differs from the host's gets a different maze. The plan hash in an ack (needs-game item 5 above) is still open.
- **Evidence.** `tools\build.ps1 -Plugin cartographer -Test`: build clean, 12 of 12 tests pass, including new `mapsync_tests` cases (a good maze identity applies both halves, a preset one the name only, a slot or kind mismatch skips both, each maze refusal leaves the name alone, MapNames off, the skip sentences, an opening applied once) and a `maze_tests` case for `ranAny()`. `check_portal_signatures.py` and `check_maze_signatures.py` against `D2RLoader_6ab3782c_dump.exe` (`--stamp 0x6ab3782c`): all signatures ok.

#### Needs game (part 2)

1. The data flag: is the one the client's `LevelBuild` hook sees the one `LevelsRow` needs for the client's tables (the `map sync: ... its name is set ... (data flag N)` log line), and is a build always seen before the host's map arrives? If not, the name is skipped. A wrong flag only leaves the stock name (the write lands in a bank nobody reads); the restore stays exact because it goes through the saved row pointer.
2. Does the identity reach a remote client before its first build of the slot level (Q2 item 2)? If not, the client logs "built here before the host's map arrived" and shows the stock maze.
3. The thread of the message callback (the apply is queued to the UI thread either way) and whether writing the row there tears with the tooltip, automap or banner reads.
4. The client's own portal tooltip, automap title and entry banner show the map's name; the name is back after leaving the game.
5. Host and client mazes match for the same `maps.txt` (same seed, same plan, compare the `plan` hash line in both logs).
6. A client with `MapNames=0`, a client with a different `slots.txt`, and a second map opened in the same game.
