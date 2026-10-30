# Changelog

## Unreleased

- The plugin is global: `ModScopedOnly` is removed, so it loads with or without a mod. Spec files are still stored next
  to the character's save, in the save folder named by the active mod's `modinfo.json` `savepath` (the mod D2RLoader
  runs, e.g. `default_mod`), and fall back to the plugin's own folder when there is none. With no mod selected the
  save folder is the retail one, `Saved Games\Diablo II Resurrected`.
