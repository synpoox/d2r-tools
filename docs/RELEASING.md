# Releasing

Plugins are versioned and tagged on their own (`<plugin>/vX.Y.Z`). Players get **suite releases** (`release-YYYY.MM.DD.N`):
one GitHub Release with a zip per plugin, a bundle of all of them, `SHA256SUMS.txt` and notes made of each plugin's changelog section.
The root `README.md` table lists released plugins only. An unreleased plugin has no row there and is not in any suite.

## 1. Release a plugin version

1. Fold the plugin's `changelog.d/*.md` fragments (one line per PR) and any old `Unreleased` lines into a new
   `## X.Y.Z - YYYY-MM-DD` heading (the release date), deleting the fragments:
   `powershell -ExecutionPolicy Bypass -File tools\changelog-fold.ps1 -Plugin <name> -Version X.Y.Z`. The release notes
   are exactly this section, so read it and edit the lines for players.
2. Bump `MODKIT_PLUGIN_VERSION` (or the plugin's version constant) to `X.Y.Z` and update the plugin's version in the
   root `README.md` table (add a row at a plugin's first release).
3. Commit to `main`, wait for `ci` to go green, then tag and push:

   ```
   git tag <name>/vX.Y.Z
   git push origin <name>/vX.Y.Z
   ```

## 2. Publish a suite release

1. Edit `release/suite.json`: set each included plugin's `version` to its tag, and write its `validation` (what was
   actually checked: build, tests, installed, live in game) and `limits`. State what was not tested. These are our record, not shown in the notes. Update
   `game.d2r` / `game.d2rloader` if the tested versions changed. Commit and push.
2. Preview locally (builds everything, publishes nothing):

   ```
   powershell -ExecutionPolicy Bypass -File tools\release.ps1
   ```

   Read `dist\release\<suite>\notes.md`. It is exactly the text GitHub will show.
3. Actions > **release** > Run workflow (leave the tag empty for `release-<today UTC>.N`, or give one). It runs the same
   script, then creates the GitHub Release marked Latest with the zips, the bundle, `SHA256SUMS.txt` and the notes.

Each plugin zip holds the DLL, the `d2rl-*` folders, `README.md` and `CHANGELOG.md`. A plugin that must ship more (an
installer script, example files) lists them in `plugins/<name>/release-files.txt`: one path per line, relative to the
plugin folder, files or folders, `#` for comments. They keep their relative path in the zip, so
`tools/install.ps1` lands at `tools\install.ps1`. List only what players need, not test or developer tools. A listed path
that does not exist stops the script.

The script refuses to run if a plugin's `<plugin>/vX.Y.Z` tag is missing on origin or its changelog has no dated
`## X.Y.Z - YYYY-MM-DD` section.

A hotfix to one plugin is a new plugin version and tag, then a new suite release (the `.N` counts releases on one day).

## Updating the SDK

```
git -C third_party/PluginSDK fetch && git -C third_party/PluginSDK checkout <tag-or-sha>
git add third_party/PluginSDK
```

Build every plugin (`tools\build.ps1`) before committing; the SDK pin applies to all of them, so a pin change usually
means a new suite release.
