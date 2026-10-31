# Changelog

## Unreleased

- No version gate: the plugin attempts to load, hook and run on any D2R or D2RLoader version. It declares PluginSDK ABI 4
  and reads optional context through the SDK getters. On game code that has moved it can crash; hooks whose entry bytes
  differ are still refused.
- A fault while the plugin is loaded is written to the plugin's log as a `FAULT` line with its cause and whether the
  plugin was involved. A later line says when the game survived it, so the last unconfirmed `FAULT` is the crash.

## 1.0.0 - 2026-10-29

First public release, for Diablo II: Resurrected 3.3 under D2RLoader 1.3.1.

- While the waypoint panel is open, the mouse wheel switches Act tabs: down for the next Act, up for the previous,
  wrapping at both ends. The cursor can be anywhere.
- It uses the tab bar's own previous/next, the same as a controller's shoulder buttons. Acts not reached yet are
  skipped, and Classic characters cycle Acts I–IV.
- It never selects a destination. With the panel closed, the wheel is untouched.
- Built on D2R 3.3.0 build 93847 with D2RLoader 1.3.1. It finds the game code by signature rather than fixed
  addresses, so it also loads on other versions. Where that code has changed, scrolling does nothing.
