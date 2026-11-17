// The Tombs case's own growers (Act 2 Tombs, LevelType 17; RE note 47): the maze builder's case calls 0x3ea0e0 before the walk (a cross of rooms round the first) and 0x3eb610 after it
// (special rooms by level id). Every slot-level Tombs build skips both (maze_hooks.cpp answers MazeBuildSkipsTombGrower): the plan builds the rooms, the entrance is an ordinary room, and a
// slot level has no special room to place. Any other level (vanilla Tombs) runs the game's own growers.
#pragma once

#include "scan.h"

#include <vector>

namespace carto {

// Finds both growers by signature and hooks them (15 bytes each). Never throws; both are off with a logged reason when either is not found or refused.
void InstallTombGrowerHooks(const D2RL::PluginContext* context, const std::vector<SignatureOverride>& overrides, const CodeRange* ranges, int count);

// True when both growers are hooked: a Tombs map can be opened.
auto TombGrowersHooked() -> bool;

// The two parts and what became of them (for 'cartographer check').
auto TombGrowerStatuses() -> std::vector<PartStatus>;

}  // namespace carto
