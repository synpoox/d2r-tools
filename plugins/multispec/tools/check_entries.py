"""Checks src/d2r.cpp's entry-byte table against a memory-layout dump of D2R.exe (file offset == RVA).

    python tools/check_entries.py <dump.exe>

Use it after re-dumping a new game or D2RLoader build: re-find the address of any row that does not say "ok". Prints the
dump's PE TimeDateStamp too. Standard library only; reads the dump, writes nothing.
"""
import re
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def main() -> int:
    if len(sys.argv) != 2:
        print(__doc__)
        return 2
    dump = Path(sys.argv[1]).read_bytes()
    pe = struct.unpack_from("<I", dump, 0x3C)[0]
    print(f"dump image stamp 0x{struct.unpack_from('<I', dump, pe + 8)[0]:08x}")

    header = (ROOT / "src" / "d2r.h").read_text()
    rvas = {m[1]: int(m[2], 16) for m in re.finditer(r"constexpr uint64_t (Rva\w+) = (0x[0-9a-fA-F]+);", header)}
    source = (ROOT / "src" / "d2r.cpp").read_text()
    rows = re.findall(r"\{(Rva\w+), (\d+), \{([^}]*)\}, (\d+)\}", source)
    bad = 0
    for name, at, data, size in rows:
        want = bytes(int(b, 16) for b in data.split(","))
        rva = rvas[name] + int(at)
        got = dump[rva:rva + len(want)]
        ok = got == want and len(want) == int(size)
        hooked = dump[rvas[name]:rvas[name] + 2] == b"\xff\x25"
        bad += not ok
        print(f"{'ok ' if ok else 'BAD'} {name:<22} 0x{rvas[name]:07x}{' (D2RLoader hook)' if hooked else ''}"
              f"{'' if ok else '  got ' + got.hex(' ')}")
    print(f"{len(rows)} rows, {bad} bad")
    return 1 if bad else 0


if __name__ == "__main__":
    sys.exit(main())
