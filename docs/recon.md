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

| # | Need | Status | Source / notes |
|---|---|---|---|
| 1 | Command dispatch (`/spec N`, `/spec`, `multispec`) | **SDK** | `RegisterConsoleCommand` with `ConsoleCommandRegistration` (aliases, `ConsoleCommandScope`). **Open:** it is not confirmed that D2RLoader routes a chat line that starts with `/` to its console commands. Verify in a game slot. Fallback: a `SharedEventService` UI-message listener on the chat submit message, also unverified |
| 2 | Town check | **In registry** | `0x34b440` (unit) -> room, `0x2f0750` (room) -> nonzero in town. Hooked or called in game per the registry. SDK alternative: `GetLevelIdFromUnit` (reimplementation ordinal 2000) |
| 3 | Unspent skill points | **In registry (function)** | Stat `newskills` (classic id 5; confirm the id from itemstatcost at load). Read `0x2f48c0` (unit, stat, layer) base value, write `0x2f7d10` (unit, stat, value, layer). Whether a server-side set reaches the client without another call: **needs verification** |
| 4 | Read a hard skill level | **Needs RE** | No unit skill-list function or offset in the registry (classic: unit skill list, `SKILLS_GetSkill`, skill +level) |
| 5 | Set hard skill levels (server, synced to the client) | **Needs RE** | Preferred: D2R's own skills respec (the Akara / Token of Absolution path) plus D2R's spend-a-point handler, so prerequisites, passives, auras and client sync stay D2R's job. Alternative: the add-skill / set-level function plus its client update |
| 6 | Server player unit from a command / game callback | **Needs RE** | The SDK gives a `PlayerHandle`, never a unit pointer. Needs the client -> player unit path, or the game's player list |
| 7 | Class of a skill (which skills belong to a spec) | **Needs RE (small)** | Skills record (0x2ec bytes, getter `0x97790`) `charclass` offset is not in the registry. Can also be checked against `DataTableService` Skills rows |
| 8 | Character identity (name, class) for per-character storage | **Needs RE** | Player data name offset is not in the registry. Class: unit +0x08 for players (registry: class id) |
| 9 | Save read/write | **Not needed for the MVP** | Proposed design keeps the active spec in the `.d2s` as D2R saves it, and stores the other specs in a plugin-owned file next to the save. No `.d2s` write by the plugin. A save hook would only be needed for an exact "saved" moment |
| 10 | Skill hotkeys per spec (F1-F16, left/right) | **Needs RE, optional** | In the `.d2s` header at 0x28 (saves.md); the live copy is not in the registry |

## RE spike: blocked

Nothing in the "needs RE" rows could be resolved in this session:

- **No game image.** D2R.exe's code is encrypted on disk; the toolkit's method needs an in-memory dump of a running game
  (`tools/dump_image.py`, Windows). This container is Linux with no D2R install, no dump and no Ghidra project. No
  computer folder was shared through Remote Control.
- **No Ghidra.** `rea`'s Ghidra provider reports `executable_missing`: `/opt/ghidra` does not exist.

No address was guessed. Rows 4-8 stay "needs RE" until a session has the dump (see the questions to the owner).

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
