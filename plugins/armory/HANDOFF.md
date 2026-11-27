# Armory — implementation handoff

**Starting point:** branch `armory/atomic-swap-poc`, implementation baseline `2f1a6bd`. Continue on this branch (or an explicitly requested successor), never directly on main. Armory is an equipment manager for D2RLoader offline and TCP/IP games, not Battle.net. Finish the plugin, not another research-only prototype.

## 1. Orient and reproduce

Read root AGENTS.md, CONTRIBUTING.md, [README](README.md), [TESTING](TESTING.md), and [the native research](../../docs/reverse-engineering/armory.md). Search the owning plugin's sources/tests. Use the existing MultiSpec patterns for save namespace, native panels, lifecycle and game-thread dispatch; do not include code across plugin directories. Read the toolkit's knowledge/d2rloader-capabilities.md, knowledge/d2r-93847-addresses.md, plugin-template and repository RVA workflow. Build against the repository's pinned SDK (`717f727a0ec52912d1558764345f8fa3453a2bd6`), not toolkit master. Do not change the pin without separate authorization and suite validation.

Windows x64 / VS 2022 / CMake 3.29+:

```powershell
git fetch origin
git worktree add ..\d2r-tools-armory --track -b armory/atomic-swap-poc origin/armory/atomic-swap-poc
cd ..\d2r-tools-armory
git submodule update --init --recursive
.\tools\build.ps1 -Plugin armory -Test
```

If the branch already exists locally, omit `--track -b` and use that branch in the worktree command (or use its existing worktree). Preserve unrelated changes. Fix any compiler/SDK errors before proceeding. Install only in a dedicated test game slot, with the game closed and a disposable character; never install over a player's working plugin or use their saves.

Target: D2R 3.3 build 93847, PE stamp `0x6AB3782C`, D2RLoader 1.3.1 as initial reference. The owner's connected Google Drive contains **D2R Dumps / 3.3 / D2RLoader_6ab3782c_dump.zip** (also five split parts). Extract outside Git; identify the unpacked memory-layout x64 PE by stamp, size 90,451,968 and SHA-256 `c6c49d1c4bf2d8f496aca6c786047a3494e99a7560e81a19aed711946c7a12d2`. If mismatched, investigate rather than reusing conclusions. Run `python plugins/armory/tools/verify_dump.py <unpacked-image>` and the built `armory_image_tests.exe <unpacked-image>`. Do not commit the dump, archive, private Drive link, game assets, saves or logs.

## 2. What is already implemented

| File | Function and limits |
| --- | --- |
| src/model.h | Pure snapshot/placement planner, unique matching, partial slots, reuse vacated storage, bounded packing, model-only atomic publication. No native calls. |
| src/loadouts.h, store.h | Named CRUD, full/partial capture, matching active loadouts, cycle, versioned/checksummed .armory sidecar, exclusive writer lock and compare/replace. Character scope and live identity not wired. |
| src/native_signatures.h, native_image.h | Unique masked signatures, bounded PE image reader, exception directory entry corroboration, relocated addresses. |
| src/native_batch.h | Data-only two-item reciprocal inventory/non-hand equipment probe, distinct expected runtime IDs, flag zero. Not a callable backend. |
| src/requirements.h | Conservative additive attribute dependency solver for both weapon sets. Not a native requirements validator. |
| src/peer_request.h | Bounded request codec, host admission with session/revision/nonce/replay/rate limit. No SDK transport or authentication integration yet. |
| src/plugin.cpp | Shared diagnostic DLL target, crash guard, runtime scan, local `armory` status and `armory-probe` read-only SDK enumeration. No writes or player-facing loadout UI. |

Prior session reported seven portable CTest targets passing on Linux GCC, sanitizer checks on the model/store, and seven unique signatures verified against the dump. Those are not Windows build or in-game results. Windows DLL build, install, native calls, TCP/IP, real item identity and the actual UI have **not** been validated. Re-run tests and record fresh evidence.

## 3. Critical path — prove safe native equipment swapping

**Do not enable production equipment writes until the gate is proven.** The existing SDK identity-preserving transaction rejects equipment; the consume/create transaction destroys identity and is forbidden. MutationService is for hooks/patches, not items.

