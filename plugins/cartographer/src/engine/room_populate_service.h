// cartographer.room-populate.v1: lets other plugins run code around every room populate PopulateHook sees, and skip the game's
// own monsters for one room. Contract: docs/cartographer/room-populate-service.md. Game thread (inside the hook).
#pragma once

#include "cartographer_room_populate_v1.h"

#include <D2RLPlugin/api.h>

#include <cstdint>

namespace carto {

// Publishes the service when the PopulateRoom hook is live (`hookLive`); otherwise nothing is offered. Best effort: without the
// SDK's communication service nothing else changes. Also honours CARTOGRAPHER_TEST_SKIP_NATIVE=<levelId> (a game check only).
void PublishRoomPopulateService(const D2RL::PluginContext* context, bool hookLive) noexcept;

// Whether any subscriber is registered (a cheap check before the hook calls the others).
auto RoomPopulateHasSubscribers() noexcept -> bool;

// Calls the before callbacks; the OR of their flags. Called by PopulateHook before the game's populate. Game thread.
auto RoomPopulateBefore(void* game, void* room, int32_t levelId) noexcept -> uint32_t;

// Calls the after callbacks. Called by PopulateHook after its own work. Game thread.
void RoomPopulateAfter(void* game, void* room, int32_t levelId, bool skipped) noexcept;

}  // namespace carto
