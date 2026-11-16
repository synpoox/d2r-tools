// cartographer.open-extra.v1: lets another plugin open a second map in a game that already has one (issue #485).
// Contract (fields, threads, refusals): docs/cartographer/open-extra-service.md. The work is in portal.cpp (OpenExtraMap).
#pragma once

#include <D2RLPlugin/api.h>

namespace carto {

// Publishes the service. Best effort: without the SDK's communication service nothing is offered, and nothing else changes.
void PublishOpenExtraService(const D2RL::PluginContext* context) noexcept;

}  // namespace carto
