# Changelog

## Unreleased

- MultiSpec is now a global plugin: install it in `<game>\d2rloader\plugins\`, not in a mod folder. It works with any mod, or none. A copy left in a mod's `d2rloader\plugins` replaces the global one while that mod runs.
- New spec panel beside the skill tree, opening and closing with it. A slot shows the active spec; the arrows or the mouse wheel step through your specs, in town. It uses the game's own art, so it matches the stock panels.
- No version gate: the plugin attempts to load, hook and run on any D2R or D2RLoader version. It declares PluginSDK ABI 4
  and reads optional context through the SDK getters. On game code that has moved it can crash; hooks whose entry bytes
  differ are still refused.
- A fault while the plugin is loaded is written to the plugin's log as a `FAULT` line with its cause and whether the
  plugin was involved. A later line says when the game was still running a few seconds after it, which is a hint, not proof.

