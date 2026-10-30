# Dump inventory (Phase 0)

The dump itself is **not** in this repository and never will be. It lives outside the repo tree
(`~/d2r-dump/`, mode 700). This file records only metadata about it.

## The file as received

| Item | Value |
|---|---|
| Delivery | 5 chunks (4 x 8 MiB + 6,127,533 bytes) concatenated in order part1..part5 |
| Container | ZIP (deflate), 39,681,965 bytes, one member, CRC test OK |
| Container SHA-256 | `f256b3771e14226bf508f7a212f049fe0805a5b7d5e4dd1ce58e553fba1c87d7` |
| Member | `D2RLoader_6ab3782c_dump.exe`, 90,451,968 bytes (0x5643000) |

## What kind of dump

**Static PE image of the main module, in memory layout.** It is not a process / minidump:

- every section's `PointerToRawData` equals its `VirtualAddress`, and the file size equals `SizeOfImage`
  (0x5643000), so **file offset = RVA**, the format produced by the toolkit's `tools/dump_image.py`
  (knowledge/engine-re.md section 1);
- it holds the image's own pages only: no heap, no stacks, no other modules (D2RCore.dll, plugins).
  `.data` is 8.4 % non-zero, consistent with statics before or without runtime heap pointers being meaningful.

## PE header

| Field | Value |
|---|---|
| Machine | 0x8664 (x64) |
| **TimeDateStamp** | **`0x6ab3782c`** (2026-09-23 06:56:44 UTC) |
| ImageBase (preferred) | 0x140000000 |
| SizeOfImage | 0x5643000 |
| AddressOfEntryPoint | 0x78d30 |
| Subsystem | 2 (Windows GUI) |
| Data directories | IAT 0x1cb0000, Exception (.pdata) 0x3bff000 size 0x1b9210, Load config 0x20dc8b0, TLS 0x4002000, Resources 0x4006000 |

## Sections (RVA = file offset)

| Name | RVA | Size | Flags | Non-zero % |
|---|---|---|---|---|
| .text | 0x00001000 | 0x01caf000 | code, RX | 90.6 |
| .rdata | 0x01cb0000 | 0x005df000 | R | 75.8 |
| .data | 0x0228f000 | 0x01970000 | RW | 8.4 |
| .pdata | 0x03bff000 | 0x001ba000 | R | 90.4 |
| _RDATA | 0x03db9000 | 0x00003000 | R | 54.1 |
| .rodata | 0x03dbc000 | 0x00001000 | R | 51.7 |
| .rsrc | 0x03dbd000 | 0x0001f000 | R | 49.4 |
| .reloc | 0x03ddc000 | 0x0004c400 | RW | 94.3 |
| .maho | 0x03e29000 | 0x00002a00 | RWX | 61.2 |
| .seija | 0x03e2c000 | 0x00000400 | RW | 27.1 |
| .koishi | 0x03e2d000 | 0x00001200 | RX | 65.2 |
| .d2rrel | 0x03e2f000 | 0x0004a000 | R | 96.9 |
| .bspace | 0x03e79000 | 0x00188000 | RW, uninitialised | 0.0 |
| .btext | 0x04001000 | 0x00000b36 | RX | 89.7 |
| .brdata | 0x04002000 | 0x000003e4 | R | 57.6 |
| .bdata | 0x04003000 | 0x00000030 | RW | 25.0 |
| .bpdata | 0x04004000 | 0x00000024 | R | 72.2 |
| .btls | 0x04005000 | 0x00001000 | RW | 0.0 |
| .brsrc | 0x04006000 | 0x0163b7a0 | R | 99.1 |
| .breloc | 0x05642000 | 0x0000001c | R | 50.0 |

The `.b*` sections and `.maho`/`.seija`/`.koishi`/`.d2rrel` are loader/packer additions (D2RLoader.exe is the
host image that maps D2R in), which is why the stamp is the D2RLoader 1.3.1 image stamp the toolkit documents.

## Does it match build 93847?

**Yes.**

1. TimeDateStamp `0x6ab3782c` = the toolkit's expected image (knowledge/d2r-93847-addresses.md, *Image*).
2. All 14 hooked entry-byte sequences listed in the registry's *Entry bytes summary* are byte-identical at their
   RVAs in this dump (14 match, 0 differ; checked with a script against the registry text).
3. Spot check: the stat getters `0x2f5020`, `0x2f48c0`, `0x2f5c60` and the state sender `0x53a210` start with
   `FF 25` (`jmp [rip+x]`), exactly as the registry says D2RLoader/D2RCore redirects them. So **these must never be
   hooked by this plugin**; calling them is fine.

No gate: the build matches.

## What this dump can answer

| Question | Can the dump answer it? |
|---|---|
| Function addresses, signatures (from the disassembly), entry bytes for hook checks | **Yes** (.text + .pdata function bounds) |
| Which functions belong to a source file (assert paths) | **Yes**: e.g. `D2Client\src\UI\Widgets\ExpBarWidget.cpp`, `D2Client\src\UI\Widgets\HUDPanelHD.cpp` are present |
| How the stat getter is called, which stat ids the XP bar code passes | **Yes** (immediates in the code) |
| How the experience table is indexed (row = level? +1? MaxLvl row?), record size, class column stride | **Yes**, from the lookup function's code |
| Struct offsets used by code (unit -> stats, client -> player unit global) | **Yes**, as displacements in the code |
| Address of the global that holds the local player unit | **Yes** (RIP-relative reference in the code), but its **value** is runtime |
| The experience **values** (level thresholds) | **No.** Tables are loaded from the game's CASC data onto the heap at run time. The plugin must read them at run time (D2RLoader `DataTableService`, `TableId::Experience`) |
| Live values: current XP of a character, the pointer in the player global, the table row size the loader reports | **No** - needs the live game (you) |
| Whether the overlay renders where we want, at what scale, over which panels | **No** - needs the live game |
| Whether D2RCore hooks something at run time beyond what is visible as `FF 25` in the image | Partly: patches applied after the dump was taken are invisible. The SDK's `DiagnosticsService` and the hook's own byte check cover this live |

## Tooling available in this sandbox

| Tool | State |
|---|---|
| Ghidra headless | **Not available.** The download from GitHub releases was refused by the network proxy (HTTP 403, "GitHub access to this repository is not enabled for this session"). Not retried by other routes |
| rea 6.7.0 (CLI present) | `rea doctor`: no analysis engine configured (Ghidra missing; Hopper/IDA/JADX not applicable). Unusable without Ghidra |
| Java 21 | present (would run Ghidra if it could be fetched) |
| **pefile 2024.8.26 + capstone 5.0.x** | **installed: the fallback used for Phase 2** |
| MinGW-w64 g++ 13 | installed: local compile check of the Windows sources (CI's MSVC is the real build) |
