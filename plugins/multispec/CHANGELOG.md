# Changelog

## Unreleased

- Spec files are found next to the character's `.d2s` by searching the Saved Games folder and its `mods` subfolders.
  The plugin no longer reads the active mod's `modinfo.json`, so it works without the mod's layout. Same-name saves in
  different slots: the newest save wins, and the note says how many matched.
- The plugin is global: `ModScopedOnly` is removed (it loads with or without a mod).
