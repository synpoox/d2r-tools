"""Re-check Cartographer's portal byte signatures against a D2R image (read-only, static).

Usage: python -I plugins/cartographer/tools/check_portal_signatures.py <image> [--stamp 0x6ab3782c]

<image> is the running game image written in memory layout (section file offset == RVA), as made by d2rAI-toolkit's
tools/dump_image.py. The packed D2R.exe on disk will not work: its code is encrypted.

The signatures are read from src/core/part_signatures.h (the ones the plugin compiles in), so this checks what ships.
For each part (docs/reverse-engineering/cartographer.md, section 3) it checks that the signature matches exactly once
in the executable sections (VirtualSize of each, as the plugin's scanner does), that the match resolves to the recorded
RVA, that the whole signature under its mask holds at that RVA (the guard), and that the recorded max patch length ends
where the signature says: the byte there starts the first relative branch, call or RIP-relative instruction. For the
hooked parts it also checks that the hook size (bytes copied for the inline hook) fits in the max patch length. The
max patch length of a part that is only called is informational (never patched) and printed as n/a. Finally it checks
that OpenRedPortal contains calls (E8) to the matched GameCounter, which ties GameCounter's one unmasked displacement to
the right copy. Exit status
0 only if every row passes. On a new build, a row that prints "no match", "N matches" or another RVA needs its signature
or RVA re-derived and the recording updated (docs/reverse-engineering/RVA-AND-INTERCEPTION-WORKFLOW.md). The sibling
check_maze_signatures.py (maze parts) works the same way.
"""
import argparse
import os
import re
import struct
import sys

# name, recorded RVA (build 93847), max patch length (bytes from the entry before the first relative branch, call or
# RIP-relative instruction, from disassembly of the 93847 image), the signature bytes that start that instruction (None:
# the max patch length is past the end of the signature), hook size (bytes the plugin copies for an inline hook; 0: the
# part is only called)
PARTS = [
    ("ItemSpellCheck", 0x5817F0, 44, None, 15),
    ("ItemSpellUse", 0x581680, 38, None, 14),
    ("ItemRecord", 0x314110, 8, "E8", 0),
    ("UnitClassOf", 0x349860, 7, "75", 0),
    ("UnitRoom", 0x34B440, 12, "75", 0),
    ("RoomLevelId", 0x2EFC10, 3, "75", 0),
    ("ClientMessage", 0x491960, 27, None, 0),
    ("OpenRedPortal", 0x5DA800, 35, None, 0),
    ("GameCounter", 0x5D88B0, 34, "75", 0),
    ("UnitStat", 0x2F5020, 0, "FF 25", 0),
    ("UnitCoords", 0x34A040, 19, "75", 0),
    ("FindFreeSpot", 0x517F90, 23, "48 8B 05", 0),
    ("PlayerTick", 0x42E350, 27, "0F 84", 14),
    # Map stats (cartographer.md section 6)
    ("NewStatList", 0x2F7300, 45, None, 0),
    ("SetListStat", 0x2F7C00, 0, "FF 25", 0),
    ("AttachStatList", 0x2F2CB0, 0, "FF 25", 0),
    ("DetachStatList", 0x2F7920, 9, "E8", 0),
    ("FreeStatList", 0x2F4180, 3, "74", 0),
    ("MonsterSetup", 0x494A60, 43, None, 14),
    # Map events (cartographer.md section 9)
    ("PopulateRoom", 0x50ACF0, 12, "48 8B 05", 12),
    ("SpawnMonster", 0x492CF0, 56, None, 0),
    ("SpawnSuperUnique", 0x499BA0, 34, None, 0),
    # Event Spawn names from the game's tables (cartographer.md section 11)
    ("DataTablesBase", 0x300A90, 16, "72", 0),
    ("NameToRow", 0xA121F0, 13, "48 8B 05", 0),
    # Shrine and Boss Drop (cartographer.md section 13). DeathDrop is hooked (15 bytes); its first relative operand (jne) is at +33.
    ("DeathDrop", 0x447B80, 33, None, 15),
    ("DropRoller", 0x441300, 20, "48 8B 05", 0),
    ("DropLevel", 0x43E8A0, 15, "E8", 0),
    ("SpawnObject", 0x4022A0, 24, "E8", 0),
    ("CreateObject", 0x490410, 19, "48 8B 05", 0),
    # Ambush and Treasure (cartographer.md section 13). OperateObject is hooked (16 bytes: four whole instructions); its first relative
    # operand is the call at +32. The others are only called (their max patch is where the signature ends or its first call).
    ("OperateObject", 0x592C40, 32, None, 16),
    ("FleeSetup", 0x597030, 30, None, 0),
    ("FleeBlock", 0x34C2C0, 18, "E8", 0),
    ("FleeTarget", 0x34C730, 28, None, 0),
    ("TcLookup", 0x397290, 18, None, 0),
    ("TcRoller", 0x4404F0, 12, None, 0),
]

# The free-spot search (0x402f50) has no signature: it is the target of the call at SpawnObject + 0xff. The 17 bytes before that call
# (mov r9d, 0x3e01; mov r8d, 5; mov [rsp+0x20], rax) and the first 20 bytes of the target are what the plugin checks too.
FREE_SPOT_RVA = 0x402F50
FREE_SPOT_SITE = 0xFF
FREE_SPOT_CONTEXT = bytes.fromhex("41B9013E000041B80500000048894424" "20")
FREE_SPOT_PREFIX = bytes.fromhex("405355565741544155415641574881EC98000000")

