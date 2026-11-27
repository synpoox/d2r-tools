# Death Recap

Development branch only; **not a playable plugin yet**. This revision builds the game-independent combat telemetry model and an inert SDK entry point. No damage hooks, native panel, history, or gameplay changes are enabled.

## Implemented
- Bounded single-producer rolling buffer with a ten-second default reporting window.
- Confirmed/inferred/unavailable fields; separate gross loss, restoration and unexplained net resource changes in raw 1/256 units.
- Synthetic tests for resource conservation, overlapping healing and damage, mana changes, rejected and missing event IDs, death freezing, rolling overflow and report ownership.
- Confirmed fatal-sequence linking; separate source/ability and elemental pre/post totals, with explicit unknown buckets and coverage counts.

## Build and test

From the repository root on Windows x64 with Visual Studio 2022 and the pinned SDK:

```powershell
powershell -ExecutionPolicy Bypass -File tools\build.ps1 -Plugin death-recap -Test
```

The portable tests passed with GCC and Clang on Linux, warnings as errors and NDEBUG, and GCC with AddressSanitizer and UndefinedBehaviorSanitizer. Windows DLL compilation and game installation have not been tested.

## Configuration

The embedded TOML contains the proposed default window and a disabled flag. No configuration is consumed by the inert entry point yet.

## Compatibility

Target: D2R 3.3 / D2RLoader 1.3.1, offline and supported modded TCP/IP. This development revision is not tested in-game and must not be released. No Battle.net online support.

See [native capture research and continuation](../../docs/reverse-engineering/death-recap.md). The telemetry buffer assumes a serialized producer; the adapter must supply unique canonical events and preserve resource provenance. The report owns its copied events; publish it as const across threads. A mathematically reconciled report can still have unclassified transitions, incomplete attribution, or incomplete capture.

MIT licensed.
