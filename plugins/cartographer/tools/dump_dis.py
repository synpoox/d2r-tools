"""Read-only disassembly and signature search over a D2R memory-layout image (static; ships no game bytes).

Usage:
  python -I plugins/cartographer/tools/dump_dis.py <image> --stamp 0x6ab3782c dis <rva> [count]
  python -I plugins/cartographer/tools/dump_dis.py <image> --stamp 0x6ab3782c pat "48 89 5C 24 08 ?? 57"

<image> is the running game image written in memory layout (section file offset == RVA), as made by d2rAI-toolkit's
tools/dump_image.py. --stamp is the PE TimeDateStamp the recording was made against; the script refuses to run on a
different image. `dis` prints `count` instructions (default 40) from the RVA. `pat` lists every match of a hex
signature (?? = any byte) in the executable sections, as check_maze_signatures.py does. Needs the capstone package
for `dis`.
"""
import argparse
import re
import struct
import sys

EXEC = 0x20000000


def load(path, stamp):
    data = open(path, "rb").read()
    pe = struct.unpack_from("<I", data, 0x3C)[0]
    nsec, = struct.unpack_from("<H", data, pe + 6)
    got, = struct.unpack_from("<I", data, pe + 8)
    if got != stamp:
        sys.exit("stamp mismatch: image has 0x%08x, expected 0x%08x" % (got, stamp))
    opt, = struct.unpack_from("<H", data, pe + 20)
    secs, off = [], pe + 24 + opt
    for _ in range(nsec):
        name = data[off:off + 8].rstrip(b"\0").decode("latin-1")
        vsize, va, _, _, _, _, _, _, chars = struct.unpack_from("<IIIIIIHHI", data, off + 8)
        secs.append((name, va, vsize, chars))
        off += 40
    return data, secs


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
    data, secs = load(a.image, a.stamp)
    if a.cmd == "dis":
        import capstone
        md = capstone.Cs(capstone.CS_ARCH_X86, capstone.CS_MODE_64)
        for n, ins in enumerate(md.disasm(data[a.rva:a.rva + a.count * 15], a.rva)):
            if n >= a.count:
                break
            print("0x%x  %s %s" % (ins.address, ins.mnemonic, ins.op_str))
    else:
        rx = b"".join(b"." if t == "??" else re.escape(bytes([int(t, 16)])) for t in a.signature.split())
        for name, va, vsize, chars in secs:
            if chars & EXEC:
                for m in re.finditer(rx, data[va:va + vsize], re.S):
                    print("%s 0x%x" % (name, va + m.start()))


main()