IMAGE_SCN_MEM_EXECUTE = 0x20000000
HEADER = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "src", "core", "part_signatures.h")


def load_signatures():
    with open(HEADER, encoding="utf-8") as f:
        text = f.read()
    return dict(re.findall(r'constexpr const char\* (\w+) = "([^"]+)";', text))


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


def check_counter_calls(img, found):
    """OpenRedPortal calls GameCounter (at +0x5e and +0x13e in build 93847); report the E8 call sites that reach it."""
    portal, counter = found.get("OpenRedPortal"), found.get("GameCounter")
    if portal is None or counter is None:
        print("OpenRedPortal -> GameCounter: not checked (a part did not resolve)")
        return 1
    sites = []
    for off in range(0, 0x200):
        if img[portal + off] == 0xE8:
            target = portal + off + 5 + struct.unpack_from("<i", img, portal + off + 1)[0]
            if target == counter:
                sites.append(off)
    ok = len(sites) >= 2
    print(f"OpenRedPortal -> GameCounter: E8 calls at {', '.join('+0x%x' % o for o in sites) or 'none'}: {'ok' if ok else 'fewer than two'}")
    return 0 if ok else 1


def check_free_spot(img, found, code):
    """SpawnObject + 0xff is a call whose target is 0x402f50: its context bytes and the target's first bytes hold."""
    base = found.get("SpawnObject")
    if base is None:
        print("FreeSpot: not checked (SpawnObject did not resolve)")
        return 1
    site = base + FREE_SPOT_SITE
    problems = []
    if img[site] != 0xE8:
        problems.append("no E8 at SpawnObject+0xff")
        target = None
    else:
        target = site + 5 + struct.unpack_from("<i", img, site + 1)[0]
        if target != FREE_SPOT_RVA:
            problems.append(f"the call goes to 0x{target:x}, recorded 0x{FREE_SPOT_RVA:x}")
        if not any(va <= target < va + sz for _, va, sz in code):
            problems.append("the target is outside the executable sections")
        elif img[target:target + len(FREE_SPOT_PREFIX)] != FREE_SPOT_PREFIX:
            problems.append("the target's first bytes differ")
    if img[site - len(FREE_SPOT_CONTEXT):site] != FREE_SPOT_CONTEXT:
        problems.append("the bytes before the call differ")
    print(f"FreeSpot       0x{(target or 0):06x}  call at SpawnObject+0xff: {'ok' if not problems else '; '.join(problems)}")
    return 1 if problems else 0


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("image")
    ap.add_argument("--stamp", help="expected PE TimeDateStamp (default: report only)")
    args = ap.parse_args()
    sigs = load_signatures()
    with open(args.image, "rb") as f:
        img = f.read()
    stamp, secs = sections(img)
    print(f"image stamp 0x{stamp:08x}, {len(img)} bytes")
    if args.stamp and int(args.stamp, 0) != stamp:
        print(f"stamp differs from expected {args.stamp}; signatures are still checked")
    code = [(n, va, sz) for (n, va, sz, fl) in secs if fl & IMAGE_SCN_MEM_EXECUTE and sz > 0]
    print("executable sections: " + ", ".join(f"{n}@0x{va:x}+0x{sz:x}" for n, va, sz in code))
    bad = 0
    found = {}
    for name, rva, maxpatch, stop, hook in PARTS:
        if name not in sigs:
            print(f"{name:15} no signature in part_signatures.h")
            bad += 1
            continue
        pat, toks = parse(sigs[name])
        hits = []
        for n, va, sz in code:
            data = img[va:va + sz]
            pos = 0
            while True:  # overlapping search
                m = pat.search(data, pos)
                if not m:
                    break
                hits.append(va + m.start())
                pos = m.start() + 1
        status = "ok"
        if len(hits) != 1:
            status = "no match" if not hits else f"{len(hits)} matches"
        elif hits[0] != rva:
            status = f"unique but resolves to 0x{hits[0]:x}, recorded 0x{rva:x}"
        elif not pat.match(img, rva):
            status = "guard (masked signature at the recorded RVA) differs"
        elif stop is not None and " ".join(toks[maxpatch:maxpatch + len(stop.split())]) != stop:
            status = f"max patch {maxpatch}: the signature has {' '.join(toks[maxpatch:maxpatch + 3])} there, not {stop}"
        elif stop is None and maxpatch < len(toks):
            status = f"max patch {maxpatch} is inside the signature but no stop bytes are recorded"
        elif hook > maxpatch:
            status = f"hook size {hook} exceeds max patch {maxpatch}"
        if status != "ok":
            bad += 1
        else:
            found[name] = rva
        shown = str(maxpatch) if hook else "n/a (called)"
        print(f"{name:15} 0x{rva:06x}  sig {len(toks)} bytes ({toks.count('??')} masked), max patch {shown}"
              f"{', hook ' + str(hook) if hook else ''}: {status}"
              + (f" ({', '.join(hex(h) for h in hits[:4])})" if hits and status != "ok" else ""))
    bad += check_counter_calls(img, found)
    bad += check_free_spot(img, found, code)
    print("all signatures ok" if not bad else f"{bad} signature(s) failed")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
