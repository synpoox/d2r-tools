// The Arcane slot level's void (issue #566). A blank floor cell of a FillBlanks room is filled by 0x3f32c0 with tile (main 30, sub 0), which blocks
// missiles, on every level but 74, which gets sub 1: the void that missiles cross (RE note 37 section 3). This hooks the room tile lookup 0x3f3c60
// (RoomTileLookup) so the Arcane slot level asks for sub 1 too. Optional: a missing or refused part leaves Arcane maps' void blocking missiles, logged.
#pragma once

#include "scan.h"

#include <D2RLPlugin/api.h>

#include <vector>

namespace carto {

// Finds RoomTileLookup and hooks it. Never throws. `ranges` are the exe's executable sections.
void InstallArcaneVoidTile(const D2RL::PluginContext* context, const std::vector<SignatureOverride>& overrides, const CodeRange* ranges, int count);
auto ArcaneVoidTilePartStatus() -> PartStatus;

}  // namespace carto
