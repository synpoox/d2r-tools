// Map drop behaviours (issue #538): the per-map settings, the cartographer.map-drop.v1 service, and the test path that sets them
// without the route table. The game side (death hook, item hook) is in event_6b.cpp; the boss loop is in event_apply.cpp.
// Contract: docs/cartographer/map-drop-service.md. Thread: the game thread.
#pragma once

#include "behaviours/behaviours.h"
#include "cartographer_map_drop_v1.h"

#include <D2RLPlugin/api.h>

namespace carto {

// Sets (replaces) the behaviours of the map opened on `level`. The route table (#537) calls this once per opened map, an empty struct included.
void SetMapBehaviours(int level, const cartographer::behaviours::MapBehaviours& b);

// The behaviours of `level`; false (out untouched) when it has none.
auto GetMapBehaviours(int level, cartographer::behaviours::MapBehaviours* out) -> bool;

// Cheap check for the death hook: some map has a behaviour set.
auto MapBehavioursAny() noexcept -> bool;

// The game was left: forget every map's behaviours. Touches no game memory.
void MapBehavioursGameLeft() noexcept;

// A map opened on `level`: sets its behaviours from CARTOGRAPHER_TEST_BEHAVIOURS when that is set (a game check only; the grammar is parseSpec's).
void MapBehavioursTestOpened(int level);

// Calls every registered callback for `item`. Game thread.
void MapDropNotify(void* item, int level, const cartographer::behaviours::MapBehaviours& b) noexcept;

// Publishes the service; best effort (nothing changes without the SDK's communication service).
void PublishMapDropService(const D2RL::PluginContext* context) noexcept;

}  // namespace carto
