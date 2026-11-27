# Armory diagnostics and transaction proof

Use a disposable test save in a dedicated D2RLoader game slot, never a player save. No game image, extracted assets, saves, or logs belong in Git.

## Portable checks

Run the repository build/test command or standalone tests in README.md. Test the private dump independently:

    python plugins/armory/tools/verify_dump.py <unpacked-memory-image.exe>
    armory_image_tests.exe <unpacked-memory-image.exe>

These validate the resolver against the file, not runtime memory or native calls. Image tests exercise relocation and signature rejection. The probe encoder deliberately permits only a two-item inventory/non-hand equipment exchange with zero flag and distinct runtime IDs. It does not execute it.

## Read-only game-slot check (not yet performed)

1. Build with the pinned SDK and normal repository build script. Install the diagnostic DLL into the disposable slot with game closed.
2. Start offline; run the local console command armory. Inspect the seven entry-resolution results and image stamp in the plugin log. A missing or ambiguous entry must remain off; no version gate and no fixed-address fallback.
3. Enter town with one equipped ring and items in inventory. Run armory-probe. Compare enumeration and direct equipment-slot observations with the native UI. The log reports counts and uncertain slots, not a guarantee of accessibility or completeness.
4. Repeat with empty slots, both weapon sets, two-handed gear, cursor occupied, personal stash open and closed, each available shared-stash tab, character reload and session transition. Verify no equipment, items, stats, or save changes from the probe. Do not treat absent stash counts as proof of missing items.
5. Repeat on a TCP/IP host and a remote client. The remote client has no authoritative game-thread callback, so the diagnostic command must refuse. No multiplayer mutation or messaging is implemented yet.

## Native write gate (not enabled)

After static verification of ownership, thread and activation/rollback behavior, use a disposable ring/armor exchange with two distinct identities and an otherwise full inventory. Observe before/after snapshots, item IDs, cursor, native stats, skills, flags, save boundaries and client synchronization. Test invalid requirements and forced failure at every native boundary, then repeat for multi-slot and weapon-swap cases. Never enable a requirement-bypass flag, recreate a failed item, or attempt an unproven compensating swap.

A successful model test, dump signature match, or read-only game probe is not proof of native atomicity.
