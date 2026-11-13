## 31. The `0x1255802` crash is `abort()` at process exit (issue #328)

Build: D2R 3.3 build 93847, D2RLoader 1.3.1-beta, image stamp `0x6ab3782c`. Every address is an RVA in the memory-layout dump (reports print them with the `0x140000000` base; those were subtracted). Note 22 here means [`22-automap-memory.md`](22-automap-memory.md) (the other file numbered 22 is the map panel). Static only: the dump was disassembled
(capstone over the dump, `.pdata` for function bounds) and the slot 5 crash reports were read; no game was started. Everything is from disassembly unless marked inference.

### What `0x1255802` is

Not an assert stub. `0x12557f0` (31 bytes) is D2R's **SIGABRT handler**: it calls a saved previous handler if one is set (`[0x35764a8]`), then
`mov byte ptr [0], 0` at `0x1255802` (the deliberate null write that kills the process). It is installed by the initializer at `0x12557ba`
(`lea rdx, [0x12557f0]; mov ecx, 0x16; call 0x12f5eb0`), and `0x12f5eb0` is the CRT `signal()` (it checks the signal number against the bitmask `0x608044`, takes the lock, and stores the
handler XOR-ed with the security cookie). `0x16` is `SIGABRT`. So `RCX = RBX = 0x16` in every report is the signal number, and the report is an ordinary `abort()` path.

`RDX = 0x1c783d0` (RVA; the report shows it as `0x141c783d0`) is **not a string**: `0x1c783d0` is `ff e0` (`jmp rax`), the control-flow-guard dispatch nop thunk, the value `RDX` held while the CRT called the handler.
There is no assert or format string to read. (The issue's "assert/format string at `0x1c783d0`" was a misreading of that register; [note 22](22-automap-memory.md) had the same reading and is corrected there.)

### The path (inner to outer; return addresses from the 2026-11-13 01:06 report, each checked to follow a `call`)

| Frame | Function | What it is |
|---|---|---|
| `0x12f5e6b`, `0x12f5b13`, `0x12f5c1d` | `0x12f5c44` `raise`, `0x12f5adc`, `0x12f5bf4` | CRT `raise(SIGABRT)` calling the handler |
| `0x12da4f4` | `0x12da4dc` | CRT `abort()`: `raise(0x16)`, then an exit tail (`0x12e5230`) |
| `0x139a97d` | `0x139a690` (750 bytes) | destructor of the `BGSInterface` object; at the call site `0x139a904` (`lea rcx,[rdi+0x50cac8]; call 0x122b180`, test at `0x139a909`) it calls `0x122b180` (`cmp [rcx],0; setne al`, i.e. `std::thread::joinable()`) on the member at `+0x50cac8`; if joinable it jumps to `0x139a978`, `call abort()`. Inference: this is `~std::thread` -> `std::terminate()` |
| `0x1c97d6a` | `0x1c97c9c` (232 bytes) | a block of static destructors; it destroys the global object at `0x35e1138` (`lea rcx, [rip+0x19493d3]; call 0x139a690` at `0x1c97d5e`) |
| `0x12fa4fb`, `0x12fa5dd` | `0x12fa470` (decodes the cookie-encoded table and calls each entry in reverse), `0x12fa5a8` | CRT `_execute_onexit_table` (`atexit` and static destructors) |
| `0x12e5043`, `0x12e4fc1`, `0x12e5147` | `0x12e4fd4`, `0x12e4f9c`, `0x12e50a4` | CRT `exit()` machinery (`common_exit`) |

Slots further out the stack (`0x78e60`, `0xfb794`, `0x12aee4`, `0x129cb1`, `0x184155`) are leftovers or not shown to be frames. `0x78e60` follows a `__chkstk` call in the
BC_ASSERT message logger (below), so it is not a caller. The code that called `exit()` is not known from the report.

### The object and what it checks

`0x35e1138` is the `D2BackendServices::BGS::BGSInterface` object (over `0x50d000` bytes), a subobject at `+0x38` of the singleton at `0x35e1100`: the getter `0x139dc70` returns `lea rax,[rip+0x2243460]` = `0x35e1100`, the shutdown `0x139e630` runs the stop on `lea rcx,[rsi+0x38]` = `0x35e1138`, and the static-destructor block destroys `0x35e1138` too. So the stop and the destructor act on the same object and the same `+0x50cac8` member. Strings at `0x1fc3f18`: `"BGS Thread"`,
`D2BackendServices\src\BGS\BGSInterface.cpp`, `BGSInterface::Connect`, `"Failed to connect to bgs: %d"`, `BGSInterface::ProcessThread`. `BGSInterface::Connect` (`0x13f94c9`) starts the worker
(1 MB stack, named `BGS Thread`) and move-assigns it into `+0x50cac8`; if that member is already joinable it calls `abort()` too (`0x13f9600`). The orderly stop is `0x13f9610` (if the running
flag at `+0x50cae0` is set, clear it and `join` through `0x122b190`), reached from the BGS shutdown `0x139e630` (call at `0x139e671`).

So what the crashing function's caller checks is (inference) `~std::thread` of the BGS worker: **the process was exiting (static destructors) with the BGS worker thread still joinable, i.e. never joined.** The `joinable` test and the `abort()` call are in the disassembly; that it is `~std::thread` / `std::terminate()` and that the worker is Battle.net's is inferred from the names in the strings.
It checks nothing about levels, units or tables.

### Can Cartographer feed it

No path was found. The report's stack holds only CRT exit, the BGS destructor and the static-destructor block. None of the Cartographer hooks (level build `0x3271c0`, the maze and
outdoor hooks, the room populator, the quality roll, the cube hook, the automap read) or table writes (slot level rows, `Warp` -1, `Vis` 0, empty `LevelEntry` and `LevelWarp`) is a caller of,
or reads, the BGS object, which is network-service state. Inference, not proof: whatever ended the game in those runs (the way the process was told to exit) skipped the BGS shutdown, and the
report does not show who called `exit()`.

