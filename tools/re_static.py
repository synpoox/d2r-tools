"""Static RE helper over a D2RLoader memory-layout dump (file offset == RVA), for machines without a Ghidra project.

Usage: python -I tools/re_static.py <dump.exe> <command> <args>   (needs: pip install capstone numpy)
Commands mirror the toolkit's Ghidra scripts:
  func <rva>              function bounds from .pdata
  dis <rva> [n]           disassemble n instructions (default: to function end)
  calls <rva>             direct call/jmp sites targeting rva (E8/E9 rel32), with containing function
  refs <rva>              RIP-relative references to rva (lea/mov/cmp...), verified by disassembly
  str <text>              find ASCII/UTF-16 strings and their RIP-relative referrers
  callees <rva>           direct calls made by the function at rva
  disp <hex>              instructions using [reg + disp] (FindDisp)
The call index is cached next to the dump (callidx.npz).
"""
import struct, sys, bisect
import numpy as np
from capstone import Cs, CS_ARCH_X86, CS_MODE_64

PATH = sys.argv[1] if __name__ == '__main__' and len(sys.argv) > 1 else None
B = open(PATH, 'rb').read() if PATH else b''
BASE = 0x140000000
TEXT = (0x1000, 0x1000 + 0x1caf000)
md = Cs(CS_ARCH_X86, CS_MODE_64)
md.detail = False

