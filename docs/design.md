# MultiSpec design

WoW-style dual spec for D2R 3.3 (build 93847, D2RLoader 1.3.1): a character keeps N saved skill allocations ("specs")
and switches between them with a command, without paying for a respec.

Status: **draft for the owner's OK**. Engine facts come from [recon.md](recon.md) and
[knowledge/d2r-93847-skills-respec.md](../knowledge/d2r-93847-skills-respec.md); all of them are static so far, and
three need a check in a game slot before the MVP can be trusted (section 9).

## 1. The behaviour you asked for, confirmed or challenged

| Asked | Verdict | Why |
|---|---|---|
| Spec = hard skill levels + unspent points | **Changed: a spec stores hard levels only; unspent points are derived** | A character's total skill points T only grows (level-ups, Den of Evil, Radament, Izual, Book of Skill). If spec 2 stored its own unspent count, every point earned while in spec 1 would never reach spec 2. With T = (sum of live hard levels) + `newskills`, spec k's unspent points are T - sum(spec k). They are still shown per spec, and the "unspent points preserved" test still holds |
| Swap anywhere | **Challenged: default town only, `anywhere` as a config value** | Your own Phase 3 list has "swap refused outside town". In the field a swap removes auras, passives and the skills that keep summons going, mid-fight, and could be used to cheat (cast with spec A's skills, swap). Town also bounds what can go wrong. WoW allows it anywhere out of combat; D2R has no "out of combat" flag to reuse |
| 2 specs, N in config | **Confirmed** | `spec_count`, 2 by default, 1-8. The file format allows up to 255 |
| Trigger `/spec N` first, UI later | **Confirmed, one open check** | A D2RLoader console command. Whether a line typed in the chat box reaches it is not verified yet (section 9). If not, Phase 2 adds a chat-message listener for `/spec` |
| Per character, survives save/load, never corrupts `.d2s` | **Confirmed, with zero `.d2s` writes** | Section 3 |
| Respect engine limits | **Confirmed** | Section 8; one new limit found (skpoints costs) |

Other choices made here (change any of them):

- A new spec starts **empty** (all points unspent). Config `new_spec = "copy"` copies the active one instead.
- Gold cost and cooldown are Phase 4 config values, both off by default.
- Skill hotkeys (F1-F16, left/right) are not part of a spec. After a swap they stay as they are, as after D2R's own
  respec. Per-spec hotkeys would need RE not done yet; listed as a later option.

## 2. Data model

```
CharacterSpecs
  name, class                     identity checks
  skillIds[n]                     the class's skills in D2R's order (0x33cb30 / 0x33dde0): the save's "if" order
  active                          1..N
  specs[N]:
    levels[n]  u8                 hard levels, same order as skillIds
    state      ok | adjusted | invalid
```

- **n comes from the loaded tables**, not a constant 30, so a mod using D2RLoader's extended save format with more
  class skills works.
- The active spec's truth is the **live character** (and so the `.d2s`). The stored copy of the active spec is
  refreshed from the live levels before every swap and at every load, so normal point spending, Akara and the Token of
  Absolution need no special handling: they change the active spec.
- Points: T = sum(live levels) + live `newskills` (stat 5). A stored spec with sum(levels) > T is `invalid` (only a
  save edit or a data change can cause it) and cannot be loaded; it is kept, never deleted.

## 3. Where specs are stored, and why

One file per character, **next to its save**: `<save folder>\<name>.multispec`, where the save folder is
`<Saved Games>\Diablo II Resurrected\mods\<savepath>\` (Saved Games from `SHGetKnownFolderPath(FOLDERID_SavedGames)`,
`savepath` from the active mod's `modinfo.json`).

| Option | Verdict |
|---|---|
| Inside the `.d2s` (a skill-bytes copy, a stat, a quest word) | **No.** Checksums, the `.d2rl` hashes and the extended format punish hand edits (characters vanish); new stats need save bits and change the format for every player |
| Item stats on a hidden item | No: items are where extended-format edits fail worst; and items can be lost or traded |
| Plugin memory | No: lost on exit |
| **Plugin file next to the save** | **Yes.** D2R never reads it (it lists `*.d2s`), so it cannot break loading. It follows the mod's own save folder, so test slots (each with its own `savepath`) stay separate, and a copy of the save folder takes it along |
| Plugin file in the mod folder | Fallback only (used, and logged, when the save folder cannot be resolved): it is lost on a mod reinstall and is shared by all slots of one install |

File format: plain text, versioned, small, written by the plugin only.

```
multispec 1
char Lyra
class 1
skills 30 36 37 38 ...                (skill ids)
active 1
spec 1 1 0 20 0 1 ...                 (n levels)
spec 2 0 0 0 0 0 ...
pending none                          (or "1>2" during a swap)
crc 9f3a01c2                          (CRC-32 of the lines above)
```

Writes are atomic: write `<name>.multispec.tmp`, flush, `MoveFileExW(tmp, file, REPLACE_EXISTING | WRITE_THROUGH)`;
the previous file is kept as `<name>.multispec.bak`. A file with a bad CRC or unknown version is never overwritten
silently: it is renamed `<name>.multispec.bad-<date>` and a fresh file is made from the live character, with a warning.

## 4. Swap algorithm

All of a swap runs **in one game-thread callback** on the server (single player, or the TCP/IP host), queued with
`ThreadService::runOnGameThread`. D2R saves on the game thread too, so it can never save a half-swapped character.

```
/spec N
 1  checks (refuse with a message, change nothing):
      engine parts on; player unit valid (round trip client -> unit -> client, type 0); alive;
      in town (unless swap_where = anywhere); 1 <= N <= spec_count; N != active; no swap running;
      every class skill's skpoints is -1 or 1 (section 8); spec N not invalid; cost/cooldown (Phase 4)
 2  snapshot  L_old = live hard levels (0x33dcd0 + skill+0x40); U_old = newskills; T = sum(L_old) + U_old
 3  file: specs[active] = L_old, pending = active>N           (atomic write; abort if it fails)
 4  reset     0x4360f0(game, player)                          D2R's skills-only respec (as Akara / Token)
      check   every class skill 0, newskills == T             else go to 7
 5  replay    L_new = specs[N]; repeat passes over the class skills in D2R order:
                for a skill below its target: class 0x436bd0, level/prereqs 0x339330, stat reqs 0x339c00,
                level < maxlvl, then 0x438670 (spend one point; sends the level to the client)
              until no pass makes progress (prerequisites resolve themselves across passes)
 6  verify    live levels == L_new and newskills == T - sum(L_new)
              ok: file active = N, pending none; log "spec 1 -> 2: 54 points in 11 skills, 3 unspent"
 7  on failure: reset again and replay L_old the same way (rollback)
              ok: file active unchanged, pending none; tell the player why the swap failed
              rollback fails too: leave the character reset (every point unspent, nothing lost), file pending
              cleared, active spec = what the live character now is; log an error with both spec lines
```

The steps D2R does itself (removing auras and passives, states, client updates) stay D2R's job: MultiSpec never writes
a skill level or `newskills` directly.

## 5. Load and reconciliation

On `LocalPlayerReady` (lifecycle service), a game-thread callback reads the live levels and the file:

| Found | Action |
|---|---|
| No file | Create it: spec 1 = live, others empty (or copies, per `new_spec`) |
| Name or class differ from the file (a new character with an old name) | Move the old file to `.bad-<date>`, start fresh |
| Live == specs[active] | Nothing |
| Live == specs[k], k != active | The game was not saved after a swap (crash, Alt-F4): active = k, logged |
| `pending a>b` and live == specs[a] or specs[b] | Same rule: whichever matches becomes active; pending cleared |
| Live matches no spec | The player spent points or respecced in the active spec: specs[active] = live |
| A stored spec's skill ids differ from the loaded class list (mod data changed) | Remap by skill id; levels of skills that are gone become unspent; state `adjusted`, logged once |
| A spec's sum > T | State `invalid`: shown, never applied |

## 6. Failure modes

| Failure | What happens |
|---|---|
| Crash or kill during a swap | The swap is one game-thread callback, so the `.d2s` holds either the old or the new spec, never a mix; the file has both and `pending`. Next load: section 5 picks the matching spec |
| Crash between the file write and the swap | Same: the file still has both specs; live matches the old one |
| File write fails (disk, permissions) | Step 3 aborts; nothing changed; message and log line |
| Corrupt or unknown-version file | Kept as `.bad-<date>`; never overwritten in place; fresh file from the live character |
| D2R refuses a point during replay (prerequisite, stat requirement, data changed) | Rollback (step 7); the message names the skill |
| Game or loader update (image stamp differs) | Build guard: every engine part stays off; `/spec` answers "MultiSpec is off: this game version is not supported"; files untouched, so nothing is lost and the active spec just stays active |
| Same stamp but a called function's bytes differ (another plugin patched it) | `specswap` and `specsync` stay off, the status names the function |
| Plugin removed | The character keeps its active spec (it is in the `.d2s`). The other specs wait in the file |
| skpoints cost not 1 in the mod | Swaps refused with a message (section 8) |
| TCP/IP client types `/spec` | MVP: refused ("only the host can swap"); the command is `LocalOnly` |
| Another MultiSpec copy loaded | Environment guard warns (template) |

## 7. Plugin parts and off-switch words

Built from `plugin-template`: build guard, `multispec-off.txt`, log, environment guard, parts table, status command.
Every engine call target is byte-checked at load with `CheckExpectedBytes` (for the D2RLoader-hooked ones, the bytes
after the jump), so a changed function turns its part off instead of being called.

| Word | Part | Needs the image | Depends on |
|---|---|---|---|
| `envguard` | Environment guard (template) | no | |
| `specstore` | Spec file: path, read, atomic write, CRC | no | |
| `specsync` | Load reconciliation on `LocalPlayerReady` | yes | `specstore` |
| `specswap` | The swap (section 4) | yes | `specstore`; works without `specsync` (it snapshots first) |
| `speccmd` | `/spec` command (and the chat route if needed) | no (refuses when `specswap` is off) | |
| `specui` | Phase 4 button / panel | no | `speccmd` |

Status command `multispec` (and `/spec` with no number): plugin version, image check, parts, off-file words,
character, file path, active spec, per spec "points used / unspent / state", counters (swaps done, refused, rolled back,
failed), last error. Log: one line per part at load, one line per swap and per reconciliation decision, before-and-after
spec lines on any failure.

Config (`multispec.toml`, embedded default; the gameplay keys listed in `[d2rl] match` so host and client agree):

```toml
[multispec]
spec_count = 2          # 1-8
swap_where = "town"     # "town" | "anywhere"
new_spec = "empty"      # "empty" | "copy"
cost_gold = 0           # Phase 4
cooldown_seconds = 0    # Phase 4
```

## 8. Engine limits that constrain this

| Limit | Effect |
|---|---|
| 30 class skills per class, one save byte each | A spec is one byte per class skill; n is read from the tables, so extended saves with more skills work |
| Hard levels are bytes in the save; maxlvl in skills.txt | Replay goes through D2R's maxlvl check |
| `.d2s` / `.d2rl` hashes, extended format | MultiSpec never writes saves |
| **D2R's respec refunds levels; spending costs `skpoints`** (new, found in the RE) | With a cost other than 1 every swap would mint or burn points. Refused per swap, warned at load |
| Skill tooltips assert on missing text | No skill tooltip changes; the Phase 4 UI is its own panel or a child layout |
| Shared plugins must match in TCP/IP; recorded in the character's plugin history | Version bumped every release; host-only swaps in the MVP |
| Unit state arrays (D2RLoader's wide states) | Untouched: states change only through D2R's own respec and spend code |

## 9. Checks needed before the MVP is trusted

1. A chat line `/spec 2` reaches the console command (else: chat listener in `speccmd`).
2. The console callback's `Client*` is the server client and runs on the game thread (else: queue to the game thread
   and find the player through the local client's unit id).
3. `0x4360f0` + `0x438670` called from a plugin leave the client in sync (tree, hotbar, auras), and what happens to
   summons whose skill drops to 0.

These are the first Phase 2 slot runs, before the swap is wired to the command.

## 10. Phase 3 tests (named checks, toolkit game slot)

| Check | Proves |
|---|---|
| `spec-status` | Plugin loaded on the right image, all parts on, file created for the test character |
| `spec-swap` | 1 -> 2 gives spec 2's levels and T - sum(spec 2) unspent |
| `spec-swap-back` | 2 -> 1 restores spec 1 exactly |
| `spec-save-load` | Swap, save and exit, reload: same active spec, same levels, file and `.d2s` valid (character still listed) |
| `spec-refused-outside-town` | `/spec 2` in the field changes nothing and says why |
| `spec-unspent-preserved` | Unspent points in each spec survive swaps, and a point earned in spec 1 shows up in spec 2 |
| `spec-crash-recovery` | Kill the game right after a swap: the next load picks the spec that matches |
| `no-new-crash-files` | Toolkit smoke check |

Checks read the plugin log and the `multispec` status output (harness console route), plus screenshots of the skill
tree.