What note 22 records fits an exit-time fault: the three reports came 3.3 to 7.7 minutes after the process started (the file name is the process start, `Generated UTC` the crash): 2026-11-07 17:15:19 to 17:18:39 = 3.3 min, 2026-11-13 01:06:47 to 01:14:28 = 7.7 min, 01:15:25 to 01:20:34 = 5.1 min, and the
Cartographer-only run that ended both games with Save and Exit left no report. Not checked here: how the 2026-11-07 run ended.

### The `warpDefs` assert (`LvlTbls.cpp:817`, seen once at the first Shorelines entry)

The mechanism is in [note 20](20-warpdefs-assert.md) and holds when re-read against the dump: `0x32c980(dataFlag, warpId, dir)` scans the `lvlwarp.txt` vector (tables `+0x1890`, count
`+0x1898`, `0x60`-byte rows), matches `row+0x2c == warpId` and direction `row+0x5c` (`'b'` matches any), and on no match logs through the assert stub at `0x32ca26` and returns NULL (`cmove rbx, 0`).
`warpId` is the level's `Warp[k]`; the slot rows have `Warp0..7 = -1`, so a stairs-type tile (orientation 10 or 11, via `0x3f4670`) in a slot level misses by design. Additions:

- The `exe+0x78EF6` the loader printed as the caller is **not** LvlTbls code. The assert stub `0x329960` is `lea rdx, text; lea rcx, descriptor; jmp 0x78e50`, and `0x78e50` is the shared BC_ASSERT message logger; `0x78ef6` is the return address after its `call 0x122a020`. Find the site from the stub call at `0x32ca26` instead.
- This assert returns and the game continues ("Continuing because assertion dialogs are disabled"); it never reaches `abort()`, so it is unrelated to the `0x1255802` crash.
- Cartographer adds no `lvlwarp.txt` rows and leaves `LevelWarp` empty (`plugins/cartographer/tools/install.ps1`), so nothing it writes changes the table the lookup scans; the one input it controls is `Warp[k] = -1`, the intended "no exit".
- Why it showed once in 11 entries is open: the stub has no once-only flag (the byte at `[rsp+0x40]` is a local "ignore" flag), so any dedupe is the loader's or the piece choice. Needs the game log of an entry with the piece list.

### Conclusion and what would confirm it

- The `0x1255802` crash is (inference) `std::terminate()` from `~std::thread` of the BGS worker (Battle.net backend) while the process exits. It is not an assert and not a level or table fault. No Cartographer code or data is on the path, so no Cartographer fix follows and none was made.
- Confirm in game: in slot 5 with only Cartographer loaded, end a game by the route the failing runs used (close the window or kill the game from outside, instead of Save and Exit) and look for a report with `RIP 0x1255802`, `RCX=0x16`; repeat with Cartographer parked. A report in both cases clears Cartographer. Record how the failing runs ended (the harness step that closed the game) and keep the last lines of `d2rloader.log`.
- If the exit route is the harness closing a running game, the fix is on the harness side (leave through Save and Exit first). If it is a D2R-only way to reach `exit()` without the BGS shutdown, it is D2R's own and needs no change here.

### Reproduce and byte witnesses

Read-only, against the memory-layout dump (needs `pip install capstone`; the script checks the stamp and ships no game bytes):

```
python -I plugins/cartographer/tools/dump_dis.py C:\ghidra-work\D2RLoader_6ab3782c_dump.exe --stamp 0x6ab3782c dis 0x12557f0 8
```

Key lines it prints (RVA, instruction):

| RVA | Instruction | Meaning |
|---|---|---|
| `0x12557f0`..`0x125580e` | `sub rsp,0x28; mov rax,[rip+0x2320cad]; test rax,rax; je 0x1255802; call rax; mov byte ptr [0],0; add rsp,0x28; ret` | SIGABRT handler; `.pdata` bounds `0x12557f0..0x125580f` |
| `0x12557ba`..`0x12557c6` | `lea rdx,[rip+0x2f]` (= `0x12557f0`); `mov ecx,0x16; call 0x12f5eb0` | installs it via CRT `signal(SIGABRT)` |
| `0x1c783d0` | `ff e0` = `jmp rax` | the `RDX` value is a CFG thunk, not a string |
| `0x139a904` / `0x139a909` / `0x139a978` | `call 0x122b180` / `test al,al; jne 0x139a978` / `call 0x12da4dc` | joinable test, then `abort()` |
| `0x139dc99` | `lea rax,[rip+0x2243460]` then `ret` at `0x139dca5` | singleton getter returns `0x35e1100` |
| `0x139e66d`, `0x139e671` | `lea rcx,[rsi+0x38]; call 0x13f9610` | shutdown stop on `0x35e1138` |
| `0x13f9610` | `movzx eax,byte ptr [rcx+0x50cae0]` ... `add rcx,0x50cac8; jmp 0x122b190` | clear running flag, `join` |
| `0x1c97d5e`, `0x1c97d65` | `lea rcx,[rip+0x19493d3]` (= `0x35e1138`); `call 0x139a690` | static destructor of the same object |
