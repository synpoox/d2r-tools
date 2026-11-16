// cartographer.arena.v1: what other plugins may ask about the fixed arenas of arenas.txt and their named spots (arenaspots.txt, issue #488).
// Contract (fields, threads, versions): docs/cartographer/arena-service.md. Read-only; the tables behind it are fixed once loaded (a reload swaps them whole).
#pragma once

#include "cartographer_arena_v1.h"
#include "engine/portal.h"

#include <D2RLPlugin/api.h>

#include <cstdint>
#include <memory>

namespace carto {

// The loaded tables (SetTables calls this). Any thread.
void ArenaServiceSetTables(std::shared_ptr<const Tables> tables);
auto ArenaServiceTables() -> std::shared_ptr<const Tables>;

// Publishes the service. Best effort: without the SDK's communication service nothing is offered, and nothing else changes.
void PublishArenaService(const D2RL::PluginContext* context) noexcept;

}  // namespace carto
