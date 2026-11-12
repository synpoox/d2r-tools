"""Re-check Cartographer's maze byte signatures against a D2R image (read-only, static).

Usage: python -I plugins/cartographer/tools/check_maze_signatures.py <image> [--stamp 0x6ab3782c]

<image> is the running game image written in memory layout (section file offset == RVA), as made by d2rAI-toolkit's
tools/dump_image.py. The packed D2R.exe on disk will not work: its code is encrypted.

For each maze function (docs/reverse-engineering/cartographer/01a-maze-signatures.md, "1a. Maze signatures") it checks that the signature
matches exactly once in the executable sections (VirtualSize of each, as the runtime scanner of scrollable-waypoints
does), that the match resolves to the recorded RVA, and that the guard (the whole signature compared under its mask at
the recorded RVA, which is what a hook checks before it patches) holds there. Where the signature reaches the end of
the recorded max patch length (bytes before the first relative branch or call), it also checks that the byte there is
a relative branch or call opcode. Exit status 0 only if every row passes. On a new build, a row that prints "no
match", "N matches" or another RVA needs its signature or RVA re-derived and the recording updated
(docs/reverse-engineering/RVA-AND-INTERCEPTION-WORKFLOW.md).
"""
import argparse
import re
import struct
import sys

# name, recorded RVA (build 93847), signature (hex bytes, ?? = wildcard: relative branch targets and one data-table
# offset), offset of the function start from the match (0: every signature starts at the function entry), max patch
# length (bytes from the entry before the first relative branch or call, from disassembly of the 93847 image)
SIGNATURES = [
    ("LevelBuild", 0x3271C0, "48 89 6C 24 20 56 41 56 41 57 48 83 EC 20 48 8B 82 C8 01 00 00 4C 8B F2", 0, 48),
    ("MazeWalk", 0x3E96D0, "48 89 7C 24 20 41 56 48 83 EC 20 4C 8B 81 C8 01 00 00 48 8B F9 48 8B 41 18", 0, 50),
    ("MazeEntranceExit", 0x3EB850, "41 56 48 83 EC 30 44 8B 0A 4D 8B F0 48 89 5C 24 40 4C 8B D2 48 8B 59 10", 0, 47),
    ("MazePiecePicker", 0x3EB960, "48 89 5C 24 18 48 89 6C 24 20 57 41 56 41 57 48 83 EC 20 4C 8B 09 33 DB", 0, 36),
    ("MazeRingGrower", 0x3E9820, "89 54 24 10 55 53 56 57 41 54 41 55 41 56 41 57 48 8B EC 48 83 EC 48 48 8B 49 10", 0, 39),
    ("MazeAddRoom", 0x3E77A0, "40 55 56 57 41 56 41 57 48 83 EC 30 48 63 EA 48 8B F9 48 8B 89 90 00 00 00", 0, 33),
    ("LvlMazeLookup", 0x32C6A0, "48 89 5C 24 08 57 48 83 EC 20 8B FA E8 ?? ?? ?? ?? 48 6B 88 ?? ?? ?? ?? ??", 0, 12),
    ("MazeLinkRooms", 0x360BB0,
     "48 89 5C 24 08 48 89 74 24 10 57 48 83 EC 20 41 8B D8 48 8B F2 48 8B F9 E8 ?? ?? ?? ?? 44 8D 43 FE", 0, 24),
    ("OverlapCheckA", 0x360520, "40 53 57 48 83 EC 38 48 8B DA 48 8D 44 24 60 48 8D 54 24 58 48 89 44 24 20", 0, 38),
    ("OverlapCheckB", 0x360470, "8B 01 4C 8B D1 44 8B 0A 41 3B C1 7D ?? 44 2B 49 08 44 2B C8 EB ?? 2B 42 08", 0, 11),
    ("OverlapCheckC", 0x360C80, "48 8B 41 10 4D 8B D8 4C 8B D2 48 85 C0 74 ?? 49 3B C2 74 ?? 49 3B C3 74 ??", 0, 13),
    ("ArrivalFinder", 0x3DA650, "48 89 5C 24 08 48 89 6C 24 18 48 89 74 24 20 57 41 54 41 55 41 56 41 57 48 83 EC 30 48 8B FA 41 8B F1 41 8B D0 41 8B D8 44 0F B6 E1 E8 ?? ?? ?? ?? 4C 8B AC 24 80 00 00 00 44 8B C3 4C 8B B4 24 88 00 00 00 48 8B D7 41 0F B6 CC 48 8B E8 41 C7 45 00 FF FF FF FF 41 C7 06 FF FF FF FF E8 ?? ?? ?? ?? 4C 8B F8 48 83 78 10 00 75 0C 48 8B D0 41 0F B6 CC E8 ?? ?? ?? ?? 83 BD 90 00 00 00 00 49 8B D7 41 0F B6 CC 74 ?? 83 FE 0D 75 0D 4D 8B CE 4D 8B C5 E8 ?? ?? ?? ?? EB 10 4D 8B CD 4C 89 74 24 20 44 8B C6 E8 ?? ?? ?? ?? 48 8B D0 41 0F B6 CC 48 8B D8 E8 ?? ?? ?? ?? 48 8B 43 58 E9 ?? ?? ?? ?? 4D 8B CE 4D 8B C5 E8 ?? ?? ?? ??", 0, 999),  # only checked, never hooked
    ("ArrivalRoomFinder", 0x3DAD90, "48 89 6C 24 18 57 41 56 41 57 48 83 EC 20 48 8B 7A 10 4D 8B F1 4D 8B F8 0F B6 E9 48 85 FF 74 15 48 8B CF E8 ?? ?? ?? ?? 85 C0 75 1A 48 8B 7F 48", 0, 999),  # only checked, never hooked
    ("ArrivalFlagTest", 0x3612D0, "8B 41 50 25 00 00 03 00 C3", 0, 999),  # only checked, never hooked
    # The outdoor slot parts (section 8 and section 8a): the max patch is from a capstone disassembly of the 93847 image, and where it lies
    # past the end of the signature the image byte there is checked. OutdoorAct5Builder is hooked (16 bytes); the others are only called.
    ("OutdoorEntry", 0x3EE320, "48 89 54 24 10 53 55 56 57 41 54 41 56 41 57 48 83 EC 50 4C 8B 7A 18 33 C0 4C 8B F2 44 0F B6 E1 49 89 87 A8 00 00 00 49 8D 4F 08 8B 42 2C 99 83", 0, 90),
    ("OutdoorAct5Builder", 0x3FE2E0, "40 55 53 56 57 41 54 41 56 41 57 48 8D 6C 24 D9 48 81 EC C0 00 00 00 83 BA F8 01 00 00 6E 48 8B F2 44 0F B6 F1", 0, 37),
    ("PieceStamp", 0x3ECFC0, "48 89 5C 24 08 44 89 4C 24 20 44 89 44 24 18 55 56 57 41 54 41 55 41 56 41 57 48 83 EC 30 4C 8B 62 18 48 8B FA 48 63 B4 24 90 00 00 00 45 8B E9 8B D6 4C 89 64 24", 0, 61),
    ("VoidCell", 0x3EDE00, "48 89 5C 24 08 48 89 74 24 10 57 48 83 EC 30 48 8B 59 18 45 33 C9 41 8B F8 C7 44 24 20 03 00 00 00 8B F2 48 8D 4B 08 E8 ?? ?? ?? ?? 48 8D 4B 58 C7 44 24 20 03 00 00 00 41 B9 00 01 00 00", 0, 39),
]

