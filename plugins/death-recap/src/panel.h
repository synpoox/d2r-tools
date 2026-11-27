#pragma once
#include <D2RLPlugin/api.h>
#include "history.h"
namespace recap::panel {
bool Install(const D2RL::PluginContext*) noexcept;
void Show(bool toggle = false) noexcept;
void Close() noexcept;
void Update(const History&, size_t selected, size_t page, bool preview = false) noexcept;
bool Open() noexcept;
}
