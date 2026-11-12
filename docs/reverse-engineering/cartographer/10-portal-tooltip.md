## 10. Red portal tooltip and area name (issue #90)

Static answers for issue #90: show each map's own `Name` on the red portal tooltip and as the area name inside the map, instead of the slot
level's string ("Colossal Summit"). Read from the build 93847 dump with `dump_dis.py` (Capstone); nothing was run in a game and no code changed.
Each claim is marked **static** (read from bytes or tables), **inference** (follows from static facts, not observed) or **needs game** (with the
exact check). The portal is opened by `portal.cpp` through `OpenRedPortal` `0x5da800` with a level id only; no string is passed (static).

| Field | Record |
|---|---|
| Date | 2026-11-12 |
| Consumer | Cartographer (issue #90): per-map name on the portal tooltip and in the map; no hook exists yet |
| Module | `D2R.exe` (`0x3e2b736` and `0x3e2b730` are D2RLoader `FF 25` thunks into `D2RCore.dll`: call, never hook) |
| Build identity | D2R 3.3 build 93847, image stamp `0x6ab3782c`, the dump `D2RLoader_6ab3782c_dump.exe` (same as sections 1a to 8) |
| Location | RVAs in the table below. Levels row: `0x18c` bytes, vector at `tables + 0x1460` (count `+0x1468`), `tables` from `0x300a90(dataFlag)`. String fields: Waypoint at `+0xec`, **LevelName `char[40]` at `+0xfd`**, LevelWarp `+0x125`, LevelEntry `+0x14d`. LevelName is a text key (a string), not a numeric id (static: loader descriptor table `{+0 name, +8 type, +0xc size, +0x10 offset}`). Objects row stride `0x168`, table `+0x1528`, SubClass `+0x127` (bit 4 = portal; objects 59 and 60 have SubClass 4 in the 3.3.0 `objects.txt`) |
| Guard | None added: no signature is written to `part_signatures.h`. Each masked prefix below was checked unique on the dump (a byte search with `??` as a wildcard, the same test as `pat`). Where a short prefix matched siblings it was extended until it matched once |
| Rule | The 14-byte rule of section 6 (a 14-byte patch at the entry, rounded up to whole instructions, must contain no relative operand): "hookable" passes, "call-only" does not. No hook is proposed or written here |
| Evidence | `dis <rva> [count]` for each function; `pat "<hex>"` for uniqueness; a scan of `E8` targets for caller counts |
| Validation | Static only. Not run in a game; not checked on Steam build 94995 |
| Failure mode | Not applicable (no code). A future hook would fail open: if the prefix does not match exactly once, leave the names alone |

### Parts

| RVA | Role | Masked prefix (1 match each) | Hook class |
|---|---|---|---|
| `0x32c4a0` | Levels row getter `(dataFlag, levelId) -> row*`; 46 callers. Three sibling getters (`0x331e50`, `0x331f50`, `0x332050`) share its first 46 bytes, so the prefix is extended to 82 bytes | `48 89 5C 24 08 55 56 57 48 83 EC 30 48 63 DA 0F B6 F1 E8 ?? ?? ?? ?? 48 8B E8 85 DB 7E 34 40 0F B6 CE E8 ?? ?? ?? ?? 48 8B B8 68 14 00 00 4C 63 C7 4C 3B C7 75 04 85 FF 79 14 48 8D 4C 24 58 C6 44 24 58 00 E8 ?? ?? ?? ?? 84 C0 74 01 CC 3B DF 7C 18` | hookable (15-byte cut ends at `48 63 DA`; first relative operand at `+0x12`) |
| `0xc7a30` | LevelName text getter: `E8` to the row getter, then reads the key at `row + 0xfd`; 11 callers | `48 83 EC 38 E8 ?? ?? ?? ?? 48 85 C0 74 ?? 48 8D 88 FD 00 00 00 80 39 00` | call-only (relative `call` at `+4`) |
| `0xc17e0` | Object hover text builder `(out buf, unit)`; the portal tooltip is built here. One caller, `0xc0916`, inside `0xc0420`. `.pdata` continues at `0xc182a` (the portal branch is in the continuation chunk) | `40 55 56 41 56 48 8D AC 24 20 FF FF FF 48 81 EC E0 01 00 00 48 8B 05 ?? ?? ?? ?? 48 33 C4 48 89 85 D0 00 00 00` | hookable (20-byte cut, no relative operand) |
| `0xc0420` | Unit label builder, calls `0xc17e0`; 3 callers, all in `0x880578` | `48 89 5C 24 10 48 89 74 24 18 48 89 7C 24 20 55 41 54 41 55 41 56 41 57 48 8D AC 24 10 F6 FF FF 48 81 EC F0 0A 00 00` | hookable (15-byte cut) |
| `0x14751e0` | Automap info text builder (string "DesecratedAutomapLevel"); no direct `E8` caller (called indirectly; inference) | `4C 8B DC 55 53 57 49 8D AB 28 F9 FF FF 48 81 EC C0 07 00 00 48 8B 05 ?? ?? ?? ?? 48 33 C4` | hookable (20-byte cut, no relative operand) |
| `0x34ad40` | Portal destination level id (loader-widened to 16 bits). It loads `objData = [unit + 0x10]` at `0x34ad7a`, then `jmp 0x3e2de40` into D2RLoader code, which returns `objData[+0x08] \| objData[+0x78] << 8` does `add rsp, 0x20` (the trampoline's own stack adjustment) and jumps back to `0x34ad86` (`pop rbx; ret`); the caller does `movzx edx, ax` (static). The prefix is unique only because of the loader's `E9` byte (`E9 rel32` after `48 8B 43 10`): the same bytes without it also match `0x34acf0` and `0x34adf0`. So this part is loader-patched and fragile: the key may shift with the loader version, and an unpatched `D2R.exe` would not match | `40 53 48 83 EC 20 48 8B D9 48 85 C9 75 13 88 4C 24 30 48 8D 4C 24 30 E8 ?? ?? ?? ?? 84 C0 74 01 CC 83 3B 02 74 14 48 8D 4C 24 30 C6 44 24 30 00 E8 ?? ?? ?? ?? 84 C0 74 01 CC 48 8B 43 10 E9` | call-only (relative `jne` inside the 14-byte cut; loader-patched tail) |
| `0x38fd00` | Objects row getter `(id)`, stride `0x168` | `48 89 5C 24 08 48 89 74 24 20 57 48 83 EC 30 8B DA E8 ?? ?? ?? ?? 48 8B F0 8B FB 48 3B 98 30 15 00 00 72 14` | hookable (15-byte cut ends at `48 83 EC 30`) |
| `0x1471bc0` | Party info text builder (strings "PartyInfoText", "LocationText"); calls `0xc7a30` at `0x1471dcd`. Not in the earlier table; checked here | `48 89 5C 24 20 55 56 57 41 54 41 55 41 56 41 57 48 8D AC 24 E0 FE FF FF 48 81 EC 20 02 00 00 0F 29 B4 24 10 02 00 00` | hookable (14-byte cut ends at `push r14`, no relative operand) |
| `0x1e9b40` | Waypoint panel builder (not a path for the portal) | `48 89 74 24 20 55 57 41 54 41 55 41 56 48 8D AC 24 80 FD FF FF 48 81 EC 80 03 00 00 48 8B 05 ?? ?? ?? ?? 48 33 C4` | hookable (21-byte cut: the `lea rbp,[rsp-0x280]` starts at `+13` and is 8 bytes; no relative operand) |
| `0x3e2b736` | Text lookup by key (`string_view*`), thunk into `D2RCore.dll`; 1217 callers | thunk (`FF 25 rel32`), not signable | call-only |
| `0x3e2b730` | Text lookup by numeric id (`0x150d` is the missing-text id), thunk into `D2RCore.dll` | thunk, not signable | call-only |

### Q1. Where does the red portal tooltip get its text?

- **Static.** In `0xc17e0` (continuation chunk at `0xc182a`). `0xc186a` calls the objects row getter `0x38fd00`, `0xc186f` reads SubClass `+0x127` and `0xc1876` tests bit 4. When it is set, `0xc1882` calls `0x34ad40`, which returns the destination level id (loader-widened to 16 bits: `objData[+0x08] | objData[+0x78] << 8`, `objData = [unit + 0x10]`; see Parts), and `0xc188e` gets the levels row from `0x32c4a0`. The LevelName key is read inline at **`0xc189f`** (`lea rcx, [rax + 0xfd]`, only when the row is not NULL; a NULL row takes `0x3e2b730(0x150d)` at `0xc18e5`). The key is looked up through `0x3e2b736` (`0xc18c6`) and copied with `0x1227840` into the output buffer (`0xc18f5`).
- **Owner suffix (static).** After the copy, `0xc18fa` calls `0x8d4b0` (true when the global at `0x2a233f4` is zero: `cmp dword ptr [rip + 0x2995f3d], 0` at `0x8d4b0`); if it returns true the builder jumps to the end and appends nothing. Otherwise (what the global means was not read; probably the multiplayer or online state, inference) it takes a name from `0x136670(unitId)` (a lookup in a global linked list by unit id at `+0x80`/`+0x84`), falling back to `0x1cb060(unit)` (`objData + 0x34`, the owner name stored on the portal, optionally passed through `0xa9bb0`), copies it with `0x1227840` into a 128-byte buffer, formats it with `0xbf7b0` and the format string `" (%s)"` into `[rsp+0x50]`, and appends it at `0xc1a95`. So a second part `" (<owner>)"` follows the level name when that global is non-zero. What the player sees in single player stays for the game (needs-game item 4).
- **Other branch (static).** SubClass bit `0x40` starts at `0xc19b3`: `0x34b440` gets the unit's room, `0x9a1b0(unit)` (`0xc19c7`) returns a text that `0xc19f9` copies into the output through `0x1227840`, `0x2efc10` gets the room's level id, `0x32c4a0` the levels row, and it appends two fixed key strings (5 and 7 characters) and then the LevelName key at `row + 0xfd` (`0xc1a5c`, looked up at `0xc1a8d`). This is **also a LevelName reader**, keyed on the level the object sits in: such an object inside the slot level would show the slot's name too. Bit 7 set (`js 0xc1b43`) skips the text.
- So yes, the portal tooltip is the target level's LevelName lookup, but it does **not** go through the LevelName getter `0xc7a30`. A hook on `0xc7a30` would not change the portal tooltip.
- **Callers of `0xc7a30` (11, static, classified by the enclosing `.pdata` function and its strings).**

  | Call site | Function | What it is |
  |---|---|---|
  | `0x147535b` | `0x1475301` (chunk of `0x14751e0`) | automap info text (string "DesecratedAutomapLevel") |
  | `0x1471dcd` | `0x1471bc0` | party info text ("PartyInfoText", "LocationText") |
  | `0x13b11fa` | `0x13b11e1` (chunk of `0x13b1100`) | progress and location list printer, level kind |
  | `0x1ea1a6` | `0x1e9b40` | waypoint panel builder |
  | `0x8e1c9` | `0x8e010` | script data model exporter ("Game.GameType", "Game.LevelId", "Game.Level", "Game.Room", "Game.Position.X"...): it calls `0xc7a30` for the unit's level id (`0x2efc10`) and copies the name into a string (`0x76210`, `0x12d3660`). The destination field was not followed. **This is the one that might feed a layout or banner** (inference: a "Game.*" data model could be what a layout binds to); needs game |
  | `0x165992` | `0x1656c0` | debug warp text ("Warp Pos: ...", "DRLG ID: %lu", "Mode: %s") |
  | `0x7c03a7` | `0x7c0305` (chunk of `0x7bfec0`) | no strings in the chunk; its neighbours are the Desecrate debug windows, so probably the same family (inference) |
  | `0x7c23aa` | `0x7c22b0` | Desecrate debug window ("%s (%d)", "DesecrateDiff#BaseLevel") |
  | `0x7c315e`, `0x7c31f7` | `0x7c2df1` (chunk of `0x7c2a00`) | Desecrate debug window ("Spawn Herald", "Zone ID", "Waypoint Level"...) |
  | `0x7c4632` | `0x7c4557` (chunk of `0x7c4470`) | Desecrate debug window ("Area", "Selected: %s") |

  None of these shows the entry banner text by its strings; `0x8e1c9` is the only one that could carry a current-level name to a layout, and it was not traced further.
- That the red portal object takes the SubClass-4 branch is inference (objects 59 and 60 have SubClass 4 in `objects.txt`; `portal.cpp` opens the portal through `OpenRedPortal`, which was not traced to its object id). **Needs game:** count hits on `0xc189f` while hovering the red portal.

### Q2. Where does the area name shown on entering a level come from, and the automap title?

- **Automap title and party text: static.** `0x14751e0` (automap info, call at `0x147535b`) and `0x1471bc0` (party location, call at `0x1471dcd`) call `0xc7a30`, so the LevelName getter is where their text comes from. `0xc7a30` is call-only (relative `call` at `+4`), so it cannot take an entry hook; reaching these two means hooking their own entries (both hookable; `0x14751e0` has no direct caller) or changing what `0xc7a30` reads (the row key, Q4 option 4). Both are inference; no hook is proposed. The progress and location list printer `0x13b1100` prints a group (kind 0) or a level (kind 1) from the levels row and the level group row (`0xc7a00` / `0x32c5a0`, table `tables + 0x1480`), also by key.
- **Area name banner on entering a level: not found.** No C++ reader of `row + 0xfd` outside the functions above turned up (scan of every caller of the row getter for `0xfd]`). The banner is probably driven from the layout or from `D2RCore.dll` (inference). The data model exporter `0x8e1c9` (Q1 callers table) is the one `0xc7a30` caller that might be involved. **Needs game:** enter the slot level with a log on `0xc7a30` and on `0x3e2b736` calls; if neither fires when the banner shows, the banner is in D2RCore and only a text replacement can change it.

### Q3. Does the server or the client know the target? Is a client-side hook enough in single player?

- **Inference** from the static reads: the tooltip builder reads the destination level id from the object data (`0x34ad40`: `objData[+0x08] | objData[+0x78] << 8`, with `objData = [unit + 0x10]`) and then the levels row itself; no server text is involved. The client therefore decides the text, and in single player the client and server share the process, so a hook in the client builder is enough. Nothing in the dump suggests the text is checked against the server.
- The slot level id is the same for every map, so the text cannot be told apart by level id alone: the hook must know the active map's `Name`. The plugin already has it (the draw).
- **Needs game:** open two different maps in a row and hover the portal; if the text does not change, a cache is involved.

### Q4. Safest hook point for the slot level id only

- **Slot level key is shared (static).** After PR #91 the slot level keeps Colossal Summit's name string, so a hook keyed on the text key (`0x3e2b736`) would also change the real Colossal Summit unless the slot gets a unique key first.
- **Options.**
  1. **`0xc7a30` (LevelName getter): not hookable at the entry.** Call-only (relative `call` at `+4`; the 24-byte prefix is unique), so no entry hook is proposed. It covers the automap title and the party text and **misses the portal tooltip** (Q1). Its text would have to be reached through the callers' entries (`0x14751e0`, `0x1471bc0`, both hookable) with the row key swapped around the original call (option 4), or by a mid-function patch at a call site; both are inference and not designed here.
  2. **`0x3e2b736` (key lookup thunk).** Call-only and a thunk into D2RCore with 1217 callers: a hot path, and the key alone does not say which level it is for. Not recommended.
  3. **`0xc17e0` (portal tooltip builder, hookable).** Wrap it: call the original, then if the unit's destination (`0x34ad40`, the loader-widened 16-bit level id) is the slot level id, overwrite the output buffer with the plugin's name. It sees the destination and runs only when the tooltip is built. The original appends the owner suffix `" (%s)"` itself (Q1, static), so the wrapper must replace only the level name part and keep the suffix. Hooking `0x34ad40` for the id is not advised: its prefix is unique only through the loader's `E9` byte (fragile), and `0xc17e0` already receives the unit. Covers the portal only.
  4. **Write the slot row's key.** Write the map's key into the slot row's `+0xfd` when the map opens; no code hook, and it would change the portal, automap and party text together (and any bit `0x40` object in the slot level, Q1). Needs the levels row to be writable and found (row getter `0x32c4a0`, hookable, or through the table); it is outside the SDK's read-only data table contract (`data_tables.h` rows are read-only, game-thread-only). **Needs game:** write a different key (at most 39 characters) at the slot row's `+0xfd` and hover the portal; check the page is writable.
- **Recommendation (inference):** option 3 for the tooltip; for the automap title and party text, option 4 if the writable-row check passes (otherwise wrap the `0x14751e0` and `0x1471bc0` entries and swap the key around the original, unverified). If the banner is in D2RCore (Q2), only option 4 or a text replacement reaches it.

### Q5. Does the SDK offer a string or localisation route that avoids an engine hook?

- **Static (SDK pin 717f727).** `LocalizationService` is lookup-only: it resolves a key to text and does not replace an engine string. A plugin's strings live in the plugin id's namespace. `ResourceService::registerResource` can register `data/.../d2rloader/<plugin-id>/...` bytes at runtime, which could carry strings (unverified that a runtime-registered string file is read by the lookup). `SharedEventService` has an item tooltip event and UI-message listeners only; there is no area name or portal tooltip event.
- **Inference (not read from the engine):** a LevelName key is `char[40]`, and if the engine lookup `0x3e2b736` resolves a `plugin-id:Key` string the way the plugin's own `getStringByKey` does, the whole key must fit 39 characters. The header documents only plugin-local keys for `getStringByKey`, plus the `d2r:` and `d2rloader:` prefixes, so whether `0x3e2b736` accepts a plugin prefix is **needs game**. The header also says "Package JSON defines strings by Key": that is a possible route for a fixed name (a key defined once in a package JSON), not for a per-map name.
- So the SDK has no per-map area or portal text route (static). A string table can give the slot level one fixed name, not a per-map name, unless the plugin rewrites the registered string at each map open (**needs game:** register, replace the resource at the next map open, and check the tooltip updates).

### Needs game

1. Does the red portal tooltip run `0xc189f`? Count hits while hovering the red portal. (Also: does the `0x8e1c9` data model exporter feed the entry banner? Log its call when entering the slot level.)
2. Does the area name banner call `0xc7a30` or `0x3e2b736`? Log both when entering the slot level.
3. Is the slot row writable? Write a different key at `+0xfd` and re-hover.
4. What does the owner suffix `" (%s)"` look like in single player (the `0x8d4b0` global decides whether it is appended)? Compare a stock portal tooltip with a hooked one.
5. Does the text cache between two maps? Open two different maps in a row.

### Game checks of 2026-11-12 (issues #87 and #90)

Live run, slot level 138 ("Cartographer Preset", LevelName key `ColossalSummit`, as installed by `tools\install.ps1` from origin/main 5c2ba9a). D2R 3.3 build 93847,
D2RLoader 1.3.1-beta, test mod and test save, single player, Normal difficulty (the two test maps had `Difficulty` set to Normal and the map was opened with
`cartographer open Shorelines`; one map per game). Screenshots of the game window only, read by hand; no hook or probe code was run. The two memory writes
below were made from outside the game with `WriteProcessMemory` into the loaded levels row (a test technique, not plugin code).

| Check | Result | Evidence |
|---|---|---|
| #87: red portal tooltip in town | Reads "Colossal Summit", not "Missing String"; no `" (owner)"` suffix in single player (needs-game item 4) | `hover_portal_town` |
| Entry banner | "Entering The Colossal Summit" | `enter_*` |
| Automap title inside the map | "Colossal Summit" | `automap_inside` |
| Return portal inside the map | "Harrogath" | `hover_portal_inside` |
| Leave, then enter again | Town banner "Entering Harrogath"; the red portal stays and still reads "Colossal Summit"; entering again works | `back_in_town`, `reenter_1`, `reenter_tip` |
| #90: the map's own `Name` ("Shorelines") | Appears nowhere: not on the tooltip, the banner or the automap. Still needs a runtime hook | all of the above |
| Item 3: is the slot row writable? | **Yes.** Writing `Harrogath` over the row's LevelName (`+0xfd`) changed the automap title inside the map to "Harrogath" at once, with no reload | `automap_after_name_write` |
| Item 1 (indirect): does the tooltip read the row key? | **Yes, live, no cache.** After the same write, the portal tooltip in town read "Harrogath" | `town_tip_after_write` |
| Banner source | The banner follows **LevelEntry** (`+0x14d`), not LevelName: writing `EnteringHarrogath` over it changed the banner on the next entry to "Entering Harrogath" | `banner_after_entry_write1` |
| Item 4: owner suffix | None in single player | `hover_portal_town` |

So the tooltip, the automap title and the banner all read the slot level's own row strings at the time they are shown. Writing existing string keys into LevelName
(`+0xfd`) and LevelEntry (`+0x14d`) changed the tooltip, the automap title and the banner live, in single player, with no hook; the tooltip is read when hovered, the
banner when entering. The row was found by a memory scan of the game process (one copy of the table; row 137 and row 138 are `0x18c` apart). The writes were made from
outside with `WriteProcessMemory`; whether the same write works in process was not tried ("writable" is shown only for that). That a plugin could find the row through the
table or the row getter is inference.

**Not shown:** the map's own name. Only existing keys were written, never a map's `Name`. Section 3 already shows that a LevelName that is not a key reads "Missing String"
(the first tooltip, #87), so a free-text `Name` such as "Shorelines" is expected to do the same. A string key per map is therefore a prerequisite for this route
(Q5: a runtime-registered string or a resolvable prefixed key; unverified). Option 4 of Q4 as written covers `+0xfd` only; the banner also needs `+0x14d`, so the route is two writes.

Static facts (dump `D2RLoader_6ab3782c_dump.exe`, stamp `0x6ab3782c`): the levels.txt loader builds its column descriptors on the stack, `0x20` bytes each
`{+0 name, +8 type, +0xc size, +0x10 offset}`; the names are loaded at `0x3300d4` (LevelName), `0x3300e2` (LevelWarp) and `0x3300f0` (LevelEntry), and the fields are stored at
`0x33024d..0x3302ad`:

| Column | type | size | offset |
|---|---|---|---|
| LevelName | 1 (string) | `0x27` | `0xfd` |
| LevelWarp | 1 | `0x27` | `0x125` |
| LevelEntry | 1 | `0x27` | `0x14d` |

Each field is `0x28` apart, a `char[40]`: a text key of at most 39 characters plus the NUL, not an index.

Conditions for the write route to be safe (for #90's design, not checked in game):

- Size: write at most 39 characters and always write the NUL; refuse, or fall back to the shared key, for a longer key (a `plugin-id:` prefix counts toward the 39).
- Restore: the row is process-wide table data and outlives the game. Save the original 40 bytes of both fields and write them back on map close, on `GameLeft` and on plugin
  unload, also for the Maze slot (139); otherwise the next game's slot (and `cheat goto 138`) shows the last map's name.
- Threads: write on the game thread when the map opens, before the portal exists, so no reader sees a half-written key. One copy of the table was found; a second data bank was not checked.
- Multiplayer: each process has its own tables. A host write changes only the host's text; a TCP/IP client keeps "Colossal Summit" unless its own plugin writes the same key, which needs the
  map identity sent to the client (#98 sync, section 12).
- Free text: needs a key (see above).

**Not checked:** hit counts on `0xc189f`, `0xc7a30` and `0x3e2b736` (items 1 and 2 directly; no probe code exists, only behaviour was observed); whether the
`0x8e1c9` data model feeds the banner (the banner is explained by LevelEntry, so probably not); two maps in two games (item 5; with one shared row key the text
was the same for every map anyway); a free-text name; the item right-click path (the right-click on a map scroll did nothing, cause not found; the log gave no gate line, so the item path was not confirmed here); a map opened in Hell; TCP/IP and a second player (the owner suffix there).
Test set-up notes: the spawned test item was a copy of the Identify scroll row, so it reads "Scroll of Identify". The character save failed every 30
seconds with an encoding error after an assert on a stat index (`BC_ASSERT nStat`). The cause is not shown. A missing `itemstatcost.txt` in the test mod is an unlikely guess (a mod without the table gets the
game's base table; the file only makes Cartographer skip map stats, and `PlayerStats` is off by default). To tell, rerun the same save and mod without `cartographer.dll`: if the assert stays,
it is setup or save state; if it goes, it is Cartographer. Not looked into; no Cartographer code was changed.

### Mod-free route for a per-map name (issue #90, desk work 2026-11-12)

The live checks above settle that the game reads the slot row's LevelName (`+0xfd`) and LevelEntry (`+0x14d`) keys when it shows the tooltip, the
automap title and the banner, and that a write there takes effect at once. What was missing is a **string key that resolves to the map's own
`Name`, without the active mod shipping a string file**. Evidence levels: **dump** (read from the build 93847 dump), **SDK** (read from the pinned SDK 717f727
headers and guides, not run), **needs game** (not shown). Nothing here was run in a game.

| Candidate | Result |
|---|---|
| (a) SDK string service that registers a key at runtime | **Route chosen.** SDK: a plugin owns the string namespace `PluginInfo.id` (`cartographer`) and defines text by a JSON array of `{Key, <language>...}` records at `data/local/lng/strings/d2rloader/<plugin-id>/<any>.json` (`LOCALIZATION.md`). `ResourceService::registerResource` takes a full virtual path under `.../d2rloader/<plugin-id>/...` and bytes from memory (`resources.h`: path "beginning with `data/`", copied before it returns, 64 MiB per resource). So the plugin needs no file in the mod: it builds the JSON from `maps.txt` and registers it. `LOCALIZATION.md` says "the full `namespace:key` must fit that field" for text fields that store the key in a fixed-size field; LevelName and LevelEntry are `char[40]` (dump, section 10 above), so the row value would be `cartographer:N<i>` (name) and `cartographer:E<i>` (entry), 15 to 18 characters. That the engine's lookup resolves a key written into a row after the tables loaded is **not** something the SDK shows (needs game, below) |
| (b) Hook the game's key lookup `0x3e2b736` | **Rejected.** Dump: `0x3e2b730` and `0x3e2b736` are `FF 25 rel32` loader thunks (bytes `ff 25 da f6 ff ff`, `ff 25 dc f6 ff ff`) into `D2RCore.dll`: not hookable by the 14-byte rule and not signable (the thunk bytes repeat across the whole thunk table: a `FF 25 ?? ?? ?? ??` pattern gives 865 matches). `D2RCore.dll` is not in the dump, so the lookup's code cannot be read or signed. The key alone also does not say which level it is for, and 1217 callers make it a hot path |
| (c) Hook the tooltip builder `0xc17e0` and whatever feeds the automap and banner | **Rejected as the main route, kept as the tooltip-only fallback.** Dump: `0xc17e0` is hookable and its prefix `40 55 56 41 56 48 8D AC 24 20 FF FF FF 48 81 EC E0 01 00 00 48 8B 05 ?? ?? ?? ?? 48 33 C4 48 89 85 D0 00 00 00` matches once (`.text 0xc17e0`, checked again 2026-11-12). It only covers the tooltip. The automap title (`0x14751e0` via `0xc7a30`) and the banner (LevelEntry) have no hook that was found, and the output buffer's encoding after `0x1227840` was not read. It also needs a new engine hook with its own guard, where the chosen route needs none |

#### What the chosen route does (written, not verified in game)

- Off by default: `MapNames=1` in `cartographer.txt` turns it on. Nothing is registered or written while it is `0`, so the working "Colossal Summit" text is untouched.
- At load and on every table reload the plugin registers one strings file: a `N<i>` record (the map's `Name`) and an `E<i>` record ("Entering " and the `Name`) for every map,
  with the same text in all 13 languages (a record with a missing language does not fall back to English). Names are trimmed, control characters become spaces, invalid UTF-8 becomes U+FFFD (so one bad name cannot spoil the whole file) and they are cut to 60 bytes at a whole UTF-8 character.
- When a map opens, on the game thread and just before `OpenRedPortal`, the plugin (1) finds the map's index in the registered list, (2) asks the localization service for both local keys
  and requires the exact registered texts back (so the loader is known to hold them before the game is pointed at them; otherwise it logs and leaves the slot's name), (3) gets the slot row
  from the levels row getter `0x32c4a0` with the data flag read from `game + 0x106` (the bank choice is **inference**, see Risks), (4) checks both fields look like text keys (a NUL inside, printable ASCII), (5) saves the original 40 bytes of both (refusing a row that already holds a `cartographer:` key, so the plugin's own key is never taken as the original),
  and (6) writes the two keys (NUL-filled to 40 bytes; a key of 40 or more characters is refused). Every page the 40 bytes touch is made writable for the copy only if it is not. Maze maps use slot level 139 the same way.
- The originals are written back on `GameLeft`, on plugin unload and when `OpenRedPortal` opens nothing. Only the first copy of a level is kept as the original. A failed open puts back only what that open wrote (the level's original if it was the first write, else the previous map's keys). A restore goes through the row pointer saved at write time, after a memory check, and touches a field only if it still holds the key the plugin wrote; it does not call the game's row getter, which could fault at process exit. If a write back fails, the saved original is dropped and the registration is kept as it is.
- A table reload (`cartographer check`) while a slot row holds a `cartographer:` key does not unregister or replace the strings file: the change (a new list, or `MapNames=0`) is kept pending and applied after the rows are back (on `GameLeft` or a failed open), so the open map's tooltip, title and banner never lose their text or show another map's. The SDK calls are made outside the plugin's lock.
- Every step fails open with a log line; the slot level keeps its own name.

| Field | Record |
|---|---|
| Part | `LevelsRow` `0x32c4a0` (levels row getter `(dataFlag, levelId) -> row*`), called only, never hooked. Signature (82 bytes, 12 masked) is the one in the Parts table above, added to `part_signatures.h` and to `tools/check_portal_signatures.py`: it matches once on the dump (`.text 0x32c4a0`); the first 46 bytes are shared with the sibling getters (`0x331e50`, `0x331f50`, `0x332050`; the 30-byte prefix also hits `0x32c5a0`), the first relative operand is the `E8` at `+0x12`. `python -I plugins/cartographer/tools/check_portal_signatures.py <dump> --stamp 0x6ab3782c`: "all signatures ok" |
| Wired to | `plugins/cartographer/src/engine/map_names.cpp` (parts, services, row writes), `plugins/cartographer/src/mapname/` (pure: keys, text, JSON, 40-byte writes, restore bookkeeping; `tests/mapname_tests.cpp`) |

#### Risks and what is still needed from the game

- **needs game (the one unproven link):** that the game's key lookup behind `0x3e2b736` resolves a `cartographer:N0` key found in a levels row. `LOCALIZATION.md` only says the full `namespace:key` must fit a fixed-size field; it does not say a key written into a row after the tables loaded resolves. The lookup lives in `D2RCore.dll`, which is not in the dump. If it does not, the tooltip, title and banner would read "Missing String" for every map while `MapNames=1` (hence off by default). Check: set `MapNames=1`, `cartographer open <Name>` and hover the portal.
- needs game: that a string file registered at runtime (not a file in an MPQ) is picked up by `getStringByKey` and the engine. The plugin verifies the first half itself before every write (`map names:` log lines, "D2RLoader does not return the registered text yet"); the engine half is the check above.
- needs game: that the row page is writable in process (the live write of 2026-11-12 used `WriteProcessMemory`, which ignores page protection); the plugin lifts the protection for the copy if needed.
- needs game: the entry banner format for a long name; the tooltip's `" (owner)"` suffix in TCP/IP (none in single player).
- Multiplayer: as above, a TCP/IP client keeps "Colossal Summit" unless its own plugin writes the same keys (needs the map identity from the #98 sync).
- **Known limit, one row per slot:** a second map opened through the same slot in one game rewrites the slot row, so the first map's portal (if it still stands) and the automap show the newest map's name. Not redesigned.
- **Data flag, inference:** the plugin picks the levels bank with the server's data flag (`game + 0x106`). The readers pick theirs by themselves; the tooltip uses the object unit's flag (`0x34a0e0`, `unit + 0x1bd`, at `0xc185d` and `0xc188a`). That both give the same bank in single player is inference, not shown. If they differed, the write would land in a bank nobody reads (the slot keeps its name).
- Restore by saved row pointer: if the game freed its levels table and left the heap untouched, bytes that still hold our key would be written into freed memory. `Readable` and the exact 40-byte key match make a stray write very unlikely, and the game loads the table once and tears it down at exit, so this is accepted.
- Restore on `GameLeft` assumes the event fires on a thread that may touch the table; one copy of the table was found in the earlier check, a second data bank was not (the plugin uses the game's own data flag).

#### Game check 2026-11-12 (MapNames)

Live check of `MapNames=1` (PR #124, off by default) on D2R 3.3 build 93847, D2RLoader 1.3.1-beta, slot 11 (test mod `D2RToolsTest`), test save `cartos` (Barbarian level 1, Normal), single player. `core-maps` moved out, the mod's own `levels.txt` plus the installer's "Cartographer Preset" (level 139), vanilla `lvlprest.txt`, `lvlmaze.txt`, `itemstatcost.txt` and a `misc.txt` with the `cm0` item, one map `Shorelines` (Preset, Corridor, Normal, no event, no stats), `MapNames=1`. Two games, same process.

**Result: the one unproven link holds.** (live game) The key lookup resolves `cartographer:N0` and `cartographer:E0` written into the slot row after the tables loaded.

- Log, at load: `map names: registered 1 map name(s) as strings`; `cartographer check` reports `Map names: on (written, not verified in game)` and `part LevelsRow: found by signature`.
- The log lines quoted below (and the `put back` restore line) were read during the run but were not kept as evidence; no log file or screenshot is stored with this PR.
- Log, on `cartographer open Shorelines`: `map names: level 139 row 0000000125941524: writing 'cartographer:N0' and 'cartographer:E0'`. On leaving: `map names: level 139 text put back`. The same pair appeared in the second game.
- The `getStringByKey` self-check logs nothing when it passes (it logs only on failure), so this run shows it passed by the absence of the failure line and by the write that followed.
- Portal tooltip (hover in town): `SHORELINES`. Not "Missing String", not "Colossal Summit".
- Entry banner on arrival: `Entering Shorelines` (red text, first screenshot after the load).
- Automap title inside the map, top right: `SHORELINES`, above the difficulty and mod lines.
- Second game in the same process: the tooltip read `SHORELINES` again after the row had been put back and written anew.
- No crash file, no error or warning line from the plugin.

Levels kept apart: unit tests and a clean build (`tools\build.ps1 -Plugin cartographer -Test`, 12 of 12) were earlier runs, not part of this PR, and were not re-run for it; live game, one map name, one language (English), one character, two games. Not checked: the stock name before a map is opened (no screen shows the slot level then; the `put back` line is the only evidence of the restore), a long map name on the banner, TCP/IP (the host's name on a client, #130), a second map in the same game (the one-row-per-slot limit above), other languages, the levels bank of the data flag (the write landed in the bank the tooltip and automap read, which settles that inference for single player), Steam build 94995.
