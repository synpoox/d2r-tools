# Armory: atomic equipment swap investigation

## Source audit (not a dump or live validation)

Repository base: a1b3bede94cd9047bfb446aefed768a3d2ec77bf. Pinned PluginSDK: 717f727a0ec52912d1558764345f8fa3453a2bd6. Toolkit main examined at 5d59cb6dbf6fabe34f17c8a14c73d6ddf45ba2bd.

- SDK item.h: ExistingItemOperation explicitly rejects equipment, cursor, belt, trade, corpse and ground moves before mutation. ExistingItemTransaction cannot implement Armory. Transaction consumes inputs and creates outputs; forbidden for Armory. MutationService transactions concern hooks/patches, not inventory.
- inventory.h exposes getLocalPlayer, getEquippedItem, forEachInventoryItem; ItemInfo supplies seeds, code, quality, affixes, location, runtime id and handle. Handles are session-bound. These fields are candidates, not proven permanent identifiers. The toolkit reports runtime service size differences on loader 1.3.1; check individual field ends and capabilities, not just header versions.
- Toolkit knowledge/d2rloader-capabilities.md confirms storage-only move transactions and authoritative host item mutations, with clients messaging host. Do not use closed Battle.net.
- Toolkit knowledge/d2r-93847-addresses.md identifies 0x34DBE0 as mark stats changed (unit, 1), not a complete inventory refresh.
- Candidate addresses supplied in task: 0x471E90 mover, 0x46E050 layout validator, 0x36BC50 requirements, 0x34DBE0 stats changed, 0x470CA0 activation refresh, 0x4AA450 equip and 0x4AABA0 unequip. Initially unverified; see the subsequent dump audit below. Do not call these functions until native contracts and safety gates are established.
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
- Build: independent plugin folder, repository SDK pin, crash guard and per-part signature guards; no new global version gate. The diagnostic CMake DLL target and repository build/test wiring have been added; enable production functionality only once the native gate is proven. The current branch is intentionally not merge-ready and does not enter the release list.

## Validation

Portable model tests exercise vacated storage reuse, all-or-nothing model publication, stale revision, missing/ambiguous fingerprints, duplicate selection, partial loadout protection, full storage, unsafe context and missing validator. Run with C++20. This does not prove native atomicity, equipment requirements, persistence or network synchronization. The model assumes the adapter supplies trustworthy owned-item snapshots and a real validator. The bounded packing search can conservatively refuse a solvable layout. Slot numbering and grid bounds are model assumptions until native verification.

Remaining: native proof, actual adapter, persistence, UI/hotkeys, host/client protocol, Windows DLL build and game-slot testing. No native write is present in this branch.

## Dump audit: image 0x6AB3782C

Inspected a private unpacked memory-layout x64 PE image from the supplied archive. Image size 90,451,968 bytes; SHA-256 `c6c49d1c4bf2d8f496aca6c786047a3494e99a7560e81a19aed711946c7a12d2`. PE stamp matches the target image registry. No game binary, archive, or private storage link is committed.

All seven candidate entries resolve uniquely in executable .text and are corroborated by x64 exception-directory function-start entries. The portable resolver in `plugins/armory/src/native_signatures.h` returns a resolved address or null for missing/ambiguous signatures; no fixed RVA or absolute address fallback. It accepts a validated readable executable span; live module discovery, runtime function-entry corroboration, and contract checks remain adapter work. Relative instruction displacements are masked. The signatures are build witnesses, not a promise of cross-build compatibility.

| Candidate | Observed static behavior |
| --- | --- |
| Mover | Native equipment handlers invoke it; counted array processing and a requirement-failure restoration branch observed. |
| Layout | Validates both primary and weapon-swap arms; diagnostic strings corroborate. |
| Requirements | Called during post-layout validation; return checked before success. |
| StatsChanged | Sets/clears unit change flags; not a complete inventory refresh. |
| Activation | Iterates inventory; invoked after layout application and again on restoration. |
| Equip / Unequip | Both invoke the mover; normal successful branch tests nonzero AL. |

### Mover contract evidence

Windows x64 register/stack dataflow shows six argument positions: player-like pointer, source location array, destination location array, expected runtime-item-ID array, count, and a byte flag. Native callers exhibit counts of one and two; another caller forwards a variable count. Source/destination records have a 16-byte stride; ID array has a four-byte stride. Full count limit, flag semantics, page policy and valid location kinds remain unproven. The apparent size checks in the implementation are internal bounds, not permission to submit that many moves.

The native mover snapshots inventory state, resolves sources against expected IDs, builds a prospective layout and invokes the layout validator. It then applies layout changes and refreshes activation before checking requirements on changed equipment. The requirement-rejection branch applies a reverse layout and refreshes activation again before returning false. This is evidence of a native transaction and an explicit restoration path, but not proof of failure atomicity: event/packet ordering, stat effects, shared stash, attribute dependencies, cursor and weapon-swap behavior still require static callee analysis and controlled game-thread tests. Do not bypass the native checks or assume the flag is safe.

### Validation performed

- Dump checker: all seven signatures matched exactly once and resolved to function entries.
- Portable C++20 model and signature tests: compiled with GCC warnings-as-errors, passed. Resolver also passed against extracted executable .text.
- No native code was executed; no item mutation, Windows DLL build, game install, or TCP/IP testing occurred.

Next: audit native layout application/restoration callees for state changes and synchronization, then implement a gated native proof using a disposable character in the authorized offline/TCP/IP environment. Only enable equipment writes after proving native rollback preserves identity and all relevant state. Runtime entry discovery must use unique signatures, never hardcoded RVAs or absolute virtual addresses.

