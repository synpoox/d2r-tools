#!/usr/bin/env python3
"""verify_dump: check every game address and byte pattern Better Auras relies on against a D2R memory dump.

The dump is the running game image written in memory layout (section file offsets = RVAs), as d2rAI-toolkit's
tools/dump_image.py writes it. Run this after any D2R or D2RLoader update: every line must say "ok". A failure names
the address to re-find. Record offsets and stat numbers are not checked here (docs/reverse-engineering/better-auras.md).

    python tools/verify_dump.py <dump.exe>

Exit status: 0 all checks passed, 1 a check failed, 2 usage error. Prints addresses only: no game data is written.
"""
from __future__ import annotations

import struct
import sys

STUDIED_STAMP = 0x6AB3782C

# Hook entries: the exact bytes src/aura_api.h passes to InstallInlineHook.
HOOKS = [
    ("aura tick", 0x437460, "48 89 5c 24 08 48 89 6c 24 10 56 57 41 54 41 56 41 57"),
    ("start right aura", 0x43B680, "40 55 56 57 41 56 48 83 ec 48 48 8b fa 48 8b e9"),
    ("skill select body", 0x438A7A, "44 89 4c 24 20 55 41 56 41 57 48 83 ec 40"),
    ("client player action", 0xFE3B0, "48 89 5c 24 10 55 56 57 41 54 41 55 41 56 41 57"),
    ("unit death", 0x44DB20, "48 89 5c 24 08 48 89 6c 24 10 48 89 74 24 18 57 41 56 41 57 48 83 ec 50"),
    ("player regen tick", 0x42E600, "48 89 5c 24 20 55 48 83 ec 40 41 8b c1 c7 44 24 30 00 00 00 00"),
]

# Called functions (src/aura_api.cpp): entry bytes. Many are short shared prologues: a witness, not proof.
# "ff 25" alone: a jump D2RCore or an import owns, whose target changes per image.
CALLED = [
    ("scheduleAuraTick", 0x43B2B0, "4c 89 64 24 20 41 55 41"),
    ("removeEvents", 0x48B890, "40 53 56 41 54 41 56 41"),
    ("tickArg", 0x3E2B64C, "ff 25"),
    ("unitGame", 0x48FF00, "40 53 48 83 ec 20 48 8b"),
    ("rightSkill", 0x34B400, "40 53 48 83 ec 20 48 8b"),
    ("skillEntryId", 0x33E080, "48 85 c9 75 03 33 c0 c3"),
    ("unitSkill", 0x33DD40, "48 89 5c 24 08 57 48 83"),
    ("skillLevel", 0x33D1E0, "ff 25"),
    ("stateList", 0x2F5940, "48 89 5c 24 10 57 48 83"),
    ("detachList", 0x2F7920, "40 53 48 83 ec 20 48 8b"),
    ("freeList", 0x2F4180, "48 85 d2 74 0a 83 7a 1c"),
    ("setState", 0x3354C0, "48 89 5c 24 08 48 89 6c"),
    ("hasState", 0x3351B0, "48 89 5c 24 08 48 89 74"),
    ("setListStat", 0x2F7C00, "ff 25"),
    ("listStat", 0x2F5DF0, "ff 25"),
    ("stat", 0x2F5020, "ff 25"),
    ("setStat", 0x2F7D10, "ff 25"),
    ("addStat", 0x2F34F0, "ff 25"),
    ("unitRoom", 0x34B440, "40 53 48 83 ec 20 48 8b"),
    ("roomIsTown", 0x2F0750, "48 83 ec 28 48 85 c9 75"),
    ("unitDead", 0x34C2C0, "48 83 ec 28 48 85 c9 75"),
]


def stamp(data: bytes) -> int:
    pe = struct.unpack_from("<I", data, 0x3C)[0]
    if data[pe:pe + 4] != b"PE\0\0":
        raise ValueError("no PE header")
    return struct.unpack_from("<I", data, pe + 8)[0]


def run(data: bytes) -> int:
    failed = 0

    def line(ok: bool, what: str, rva: int) -> None:
        nonlocal failed
        failed += not ok
        print(f"{'ok  ' if ok else 'FAIL'} 0x{rva:x} {what}")

    for what, rva, text in HOOKS + CALLED:
        want = bytes.fromhex(text.replace(" ", ""))
        line(data[rva:rva + len(want)] == want, what, rva)
    return failed


def main(argv: list[str]) -> int:
    if len(argv) != 2:
        print(__doc__)
        return 2
    with open(argv[1], "rb") as f:
        data = f.read()
    if data[:2] != b"MZ":
        print("not a PE image (no MZ header)")
        return 2
    print(f"image stamp 0x{stamp(data):08x} (addresses studied on 0x{STUDIED_STAMP:08x})")
    failed = run(data)
    print(f"{failed} check(s) failed" if failed else "all checks passed")
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
