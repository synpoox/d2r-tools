"""Check the plugin's actual signatures, call targets and whole entry instructions.

Usage: python verify_dump.py <memory-layout PE dump>. Requires capstone.
No game process is opened or changed.
"""
import hashlib
from pathlib import Path
import re
import struct
import sys
from capstone import Cs, CS_ARCH_X86, CS_MODE_64


def verify(path):
    data = Path(path).read_bytes()
    pe = struct.unpack_from('<I', data, 0x3c)[0]
    assert data[:2] == b'MZ' and data[pe:pe + 4] == b'PE\0\0'
    count, stamp = struct.unpack_from('<HI', data, pe + 6)
    optional_size = struct.unpack_from('<H', data, pe + 20)[0]
    sections = []
    for i in range(count):
        pos = pe + 24 + optional_size + i * 40
        size, rva = struct.unpack_from('<II', data, pos + 8)
        flags = struct.unpack_from('<I', data, pos + 36)[0]
        if flags & 0x20000000:
            sections.append((rva, data[rva:rva + size]))
    source = (Path(__file__).parent.parent / 'src/signatures.h').read_text()
    entries = re.findall(r'Signature (\w+) \{"[^"]+", "([^"]+)", (\d+), (\d+)\}', source)
    assert len(entries) == 8
    witnesses = re.findall(r'const char\* (\w+Sig) = "([^"]+)"', source)
    witness_rvas = {'GetterSig': 0x2f5020, 'SetterSig': 0x2f7d10, 'AdderSig': 0x2f34f0,
                    'ClientSig': 0x14cc8c0, 'FindPanelSig': 0x846170, 'FindChildSig': 0x856220, 'SetTextSig': 0x86dd60,
                    'NameLayoutSig': 0x39dccc, 'HasStateSig': 0x3351b0}
    for name, pattern in witnesses:
        regex = b''.join(b'.' if x == '??' else re.escape(bytes([int(x, 16)])) for x in pattern.split())
        matches = [rva + m.start() for rva, section in sections for m in re.finditer(regex, section, re.DOTALL)]
        assert matches == [witness_rvas[name]], (name, matches)
        print(f'{name}: unique 0x{matches[0]:x}')
    expected = {'LifeDebit': (0x44d093, 0x2f7d10), 'ManaDebit': (0x44d0c9, 0x2f7d10),
                'LifeRegen': (0x42e6d1, 0x2f7d10), 'ManaRegen': (0x42e540, 0x2f34f0)}
    md = Cs(CS_ARCH_X86, CS_MODE_64)
    for name, pattern, call_offset, entry_size in entries:
        regex = b''.join(b'.' if x == '??' else re.escape(bytes([int(x, 16)])) for x in pattern.split())
        matches = [rva + m.start() for rva, section in sections for m in re.finditer(regex, section, re.DOTALL)]
        assert len(matches) == 1, (name, matches)
        address = matches[0]
        if int(entry_size):
            instructions = list(md.disasm(data[address:address + int(entry_size)], address))
            assert sum(i.size for i in instructions) == int(entry_size), name
            assert all('rip' not in i.op_str and not i.mnemonic.startswith(('call', 'j')) for i in instructions), name
        else:
            call = address + int(call_offset)
            assert data[call] == 0xe8
            target = call + 5 + struct.unpack_from('<i', data, call + 1)[0]
            assert (call, target) == expected[name], (name, hex(call), hex(target))
            assert data[target:target + 2] == b'\xff\x25'
        print(f'{name}: unique 0x{address:x}, whole instructions/target verified')
    print(f'PASS stamp={stamp:08x} size={len(data)} sha256={hashlib.sha256(data).hexdigest()}')


if __name__ == '__main__':
    verify(sys.argv[1])
