"""Re-check Cartographer's colour-grading parts (issue #442) against a D2R image (read-only, static).

Usage: python -I plugins/cartographer/tools/check_lut_grading_signatures.py <image>

<image> is the running game image in memory layout (section file offset == RVA), as made by d2rAI-toolkit's tools/dump_image.py.
Each of LutSubmit, LutFallbackCall, LutUploadCall and LutReleaseCall (src/core/part_signatures.h) must match exactly once in the
executable sections, at the address in note 50, and the call targets they give must be the ones the plugin uses. The upload
function's 25 guard bytes (src/engine/lut_grade.cpp) must be at its target. Exit status 0 only if every check passes.
"""
import os
import re
import struct
import sys

SIGNATURES = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "src", "core", "part_signatures.h")
ENGINE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "src", "engine", "lut_grade.cpp")


def main():
    with open(sys.argv[1], "rb") as f:
        img = f.read()
    with open(SIGNATURES, encoding="utf-8") as f:
        text = f.read()
    pe = struct.unpack_from("<I", img, 0x3C)[0]
    n = struct.unpack_from("<H", img, pe + 6)[0]
    opt = struct.unpack_from("<H", img, pe + 20)[0]
    sections = []
    for i in range(n):
        o = pe + 24 + opt + i * 40
        vsize, va = struct.unpack_from("<II", img, o + 8)
        if struct.unpack_from("<I", img, o + 36)[0] & 0x20000000:
            sections.append((va, vsize))

    def find(name):
        sig = re.search(name + r' = "([^"]+)"', text).group(1)
        pat = re.compile(b"".join(b"." if t == "??" else re.escape(bytes([int(t, 16)])) for t in sig.split()), re.S)
        return [va + m.start() for va, vsize in sections for m in pat.finditer(img[va:va + vsize])]

    def target(at):
        return at + 5 + struct.unpack_from("<i", img, at + 1)[0]

    ok = True
    submit = find("LutSubmit")
    print("LutSubmit: %s (expected [0xe291f0])" % [hex(h) for h in submit])
    ok &= submit == [0xE291F0]
    fallback = find("LutFallbackCall")
    got = fallback and (target(fallback[0]), target(fallback[0] + 15))
    print("LutFallbackCall: %s, targets %s (expected [0xee892f] -> 0xe21140, 0xe1ae50)" % ([hex(h) for h in fallback], got and [hex(t) for t in got]))
    ok &= fallback == [0xEE892F] and got == (0xE21140, 0xE1AE50)
    upload = find("LutUploadCall")
    got = upload and target(upload[0])
    print("LutUploadCall: %s, target %s (expected [0xe1b0c3] -> 0xfc3c60)" % ([hex(h) for h in upload], got and hex(got)))
    ok &= upload == [0xE1B0C3] and got == 0xFC3C60
    release = find("LutReleaseCall")
    got = release and target(release[0])
    print("LutReleaseCall: %s, target %s (expected [0xe2924c] -> 0xe90d70)" % ([hex(h) for h in release], got and hex(got)))
    ok &= release == [0xE2924C] and got == 0xE90D70
    with open(ENGINE, encoding="utf-8") as f:
        guard = re.search(r"UploadGuard\[UploadHookSize\] = \{([^}]+)\}", f.read()).group(1)
    guard = bytes(int(x, 16) for x in re.findall(r"0x([0-9A-Fa-f]{2})", guard))
    print("upload guard %s at 0xfc3c60: %s" % (guard.hex(" "), img[0xFC3C60:0xFC3C60 + 25] == guard))
    ok &= img[0xFC3C60:0xFC3C60 + 25] == guard
    print("OK" if ok else "FAILED")
    sys.exit(0 if ok else 1)


main()
