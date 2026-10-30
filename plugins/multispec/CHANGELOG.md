# Changelog

## Unreleased

- MultiSpec is now a global plugin: install it in `<game>\d2rloader\plugins\`, not in a mod folder. It works with any mod, or none. A copy left in a mod's `d2rloader\plugins` replaces the global one while that mod runs.
- New diagnosis part `uiprobe` (`msprobe` console command) for the spec panel spike; it logs nothing until switched on.
- No version gate: the plugin attempts to load, hook and run on any D2R or D2RLoader version. It declares PluginSDK ABI 4
  and reads optional context through the SDK getters. On game code that has moved it can crash; hooks whose entry bytes
  differ are still refused.
- A fault while the plugin is loaded is written to the plugin's log as a `FAULT` line with its cause and whether the
  plugin was involved. A later line says when the game was still running a few seconds after it, which is a hint, not proof.

