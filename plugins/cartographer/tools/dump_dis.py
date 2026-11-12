"""Read-only disassembly and signature search over a D2R memory-layout image (static; ships no game bytes).

Usage:
  python -I plugins/cartographer/tools/dump_dis.py <image> --stamp 0x6ab3782c dis <rva> [count]
  python -I plugins/cartographer/tools/dump_dis.py <image> --stamp 0x6ab3782c pat "48 89 5C 24 08 ?? 57"

<image> is the running game image written in memory layout (section file offset == RVA), as made by d2rAI-toolkit's
tools/dump_image.py. --stamp is the PE TimeDateStamp the recording was made against; the script refuses to run on a
different image. `dis` prints `count` instructions (default 40) from the RVA. `pat` lists every match of a hex
signature (?? = any byte) in the executable sections, as check_maze_signatures.py does. Needs the capstone package
for `dis`. Exit status 0 on success; 1 when `pat` finds nothing, `dis` gets an RVA outside the image, or the image,
stamp or a hex token is bad.
"""
import argparse
import os
import re
import struct
import sys

IMAGE_SCN_MEM_EXECUTE = 0x20000000


def load(path, stamp):
    try:
        with open(path, "rb") as f:
            data = f.read()
    except OSError as e:
        sys.exit("cannot read %s: %s" % (path, e.strerror or e))
    if len(data) < 0x40 or data[:2] != b"MZ":
        sys.exit("%s is not a PE image (no MZ header)" % path)
    pe = struct.unpack_from("<I", data, 0x3C)[0]
    if pe + 24 > len(data) or data[pe:pe + 4] != b"PE\0\0":
        sys.exit("%s is not a PE image (no PE signature)" % path)
    nsec, = struct.unpack_from("<H", data, pe + 6)
    got, = struct.unpack_from("<I", data, pe + 8)
    if got != stamp:
        sys.exit("stamp mismatch: image has 0x%08x, expected 0x%08x" % (got, stamp))
    opt, = struct.unpack_from("<H", data, pe + 20)
    secs, off = [], pe + 24 + opt
    if off + nsec * 40 > len(data):
        sys.exit("%s is truncated (section table runs past the end)" % path)
    for _ in range(nsec):
        name = data[off:off + 8].rstrip(b"\0").decode("latin-1")
        vsize, va, _, _, _, _, _, _, chars = struct.unpack_from("<IIIIIIHHI", data, off + 8)
        secs.append((name, va, vsize, chars))
        off += 40
    return data, secs


def parse(sig):
    toks = sig.split()
    if not toks:
        sys.exit("empty signature")
    parts = []
    for t in toks:
        if t == "??":
            parts.append(b".")
        else:
            try:
                if len(t) != 2:
                    raise ValueError
                parts.append(re.escape(bytes([int(t, 16)])))
            except ValueError:
                sys.exit("bad hex token %r in signature (use two hex digits or ??)" % t)
    return re.compile(b"".join(parts), re.S)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("image")
    ap.add_argument("--stamp", required=True, type=lambda s: int(s, 0))
    sub = ap.add_subparsers(dest="cmd", required=True)
    d = sub.add_parser("dis")
    d.add_argument("rva", type=lambda s: int(s, 0))
    d.add_argument("count", nargs="?", type=int, default=40)
    p = sub.add_parser("pat")
    p.add_argument("signature")
    a = ap.parse_args()
    if a.cmd == "dis":
        try:
            import capstone
        except ImportError:
            sys.exit("dis needs capstone (pip install capstone)")
    else:
        rx = parse(a.signature)
    data, secs = load(a.image, a.stamp)
    if a.cmd == "dis":
        if not 0 <= a.rva < len(data):
            sys.exit("RVA 0x%x is outside the image (0x%x bytes)" % (a.rva, len(data)))
        md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
        for n, ins in enumerate(md.disasm(data[a.rva:a.rva + a.count * 15], a.rva)):
            if n >= a.count:
                break
            print("0x%x  %s %s" % (ins.address, ins.mnemonic, ins.op_str))
        return 0
    hits = 0
    for name, va, vsize, chars in secs:
        if chars & IMAGE_SCN_MEM_EXECUTE:
            for m in re.finditer(rx, data[va:va + vsize]):
                print("%s 0x%x" % (name, va + m.start()))
                hits += 1
    if not hits:
        print("no match in the executable sections")
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
