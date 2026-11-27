# Armory (experimental diagnostics)

Armory will save named equipment loadouts and switch between them in town. This branch is not a player-ready release. It contains a portable loadout manager, transaction planner, sidecar store, signature resolver, and a read-only diagnostic DLL target. Equipment switching and in-game loadout persistence are not enabled.

## Build and test

Windows x64, Visual Studio 2022, CMake 3.29+, and the repository's pinned PluginSDK:

    powershell -ExecutionPolicy Bypass -File tools\build.ps1 -Plugin armory -Test

The Windows DLL target has not yet been built or installed. Use only a disposable game slot for diagnostics; see [TESTING.md](TESTING.md).

The portable tests also build without the SDK:

    cmake -S plugins/armory/tests -B build/armory-model
    cmake --build build/armory-model --config Release
    ctest --test-dir build/armory-model -C Release --output-on-failure

The model treats an absent equipment slot as unchanged, an explicit empty slot as unequip, and a fingerprint as a required item. It rejects missing/ambiguous matches, unsafe contexts and stale revisions. Outgoing equipment can reuse incoming items' vacated storage cells in the hypothetical final layout.

Named loadouts support capture, add/rename/update/delete, cycle, multiple active matches, and a bounded character-scoped sidecar. The store rejects non-.armory paths, checks a checksum, and holds an OS writer lock through compare-and-replace. The caller must supply an isolated Armory path, verified scope and synchronize the in-memory character state. Fingerprints and the file format remain experimental.

See the [native transaction investigation](../../docs/reverse-engineering/armory.md) for the release gate. No native equipment function is invoked by the DLL target, even when all signatures resolve.

Developer continuation: [implementation handoff](HANDOFF.md).