## Follow-up: native validation limitations

Disassembly of the mover's primary body (ends at 0x472581; the adjacent function at 0x472590 is separate) shows the sixth byte argument controls more than placement: a nonzero value branches around the post-activation requirements loop at 0x472274. Never use this flag to force a loadout. The normal loop visits body slots 1 through 10, skips unchanged item pointers and accepts the special paired-hand case. It does not establish that unchanged dependents or both inactive weapon slots still meet requirements. Final activation is not an adequate substitute for validating all required items and attribute dependencies.

The apply helper at 0x46C420 performs native item-mode and page changes, grid placement, and removal operations; its internal errors include assertion paths rather than a transactional result. The mover applies this helper before requirements validation, including calls to activation and hand state maintenance. The rollback branch calls it again in the reverse direction and refreshes activation. Success-only code calls 0x475E00 and 0x474D60. These facts do not prove that the earlier calls lack externally visible side effects or that rollback can never fail. The native write gate remains closed.

Portable loadout manager and store added: full/partial capture, exact named CRUD, cycle, multiple active matches, bounded binary encoding, scope validation, compare-before-replace, exclusive temporary file and flush. Model tests exercise truncated/corrupt/wrong-scope data, duplicate names and matches, stale writers and failed file writes. Linux GCC ASan/UBSan passed. Windows file operations and session identity integration remain untested. Chosen prototype limits: 64 loadouts, 64-byte names and 512-byte scope; case-sensitive names, explicit empty slots. The format is experimental and must not be treated as stable until item fingerprint persistence is verified. A caller must serialize writes per character; the temporary-file mechanism alone is not an interprocess lock spanning read and replace.

## Native location records and diagnostic target

Constructors at 0x382DF0, 0x382E10 and 0x382E30 and validation at 0x385550 establish a 16-byte record: kind u32 at +0, page u32 at +4, x/y u16 at +8/+10, body u32 at +12. Normal inventory location is kind 0/page 0/body 0. Equipment is kind 1/page 255, x and body equal the slot, y zero. The syntactic validator accepts equipment slots 1–12 and stored pages 0, 3 and 4 with coordinates below 16. Cursor kind 4 and belt kind 2 exist but are excluded. A syntactic location is not evidence of actual page accessibility, ownership, grid dimensions or class legality. The policy helper at 0x474700 requires additional state for page 4 with flag zero; shared stash tab semantics are not established. Do not use a nonzero flag to bypass this.

The data-only native probe encoder limits the first candidate to exactly two reciprocal moves between normal inventory and a non-hand primary equipment slot, distinct expected runtime IDs, and flag zero. It does not invoke the mover. The native image resolver copies executable .text and exception-directory records through a bounded reader, corroborates unique signature matches as function entries, and returns relocated addresses. Tested against the dump at two simulated module bases and after corrupting a signature byte. No runtime RVA fallback. The new diagnostic DLL target uses the pinned SDK, module allocation/protection checks, the existing repository crash/log helper pattern, and local status/read-only enumeration commands. Native calls are disabled; the actual Windows DLL build, install and live observation remain unverified. The portable C++20 suite passes all five CTest targets with GCC warnings-as-errors. See plugins/armory/TESTING.md for the game-slot checks.

The source-only diagnostic target does not establish player identity across reloads, item dimensions, complete stash coverage, native requirements or authoritative peer inventory access. The UI, hotkey dispatch and write path remain pending.

## Engine-independent preflight and peer admission

The conservative requirement solver checks level/class and both weapon sets independently, requiring a dependency order from a verified non-equipment attribute baseline. Unknown, negative and conditional contributions must be rejected rather than approximated; the model cannot prove native item effects, two-handed pairing, set bonuses, or requirement reductions. It is a conservative extra check, never a replacement for native validation.

The bounded equip-request codec carries host-issued session token, revision, monotonic nonce and desired slots/fingerprints only. Host admission rejects wrong sessions, replay, rate-limited or stale requests, and invokes the existing planner on a supplied authoritative snapshot. It accepts no client-provided player identity, runtime handle or placement. The 250 ms rate limit and 1 KiB packet cap are provisional. The SDK channel, authenticated peer-to-player lookup, host-issued token handshake, response codec and game-thread scheduling are not wired up. No client packet is consumed by the DLL target.

Portable requirement and peer-admission tests were added; all seven CTest targets passed on Linux GCC with warnings-as-errors. This is model validation only; the diagnostic DLL target remains unbuilt and untested in game.

## Persistence hardening and SDK bridge audit

The experimental sidecar now includes CRC32 corruption detection, rejects paths without the .armory extension, and acquires an OS writer lock across compare-and-replace. Linux tests reject a conflicting writer, corrupted payload and attempted .d2s path. All seven CTest targets and the sidecar ASan/UBSan test passed. The Windows file lock and replacement path remain untested. The caller still owns in-memory/session synchronization.

The pinned SDK's NetworkService::getPeerPlayer maps authenticated peers to session-bound PlayerHandles, not native player pointers. InventoryService also yields opaque handles, not native player pointers. ItemService::editNativeItem requires NativeHooks and gives a temporary native item pointer only; the SDK explicitly does not validate or publish mutations made there. Do not repurpose it as an unchecked equipment transaction or retain its pointer. A supported native-player bridge or guarded host resolution with authoritative identity corroboration remains necessary before invoking the candidate mover. The pinned SDK reimplementation exports include level/handedness helpers, but they require native unit pointers and do not establish that bridge.
