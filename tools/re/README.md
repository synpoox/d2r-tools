# RE helper scripts (pefile + capstone)

Ghidra could not be downloaded in the analysis sandbox (see docs/re/dump-inventory.md), so the reverse engineering
uses these small scripts. They read a **memory-layout** image dump (file offset = RVA, as written by the toolkit's
`tools/dump_image.py`) from a path you pass; the dump stays outside this repository. They print addresses, bytes and
disassembly lines to the terminal; do not commit their bulk output.

    pip install pefile capstone
    python3 -I tools/re/pe_inventory.py ~/d2r-dump/D2RLoader_6ab3782c_dump.exe
    python3 -I tools/re/registry_entry_check.py ~/d2r-dump/D2RLoader_6ab3782c_dump.exe <toolkit>/knowledge/d2r-93847-addresses.md