# Where an image byte past the end of a signature is checked: part name -> the instruction boundaries (bytes from the entry) of the
# prologue up to the first relative operand, from the same disassembly. The outdoor rows were produced with capstone on the build 93847 dump
# (x86-64, Cs(CS_ARCH_X86, CS_MODE_64), md.disasm(code_at_rva, rva)): the offsets of the instructions from the entry, stopping at the first
# call, jmp or jcc with a relative operand (docs/reverse-engineering/cartographer/08a-outdoor-build-hook.md). A hook length must be one of these and not above the max patch.
BOUNDARIES = {
    "LevelBuild": [5, 6, 8, 10, 14, 21, 24, 28, 34, 41, 48],
    "OutdoorEntry": [5, 6, 7, 8, 9, 11, 13, 15, 19, 23, 25, 28, 32, 39, 43, 46, 47, 50, 53, 56, 63, 67, 68, 71, 73, 75, 78, 81, 88, 90],
    "OutdoorAct5Builder": [2, 3, 4, 5, 7, 9, 11, 16, 23, 30, 33, 37],
    "PieceStamp": [5, 10, 15, 16, 17, 18, 20, 22, 24, 26, 30, 34, 37, 45, 48, 50, 55, 58, 61],
    "VoidCell": [5, 10, 11, 15, 19, 22, 25, 33, 35, 39],
}
# name -> bytes the plugin's hook copies (the others are only called)
HOOK_LENGTHS = {"LevelBuild": 14, "OutdoorAct5Builder": 16}

