# Armory (experimental, not installable)

Armory will save named equipment loadouts and switch between them in town.

This branch contains a portable transaction planner and model tests only. It does not produce a plugin DLL, touch game items or enable equipment switching. Do not merge or release until the native transaction gate in [the investigation](../../docs/reverse-engineering/armory.md) is satisfied and the normal plugin build is wired up.

Run the model tests:

    cmake -S plugins/armory/tests -B build/armory-model
    cmake --build build/armory-model --config Release
    ctest --test-dir build/armory-model -C Release --output-on-failure

The model treats an absent equipment slot as unchanged, an explicit empty slot as unequip, and a fingerprint as a required item. It refuses missing/ambiguous matches, unsafe contexts and stale revisions. Outgoing equipment is packed into supported storage grids after incoming cells are released in the hypothetical final layout. This does not prove the engine can execute that layout atomically.
