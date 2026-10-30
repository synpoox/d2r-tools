# Finding RVAs and interception points

The repeatable way to find a D2R function, prove its contract and turn it into a guarded hook. Tools are
interchangeable (Ghidra through d2rAI-toolkit's `tools/ghidra_query.ps1`, or Capstone over `.pdata`); the evidence
they leave must be the same.

## What may be committed

Derived facts only: RVAs, short byte witnesses, structure offsets, call relationships, image stamps, hashes and
validation notes. Never: `D2R.exe`, `D2RCore.dll`, memory dumps, extracted assets, process addresses without the module
base, personal paths, saves, logs or credentials. Keep dumps outside the repository.

## Address vocabulary

- **Process address:** valid for one process only (ASLR). Never a durable identifier.
- **Module base:** where `D2R.exe` or `D2RCore.dll` is loaded in that process.
- **RVA:** process address minus module base. Source and docs record this.
- **File offset:** position in an on-disk PE. Not an RVA; the packed `D2R.exe` on disk is not the running image.
- **Interception point:** the reviewed function entry, call site or instruction a hook or patch uses.

## Workflow

1. **Freeze the build identity.** Record the image stamp (`context->exeBase` PE `TimeDateStamp`), the game version
   label and the D2RLoader version. Dump the running image in memory layout (d2rAI-toolkit `tools/dump_image.py`) so
   file offset = RVA.
2. **Start from a semantic anchor**, not a guessed address: an assert path string (`D2Client\src\UI\npcmenu.cpp`), a
   command id (`CCMD`/`SCMD` name switches), a known caller in the toolkit's address registry, an SDK service, or a
   structure field with a visible effect.
3. **Walk callers and callees** until the function that owns the behaviour is found. Prefer a function with one
   job and few callers. Write down every caller and whether it uses the result.
4. **Prove the contract**: argument registers, return value use, which thread calls it (game, UI, network), and what
   it mutates. Read the getters to confirm structure offsets rather than trusting one access.
5. **Choose the guard**: at least 14 bytes of whole instructions at the hook entry, with no RIP-relative instruction
   in them; or an interior witness when the entry belongs to another hook. Check for an existing `FF 25` detour and
   for a D2RLoader service that already exposes the behaviour.
6. **Write a dump checker** in the plugin's `tools/` (see `plugins/quick-npc/tools/verify_dump.py`) that re-checks
   every RVA, byte pattern and layout. Re-run it after any game or D2RLoader update.
7. **Hook through `MutationService`** in one transaction, so all hooks go in or none. Fail open: on a stamp or byte
   mismatch, log which part is off and change nothing.
8. **Record it** in a feature document here using the [recording format](README.md#recording-format), and add the
   useful entries to the [registry table](README.md#known-high-value-entries-image-0x6ab3782c).
9. **Validate at each level and say which**: unit tests, dump verification, a clean build, an install, live in-game
   testing. Do not mark a contract as validated in game unless it was.

## After a game or D2RLoader update

Run every plugin's dump checker against a new dump. For each failure, re-find the address from its anchor (step 2),
update the source constant, the feature document and this registry together, and add a new compatibility profile
row rather than editing the old one.
