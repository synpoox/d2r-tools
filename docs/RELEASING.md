# Releasing

1. In `plugins/<name>/`, move `CHANGELOG.md` entries from `Unreleased` to a new `X.Y.Z` heading.
2. Bump `MODKIT_PLUGIN_VERSION` in `plugins/<name>/CMakeLists.txt` (shared plugins must match in multiplayer).
3. Commit to `main`, then tag and push:

   ```
   git tag <name>/vX.Y.Z
   git push origin <name>/vX.Y.Z
   ```

The `release` workflow builds that plugin on Windows and publishes `<name>-vX.Y.Z.zip` as a GitHub Release.

Manual fallback: `tools\build.ps1 -Plugin <name>`, zip `dist\<name>\`, `gh release create <name>/vX.Y.Z <zip>`.

## Updating the SDK

```
git -C third_party/PluginSDK fetch && git -C third_party/PluginSDK checkout <tag-or-sha>
git add third_party/PluginSDK
```

Build every plugin (`tools\build.ps1`) before committing; the SDK pin applies to all of them.
