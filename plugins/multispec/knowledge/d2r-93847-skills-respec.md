# Player skills, skill points, respec, client: build 93847

**Build 93847 only**: D2R 3.3, D2RLoader 1.3.1, image PE timestamp **`0x6ab3782c`**. RVAs. Found for MultiSpec by
static analysis of an in-memory dump of the owner's own install (`D2RLoader_6ab3782c_dump.exe`, memory layout, file
offset = RVA; sections, stamp and three registry entry-byte sequences checked against the toolkit's
`d2r-93847-addresses.md`). Tool: capstone 5 over the dump plus a `.pdata` function index (rea's Ghidra provider was not
installed). Nothing here has run in game yet: "static" = read from code, "in game" = also seen working.

Fast lead that found most of this: D2R ships its **built-in test cases**. Their `BC_ASSERT:` strings name engine
functions (`SkillsGetLevel(player, skill, false) == 1`, `UnitIsInTown(player)`, `ClientGetControlUnit(hClient) ==
hPlayer`). Path: string -> 8-byte pointer to it in `.data` (assert descriptor) -> the small stub that loads the
descriptor (`lea rcx, [rip+desc]`, then `jmp` to the assert handler) -> the caller of the stub = the test body.

## Already hooked by D2RLoader 1.3.1 (`FF 25` at entry)

Call them normally; **never inline-hook them**, and do not expect their original entry bytes.

| RVA | Function |
|---|---|
| `0x2f5020` | stat total (unit, stat, layer) |
| `0x2f48c0` | stat base (unit, stat, layer) |
| `0x2f34f0` | add stat (unit, stat, value, layer) |
| `0x33d1e0` | `SkillsGetLevel` (below) |

Bytes after the 6-byte jump stay as in the dump: `0x2f5020` and `0x2f48c0` continue `90 90 90 90 48 89 74 24 20`,
`0x2f34f0` `57 41 56 48 83 EC 40 45 0F B7 F1`, `0x33d1e0` `48 8B F9 48 85 C9 75 1B`.

## Units and clients (server side)

| RVA / offset | Signature | Meaning | Evidence |
|---|---|---|---|
| `0x34b9d0` | (unit) -> i32 | unit type (0 player, 1 monster) | gates player-only paths in `0x4360f0`, `0x48fde0`, `0x436bd0` |
| `0x34b240` | (unit) -> PlayerData* | player data = unit +0x10 (asserts the unit is a player) | disassembly |
| PlayerData +0x00 | char[] | **character name** | passed as the `Character: %s` argument of the "Wrong count" log in `0x4b3ee0` |
| PlayerData +0xf0 | Client* | the player's server client | `0x48fde0` |
| `0x48fde0` | (unit) -> Client* | unit -> client (null unless player) | disassembly |
| unit +0xd8 | Game* | the unit's game; getter `0x48ff00` | disassembly; spend function passes it on |
| `0x484c90` | (client, 0) -> unit | **`ClientGetControlUnit`**: looks the unit up by type/id (client +0x26c / +0x270) in client +0x2b0's game via `0x48fe80`; 0 if client +0x278 is null or flag +0x4e0 bit 0 is set | test `ClientGetControlUnit(hClient) == hPlayer` (`0x5989c0`) and `player == ClientGetControlUnit(client)` (`0x4b60e0`) both call it |
| Client +0x2c | char[] | character name | `%s` argument (`lea r8, [r14+0x2c]`) in `0x484550`'s load-error log (`ClientAddPlayerToGame() Error Loading:%s`) |
| Client +0x2b0 | Game* | client's game | `0x484c90` |
| Game +0x106 | u8 | dataFlag (matches the registry) | `0x4b3ee0`, `0x4360f0` |

## Town

`UnitIsInTown(unit)` is **`0x2f0750(0x34b440(unit))`**: room of unit, then room -> "in town" (room +0x18 -> level
`0x360fc0` -> `0x327850`). Evidence: D2R's own test for `BC_ASSERT: UnitIsInTown(player)` (`0x25a100`) does exactly
this. Same pair as the registry.

## Skills on a unit

