# Armory: atomic equipment swap investigation

## Source audit (not a dump or live validation)

Repository base: a1b3bede94cd9047bfb446aefed768a3d2ec77bf. Pinned PluginSDK: 717f727a0ec52912d1558764345f8fa3453a2bd6. Toolkit main examined at 5d59cb6dbf6fabe34f17c8a14c73d6ddf45ba2bd.

- SDK item.h: ExistingItemOperation explicitly rejects equipment, cursor, belt, trade, corpse and ground moves before mutation. ExistingItemTransaction cannot implement Armory. Transaction consumes inputs and creates outputs; forbidden for Armory. MutationService transactions concern hooks/patches, not inventory.
- inventory.h exposes getLocalPlayer, getEquippedItem, forEachInventoryItem; ItemInfo supplies seeds, code, quality, affixes, location, runtime id and handle. Handles are session-bound. These fields are candidates, not proven permanent identifiers. The toolkit reports runtime service size differences on loader 1.3.1; check individual field ends and capabilities, not just header versions.
- Toolkit knowledge/d2rloader-capabilities.md confirms storage-only move transactions and authoritative host item mutations, with clients messaging host. Do not use closed Battle.net.
- Toolkit knowledge/d2r-93847-addresses.md identifies 0x34DBE0 as mark stats changed (unit, 1), not a complete inventory refresh.
- Candidate addresses supplied in task: 0x471E90 mover, 0x46E050 layout validator, 0x36BC50 requirements, 0x34DBE0 stats changed, 0x470CA0 activation refresh, 0x4AA450 equip and 0x4AABA0 unequip. All are unverified for Armory. No byte guards or calling contracts have been established. Do not bind or call any of them.
- The domain guide path referenced by AGENTS.md was not found on main. CONTRIBUTING.md, AGENTS.md, RVA workflow and MultiSpec conventions were reviewed.

## Gate: native proof of concept

Use a disposable test character in a separate game slot, D2R 3.3 build 93847. Record running image stamp, loader build, dump hash and module identity. Follow RVA-AND-INTERCEPTION-WORKFLOW.md. Start from native equip/unequip callers and establish the precise mover request layout, count limits, allowed source/destination kinds, thread, ownership checks, return values, stat/skill/visual synchronization, transaction journal and rollback behavior. Record byte witnesses and unique signatures in a dump checker. Inspect every failure exit, not just success.

First prove swapping one worn item with an incoming stored item, with no spare cells except those freed by the incoming item. Verify exact original native identities, item counts, coordinates, sockets, stats, active weapon set and client/host state. Force rejection before staging and at each native failure boundary. Repeat for two or more items, different grids, alternate weapon sets, class restrictions, two-handed/dual wield, strength/dex dependency chains and circular dependencies; reject unsupported cases. Verify no intermediate observable state, packets, saves or side effects that survive failure. Never create or destroy items as recovery. If the engine has no suitable primitive, request an identity-preserving equipment transaction in D2RLoader rather than chaining equip handlers and claiming rollback.

## Architecture after gate

- Model: immutable authoritative snapshots and plans; absent slot = keep, explicit null = empty. Resolve fingerprints uniquely; no guessing. Extend seed/affix identity with verified intrinsic stats, socket contents and distinguishing attributes, excluding mutable durability/location. Validate across reloads and trades; unresolved items require explicit rebind. Never trust runtime IDs as persistent identity.
- Adapter: resolve fresh handles and native ownership; enumerate only supported accessible pages with verified dimensions and policies. Snapshot revision and session generation; reject cursor, trade, dead/transitioning player, non-town, busy or stale state. Resolve complete final equipment including unchanged slots. Native validation must model attribute prerequisites and activation ordering without bypassing requirements. Use one authoritative native atomic commit or no mutation. Final layout is not an execution sequence. Validate after commit and disable on invariant failure.
- Persistence: versioned Armory sidecar per character/mod/save namespace; bounded parser, temporary file + atomic replace, preserve prior valid file on I/O error, never modify D2S/D2RL. CRUD and active status derived from current equipment, not a stale selected id.
- UI: native plugin-owned panel and resources, named list, active/availability, Save Current and Equip, expandable details and context actions. Feature-detect native item graphic/tooltip APIs and fall back to text. Configurable hotkeys; game-thread dispatch.
- TCP/IP: Shared plugin; authenticated peer resolves its own player on host, request only intended loadout and nonce, no client-trusted handles, placements, owner or validation. Bound/rate-limit messages, correlate replies and revisions; reject disconnect or character transitions. Host serializes inventory work with native game operations. Offline and TCP/IP tested separately.
- Build: independent plugin folder, repository SDK pin, crash guard and per-part signature guards; no new global version gate. Add normal CMake DLL target and repository build/test wiring once native gate is proven. The current branch is intentionally not merge-ready and does not enter the release list.

## Validation

Portable model tests exercise vacated storage reuse, all-or-nothing model publication, stale revision, missing/ambiguous fingerprints, duplicate selection, partial loadout protection, full storage, unsafe context and missing validator. Run with C++20. This does not prove native atomicity, equipment requirements, persistence or network synchronization. The model assumes the adapter supplies trustworthy owned-item snapshots and a real validator. The bounded packing search can conservatively refuse a solvable layout. Slot numbering and grid bounds are model assumptions until native verification.

Remaining: dump check, native proof, actual adapter, persistence, UI/hotkeys, host/client protocol, Windows DLL build and game-slot testing. No native write is present in this branch.
