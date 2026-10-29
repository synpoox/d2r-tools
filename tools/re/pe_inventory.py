#!/usr/bin/env python3
"""PE inventory of a memory-layout D2R image dump: header, sections, layout. Usage: pe_inventory.py <dump.exe>
Reads the dump from outside the repo; prints metadata only."""
import pefile, sys, hashlib, datetime
p = sys.argv[1]
pe = pefile.PE(p, fast_load=True)
fh, oh = pe.FILE_HEADER, pe.OPTIONAL_HEADER
print("size", len(pe.__data__))
print("machine 0x%x  stamp 0x%08x (%s UTC)" % (fh.Machine, fh.TimeDateStamp, datetime.datetime.fromtimestamp(fh.TimeDateStamp, datetime.timezone.utc)))
print("ImageBase 0x%x SizeOfImage 0x%x Entry 0x%x Subsys %d" % (oh.ImageBase, oh.SizeOfImage, oh.AddressOfEntryPoint, oh.Subsystem))
print("%-10s %10s %10s %10s %10s %s %s" % ("name","VA","VSize","RawPtr","RawSize","chars","nonzero%"))
for s in pe.sections:
    d = s.get_data()
    nz = sum(1 for b in d[::4096] if b) if d else 0
    nzfrac = (len(d) - d.count(0)) / max(len(d),1) * 100
    print("%-10s 0x%08x 0x%08x 0x%08x 0x%08x 0x%08x %.1f" % (s.Name.rstrip(b'\0').decode(errors='replace'), s.VirtualAddress, s.Misc_VirtualSize, s.PointerToRawData, s.SizeOfRawData, s.Characteristics, nzfrac))
print("layout:", "memory (rawptr==VA)" if all(s.PointerToRawData==s.VirtualAddress for s in pe.sections) else "file")
for i,dd in enumerate(oh.DATA_DIRECTORY):
    if dd.VirtualAddress: print("dir", dd.name, hex(dd.VirtualAddress), hex(dd.Size))
