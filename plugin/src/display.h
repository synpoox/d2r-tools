// The XP display: session tracking (core), reads on the UI thread (xp_reader), drawing (D2RLoader OverlayService),
// game join/leave (LifecycleService), hotkeys (InputService), config (D2RLoader's better-xp-bar.toml).
//
// NOT RUN IN GAME YET: every game-facing function here "needs live check" (TESTING.md).
#pragma once

#include <D2RLPlugin/api.h>

#include <cstddef>

namespace display {

// Part "xp": lifecycle listeners + the UI-thread reads. Needs the expected image and xp_reader::Install first.
void InstallTracking(const D2RL::PluginContext* context, char* report, size_t size) noexcept;
// Part "overlay": the frame callback that draws the lines.
void InstallOverlay(const D2RL::PluginContext* context, char* report, size_t size) noexcept;
// Part "hotkeys": "Toggle XP display" and "Reset XP session" in D2R's Controls menu (category "Better XP Bar").
void InstallHotkeys(const D2RL::PluginContext* context, char* report, size_t size) noexcept;

// Reads better-xp-bar.toml (D2RLoader creates it from the embedded defaults). Logs warnings. Safe to call again.
void LoadConfig(const D2RL::PluginContext* context) noexcept;

// Console sub-commands ("better-xp-bar reset|toggle|reload|show"). They queue work to the UI thread.
void RequestReset() noexcept;
void RequestToggle() noexcept;
void RequestReload() noexcept;

// One status line for the status command: game state, the last reading, counters.
void Status(char* out, size_t size) noexcept;
// The current display lines joined with " | " (for "better-xp-bar show" and the log).
void Lines(char* out, size_t size) noexcept;

}  // namespace display
