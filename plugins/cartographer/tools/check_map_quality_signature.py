"""Re-check the byte signature of Cartographer's item quality roll hook against a D2R image (read-only, static).

Usage: python -I plugins/cartographer/tools/check_map_quality_signature.py <image> [--stamp 0x6ab3782c]

<image> is the running game image in memory layout (section file offset == RVA), as made by d2rAI-toolkit's
tools/dump_image.py. The signature is read from src/engine/map_quality.cpp (the one the plugin compiles in). It must match
exactly once in the executable sections, at the recorded RVA 0x444200; the 16 bytes the hook cuts must be whole
instructions (the guard below), and the first relative operand (the call) must start at byte 19, after the hook.
Notes: docs/reverse-engineering/cartographer/23-map-quality.md. Exit status 0 only if every check passes.
"""
import argparse
import os
import re
import struct
import sys

RVA = 0x444200
HOOK_SIZE = 16
# 48 89 54 24 10 (mov [rsp+10],rdx) 41 55 (push r13) 41 57 (push r15) 48 83 EC 58 (sub rsp,58) 4C 8B EA (mov r13,rdx): 16 bytes;
# 4C 8B F9 (mov r15,rcx) ends at 19 and E8 (call) follows.
GUARD = bytes.fromhex("48895424104155415748" "83EC584C8BEA")
SOURCE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "src", "engine", "map_quality.cpp")
IMAGE_SCN_MEM_EXECUTE = 0x20000000


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("image")
    ap.add_argument("--stamp", default="0x6ab3782c")
    args = ap.parse_args()
    with open(SOURCE, encoding="utf-8") as f:
        sig = re.search(r'ItemQualityRoll = "([^"]+)"', f.read()).group(1)
    with open(args.image, "rb") as f:
        img = f.read()
    pe = struct.unpack_from("<I", img, 0x3C)[0]
    stamp = struct.unpack_from("<I", img, pe + 8)[0]
    n = struct.unpack_from("<H", img, pe + 6)[0]
    opt = struct.unpack_from("<H", img, pe + 20)[0]
    ok = True
    print("image stamp 0x%x (expected %s)" % (stamp, args.stamp))
    pat = re.compile(b"".join(b"." if t == "??" else re.escape(bytes([int(t, 16)])) for t in sig.split()), re.S)
    hits = []
    for i in range(n):
        o = pe + 24 + opt + i * 40
        vsize, va = struct.unpack_from("<II", img, o + 8)
        flags = struct.unpack_from("<I", img, o + 36)[0]
        if flags & IMAGE_SCN_MEM_EXECUTE:
            hits += [va + m.start() for m in pat.finditer(img[va:va + vsize])]
    print("signature matches: %s" % [hex(h) for h in hits])
    ok &= hits == [RVA]
    entry = img[RVA:RVA + 20]
    print("guard (%d bytes): %s" % (HOOK_SIZE, "ok" if entry[:HOOK_SIZE] == GUARD else "MISMATCH " + entry.hex(" ")))
    ok &= entry[:HOOK_SIZE] == GUARD
    print("first relative operand at +19: %s" % ("ok" if entry[19] == 0xE8 and entry[16:19] == bytes.fromhex("4C8BF9") else "MISMATCH"))
    ok &= entry[19] == 0xE8 and entry[16:19] == bytes.fromhex("4C8BF9")
    print("OK" if ok else "FAILED")
    sys.exit(0 if ok else 1)


main()
