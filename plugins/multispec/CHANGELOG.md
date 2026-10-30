# Changelog

## Unreleased

- No version gate: the plugin attempts to load, hook and run on any D2R or D2RLoader version. It declares PluginSDK ABI 4
  and reads optional context through the SDK getters. On game code that has moved it can crash; hooks whose entry bytes
  differ are still refused.
- Any crash while the plugin is loaded is written to the plugin's log with the fault, its cause and whether the
  plugin was involved.

