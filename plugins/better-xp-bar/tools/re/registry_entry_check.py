#!/usr/bin/env python3
"""Compare the toolkit registry's "Entry bytes summary" with a memory-layout dump (file offset = RVA).
Usage: registry_entry_check.py <dump.exe> <d2r-93847-addresses.md>"""
import sys, re
data = open(sys.argv[1],'rb').read()
reg = open(sys.argv[2]).read()
sec = reg.split("## Entry bytes summary")[1].split("Table slots")[0]
ok=bad=0
for rva, bts in re.findall(r"\| `0x([0-9a-f]+)` \| `([0-9A-F ]+)`", sec):
    rva=int(rva,16); exp=bytes.fromhex(bts)
    got=data[rva:rva+len(exp)]
    m = got==exp; ok+=m; bad+= not m
    print("0x%06x %s %s" % (rva, "OK  " if m else "DIFF", got.hex(' ')))
print(ok,"match",bad,"differ")
