#!/usr/bin/env python3
"""verify_dump: check every game address, byte pattern and layout Quick NPC relies on against a D2R memory dump.

The dump is the running game image written in memory layout (section file offsets = RVAs), as d2rAI-toolkit's
tools/dump_image.py writes it. Run this after any D2R or D2RLoader update before changing the plugin's expected image:
every line must say "ok". A failure names the address to re-find (docs/reverse-engineering.md says how each was found).

    python tools/verify_dump.py <dump.exe>            pattern checks only (standard library)
    python tools/verify_dump.py <dump.exe> --table    also emulate D2R's NPC menu initializer and list the vendor
                                                      classes the plugin's rule finds (needs: pip install unicorn)

Exit status: 0 all checks passed, 1 a check failed, 2 usage error. Prints addresses and class numbers only: no game
data is written to disk.
"""
from __future__ import annotations

import struct
import sys

EXPECTED_STAMP = 0x6AB3782C

# Hook entries: whole instructions, >= 14 bytes, the exact bytes plugin/src/quick_npc.cpp passes to InstallInlineHook.
HOOKS = {
    "interact 0x101910": (0x101910, "40 53 55 56 57 41 56 48 83 ec 30 41 8b f0"),
    "open NPC menu 0x1141d0": (0x1141D0, "40 55 57 41 55 41 57 48 8b ec 48 83 ec 48"),
}

# (what, instruction RVA, exact bytes) for code the plugin's logic depends on.
CODE = [
    ("player action: left press sets flags 1|4", 0xFE65A, "83 ca 05"),
    ("NPC interaction only on a press (flag 4)", 0x102919, "f6 c3 04"),
    ("player command target at +0x10", 0x10192F, "48 8b 4f 10"),
    ("in range: CCMD_GET_QUEST_DIALOG 0x2f", 0x101E10, "b9 2f 00 00 00"),
    ("out of range: pending 0x2f on arrival", 0x101DC6, "c7 80 cc 01 00 00 2f 00 00 00"),
    ("SCMD_INTERACTION_ENABLE path calls open menu", 0x10EF90, "e8 3b 52 00 00"),
    ("speech-finished path calls open menu", 0x10DCB7, "e8 14 65 00 00"),
    ("Trade handler: mov edx, 1 (mode trade)", 0x112FC6, "ba 01 00 00 00"),
    ("Trade handler: mov cl, 0x38 (CCMD_SUBSCRIBE_TO_VENDOR)", 0x112FCB, "b1 38"),
    ("Trade handler entry", 0x112F30, "40 53 48 83 ec 20 33 db"),
    # Ctrl (the default hotkey) is D2R's default Run key; Run only adds the run flag, Stand Still blocks interaction.
    ("Run state read from key binding 0x22 (Run)", 0x83EEF, "8d 4b 22"),
    ("Run held adds command flag 0x40", 0xFE603, "83 ca 40"),
    ("Stand Still adds command flag 0x20", 0xFE619, "83 ca 20"),
    ("flag 0x20 (Stand Still) skips the interaction branch", 0x102624, "8b eb 83 e5 20 0f 85 63 03 00 00"),
    ("default bindings A: Run (0x22) = Ctrl (VK 0x11)", 0x22A9556, "00 00 11 00 01 00 00 00 22 00"),
    ("default bindings A: Stand Still (0x24) = Shift (VK 0x10)", 0x22A957E, "00 00 10 00 01 00 00 00 24 00"),
    ("default bindings B: Run (0x22) = Ctrl (VK 0x11)", 0x22A9AC6, "00 00 11 00 01 00 00 00 22 00"),
    ("default bindings B: Stand Still (0x24) = Shift (VK 0x10)", 0x22A9AEE, "00 00 10 00 01 00 00 00 24 00"),
]

# (what, instruction RVA, instruction length, offset of disp32 in the instruction, expected target RVA)
GLOBALS = [
    ("menu rows (vector data)", 0x10B752, 7, 3, 0x2A48830),
    ("menu row count", 0x10B759, 6, 2, 0x2A48838),
    ("menu NPC set flag", 0x1142C4, 10, 2, 0x2A48760),
    ("menu NPC unit id", 0x1142D9, 6, 2, 0x2A4875C),
    ("menu NPC class", 0x1142F0, 7, 3, 0x2A48764),
    ("NPC UI state = 1 at menu build", 0x114913, 6, 2, 0x2A486A4),
    ("menu object", 0x11485B, 7, 3, 0x2A487B0),
    ("Trade handler sets UI state 2", 0x112FA7, 10, 2, 0x2A486A4),
    ("Run held state written", 0x83F0A, 6, 2, 0x2A23758),
    ("player action reads Run held state", 0x8AFDD, 7, 2, 0x2A23758),
    ("player action reads Stand Still state", 0x8B018, 7, 2, 0x2A23760),
]

