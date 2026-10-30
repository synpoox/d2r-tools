# Phase 0: recon

Target: D2R 3.3 build 93847 (image stamp `0x6ab3782c`) under D2RLoader 1.3.1, PluginSDK 0.3.0.

## Repo inventory (start of project)

The repository held only `.mcp.json` (the `rea` MCP server, Ghidra provider, `GHIDRA_INSTALL_DIR=/opt/ghidra`). There was
no README, source, build file or earlier design. MultiSpec starts from the toolkit's `plugin-template/`.

Reused from the toolkit (d2rAI-toolkit; in this environment it is at `/opt/d2rAI-toolkit`, not `../d2rAI-toolkit`):

| Piece | Use in MultiSpec |
|---|---|
| `plugin-template/` | Whole skeleton: `build_guard.h` (image stamp), `off_switch.*` (`multispec-off.txt`), `log.*`, `env_guard.*`, `d2r.h/.cpp` (registry bindings), the parts table and status command in `plugin.cpp`, `CMakeLists.txt`, `build.ps1` |
| `knowledge/d2r-93847-addresses.md` | Town check, stat get/set, unit room |
| PluginSDK services | Console commands, `LifecycleService` (game join/leave, local player ready, level changed), `ThreadService` (run work on the game thread), `GameRuleService::canAllocateSkill`, `DataTableService` (skills rows), `PanelService` / `registerChildLayout` (Phase 4 button) |
| `testing/` | Game slots, `game_checks.py`, harness protocol (Phase 3) |
| `tools/check_public.py`, `tools/hooks/pre-commit` | Privacy scan before every commit |

Not reused: the template's example parts (`skills` slot-172 missile fan, `hook` Whirlwind hook), the sound guard and the
item type check. MultiSpec adds no sounds or item types.

## Hooks

Second pass, after the owner supplied a memory dump (stamp `0x6ab3782c`). Details and evidence for every "found" row:
[multispec.md](multispec.md). "Static" = read from code, not yet run
in game.

| # | Need | Status | Address / notes |
|---|---|---|---|
| 1 | Command dispatch (`/spec N`) | **SDK; routing to verify in game** | `RegisterConsoleCommand`. Unverified: that a chat line `/spec 2` reaches it, and that the callback's `Client*` is the server client on the game thread. The plugin checks the round trip before using it |
| 2 | Town check | **In registry, confirmed** | `0x2f0750(0x34b440(unit))`, the same pair D2R's own `UnitIsInTown(player)` test uses |
| 3 | Unspent skill points | **Found (static)** | Stat 5 = `STATS_NEWSKILLS` (D2R's test). Read `0x2f48c0` / `0x2f5020`. Written only by D2R's own respec and spend functions, which sync the client |
| 4 | Read a hard skill level | **Found (static)** | `0x33dcd0(unit, id, -1)` -> skill, then skill +0x40, or `0x33d1e0(unit, skill, 0, 0)` (D2RLoader-hooked: call, never hook) |
| 5 | Set hard skill levels with client sync | **Found (static)** | Reset: `0x4360f0(game, player)`, D2R's skills-only respec (Akara and Token use it). Replay: per point the checks `0x436bd0`, `0x339330`, `0x339c00` and maxlvl, then `0x438670(dataFlag, player, id)`, exactly as the add-skill packet handler `0x4b3ee0` does. Both send the levels to the client |
| 6 | Server player unit from a command | **Found (static)** | `0x484c90(client, 0)` = `ClientGetControlUnit`; game at client +0x2b0 or unit +0xd8; dataFlag game +0x106 |
| 7 | Class skill list | **Found (static)** | `0x33cb30(dataFlag, class)` count, `0x33dde0(dataFlag, class, i)` id: the save's "if" byte order. Record +0x2c = charclass |
| 8 | Character name | **Found (static), two sources** | PlayerData +0x00 (`0x34b240(unit)`) and Client +0x2c. The plugin logs both and refuses when they differ |
| 9 | Save read/write | **Not needed** | Active spec stays in the `.d2s`, saved by D2R. Other specs in a plugin-owned file |
| 10 | Skill hotkeys per spec | **Not found, optional** | Not looked for. D2R's respec leaves the hotkeys as they are; so would MultiSpec |

Tooling note: rea's Ghidra provider is not installed here (`/opt/ghidra` missing), so the analysis used capstone over the
dump plus a `.pdata` function index. The fastest lead was D2R's compiled-in test cases (`BC_ASSERT:` strings naming
`SkillsGetLevel`, `UnitIsInTown`, `ClientGetControlUnit`).

Constraint found: the respec refunds the **sum of levels**, while spending charges the skills.txt `skpoints` formula
(record +0x210; -1 = 1). A mod whose skpoints costs are not 1 would gain or lose points on every swap. MultiSpec refuses
to swap when any class skill's skpoints is not -1/1 (checked at load and per swap).

## Engine limits that shape the design

| Limit (source) | Effect on MultiSpec |
|---|---|
| 30 class skills per class; the save stores one byte per class skill (engine-limits, saves "if") | A spec is one byte per class skill, in skill-id order. Store the count with the spec and read the class's skill list from the loaded tables, so an extended-format mod with more skills works. A spec whose count or skill ids differ from the loaded tables is refused, never applied |
| Hard levels are bytes in the save | Levels 0-255 per skill; the game's own caps (maxlvl) apply |
| `.d2s` checksum, size and the `.d2rl` sidecar hashes (saves.md) | Never write the `.d2s` or `.d2rl` from the plugin. Let D2R save the active spec itself |
| Extended save format: hand-built data is rejected | Same reason: MultiSpec never edits saves |
| Skill tooltips assert on missing text (engine-limits) | No new skilldesc lines. Phase 4 UI uses a plugin panel or child layout, not skill tooltip lines |
| Indexed tables append-only | MultiSpec adds no table rows in the MVP |
| Shared plugins must match in TCP/IP and are recorded in the character's plugin history (d2rloader.md) | Characters played with MultiSpec record it. Bump the version every release. The swap runs on the server (host) |