| RVA / offset | Signature | Meaning | Evidence |
|---|---|---|---|
| unit +0x100 | SkillList* | getter `0x34b6e0` (unit) | disassembly |
| SkillList +0x00 | Skill* | first skill; `0x34b720` (unit) also returns a first skill (used by add-skill) | `0x33dcd0`, `0x33a010` |
| Skill +0x00 | SkillsTxt* | record; record +0x00 i16 = skill id | `0x33dcd0` compares `movsx [rec]` with the id |
| Skill +0x08 | Skill* | next | list walks |
| Skill +0x10 | i32 | set from record +0x30 / +0x31 at creation (mode?) | `0x33a010` |
| Skill +0x40 | i32 | **hard (base) level** | `0x33d1e0` reads it as the base; `0x33a010` increments it |
| Skill +0x4c | i32 | owner item id, -1 = the unit's own skill | lookups compare with -1 |
| `0x33dcd0` | (unit, skillId, itemId -1) -> Skill* | `SkillsGetSkill` | test body `0x260590`: `0x33dcd0(player, 6, -1)` then `0x33d1e0(...) == 1` (`SkillsGetLevel(player, skill, false) == 1`) |
| `0x33d1e0` | (unit, skill, bonus, ?) -> i32 | `SkillsGetLevel`; bonus 0 = +0x40 only, else adds `0x3400a0`; clamped to the max from `0x300c70`. **D2RLoader-hooked** | same test; assert `SkillsGetLevel(hUnit, hSkill, TRUE, true)` |
| `0x33a010` | (unit, skillId) -> Skill* | `SkillsAddSkill`: +1 level (to max `0x214220`), or creates the skill at level 1 and appends it; applies the passive state (record +0xb4) | disassembly |
| `0x33cb30` | (dataFlag, class 0-7) -> count | number of class skills (data tables +0x11d0 per-class lists) | `0x4360f0` loop bound |
| `0x33dde0` | (dataFlag, class, index) -> skillId | the class's index-th skill: **the order of the save's "if" bytes** | `0x4360f0` loop |
| `0x97790` | (dataFlag, skillId) -> SkillsTxt* | record getter (registry); 0x2ec bytes | |

