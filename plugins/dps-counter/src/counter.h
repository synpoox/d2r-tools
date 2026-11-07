// The counter: a hook on the server's damage apply feeds it, a native text widget in a plugin panel shows it.
#pragma once

#include <D2RLPlugin/api.h>

#include <cstddef>

// False (and why) when it is off.
auto InstallCounter(const D2RL::PluginContext* context, char* report, size_t size) noexcept -> bool;
void UninstallCounter() noexcept;
void CounterStatus(char* out, size_t size) noexcept;
