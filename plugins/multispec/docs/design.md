# MultiSpec design

WoW-style dual spec for D2R 3.3 (D2RLoader 1.3.1): a character keeps N saved skill allocations ("specs")
and switches between them with a command, without paying for a respec.

Status: **approved by the owner** (town only, empty new specs). Engine facts come from [multispec-recon.md](../../../docs/reverse-engineering/multispec-recon.md) and
[multispec.md](../../../docs/reverse-engineering/multispec.md); all of them are static so far, and
three need a check in a game slot before the MVP can be trusted (section 9).

## 1. The behaviour you asked for, confirmed or challenged

| Asked | Verdict | Why |
|---|---|---|
| Spec = hard skill levels + unspent points | **Changed: a spec stores hard levels only; unspent points are derived** | A character's total skill points T only grows (level-ups, Den of Evil, Radament, Izual, Book of Skill). If spec 2 stored its own unspent count, every point earned while in spec 1 would never reach spec 2. With T = (sum of live hard levels) + `newskills`, spec k's unspent points are T - sum(spec k). They are still shown per spec, and the "unspent points preserved" test still holds |
| Swap anywhere | **Changed (owner's decision): town only, not configurable** | Your own Phase 3 list has "swap refused outside town". In the field a swap removes auras, passives and the skills that keep summons going, mid-fight, and could be used to cheat (cast with spec A's skills, swap). Town also bounds what can go wrong. WoW allows it anywhere out of combat; D2R has no "out of combat" flag to reuse |
| 2 specs, N in config | **Confirmed** | `spec_count`, 3 by default, 1-99 |
| Trigger `/spec N` first, UI later | **Confirmed, one open check** | A D2RLoader console command. Whether a line typed in the chat box reaches it is not verified yet (section 9). If not, Phase 2 adds a chat-message listener for `/spec` |
| Per character, survives save/load, never corrupts `.d2s` | **Confirmed, with zero `.d2s` writes** | Section 3 |
| Respect engine limits | **Confirmed** | Section 8; one new limit found (skpoints costs) |

Other choices made here (change any of them):

- A new spec starts **empty** (all points unspent). Owner's decision; not configurable.
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
  refreshed from the live levels at every `spec` command, so normal point spending, Akara and the Token of
  Absolution need no special handling: they change the active spec.
- Points: T = sum(live levels) + live `newskills` (stat 5). A stored spec with sum(levels) > T is `invalid` (only a
  save edit or a data change can cause it) and cannot be loaded; it is kept, never deleted.

## 3. Where specs are stored, and why

One file per character, **next to its save**: `<save folder>\<name>.multispec`, where the save folder is
`<Saved Games>\Diablo II Resurrected\mods\<savepath>\` (Saved Games from `SHGetKnownFolderPath(FOLDERID_SavedGames)`,
`savepath` from the active mod's `modinfo.json`). With **no mod selected** D2R runs as retail and saves into
`<Saved Games>\Diablo II Resurrected\` itself, so that is the save folder; the same holds for a mod whose `modinfo.json`
has no `savepath`. `modinfo.json` is read only when a mod is active.

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
spec 1 ok 0 20 0 1 ... sel 36 -1 40 -1  (state, n levels; left and right mouse skill: id, item id)
spec 2 ok 0 0 0 0 ... sel 0 0 0 0
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
      in town; 1 <= N <= spec_count; N != active; no swap running;
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
              rollback fails too: leave the character reset (every point unspent, nothing lost) and keep `pending`
              in the file: "recovery". No spec is overwritten while in recovery; `spec N` loads any spec (the
              active one too) and clears it. Log an error with both reasons
```

The steps D2R does itself (removing auras and passives, states, client updates) stay D2R's job: MultiSpec never writes
a skill level or `newskills` directly.

## 5. Reconciliation

**Built in Phase 2: at the start of every `spec` command** (status or swap), in the same game-thread callback, before
anything else. Doing it on `LocalPlayerReady` instead would need the server player without a command's client, which
needs RE not done yet; the rules are the same, and D2R saves the live (active) spec either way, so nothing is lost by
waiting for the first command. The file is simply created or brought up to date then.

| Found | Action |
|---|---|
| No file | Create it: spec 1 = live, others empty |
| Name or class differ from the file (a new character with an old name) | Start fresh; the old file stays as `.bak` |
| Live == specs[active] | Nothing |
| Live == specs[k], k != active, live has points | The game was not saved after a swap (crash, Alt-F4): active = k, logged |
| `pending a>b` and live == specs[a] or specs[b] | Same rule: whichever matches becomes active; pending cleared |
| `pending a>b` and live matches no spec | Recovery (a swap failed both ways): no spec is changed; the player is told to load one with `spec N` |
| Live matches no spec by these rules | The player spent points or respecced in the active spec: specs[active] = live |
| A stored spec's skill ids differ from the loaded class list (mod data changed) | Remap by skill id; levels of skills that are gone become unspent; state `adjusted`, logged once |
| A spec's sum > T | State `invalid`: shown, never applied |

## 6. Failure modes

