# 53. The once-per-game superunique bitmap and the bosses.txt cap (issue #552)

Build 93847 (stamp `0x6ab3782c`). Static analysis of the dump with `plugins/cartographer/tools/dump_dis.py` and the toolkit's Ghidra
scripts (`FindDispRange.java`, `Decompile.java` through `tools/ghidra_query.ps1`). Evidence labels: **static**, **inference**, **seen in game**.

## Question

`bosses.txt` was capped at 16 entries (`MaxBosses`) because each entry gets its own `superuniques.txt` row and the game keeps an
"already spawned" bit per row in a bitmap whose length was not known (section 9). Is there a memory-safety bound, and where?

## Findings

| Fact | Evidence | Label |
|---|---|---|
| The bit is indexed by the **superuniques row** passed to `0x499ba0` (its 5th argument, `movsxd r15, [rbp+0x110]`), not by Cartographer's entry index and not by `hcIdx`. The installer appends the `cartoboss:` rows after every other row, so entry *n* is row `rows before + n` | `dis 0x499ba0` | static |
| Read `movzx eax, byte [rdx + r14 + 0x1d8]` with `rdx = row >> 3` (`0x499c6d`), bit mask from a dword table `[0x1d996d0 + (row & 7) * 4]` (`0x499c76`); write `or byte [rcx + r14 + 0x1d8], al` after a unit is made (`0x499d97`). The check is skipped when row `+0x29` is set, or when the global `0x260cded` is set; the write is not | `dis 0x499ba0 120` | static |
| The only bound on the row is the table: `0x397220` returns null for a row past the table's count (`[dt + 0x1080]`). Nothing compares the row with the bitmap's length | `dis 0x499ba0`, section 11 (`0x397220`) | static |
| These two are the only indexed accesses to `+0x1d8` in the image; nothing else clears or copies the bitmap | `FindDispRange 0x1d8 0x260` over the whole program | static |
| The bitmap is inline in the game record. A game is a hash-map node of `0x1aaf8` bytes, game = node + 0x10, zeroed with `memset(game, 0, 0x1aae8)` at `0x404f07` and built by `0x4034c0`, which sets nothing between `+0x1c0` and `+0x3838` | `Decompile 0x4049d0`, `Decompile 0x4034c0` | static |
| No game function found touches a field between `+0x1d9` and `+0x5820`: the 28 server functions that read the regions pointer `game + 0x1d0` were decompiled and their game-relative offsets listed; the game teardown `0x4057c0` reads `game + 0x1d0` and then nothing until `+0x5818` | `Decompile` of those functions, `dis 0x4057c0` | static |
| The code is the classic `pGame->nBossFlagList[nSuperUnique >> 3] & gdwBitMasks[nSuperUnique & 7]` / `|=` pair, with the same dword mask table; classic D2 declares `uint8_t nBossFlagList[64]` | D2MOO `Game.h` and `MonsterUnique.cpp` (public reconstruction of 1.10) | inference |
| Boss rows past vanilla's 66 (row 66 and up, byte 8 and up) spawned and set their bit in earlier game checks (sections 9, 34) | earlier checks | seen in game |

## Answer

The bitmap is **64 bytes at `game + 0x1d8`, rows 0 to 511** (static layout plus the classic declaration; no field in the gap was found, so a
smaller size is not supported by anything in the dump). The game indexes it with no bound of its own, so the memory-safety limit is on the
**row**, not on the number of `bosses.txt` entries: a superuniques row of 512 or more would read and write past the bitmap. Only one map is open
per game, and Cartographer already clears a Boss's bit before it spawns it again (section 34), so remembering which superuniques spawned has
no other reason to bound Cartographer.

## What changed

- `MaxBosses` (plugin) and `$MaxBosses` (installer) are gone: `bosses.txt` takes any number of entries.
- `install.ps1` skips an entry whose row would be 512 or later (`$MaxSuperRows`), with a line in the install output.
- `ep::kMaxSuperRow` is 511 (was `0xFFFF`): `superSeen` never reads past the bitmap, an event spawn of a superunique past it spawns its Class
  instead, and a map Boss whose row is past it is refused with a warning, so the game's unbounded write is never reached.
- Unit tests: `LoadBosses` with 40 entries keeps all 40 (`tables_tests`); `superSeen` reads row 511 and refuses 512 (`eventplan_tests`).

## Needs game

A load check with more than 16 bosses (done for pd2-resurrected #1419, see that PR). Not checked: a mod whose `superuniques.txt` reaches 512 rows.
