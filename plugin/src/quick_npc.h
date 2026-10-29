// Quick NPC: hold the "Quick NPC: trade" key and left-click a vendor NPC to go straight to its trade screen.
//
// D2R 3.3.0 build 93847 under D2RLoader 1.3.1 only (image 0x6ab3782c); plugin.cpp installs this part only on that image.
// How it works and where every address comes from: quick_npc.cpp and docs/reverse-engineering.md.
#pragma once

#include <D2RLPlugin/api.h>

#include <cstddef>

// Installs the two hooks and registers the input action. Writes "on: ..." or "off: <why>" into `report`.
auto InstallQuickNpc(const D2RL::PluginContext* context, char* report, size_t reportSize) noexcept -> bool;

// Live counters for the status command.
void QuickNpcStatus(char* out, size_t size) noexcept;

// Runtime switch (console "quick-npc on|off"). Off: both hooks only call the original function.
void SetQuickNpcEnabled(bool enabled) noexcept;
auto QuickNpcEnabled() noexcept -> bool;

// Plugin unload: stop acting and forget any pending interaction. D2RLoader drops the queued callback and the action.
void ShutdownQuickNpc() noexcept;
