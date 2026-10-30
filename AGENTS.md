# Agent notes

- Monorepo of independent D2RLoader plugins under `plugins/`; do not add cross-plugin code dependencies.
- Shared SDK is the `third_party/PluginSDK` submodule (never vendored, never edited here).
- Build/verify with `tools/build.ps1 -Plugin <name>`; releases via plugin-scoped tags (`docs/RELEASING.md`).
- Native offsets/signatures are build-specific; keep them in the owning plugin and document the target D2R build.
- Update the plugin's `CHANGELOG.md` under `Unreleased` for every user-visible change.
