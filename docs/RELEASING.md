# Releasing

Plugins are versioned and tagged on their own (`<plugin>/vX.Y.Z`). Players get **suite releases** (`release-YYYY.MM.DD.N`):
one GitHub Release with a zip per plugin, a bundle of all of them, `SHA256SUMS.txt` and notes written for players.
The root `README.md` table lists released plugins only. An unreleased plugin has no row there and is not in any suite.

## 1. Release a plugin version

1. In `plugins/<name>/`, move `CHANGELOG.md` entries from `Unreleased` to a new `## X.Y.Z - YYYY-MM-DD` heading (the
   release date). The release notes take the player-facing text from this section, so write it for players.
2. Bump `MODKIT_PLUGIN_VERSION` (or the plugin's version constant) to `X.Y.Z` and update the plugin's version in the
   root `README.md` table (add a row at a plugin's first release).
3. Commit to `main`, wait for `ci` to go green, then tag and push:

   ```
   git tag <name>/vX.Y.Z
   git push origin <name>/vX.Y.Z
   ```

## 2. Publish a suite release

1. Edit `release/suite.json`: set each included plugin's `version` to its tag, and write its `validation` (what was
   actually checked: build, tests, installed, live in game) and `limits`. State what was not tested. Update
   `game.d2r` / `game.d2rloader` if the tested versions changed. Commit and push.
2. Preview locally (builds everything, publishes nothing):

   ```
   powershell -ExecutionPolicy Bypass -File tools\release.ps1
   ```

   Read `dist\release\<suite>\notes.md`. It is exactly the text GitHub will show.
3. Actions > **release** > Run workflow (leave the tag empty for `release-<today UTC>.N`, or give one). It runs the same
   script, then creates the GitHub Release marked Latest with the zips, the bundle, `SHA256SUMS.txt` and the notes.

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
