## 31. The `0x1255802` crash is `abort()` at process exit (issue #328)

Build: D2R 3.3 build 93847, D2RLoader 1.3.1-beta, image stamp `0x6ab3782c`. Addresses are RVAs in the memory-layout dump. Static only: the dump was disassembled
(capstone over the dump, `.pdata` for function bounds) and the slot 5 crash reports were read; no game was started. Everything is from disassembly unless marked inference.

### What `0x1255802` is

Not an assert stub. `0x12557f0` (31 bytes) is D2R's **SIGABRT handler**: it calls a saved previous handler if one is set (`[0x35764a8]`), then
`mov byte ptr [0], 0` at `0x1255802` (the deliberate null write that kills the process). It is installed by the initializer at `0x12557ba`
(`lea rdx, [0x12557f0]; mov ecx, 0x16; call 0x12f5eb0`), and `0x12f5eb0` is the CRT `signal()` (it checks the signal number against the bitmask `0x608044`, takes the lock, and stores the
handler XOR-ed with the security cookie). `0x16` is `SIGABRT`. So `RCX = RBX = 0x16` in every report is the signal number, and the report is an ordinary `abort()` path.

`RDX = 0x141c783d0` is **not a string**: `0x1c783d0` is `ff e0` (`jmp rax`), the control-flow-guard dispatch nop thunk, the value `RDX` held while the CRT called the handler.
There is no assert or format string to read. (The issue's "assert/format string at `0x1c783d0`" was a misreading of that register.)

### The path (inner to outer; return addresses from the 2026-11-13 01:06 report, each checked to follow a `call`)

| Frame | Function | What it is |
|---|---|---|
| `0x12f5e6b`, `0x12f5b13`, `0x12f5c1d` | `0x12f5c44` `raise`, `0x12f5adc`, `0x12f5bf4` | CRT `raise(SIGABRT)` calling the handler |
| `0x12da4f4` | `0x12da4dc` | CRT `abort()`: `raise(0x16)`, then an exit tail (`0x12e5230`) |
| `0x139a97d` | `0x139a690` (750 bytes) | destructor of the `BGSInterface` object; at `0x139a909` it calls `0x14122b180` (`cmp [rcx],0; setne al`, i.e. `std::thread::joinable()`) on the member at `+0x50cac8`; if joinable it calls `abort()` at `0x139a978`. This is `~std::thread` -> `std::terminate()` |
| `0x1c97d6a` | `0x1c97c9c` (232 bytes) | a block of static destructors; it destroys the global object at `0x35e1138` (`lea rcx, [rip+0x19493d3]; call 0x139a690` at `0x1c97d5e`) |
| `0x12fa4fb`, `0x12fa5dd` | `0x12fa470` (decodes the cookie-encoded table and calls each entry in reverse), `0x12fa5a8` | CRT `_execute_onexit_table` (`atexit` and static destructors) |
| `0x12e5043`, `0x12e4fc1`, `0x12e5147` | `0x12e4fd4`, `0x12e4f9c`, `0x12e50a4` | CRT `exit()` machinery (`common_exit`) |

Slots further out the stack (`0x78e60`, `0xfb794`, `0x12aee4`, `0x129cb1`, `0x184155`) are leftovers or not shown to be frames. `0x78e60` follows a `__chkstk` call in the
BC_ASSERT message logger (below), so it is not a caller. The code that called `exit()` is not known from the report.

### The object and what it checks

`0x35e1138` is the `D2BackendServices::BGS::BGSInterface` singleton (a very large object, over `0x50d000` bytes). Strings at `0x1fc3f18`: `"BGS Thread"`,
`D2BackendServices\src\BGS\BGSInterface.cpp`, `BGSInterface::Connect`, `"Failed to connect to bgs: %d"`, `BGSInterface::ProcessThread`. `BGSInterface::Connect` (`0x13f94c9`) starts the worker
(1 MB stack, named `BGS Thread`) and move-assigns it into `+0x50cac8`; if that member is already joinable it calls `abort()` too (`0x13f9600`). The orderly stop is `0x13f9610` (if the running
flag at `+0x50cae0` is set, clear it and `join` through `0x14122b190`), reached from the BGS shutdown `0x139e630` (call at `0x139e671`).

So what the crashing function's caller checks is `~std::thread` of the BGS worker: **the process was exiting (static destructors) with the BGS worker thread still running and never joined.**
It checks nothing about levels, units or tables.

### Can Cartographer feed it

No path was found. The report's stack holds only CRT exit, the BGS destructor and the static-destructor block. None of the Cartographer hooks (level build `0x3271c0`, the maze and
outdoor hooks, the room populator, the quality roll, the cube hook, the automap read) or table writes (slot level rows, `Warp` -1, `Vis` 0, empty `LevelEntry` and `LevelWarp`) is a caller of,
or reads, the BGS object, which is network-service state. Inference, not proof: whatever ended the game in those runs (the way the process was told to exit) skipped the BGS shutdown, and the
report does not show who called `exit()`.

