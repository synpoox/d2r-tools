"""Re-check the byte signatures of Cartographer's map quality hooks against a D2R image (read-only, static).

Usage: python -I plugins/cartographer/tools/check_map_quality_signature.py <image> [--stamp 0x6ab3782c]

<image> is the running game image in memory layout (section file offset == RVA), as made by d2rAI-toolkit's
tools/dump_image.py. The signatures are read from src/engine/map_quality.cpp (the ones the plugin compiles in). Each must match
exactly once in the executable sections, at its recorded RVA; the bytes a hook cuts must be whole instructions (the guard
below); the item maker's own roll call (0x442e7b) must target the roll and end at 0x442e80, the return address the plugin counts.
Notes: docs/reverse-engineering/cartographer/26-map-quality.md. Exit status 0 only if every check passes.
"""
import argparse
import os
import re
import struct
import sys

# name: (RVA, hook size, guard = the whole instructions the hook cuts)
PARTS = {
    # mov [rsp+10],rdx / push r13 / push r15 / sub rsp,58 / mov r13,rdx; mov r15,rcx ends at 19 and E8 (call) follows
    "ItemQualityRoll": (0x444200, 16, bytes.fromhex("48895424104155415748" "83EC584C8BEA")),
    # mov [rsp+8],rbx / mov [rsp+10],rbp / mov [rsp+18],rsi / push rdi
    "MagicFallback": (0x442750, 16, bytes.fromhex("48895C2408" "48896C2410" "4889742418" "57")),
    "NormalFallback": (0x442820, 0, b""),
}
MAIN_ROLL_CALL = 0x442E7B
SOURCE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "src", "engine", "map_quality.cpp")
IMAGE_SCN_MEM_EXECUTE = 0x20000000


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("image")
    ap.add_argument("--stamp", default="0x6ab3782c")
    args = ap.parse_args()
    with open(SOURCE, encoding="utf-8") as f:
        text = f.read()
    with open(args.image, "rb") as f:
        img = f.read()
    pe = struct.unpack_from("<I", img, 0x3C)[0]
    stamp = struct.unpack_from("<I", img, pe + 8)[0]
    n = struct.unpack_from("<H", img, pe + 6)[0]
    opt = struct.unpack_from("<H", img, pe + 20)[0]
    ok = True
    print("image stamp 0x%x (expected %s)" % (stamp, args.stamp))
    sections = []
    for i in range(n):
        o = pe + 24 + opt + i * 40
        vsize, va = struct.unpack_from("<II", img, o + 8)
        flags = struct.unpack_from("<I", img, o + 36)[0]
        if flags & IMAGE_SCN_MEM_EXECUTE:
            sections.append((va, vsize))
    for name, (rva, size, guard) in PARTS.items():
        sig = re.search(name + r' = "([^"]+)"', text).group(1)
        pat = re.compile(b"".join(b"." if t == "??" else re.escape(bytes([int(t, 16)])) for t in sig.split()), re.S)
        hits = []
        for va, vsize in sections:
            hits += [va + m.start() for m in pat.finditer(img[va:va + vsize])]
        print("%s: signature matches %s" % (name, [hex(h) for h in hits]))
        ok &= hits == [rva]
        if size:
            good = img[rva:rva + size] == guard
            print("%s: guard (%d bytes) %s" % (name, size, "ok" if good else "MISMATCH " + img[rva:rva + size].hex(" ")))
            ok &= good
    roll = PARTS["ItemQualityRoll"][0]
    entry = img[roll:roll + 20]
    good = entry[19] == 0xE8 and entry[16:19] == bytes.fromhex("4C8BF9")
    print("ItemQualityRoll: first relative operand at +19: %s" % ("ok" if good else "MISMATCH"))
    ok &= good
    call = img[MAIN_ROLL_CALL:MAIN_ROLL_CALL + 5]
    target = (MAIN_ROLL_CALL + 5 + struct.unpack_from("<i", call, 1)[0]) & 0xFFFFFFFF
    good = call[0] == 0xE8 and target == roll
    print("item maker roll call at 0x%x targets 0x%x: %s" % (MAIN_ROLL_CALL, target, "ok" if good else "MISMATCH"))
    ok &= good
    print("OK" if ok else "FAILED")
    sys.exit(0 if ok else 1)


main()