TRADE_RVA = 0x112F30
INIT_RVA, INIT_COPY_RVA = 0x360D0, 0x37210  # NPC menu table initializer; stop before it copies the stack array out
ROW_SIZE, ROWS = 0x48, 0x30
BASE = 0x140000000


def load(path: str) -> bytes:
    data = open(path, "rb").read()
    if data[:2] != b"MZ":
        raise ValueError("not a PE image (no MZ header)")
    return data


def stamp(data: bytes) -> int:
    pe = struct.unpack_from("<I", data, 0x3C)[0]
    if data[pe:pe + 4] != b"PE\0\0":
        raise ValueError("no PE header")
    return struct.unpack_from("<I", data, pe + 8)[0]


def hexbytes(text: str) -> bytes:
    return bytes.fromhex(text.replace(" ", ""))


def first_vendor_rule(rows: list[tuple[int, int, list[int]]], class_id: int, trade: int) -> bool:
    """The plugin's rule (quick_npc_logic.cpp): first row with the 16-bit class; Trade among options 0..count-2."""
    for cls, count, handlers in rows:
        if cls & 0xFFFF == class_id & 0xFFFF:
            shown = min(max(count - 1, 0), 5)
            return trade in handlers[:shown]
    return False


def emulate_table(data: bytes) -> list[tuple[int, int, list[int]]]:
    from unicorn import Uc, UC_ARCH_X86, UC_MODE_64  # type: ignore
    from unicorn.x86_const import UC_X86_REG_RSP  # type: ignore

    mu = Uc(UC_ARCH_X86, UC_MODE_64)
    mu.mem_map(BASE, (len(data) + 0xFFF) & ~0xFFF)
    mu.mem_write(BASE, data)
    stack = 0x7F0000000000
    mu.mem_map(stack, 0x100000)
    mu.reg_write(UC_X86_REG_RSP, stack + 0x80000)
    mu.emu_start(BASE + INIT_RVA, BASE + INIT_COPY_RVA)
    raw = bytes(mu.mem_read(mu.reg_read(UC_X86_REG_RSP) + 0x20, ROW_SIZE * ROWS))
    rows = []
    for i in range(ROWS):
        cls, count = struct.unpack_from("<II", raw, i * ROW_SIZE)
        handlers = list(struct.unpack_from("<5Q", raw, i * ROW_SIZE + 0x18))
        rows.append((cls, count, handlers))
    return rows


def main(argv: list[str]) -> int:
    args = [a for a in argv[1:] if not a.startswith("--")]
    if len(args) != 1:
        print(__doc__)
        return 2
    try:
        data = load(args[0])
    except (OSError, ValueError) as e:
        print(f"cannot read {args[0]}: {e}")
        return 2

    failed = 0

    def report(ok: bool, text: str) -> None:
        nonlocal failed
        failed += not ok
        print(f"{'ok  ' if ok else 'FAIL'} {text}")

    s = stamp(data)
    report(s == EXPECTED_STAMP, f"image stamp 0x{s:08x} (expects 0x{EXPECTED_STAMP:08x})")
    for what, (rva, expected) in HOOKS.items():
        live = data[rva:rva + len(hexbytes(expected))]
        report(live[:2] != b"\xff\x25", f"{what}: not hooked by D2RLoader (no jmp [rip+x])")
        report(live == hexbytes(expected), f"{what}: entry bytes {live.hex(' ')}")
    for what, rva, expected in CODE:
        live = data[rva:rva + len(hexbytes(expected))]
        report(live == hexbytes(expected), f"{what} at 0x{rva:x}")
    for what, rva, length, disp_at, target in GLOBALS:
        disp = struct.unpack_from("<i", data, rva + disp_at)[0]
        report(rva + length + disp == target, f"{what}: 0x{target:x} (referenced at 0x{rva:x})")

    if "--table" in argv:
        try:
            rows = emulate_table(data)
        except ImportError:
            print("skip --table: pip install unicorn")
        else:
            trade = BASE + TRADE_RVA
            report(sum(1 for _, _, h in rows if trade in h) > 0, f"menu table: {len(rows)} rows built, Trade handler present")
            vendors = sorted({c for c, _, _ in rows if first_vendor_rule(rows, c, trade)})
            others = sorted({c for c, _, _ in rows} - set(vendors))
            print(f"     vendor classes (Trade shown): {', '.join(str(c) for c in vendors)}")
            print(f"     other menu classes (no Trade): {', '.join(str(c) for c in others)}")

    print("all checks passed" if failed == 0 else f"{failed} check(s) failed: re-find these before using this image")
    return 0 if failed == 0 else 1


if __name__ == "__main__":
    sys.exit(main(sys.argv))