# .pdata RUNTIME_FUNCTION {begin, end, unwind}
_P0, _PN = 0x3bff000, 0x1ba000
_pd = np.frombuffer(B, dtype='<u4', count=(_PN // 12) * 3, offset=_P0).reshape(-1, 3)
_pd = _pd[_pd[:, 0] != 0]
_pd = _pd[np.argsort(_pd[:, 0])]
BEG = _pd[:, 0].astype(np.int64)
END = _pd[:, 1].astype(np.int64)
UNW = _pd[:, 2].astype(np.int64)


def chained_root(i):
    """Follow UNW_FLAG_CHAININFO to the primary function entry."""
    seen = 0
    while seen < 16:
        u = int(UNW[i])
        if u & 1:
            u &= ~1
        if not (0 < u < len(B) - 4):
            return int(BEG[i])
        flags = B[u] >> 3
        if not flags & 4:
            return int(BEG[i])
        cnt = B[u + 2]
        off = u + 4 + ((cnt + 1) & ~1) * 2
        b, e, w = struct.unpack_from('<III', B, off)
        j = int(np.searchsorted(BEG, b))
        if j >= len(BEG) or BEG[j] != b:
            return b
        i = j
        seen += 1
    return int(BEG[i])


def func(rva):
    i = int(np.searchsorted(BEG, rva, side='right')) - 1
    if i < 0 or not (BEG[i] <= rva < END[i]):
        return None
    return int(BEG[i]), int(END[i]), chained_root(i)


def dis(rva, n=None, end=None):
    if end is None:
        f = func(rva)
        end = f[1] if f else rva + 0x200
    out = []
    for ins in md.disasm(B[rva:end], rva):
        out.append(ins)
        if n and len(out) >= n:
            break
    return out


def fmt(ins):
    return f'{ins.address:#09x}: {ins.bytes.hex(" "):<36} {ins.mnemonic} {ins.op_str}'


_text = np.frombuffer(B, dtype=np.uint8, count=TEXT[1] - TEXT[0], offset=TEXT[0])


def _rel32_hits(target, opcodes=(0xE8, 0xE9)):
    t = _text
    d = np.frombuffer(B[TEXT[0]:TEXT[1] - 3], dtype=np.uint8)
    # int32 at every position
    n = len(t) - 5
    v = (t[1:n + 1].astype(np.int64) | (t[2:n + 2].astype(np.int64) << 8) |
         (t[3:n + 3].astype(np.int64) << 16) | (t[4:n + 4].astype(np.int64) << 24))
    v = np.where(v >= 1 << 31, v - (1 << 32), v)
    pos = np.arange(n, dtype=np.int64) + TEXT[0]
    hit = (pos + 5 + v) == target
    m = np.zeros(n, dtype=bool)
    for op in opcodes:
        m |= (t[:n] == op)
    return pos[hit & m]


_DISP = None


def _disp_all():
    global _DISP
    if _DISP is None:
        t = _text
        n = len(t) - 4
        v = (t[0:n].astype(np.int64) | (t[1:n + 1].astype(np.int64) << 8) |
             (t[2:n + 2].astype(np.int64) << 16) | (t[3:n + 3].astype(np.int64) << 24))
        _DISP = np.where(v >= 1 << 31, v - (1 << 32), v)
    return _DISP


def calls(target):
    res = []
    for p in _rel32_hits(target):
        p = int(p)
        ins = next(md.disasm(B[p:p + 5], p), None)
        if ins and ins.mnemonic in ('call', 'jmp'):
            res.append((p, ins.mnemonic, func(p)))
    return res


def refs(target, also_calls=False):
    """RIP-relative references: disp32 at position i, instruction ends at i+4+k (k = imm size 0,1,2,4)."""
    v = _disp_all()
    pos = np.arange(len(v), dtype=np.int64) + TEXT[0]
    res = []
    for k in (0, 1, 2, 4):
        for i in pos[(pos + 4 + k + v) == target]:
            i = int(i)
            # try instruction starts before i
            for s in range(1, 9):
                st = i - s
                ins = next(md.disasm(B[st:st + 16], st), None)
                if ins and ins.address + ins.size == i + 4 + k and 'rip' in ins.op_str:
                    if also_calls or ins.mnemonic not in ('call', 'jmp'):
                        res.append((st, ins, func(st)))
                    break
    seen = set()
    out = []
    for r in sorted(res, key=lambda r: r[0]):
        if r[0] not in seen:
            seen.add(r[0])
            out.append(r)
    return out


def find_bytes(pat, lo=0, hi=None):
    hi = hi or len(B)
    res = []
    i = B.find(pat, lo, hi)
    while i != -1:
        res.append(i)
        i = B.find(pat, i + 1, hi)
    return res


def cstr(rva, n=200):
    e = B.find(b'\0', rva, rva + n)
    return B[rva:e].decode('latin1')


def strs(text):
    hits = []
    for enc in ('ascii', 'utf-16le'):
        pat = text.encode(enc)
        for o in find_bytes(pat, 0x1cb0000, 0x3bff000):
            # walk back to string start
            st = o
            if enc == 'ascii':
                while st > 0 and 0x20 <= B[st - 1] < 0x7f:
                    st -= 1
            hits.append((st, enc))
    return hits


def callees(rva):
    f = func(rva)
    out = []
    for ins in dis(f[0] if f else rva):
        if ins.mnemonic == 'call':
            out.append((ins.address, ins.op_str))
    return out


def finddisp(disp, lo=TEXT[0], hi=TEXT[1]):
    """Instructions using [reg + disp32] (non-RIP), found by byte pattern then verified."""
    pat = struct.pack('<i', disp)
    out = []
    for o in find_bytes(pat, lo, hi):
        for s in range(2, 8):
            st = o - s
            for ins in md.disasm(B[st:st + 15], st):
                if ins.address + ins.size >= o + 4 and ins.address <= o - 2:
                    if hex(disp) in ins.op_str and 'rip' not in ins.op_str and 'rsp' not in ins.op_str and 'rbp' not in ins.op_str:
                        out.append(ins)
                break
    seen = {}
    for i in out:
        seen[i.address] = i
    return [seen[k] for k in sorted(seen)]


_CALLIDX = None


def _build_callidx():
    global _CALLIDX
    import os, pickle
    cache = os.path.join(os.path.dirname(PATH), 'callidx.npz')
    if os.path.exists(cache):
        z = np.load(cache)
        _CALLIDX = (z['tgt'], z['pos'])
        return
    t = _text
    n = len(t) - 5
    m = (t[:n] == 0xE8) | (t[:n] == 0xE9)
    idx = np.nonzero(m)[0]
    v = (t[idx + 1].astype(np.int64) | (t[idx + 2].astype(np.int64) << 8) |
         (t[idx + 3].astype(np.int64) << 16) | (t[idx + 4].astype(np.int64) << 24))
    v = np.where(v >= 1 << 31, v - (1 << 32), v)
    pos = idx.astype(np.int64) + TEXT[0]
    tgt = pos + 5 + v
    ok = (tgt >= TEXT[0]) & (tgt < TEXT[1])
    tgt, pos = tgt[ok], pos[ok]
    o = np.argsort(tgt, kind='stable')
    _CALLIDX = (tgt[o], pos[o])
    np.savez(cache, tgt=_CALLIDX[0], pos=_CALLIDX[1])


def calls(target):
    if _CALLIDX is None:
        _build_callidx()
    tgt, pos = _CALLIDX
    a, b = np.searchsorted(tgt, target), np.searchsorted(tgt, target, side='right')
    res = []
    for p in pos[a:b]:
        p = int(p)
        ins = next(md.disasm(B[p:p + 5], p), None)
        if ins and ins.mnemonic in ('call', 'jmp') and ins.size == 5:
            res.append((p, ins.mnemonic, func(p)))
    return res


if __name__ == '__main__':
    cmd = sys.argv[2]
    a = [int(x, 16) if x.startswith('0x') else x for x in sys.argv[3:]]
    if cmd == 'func':
        f = func(a[0]); print(f and tuple(hex(x) for x in f))
    elif cmd == 'dis':
        n = a[1] if len(a) > 1 else None
        if isinstance(n, str): n = int(n)
        for ins in dis(a[0], n):
            print(fmt(ins))
    elif cmd == 'calls':
        for p, m, f in calls(a[0]):
            print(f'{m} {p:#x} in {f and hex(f[2])}')
    elif cmd == 'refs':
        for p, ins, f in refs(a[0]):
            print(f'{fmt(ins)}   ; fn {f and hex(f[2])}')
    elif cmd == 'str':
        for st, enc in strs(' '.join(sys.argv[3:])):
            s = cstr(st) if enc == 'ascii' else B[st:st + 120].decode('utf-16le', 'replace').split('\0')[0]
            print(f'{st:#x} {enc} {s!r}')
            for p, ins, f in refs(st):
                print(f'    <- {fmt(ins)}   ; fn {f and hex(f[2])}')
    elif cmd == 'disp':
        for ins in finddisp(a[0]):
            f = func(ins.address); print(f'{fmt(ins)}   ; fn {f and hex(f[2])}')
    elif cmd == 'callees':
        for p, o in callees(a[0]):
            print(f'{p:#x}: call {o}')
