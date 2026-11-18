# Known RVAs reference (build 92777)

An outside list of named D2R addresses, kept as a reference. It is not a source of addresses for our plugins.

## The source

- File: `research/d2r/known-rvas.json` in [RuffDood/RuffnecKk-D2RLoader-Suite](https://github.com/RuffDood/RuffnecKk-D2RLoader-Suite/blob/main/research/d2r/known-rvas.json), MIT license. We link to it and do not copy its entries here.
- Checked on 2026-11-18.
- `schemaVersion` 1, `build` 92777, `imageBase` 0x140000000, 1252 entries.
- Entry fields: `rva`, `name`, `kind`, `confidence`, `source` and `notes`. The notes often give the function signature, the register roles and the original instruction bytes.
- Kinds: function 758, call-site 144 (plus 33 spelled `callsite`), layout-witness 137, patch-site 111, data-table 14.
- Confidence: high 1235, medium 17.

## What it does not cover

- The list is for build 92777. It is not for the builds we ship on: 93847, and 95032 where noted. None of its RVAs apply to our builds as they are.
- Do not paste an RVA from it into plugin code, and do not hook one.

## Rule for new code

New code never uses fixed RVAs. Plugins find their addresses by byte signature lookup, as [Finding RVAs and interception points](RVA-AND-INTERCEPTION-WORKFLOW.md) describes. Keep each signature to the bytes the code needs, and check it matches exactly once.

## How to use the list

1. Use its names and notes as a map. Search by name to learn what an engine function is for, what its arguments are, and which patch sites sit near it.
2. Find the same function in our build. Use Ghidra and any of these: its strings, its callers, the instruction bytes quoted in its notes, or its constants.
3. Write a byte signature from our build. Check that it matches exactly once in both 93847 and 95032.
4. To take the bytes at each RVA directly, you need a 92777 `D2R.exe`. We do not keep one. The toolkit's `d2r-data` holds 93847 and 95032 only. So this step needs you to supply that image.
5. Layout-witness entries (struct offsets) are often stable across builds. Still confirm each one in our build before you use it.

## Credit

Credit the source: RuffDood's RuffnecKk-D2RLoader-Suite, MIT license. Link to the file above when you cite an entry.
