#!/usr/bin/env python3
"""Small static RE helper for a memory-layout D2R image dump (file offset = RVA), using numpy + capstone.

A stand-in for the toolkit's Ghidra query scripts when Ghidra is not available. Read-only. Prints RVAs, bytes and
short disassembly to the terminal; do not commit bulk output.

    d2rre.py <dump> strings <text>              where a string (ASCII or UTF-16) is stored
    d2rre.py <dump> xref <rva>                  instructions addressing <rva> RIP-relatively (lea/mov/cmp ...)
    d2rre.py <dump> strxref <text>              xref every copy of a string; prints the containing functions
    d2rre.py <dump> calls <rva>                 E8 rel32 calls and E9 jumps to <rva>
    d2rre.py <dump> func <rva>                  the .pdata function containing <rva>
    d2rre.py <dump> dis <rva> [count]           disassemble from <rva> (default: to the function end, max 400)
    d2rre.py <dump> disf <rva>                  disassemble the whole function containing <rva>
    d2rre.py <dump> bytes <rva> [n]             raw bytes
    d2rre.py <dump> qword <rva> [n]             n 8-byte values (pointer tables, vtables) as RVAs when in image
"""
from __future__ import annotations

import bisect
import struct
import sys

import numpy as np
from capstone import CS_ARCH_X86, CS_MODE_64, Cs
from capstone.x86 import X86_OP_MEM, X86_REG_RIP

IMAGE_BASE = 0x140000000


class Image:
    def __init__(self, path: str):
        self.data = open(path, "rb").read()
        e = struct.unpack_from("<I", self.data, 0x3C)[0]
        nsec = struct.unpack_from("<H", self.data, e + 6)[0]
        optsz = struct.unpack_from("<H", self.data, e + 20)[0]
        sec = e + 24 + optsz
        self.sections = []
        for i in range(nsec):
            o = sec + 40 * i
            name = self.data[o:o + 8].rstrip(b"\0").decode(errors="replace")
            vsize, va = struct.unpack_from("<II", self.data, o + 8)
            chars = struct.unpack_from("<I", self.data, o + 36)[0]
            self.sections.append((name, va, vsize, chars))
        opt = e + 24
        # data directory 3 = exception (.pdata)
        exc_rva, exc_size = struct.unpack_from("<II", self.data, opt + 112 + 3 * 8)
        n = exc_size // 12
        arr = np.frombuffer(self.data, dtype="<u4", count=n * 3, offset=exc_rva).reshape(n, 3)
        order = np.argsort(arr[:, 0])
        self.fn_begin = arr[order, 0].astype(np.int64)
        self.fn_end = arr[order, 1].astype(np.int64)
        self.md = Cs(CS_ARCH_X86, CS_MODE_64)
        self.md.detail = True
        self._i32 = None

    def section(self, name):
        for s in self.sections:
            if s[0] == name:
                return s
        raise KeyError(name)

    def code_range(self):
        _, va, size, _ = self.section(".text")
        return va, va + size

    def func(self, rva: int):
        i = int(np.searchsorted(self.fn_begin, rva, side="right")) - 1
        while i >= 0:
            b, e = int(self.fn_begin[i]), int(self.fn_end[i])
            if b <= rva < e:
                return b, e
            if e <= rva and rva - b > 0x100000:
                break
            i -= 1
            # chained entries can split a function; only accept a direct hit
            break
        return None

    def i32_all(self):
        if self._i32 is None:
            lo, hi = self.code_range()
            # int32 value starting at every byte position p in [lo, hi)
            seg = np.frombuffer(self.data, dtype=np.uint8, count=hi - lo + 3, offset=lo)
            b = seg.astype(np.int64)
            v = b[:-3] | (b[1:-2] << 8) | (b[2:-1] << 16) | (b[3:] << 24)
            v = np.where(v >= 2**31, v - 2**32, v)
            self._i32 = (lo, v)
        return self._i32

    def rip_candidates(self, target: int, tail_max: int = 5):
        """Positions of disp32 fields with p + 4 + k + disp == target for k in 0..tail_max (immediate after disp)."""
        lo, v = self.i32_all()
        pos = np.arange(lo, lo + len(v), dtype=np.int64)
        hits = set()
        for k in range(tail_max + 1):
            for p in np.nonzero(pos + 4 + k + v == target)[0]:
                hits.add(int(p) + lo)
        return sorted(hits)

    def insn_at_containing(self, field_pos: int):
        """Find an instruction (start 2..10 bytes before field_pos) whose RIP-relative memory operand covers field_pos."""
        for back in range(1, 11):
            start = field_pos - back
            for ins in self.md.disasm(self.data[start:start + 16], start, 1):
                if ins.address + ins.size < field_pos + 4:
                    continue
                for op in ins.operands:
                    if op.type == X86_OP_MEM and op.mem.base == X86_REG_RIP:
                        return ins
        return None

    def xref(self, target: int):
        out = []
        for p in self.rip_candidates(target):
            ins = self.insn_at_containing(p)
            if ins is None:
                continue
            for op in ins.operands:
                if op.type == X86_OP_MEM and op.mem.base == X86_REG_RIP and ins.address + ins.size + op.mem.disp == target:
                    out.append(ins)
                    break
        uniq = {i.address: i for i in out}
        return [uniq[a] for a in sorted(uniq)]

    def calls(self, target: int):
        lo, v = self.i32_all()
        pos = np.arange(lo, lo + len(v), dtype=np.int64)
        res = []
        for p in np.nonzero(pos + 4 + v == target)[0]:
            p = int(p) + lo
            op = self.data[p - 1]
            if op in (0xE8, 0xE9):
                res.append((p - 1, "call" if op == 0xE8 else "jmp"))
        return res

    def dis(self, rva: int, count: int = 400, end: int | None = None):
        out = []
        limit = end if end is not None else rva + 0x4000
        for ins in self.md.disasm(self.data[rva:limit], rva):
            out.append(ins)
            if len(out) >= count:
                break
        return out