| Failure | What happens |
|---|---|
| Crash or kill during a swap | The swap is one game-thread callback, so the `.d2s` holds either the old or the new spec, never a mix; the file has both and `pending`. The next `spec` command (section 5) picks the matching spec |
| Crash between the file write and the swap | Same: the file still has both specs; live matches the old one |
| File write fails (disk, permissions) | Step 3 aborts; nothing changed; message and log line |
| Corrupt or unknown-version file | Kept as `.bad-<date>`; never overwritten in place; fresh file from the live character |
| D2R refuses a point during replay (prerequisite, stat requirement, data changed) | Rollback (step 7); the message names the skill |
| Game or loader update | No version gate: MultiSpec attempts to run. `CheckEntries` leaves `specswap` off if a function's bytes differ; other faults are logged as `FAULT` lines |
| Same stamp but a called function's bytes differ (another plugin patched it) | `specswap` stays off, the status names the function |
| A failed swap whose rollback fails too | Character reset (all points unspent), file in recovery: specs untouched until `spec N` |
| A game session ends while a request is queued | D2RLoader drops the queued callback; the command is free again after 10 s |
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
| `specstore` | Spec file: save folder from modinfo.json, fallback folder | no | |
| `specswap` | Every engine call: reading the character, reconciliation (section 5), the swap (section 4) | yes | `specstore` |
| `speccmd` | `spec` console command (and a chat route if section 9 check 1 needs one) | no (refuses when `specswap` or `specstore` is off) | |
| `specui` | Phase 4 button / panel | no | `speccmd` |

Status command `multispec` (and `/spec` with no number): plugin version, image check, parts, off-file words,
character, file path, active spec, per spec "points used / unspent / state", counters (swaps done, refused, rolled back,
failed), last error. Log: one line per part at load, one line per swap and per reconciliation decision, before-and-after
spec lines on any failure.

Config (`multispec.toml`, embedded default; the gameplay keys listed in `[d2rl] match` so host and client agree):

```toml
[multispec]
spec_count = 3          # 1-99
```

Phase 4 adds `cost_gold` and `cooldown_seconds`.

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

## 11. Spec panel UI (Phase 4, `specui`)

Goal: swap specs from the skill tree without the D2RLoader console.

Constraint (owner): MultiSpec is a **global plugin, never mod-scoped**, and needs nothing from the active mod. Spike 1
(`multispec.md`, "Skill tree layout") found the plugin can't add widgets to the stock skill tree: in PluginSDK 0.3.0
`registerChildLayout` takes only `StockPanel::PlayerInventory`, and a plugin's files can't replace
`skillstreepanelhd.json`. So the UI is **a plugin panel docked to the skill tree's left edge**, opened with it.

```
  +------+----------------------------------+
  | Spec |  skill tree (stock)              |
  | [^]  |                                  |
  | [ 2] |                                  |
  | [v]  |                                  |
  +------+                                  |
         +----------------------------------+
```

One slot shows the active spec, with an arrow above and below, so any number of specs fits (owner's decision).

| Piece | Design |
|---|---|
| Panel | `PanelService::registerPanel`, local id `SpecPanel`, logical name `multispec/SpecPanel`, flags `None`. Not `CloseOnEscape`: that takes Escape from the tree, which then stays open (spike 5). Not `GameplayLeftSlot`: that pairs it with the inventory. A `ClickCatcherWidget` over the visible part keeps clicks from reaching the world (spike 5) |
| Generated layout | The plugin writes the JSON at load (`spec_panel_layout.*`) and gives it with `ResourceService` at `data/global/ui/layouts/multispec/SpecPanelhd.json` |
| Placement | Root `anchor` `$RightPanelAnchor` (as the skill tree), `priority` -5, so the tree draws over it (spike 8). It moves with the tree at every resolution (spike 6) |
| Art | Stock sprites only, referenced by path, so the player's install supplies them and no game art is in the repository. The SDK can't read game files, so sprites can't be cropped at load; the tree covers what isn't wanted. Wing: `PANEL\Modals\Modal_No_Title_BG` (1245 x 453), left 160 px visible. Slot: `PANEL\HUD_02\belt_popped_row`, whose other three slots lie under the tree. Arrows: `FrontEnd\HD\Final\FrontEnd_UpButton` and `_DownButton` |
| Title | "Spec", the skill tree's `$StyleTitleBlock` at 85% size (`$MediumLargeFontSize`) |
| Number | The widget API cannot set text, so the layout holds a number for every possible spec and `setWidgetVisible` shows the active one. That includes specs above a lowered `spec_count`: the character keeps that spec, up steps to 1 and down to `spec_count` |
| Opens with the tree | The UI listener matches `PanelManager` `OpenPanel` / `ClosePanel` with text `SkillsTreePanel` (spike 4: sent for every route) and queues `openPanel` / `closePanel`. Opening asks for the active spec and whether the player is in town |
| Arrows | `onClickMessage` = `PanelManager:OpenPanel:multispec/SpecUp` or `SpecDown`. The listener consumes it (no such panel exists). A button's own target is never delivered (spike 5). Up is the next spec, and both directions wrap |
| Wheel | The SDK has no mouse input, so the plugin subclasses the game window (class `OsWindow`), as scrollable-waypoints does. A notch counts anywhere while the panel is open: the panels' screen position depends on the aspect ratio, and the tree has no use for the wheel |
| Swap | A step runs the hidden `spec-panel N` command through D2RCore's `ExecuteConsoleCommand` from the UI thread, so it takes the same path and checks as a typed `spec N`. One swap at a time: extra steps are dropped until the answer arrives, or for 5 s |
| Console | The panel prints nothing, except a swap that failed or rolled back. Everything is in the plugin log. Typed `spec` prints as before |
| Town only | The swap's own check refuses outside town. `setWidgetEnabled` has no visible effect, so greyed-out copies of the arrows (frame 2) replace them. The arrows update when the panel opens: walking out of town with the tree open leaves them lit until it reopens, and a swap is still refused |
| Off-switch | `specui` in `multispec-off.txt`: the panel and listener are not registered, and `spec` keeps working |

Spike results are in `docs/reverse-engineering/multispec.md`; test rows are in `TESTING.md`. Not seen in game: legacy
graphics and large font mode.
