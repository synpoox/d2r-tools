// Maze steering for the slot level: hooks D2R's level build (0x3271c0), maze walk (0x3e96d0), special-room placer
// (0x3eb850) and ring grower (0x3e9820), found by byte signature, and builds the maze a map's layout asks for instead of
// the random one. Only the Maze slot level is touched; every other level runs the game's own code. All four hooks go in
// at load and pass straight through until every part is on; a part that is missing or refused turns only the maze steering
// off (the preset maps keep working). If the plan cannot be built or the engine refuses a step, that build falls back to
// the game's own maze with the reason logged. Notes: docs/reverse-engineering/cartographer.md sections 1, 1a and 5.
#pragma once

#include "portal.h"
#include "scan.h"

#include <memory>
#include <vector>

namespace carto {

// Finds the maze parts and installs the four hooks. Never throws. `ranges` are the exe's executable sections.
void InstallMazeHooks(const D2RL::PluginContext* context, const std::vector<SignatureOverride>& overrides, const CodeRange* ranges, int count);

// True when every maze part is on and the hooks are in: a Cave or Crypt map can be opened.
auto MazeSteeringLive() -> bool;

// The maze parts and what became of them (for 'cartographer check').
auto MazePartStatuses() -> std::vector<PartStatus>;

// The tables the hooks use: the Maze slot level and the MazeProbe switch. Any thread, any time.
void SetMazeTables(const std::shared_ptr<const Tables>& tables);

// A maze map is about to be opened through the Maze slot: the next build of that level uses it. Call before the portal opens.
void MazeMapOpened(const MapDef& map, const Tables& tables, uint64_t seedSalt);

// The portal did not open after all: forget the map.
void MazeMapCancelled();

// Leaving a game: forget the map and its plan.
void MazeGameLeft();

// The local player's room was seen in the Maze slot level (probe logging only; does nothing unless MazeProbe is on).
void MazeProbeArrival(void* room, int32_t levelId);

// True when the Maze slot level has been built in this game: it is not rebuilt, so another map cannot be opened through it
// until the game is left.
auto MazeSlotBuilt() -> bool;

// True while the arrival probe still has something to log (MazeProbe on, a slot, not logged yet). Cheap; the tick asks first.
auto MazeArrivalPending() -> bool;

// Empty, or why the Maze slot level cannot build `map` (a Cave level cannot build a Crypt map, and the other way round).
auto MazeTilesetProblem(const MapDef& map, const Tables& tables) -> std::string;

// The Maze slot level id, or -1.
auto MazeSlotLevelId() -> int;

}  // namespace carto
