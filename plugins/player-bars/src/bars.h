// The bars: game reads and orb hiding on the UI thread, drawing in the overlay frame callback.
#pragma once

#include <D2RLPlugin/api.h>

#include <cstddef>

// False (and why) when a part is off. Hooks nothing; the signatures only find functions to call.
auto InstallBars(const D2RL::PluginContext* context, char* report, size_t size) noexcept -> bool;
void UninstallBars() noexcept;
void BarsStatus(char* out, size_t size) noexcept;