- Start with the diagnostic DLL: `armory` and `armory-probe` in a disposable offline game. Cross-check both weapon sets, empty slots, cursor, stash access, reloads and host/client behavior. Count logs alone do not prove full enumeration.
- Resolve authoritative native player identity on the game thread. SDK PlayerHandle is opaque and session-bound; NetworkService::getPeerPlayer resolves authenticated peers to handles, not native pointers. Do not cast a handle, trust a client pointer, retain a native item callback pointer, or use editNativeItem as a transaction. Prefer a supported SDK bridge; otherwise prove a guarded native resolution path against the dump and corroborate ownership and identity. Audit existing MultiSpec and toolkit player lookup, adapting only verified behavior.
- Complete the candidate mover contract and call graph. Documentation records the research-only offsets and byte witnesses. The apparent six arguments are player-like pointer, source array, destination array, expected runtime-ID array, count, byte flag; native callers test AL and use counts 1 and 2, plus a variable-count caller. Prove the precise types, lifetime, flag meaning, bound and preconditions. Never use a nonzero flag: it skips a post-activation requirement check. The current 16-byte record and pair encoder are static evidence, not permission to call.
- Analyze layout application, restoration, activation and success-only notification callees for mutation, assertion paths, packet ordering, stat/skill/visual state, cursor, save and disconnect effects. The restoration branch is encouraging but does not prove atomicity; native requirement checks skip unchanged gear and do not fully validate alternate slots. No hardcoded runtime RVAs or absolute addresses: use unique signature-resolved addresses, executable/function-entry checks and fail closed on absent/ambiguous/mismatched parts; retain offsets only as research documentation/test expectations. No global game-version gate.
- Implement a narrowly scoped, explicitly gated developer probe only after the static contract is sufficient: two distinct non-hand items (e.g. rings), one worn and one in inventory, exact reciprocal destinations, flag zero, in town, empty cursor, idle authoritative player and fresh snapshot. Compare pre/post identity, count, coordinates, socket contents, attributes, stats, skills, activation, client state and persistence boundaries. Test success and rejection (stale ID, invalid requirements, invalid layout, ownership and storage failures) on disposable saves. Inspect rollback and external visibility at each reachable failure boundary. If a native primitive cannot guarantee all-or-nothing, disable the write path and pursue a proper identity-preserving equipment transaction in D2RLoader; no sequential equip/unequip fallback or speculative compensating moves.

## 4. Complete the plugin once the native gate is satisfied

1. **Snapshot + identity adapter:** map SDK item info to live owned-item snapshots; acquire verified dimensions, grid/page policy, complete supported stash coverage, native slot semantics, base attributes and context. Distinguish inaccessible storage from missing items. Establish stable fingerprints through reloads/trades (seeds, code, quality, record, affixes, intrinsic stats, sockets and other verified distinctions); exclude mutable durability and location; never silently substitute ambiguity. Reject unsupported items and pages. Test two-handed/dual wield, class rules, paired hands and both weapon sets.
2. **Atomic swap service:** compute complete target including unchanged equipment, validate level/class/strength/dexterity dependencies and native requirements for both weapon sets. Conservative solver is supplemental; conditional/set bonuses, charms, negative stats, requirement reductions and activation ordering need verified native semantics or rejection. Find legal storage placement, including freed cells; prove batch limits and rollback before expanding beyond the two-item case. Re-resolve ownership/IDs, inventory revision, player/session and context immediately before one native transaction on the authoritative game thread. Reject concurrent changes, trade/cursor/death, disconnects and non-town. Verify results and disable on invariant failure; never create, destroy or automatically drop an item.
3. **Persistence + controller:** integrate per-character/mod/save namespace (follow MultiSpec's optional savepath/fallback pattern), use sidecar only. Bind reads/writes to correct character session, keep prior valid data on failure, do not overwrite corrupt files, clear cached handles on transition. Validate Windows file locking/replacement and reload. Provide save current (full/partial, including explicit empty slots and alternate hands), rename, update, delete, select/cycle and equip. Recompute active/availability from live gear; multiple partial matches may be active. Provisional limits are 64 loadouts, 64-byte names, 512-byte scope; case-sensitive names. Revisit with evidence; file format is experimental.
4. **Native UI + inputs:** minimal plugin-owned panel consistent with repository plugins, loadout list, active/availability indicator, Save Current/Equip actions; contextual rename/update/delete and expandable equipment details with native graphics/tooltips when supported. Detect SDK field availability (loader 1.3.1 can expose smaller service structs); graceful text fallback. Add configurable select/cycle hotkeys, defaults/config, focus and lifecycle guards. UI requests go through same transaction path; actionable missing/ambiguous/requirements/full-storage/unsupported messages.
5. **TCP/IP:** use Shared plugin and SDK NetworkService. Implement host-issued session token/authoritative revision, bounded request/reply codec, authenticated peer-to-player lookup, per-player serialization, nonce correlation, disconnect cleanup, stale/replay/rate-limit rejection and game-thread dispatch. Host resolves all references itself; never trust client item IDs, positions, owner, stats or validation. Existing 250 ms interval/1 KiB cap are provisional. Verify remote client success and failure synchronization, including reconnect and mixed sessions; single-player success does not prove networking.

Defer mercenary gear, charms layout, skill bindings and other extended loadout features.

## 5. Finish and acceptance

- Extend portable tests for new adapters/models, malformed persistence/network data, partial/full loadouts, alternate hands, attribute cycles, packing failure and stale revisions. Execute Windows build/test workflow; record dump verification, build, install, offline live tests and two-peer TCP/IP tests separately.
- Acceptance: same physical item usable by multiple named loadouts; correct reload/rebind behavior; full inventory exchange reuses incoming cells; all supported equipment and stash locations work; failures preserve exact original items and state; non-town and unauthorized attempts refused; clear UI status and usable hotkeys.
- Add guarded native findings/short byte witnesses to the reverse-engineering registry and dump checker, update README, TESTING, config defaults and one-line player-visible changelog fragment. Root README only lists released plugins. Keep all work scoped; no release, tag, PR or merge unless owner requests.
- If blocked, leave write path disabled and report precisely what is verified, missing, or unsafe, with reproducible next steps. Do not claim the diagnostic target or passing model tests are a completed plugin.
