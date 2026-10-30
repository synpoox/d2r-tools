// The three parts. Each installs on its own: a refused hook or patch in one leaves the others as they are.
//
// Install* writes one status line into `report` ("on: ..." or "off: why") and returns whether the part is active.
// Status* adds live counters for the console command.
#pragma once

#include <D2RLPlugin/api.h>

#include <cstddef>

// minimap: the mini map's side (position = "right" / "left").
auto InstallMinimapSide(const D2RL::PluginContext* context, char* report, size_t size) noexcept -> bool;

// overlay: the automap key shows the full-screen overlay over a visible mini map and brings the mini map back.
auto InstallOverlayToggle(const D2RL::PluginContext* context, char* report, size_t size) noexcept -> bool;
void StatusOverlayToggle(char* out, size_t size) noexcept;

// zoom: mouse wheel zoom on the full-screen overlay.
auto InstallOverlayZoom(const D2RL::PluginContext* context, char* report, size_t size) noexcept -> bool;
void StatusOverlayZoom(char* out, size_t size) noexcept;