IMAGE_SCN_MEM_EXECUTE = 0x20000000


def is_rel_branch(b):
    # call, jmp rel32, jmp rel8, jcc rel8 (jcc rel32 is the two bytes 0F 80..8F: see is_rel_branch_at)
    return b in (0xE8, 0xE9, 0xEB) or 0x70 <= b <= 0x7F


def is_rel_branch_at(img, at):
    # The same test on the image, also accepting jcc rel32 (0F 80..8F), which OutdoorAct5Builder's first branch is.
    if at >= len(img):
        return False
    return is_rel_branch(img[at]) or (img[at] == 0x0F and at + 1 < len(img) and 0x80 <= img[at + 1] <= 0x8F)


def parse(sig):
    toks = sig.split()
    pat = re.compile(b"".join(b"." if t == "??" else re.escape(bytes([int(t, 16)])) for t in toks), re.S)
    return pat, toks


def sections(img):
    if len(img) < 0x40 or img[:2] != b"MZ":
        sys.exit("not a PE image (no MZ header)")
    pe = struct.unpack_from("<I", img, 0x3C)[0]
    if pe + 24 > len(img) or img[pe:pe + 4] != b"PE\0\0":
        sys.exit("not a PE image (no PE signature)")
    stamp = struct.unpack_from("<I", img, pe + 8)[0]
    n = struct.unpack_from("<H", img, pe + 6)[0]
    opt = struct.unpack_from("<H", img, pe + 20)[0]
    out = []
    for i in range(n):
        o = pe + 24 + opt + i * 40
        name = img[o:o + 8].rstrip(b"\0").decode("latin1")
        vsize, va = struct.unpack_from("<II", img, o + 8)
        flags = struct.unpack_from("<I", img, o + 36)[0]
        out.append((name, va, vsize, flags))
    return stamp, out


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("image")
    ap.add_argument("--stamp", help="expected PE TimeDateStamp (default: report only)")
    args = ap.parse_args()
    with open(args.image, "rb") as f:
        img = f.read()
    stamp, secs = sections(img)
    print(f"image stamp 0x{stamp:08x}, {len(img)} bytes")
    if args.stamp and int(args.stamp, 0) != stamp:
        print(f"stamp differs from expected {args.stamp}; signatures are still checked")
    code = [(n, va, sz) for (n, va, sz, fl) in secs if fl & IMAGE_SCN_MEM_EXECUTE and sz > 0]
    print("executable sections: " + ", ".join(f"{n}@0x{va:x}+0x{sz:x}" for n, va, sz in code))
    bad = 0
    for name, rva, sig, off, maxpatch in SIGNATURES:
        pat, toks = parse(sig)
        hits = []
        for n, va, sz in code:
            data = img[va:va + sz]
            pos = 0
            while True:  # overlapping search
                m = pat.search(data, pos)
                if not m:
                    break
                hits.append(va + m.start() - off)
                pos = m.start() + 1
        status = "ok"
        if len(hits) != 1:
            status = "no match" if not hits else f"{len(hits)} matches"
        elif hits[0] != rva:
            status = f"unique but resolves to 0x{hits[0]:x}, recorded 0x{rva:x}"
        elif not pat.match(img, rva + off):
            status = "guard (masked signature at the recorded RVA) differs"
        elif maxpatch < len(toks) and (toks[maxpatch] == "??" or not is_rel_branch_at(img, rva + off + maxpatch)):
            status = f"byte {maxpatch} of the signature is not a relative branch or call opcode"
        elif len(toks) <= maxpatch < 999 and not is_rel_branch_at(img, rva + off + maxpatch):
            status = f"byte {maxpatch} of the image (past the signature) is not a relative branch or call opcode"
        elif name in BOUNDARIES and (maxpatch != BOUNDARIES[name][-1] or HOOK_LENGTHS.get(name, 0) not in [0] + BOUNDARIES[name]):
            status = "the hook length is not an instruction boundary below the max patch, or the max patch is not the last boundary"
        if status != "ok":
            bad += 1
        print(f"{name:18} 0x{rva:06x}  sig {len(toks)} bytes ({toks.count('??')} masked), max patch {maxpatch}: "
              f"{status}" + (f" ({', '.join(hex(h) for h in hits[:4])})" if hits and status != "ok" else ""))
    print("all signatures ok" if not bad else f"{bad} signature(s) failed")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