def fmt(ins, img: Image | None = None):
    extra = ""
    for op in ins.operands:
        if op.type == X86_OP_MEM and op.mem.base == X86_REG_RIP:
            t = ins.address + ins.size + op.mem.disp
            extra = f"   ; -> 0x{t:x}"
            if img is not None:
                s = img.data[t:t + 48].split(b"\0")[0]
                if len(s) >= 4 and all(32 <= c < 127 for c in s):
                    extra += f' "{s.decode()}"'
    return f"0x{ins.address:07x}: {ins.bytes.hex(' '):<32} {ins.mnemonic} {ins.op_str}{extra}"


def find_strings(img: Image, text: str):
    res = []
    for enc, label in ((text.encode(), "ascii"), (text.encode("utf-16-le"), "utf16")):
        start = 0
        while True:
            i = img.data.find(enc, start)
            if i < 0:
                break
            res.append((i, label))
            start = i + 1
    return res


def string_start(img: Image, at: int) -> int:
    s = at
    while s > 0 and 32 <= img.data[s - 1] < 127:
        s -= 1
    return s


def main(argv):
    if len(argv) < 3:
        print(__doc__)
        return 2
    img = Image(argv[1])
    cmd = argv[2]
    args = argv[3:]
    num = lambda s: int(s, 0)
    if cmd == "strings":
        for at, enc in find_strings(img, args[0]):
            st = string_start(img, at) if enc == "ascii" else at
            full = img.data[st:st + 120].split(b"\0")[0]
            print(f"0x{at:x} ({enc}) string starts 0x{st:x}: {full[:100]!r}")
    elif cmd == "xref":
        for ins in img.xref(num(args[0])):
            f = img.func(ins.address)
            print(fmt(ins, img), f"  [fn 0x{f[0]:x}]" if f else "")
    elif cmd == "strxref":
        for at, enc in find_strings(img, args[0]):
            st = string_start(img, at) if enc == "ascii" else at
            for ins in img.xref(st):
                f = img.func(ins.address)
                print(f"str 0x{st:x}: " + fmt(ins, img), f"  [fn 0x{f[0]:x}-0x{f[1]:x}]" if f else "")
    elif cmd == "calls":
        for p, kind in img.calls(num(args[0])):
            f = img.func(p)
            print(f"0x{p:x} {kind}", f"[fn 0x{f[0]:x}]" if f else "")
    elif cmd == "func":
        f = img.func(num(args[0]))
        print("none" if f is None else f"0x{f[0]:x}-0x{f[1]:x} ({f[1] - f[0]} bytes)")
    elif cmd == "dis":
        rva = num(args[0])
        count = num(args[1]) if len(args) > 1 else 400
        f = img.func(rva)
        for ins in img.dis(rva, count, f[1] if f and len(args) < 2 else None):
            print(fmt(ins, img))
    elif cmd == "disf":
        f = img.func(num(args[0]))
        if f is None:
            print("no .pdata function")
            return 1
        print(f"; function 0x{f[0]:x}-0x{f[1]:x}")
        for ins in img.dis(f[0], 5000, f[1]):
            print(fmt(ins, img))
    elif cmd == "bytes":
        rva = num(args[0])
        n = num(args[1]) if len(args) > 1 else 32
        print(img.data[rva:rva + n].hex(" "))
    elif cmd == "qword":
        rva = num(args[0])
        n = num(args[1]) if len(args) > 1 else 8
        for i in range(n):
            q = struct.unpack_from("<Q", img.data, rva + 8 * i)[0]
            r = q - IMAGE_BASE
            tag = f" = rva 0x{r:x}" if 0 <= r < len(img.data) else ""
            print(f"0x{rva + 8 * i:x}: 0x{q:016x}{tag}")
    else:
        print(__doc__)
        return 2
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