What note 22 records fits an exit-time fault: the reports came 5.1 to 7.7 minutes after the process started (the file name is the process start, `Generated UTC` the crash), and the
Cartographer-only run that ended both games with Save and Exit left no report. Not checked here: how the 2026-11-07 run ended.

### The `warpDefs` assert (`LvlTbls.cpp:817`, seen once at the first Shorelines entry)

The mechanism is in [note 20](20-warpdefs-assert.md) and holds when re-read against the dump: `0x32c980(dataFlag, warpId, dir)` scans the `lvlwarp.txt` vector (tables `+0x1890`, count
`+0x1898`, `0x60`-byte rows), matches `row+0x2c == warpId` and direction `row+0x5c` (`'b'` matches any), and on no match logs through the assert stub at `0x32ca26` and returns NULL (`cmove rbx, 0`).
`warpId` is the level's `Warp[k]`; the slot rows have `Warp0..7 = -1`, so a stairs-type tile (orientation 10 or 11, via `0x3f4670`) in a slot level misses by design. Additions:

- The `exe+0x78EF6` the loader printed as the caller is **not** LvlTbls code. The assert stub `0x329960` is `lea rdx, text; lea rcx, descriptor; jmp 0x78e50`, and `0x78e50` is the shared BC_ASSERT message logger; `0x78ef6` is the return address after its `call 0x14122a020`. Find the site from the stub call at `0x32ca26` instead.
- This assert returns and the game continues ("Continuing because assertion dialogs are disabled"); it never reaches `abort()`, so it is unrelated to the `0x1255802` crash.
- Cartographer adds no `lvlwarp.txt` rows and leaves `LevelWarp` empty (`plugins/cartographer/tools/install.ps1`), so nothing it writes changes the table the lookup scans; the one input it controls is `Warp[k] = -1`, the intended "no exit".
- Why it showed once in 11 entries is open: the stub has no once-only flag (the byte at `[rsp+0x40]` is a local "ignore" flag), so any dedupe is the loader's or the piece choice. Needs the game log of an entry with the piece list.

### Conclusion and what would confirm it

- The `0x1255802` crash is `std::terminate()` from `~std::thread` of the Battle.net BGS worker while the process exits. It is not an assert and not a level or table fault. No Cartographer code or data is on the path, so no Cartographer fix follows and none was made.
- Confirm in game: in slot 5 with only Cartographer loaded, end a game by the route the failing runs used (close the window or kill the game from outside, instead of Save and Exit) and look for a report with `RIP 0x1255802`, `RCX=0x16`; repeat with Cartographer parked. A report in both cases clears Cartographer. Record how the failing runs ended (the harness step that closed the game) and keep the last lines of `d2rloader.log`.
- If the exit route is the harness closing a running game, the fix is on the harness side (leave through Save and Exit first). If it is a D2R-only way to reach `exit()` without the BGS shutdown, it is D2R's own and needs no change here.