Skills record offsets found here (add to the registry's list): **+0x2c i8 charclass** (`0x436bd0`), +0x184 i16 maxlvl
(0 -> 20) (`0x4b3ee0`), +0x210 i32 skpoints formula (-1 = cost 1) (`0x438670`), +0x216 / +0x218 i16 requirement fields
compared with stats 0 / 2 (`0x339c00`), +0x21e i16 first prerequisite skill (`0x339330`), +0xb4 passive state.

## Skill points (stat 5 `newskills`)

`STATS_NEWSKILLS` = **5**: the skill-up test (`0x260590`) asserts `StatsGetCurrentStat(player, STATS_NEWSKILLS) == 0`
with `0x2f5020(player, 5, 0)`. Grants seen: level-up `0x52da30` (charstats SkillsPerLevel x levels), quests
`0x5193ee` (+1, +1, +2), others `0x412f60`, `0x580750`, `0x5de448`, `0x5e6d70`.

## Spending a point (server)

| RVA | Signature | What it does |
|---|---|---|
| `0x4b3ee0` | (game, player, packet, size 5) -> 1 | client "add skill" packet handler. Packet `{u8 cmd, u16 skill, u16 count}`. Per point: skill < count of skills, **`0x436bd0`** (player, skill) class check (record +0x2c == player class), **`0x339330`** (player, skill) level and prerequisite skills, **`0x339c00`** (player, skill) stat requirements, hard level < maxlvl, then `0x438670`. Logs "Wrong count" when no point was spent |
| `0x438670` | (dataFlag u8, player, skillId) -> 1 ok / 0 | **spend one point, no prerequisite checks**: cost = skpoints formula (record +0x210, `0x3b5160`) or 1; refuses when `newskills` (base, `0x2f48c0`) < cost; `newskills -= cost` (`0x2f34f0`); `0x33a010` add level; `0x33f830(player, 1)` refresh; passive callbacks; **`0x47f330`** sends the new level; states (`0x337720`, `0x3354c0`, `0x337890`). Called by `0x4b3ee0`, `0x433c19`, `0x475e6c` |
| `0x47f330` | (client, player, u16 skill, u8 level, i32 flag) | sends a skill's level to the client (`0x4360f0` passes level 0, flag 1; `0x438670` the new level, flag 0) |

## Respec (skills only)

`0x4360f0` (game, player): returns at once unless the unit is a player. For each class skill i (`0x33cb30`,
`0x33dde0`): skill = `0x33dcd0(player, id, -1)`; if present, total += `0x33d1e0(player, skill, 0, 0)`;
`0x433910(player, id, 0, 1)` removes it (with its auras, passives, states); `0x47f330(client, player, id, 0, 1)`.
Then `newskills += total` (`0x2f34f0`), `0x46f220(game, player)`, `0x474d20(game, player, 1)`. **Stats are not
touched.** Refund = sum of levels, not of skpoints costs.

Callers: `0x4106c0` (thunk returning 1: an NPC/quest table entry, likely Akara) and `0x580f20` (Token of Absolution:
`0x4360f0`, then `0x52ddf0` stat respec, then `0x470c90`).

## Entry bytes (static, from the dump)

Whole instructions, at least 14 bytes. For a byte check before calling, not for hooks.

| RVA | Bytes |
|---|---|
| `0x2f0750` | `48 83 EC 28 48 85 C9 75 07 33 C0 48 83 C4 28` |
| `0x34b440` | `40 53 48 83 EC 20 48 8B D9 48 85 C9 75 13` |
| `0x34a0e0` | `48 83 EC 28 48 85 C9 75 1A 88 4C 24 30 48 8D 4C 24 30` |
| `0x349860` | `48 83 EC 28 48 85 C9 75 1D 88 4C 24 30 48 8D 4C 24 30` |
| `0x34b9d0` | `48 83 EC 28 48 85 C9 75 1D 88 4C 24 30 48 8D 4C 24 30` |
| `0x34b240` | `40 53 48 83 EC 20 48 8B D9 48 85 C9 75 13` |
| `0x34b6e0` | `40 53 48 83 EC 20 48 8B D9 48 85 C9 75 20` |
| `0x33dcd0` | `48 89 5C 24 08 57 48 83 EC 20 41 8B D8 8B FA` |
| `0x33cb30` | `48 89 5C 24 08 57 48 83 EC 30 48 63 DA 0F B6 F9` |
| `0x33dde0` | `48 89 5C 24 08 48 89 74 24 18 57 48 83 EC 20` |
| `0x97790` | `48 89 5C 24 08 48 89 74 24 20 57 48 83 EC 30` |
| `0x436bd0` | `48 89 5C 24 10 57 48 83 EC 20 8B FA 48 8B D9` |
| `0x339330` | `48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18` |
| `0x339c00` | `48 89 6C 24 10 48 89 74 24 18 57 48 83 EC 20` |
| `0x438670` | `48 89 5C 24 18 48 89 6C 24 20 57 41 56 41 57` |
| `0x4360f0` | `48 85 D2 0F 84 3D 01 00 00 55 57 48 83 EC 38` |
| `0x4b3ee0` | `40 55 53 56 57 41 55 41 56 41 57 48 8D 6C 24 D9` |
| `0x484c90` | `48 89 5C 24 10 57 48 83 EC 20 8B FA 48 8B D9` |
| `0x48fde0` | `40 53 48 83 EC 20 48 8B D9 48 85 C9 74 1E` |
| `0x48ff00` | `40 53 48 83 EC 20 48 8B D9 48 85 C9 75 20` |
| `0x47f330` | `48 85 D2 0F 84 D6 00 00 00 53 55 56 57 48 83 EC 48` |
| `0x33a010` | `48 89 5C 24 08 48 89 6C 24 10 48 89 74 24 18` |

## Still to verify in game

- The `D2R::Game::Client*` a D2RLoader console callback receives: is it the server client above (so
  `0x484c90(client, 0)` gives the player), and does the callback run on the game thread? Check: the round trip
  `0x48fde0(0x484c90(client, 0)) == client` and unit type 0, before anything else.
- Whether a chat line `/spec 2` reaches a registered console command.
- Calling `0x4360f0` and `0x438670` from a plugin (outside the NPC, item and packet paths) leaves the client in sync.
