"""Re-check the call-site signature of Cartographer's map lighting hook against a D2R image (read-only, static).

Usage: python -I plugins/cartographer/tools/check_map_lighting_signature.py <image>

<image> is the running game image in memory layout (section file offset == RVA), as made by d2rAI-toolkit's
tools/dump_image.py. InstanceCall (src/core/part_signatures.h) must match exactly once in the executable sections, at the
call 0xf5d837 in VisualDataSystem::Update, and resolve (match + 5) to the return address 0xf5d83c the plugin counts; the other
call of VisualApply (0xf5d9b2) must not match. Notes: docs/reverse-engineering/cartographer/32-level-lighting.md.
Exit status 0 only if every check passes.
"""
import os
import re
import struct
import sys

CALL, RETURN, OTHER_CALL = 0xF5D837, 0xF5D83C, 0xF5D9B2
SIGNATURES = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "src", "core", "part_signatures.h")


def main():
    with open(sys.argv[1], "rb") as f:
        img = f.read()
    with open(SIGNATURES, encoding="utf-8") as f:
        sig = re.search(r'InstanceCall = "([^"]+)"', f.read()).group(1)
    pe = struct.unpack_from("<I", img, 0x3C)[0]
    n = struct.unpack_from("<H", img, pe + 6)[0]
    opt = struct.unpack_from("<H", img, pe + 20)[0]
    sections = []
    for i in range(n):
        o = pe + 24 + opt + i * 40
        vsize, va = struct.unpack_from("<II", img, o + 8)
        if struct.unpack_from("<I", img, o + 36)[0] & 0x20000000:
            sections.append((va, vsize))
    pat = re.compile(b"".join(b"." if t == "??" else re.escape(bytes([int(t, 16)])) for t in sig.split()), re.S)
    hits = [va + m.start() for va, vsize in sections for m in pat.finditer(img[va:va + vsize])]
    print("InstanceCall: signature matches %s, resolves to %s (expected [%s] -> %s)" % (
        [hex(h) for h in hits], [hex(h + 5) for h in hits], hex(CALL), hex(RETURN)))
    ok = hits == [CALL] and hits[0] + 5 == RETURN and OTHER_CALL not in hits
    print("OK" if ok else "FAILED")
    sys.exit(0 if ok else 1)


main()
