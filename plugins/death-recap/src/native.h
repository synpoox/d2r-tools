#pragma once
#include <D2RLPlugin/api.h>
#include "capture.h"
namespace recap::native {
bool Install(const D2RL::PluginContext* context) noexcept;
void Identity(uint64_t token) noexcept;
void GameTick(const D2RL::PluginContext*, void*) noexcept;
bool Pop(Event& e) noexcept;
bool ClientSample(Endpoints& p, uint32_t& mode) noexcept;
uint64_t Dropped() noexcept;
uint32_t Parts() noexcept;
void Stop() noexcept;
bool Faulted() noexcept;
void StateCount(uint32_t bank, uint32_t count) noexcept;
}
