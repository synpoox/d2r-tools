// The counter: belt items read through the SDK, one native text widget per belt column in a plugin panel.
#pragma once

#include <D2RLPlugin/api.h>

#include <cstddef>

// False (and why) when it is off.
auto InstallCounter(const D2RL::PluginContext* context, char* report, size_t size) noexcept -> bool;
void UninstallCounter() noexcept;
void CounterStatus(char* out, size_t size) noexcept;
