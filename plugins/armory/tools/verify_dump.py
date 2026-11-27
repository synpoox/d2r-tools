"""Verify the Armory research signatures in an unpacked memory-layout PE.

Only derived facts are printed. Never check the dump into the repository.
"""
import hashlib
import pathlib
import re
import struct
import sys


def main(path):
    image = pathlib.Path(path).read_bytes()
    pe, = struct.unpack_from('<I', image, 0x3c)
    if image[:2] != b'MZ' or image[pe:pe + 4] != b'PE\0\0':
        raise ValueError('not a PE image')
    machine, count, stamp = struct.unpack_from('<HHI', image, pe + 4)
    optional_size, = struct.unpack_from('<H', image, pe + 20)
    opt = pe + 24
    if machine != 0x8664 or struct.unpack_from('<H', image, opt)[0] != 0x20b:
        raise ValueError('not an x64 PE32+ image')
    table = opt + optional_size
    text = None
    for n in range(count):
        s = table + n * 40
        size, rva, raw_size, raw_offset = struct.unpack_from('<IIII', image, s + 8)
        flags, = struct.unpack_from('<I', image, s + 36)
        if image[s:s + 8].rstrip(b'\0') == b'.text':
            if not flags & 0x20000000 or raw_offset != rva or raw_size != size or rva + size > len(image):
                raise ValueError('expected executable memory-layout .text')
            text = image[rva:rva + size]
            break
    if text is None:
        raise ValueError('missing .text')
    # Exception directory entries corroborate starts of native x64 functions.
    pdata, pdata_size = struct.unpack_from('<II', image, opt + 112 + 3 * 8)
    if pdata + pdata_size > len(image) or pdata_size % 12:
        raise ValueError('invalid exception directory')
    starts = {a for a, _, _ in struct.iter_unpack('<III', image[pdata:pdata + pdata_size])}
    header = pathlib.Path(__file__).resolve().parents[1] / 'src/native_signatures.h'
    patterns = re.findall(r'Signature\{"([^"]+)", "([0-9A-F? ]+)"\}', header.read_text())
    if len(patterns) != 7:
        raise ValueError('expected seven research signatures')
    print(f'PE stamp={stamp:#010x} SHA256={hashlib.sha256(image).hexdigest()}')
    ok = True
    for name, sig in patterns:
        pattern = b''.join(b'.' if token == '??' else re.escape(bytes.fromhex(token)) for token in sig.split())
        hits = [rva + m.start() for m in re.finditer(b'(?=' + pattern + b')', text, re.S)]
        valid = len(hits) == 1 and hits[0] in starts
        print(f'{name}: {len(hits)} matches, {[hex(h) for h in hits[:8]]}, function entry={valid}')
        ok &= valid
    return 0 if ok else 1


if __name__ == '__main__':
    if len(sys.argv) != 2:
        sys.exit('usage: verify_dump.py unpacked-memory-image.exe')
    sys.exit(main(sys.argv[1]))
