// A map's own look (issue #380): Brightness and Warmth from maps.txt, applied while the player is in
// the map. Maps that share a tileset share one level, one biome and so one set of vis files, so nothing is written into the
// tileset: the game's visual data system builds the level's `VisualDataComponent` every frame and hands it to its apply
// function (D2Render visual_data_system.cpp), which copies it into the frame settings. That function is hooked (found by byte
// signature, guarded); while the local player stands in the map's slot level the hook changes the component before the game
// copies it. Nothing is restored on leaving: the game rebuilds the component every frame, so the next frame is the tileset's own
// look again. A part that is missing or whose guard fails leaves the hook out. Notes: docs/reverse-engineering/cartographer/32-level-lighting.md.
#pragma once

#include "core/tables.h"
#include "engine/portal.h"

#include <D2RLPlugin/api.h>

#include <string>
#include <vector>

namespace carto {

// Finds the apply part and installs the hook. Call once at load. Returns whether the hook is live; `parts` gets the part.
auto InstallMapLighting(const D2RL::PluginContext* context, const std::vector<SignatureOverride>& overrides, std::vector<PartStatus>* parts) -> bool;
auto MapLightingPartStatuses() -> std::vector<PartStatus>;

// A map was opened through slot level `slotLevel`: its look is applied while the local player is in that level. A map with no look
// columns set changes nothing. Any thread.
void MapLightingMapOpened(const MapDef& map, int slotLevel);

// Called from the player tick with the local player's level (game thread).
void MapLightingPlayerTick(int playerLevel) noexcept;

// The game was left: forget the map. Nothing to put back (see above).
void MapLightingGameLeft() noexcept;

auto MapLightingSummary() -> std::string;

}  // namespace carto
