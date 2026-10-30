// Quick NPC: hold the configured key (quick-npc.toml, default Left Ctrl) and left-click a vendor NPC to go straight to
// its trade screen.
//
// D2R 3.3.0 build 93847 under D2RLoader 1.3.1 only (image 0x6ab3782c); plugin.cpp installs this part only on that image.
// How it works: quick_npc.cpp; every address is checked by tools/verify_dump.py.
#pragma once

#include <D2RLPlugin/api.h>

#include <cstddef>

// Installs the two hooks and reads quick-npc.toml. Writes "on: ..." or "off: <why>" into `report`.
auto InstallQuickNpc(const D2RL::PluginContext* context, char* report, size_t reportSize) noexcept -> bool;

// Live counters for the status command.
void QuickNpcStatus(char* out, size_t size) noexcept;

// Re-reads quick-npc.toml (console "quick-npc reload"); false when the file could not be read (defaults are used).
auto ReloadQuickNpcConfig() noexcept -> bool;

// Runtime switch (console "quick-npc on|off"). Off: both hooks only call the original function.
void SetQuickNpcEnabled(bool enabled) noexcept;
auto QuickNpcEnabled() noexcept -> bool;

// Plugin unload: stop acting and forget any pending interaction. D2RLoader drops the queued callback and removes the
// hooks it installed in the transaction.
void ShutdownQuickNpc() noexcept;
