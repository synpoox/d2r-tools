// The outdoor build hook for the Outdoor slot level (issue #141, phase 5): builds the level a map's layout asks for instead of the
// game's own. It hooks the Act 5 outdoor builder (0x3fe2e0), found by byte signature, and takes over the slot level's build inside the
// game's level build (0x3271c0): it writes the level rectangle, calls the game's outdoor entry (0x3ee320), whose Act 5 branch is our
// hook and stamps the plan's pieces with the game's own piece stamper (0x3ecfc0) and marks the unused cells void (0x3ede00), then lets
// the game's native tail run. Only the Outdoor slot level is ever touched; every other level, in every act, runs the game's own code.
//
// OFF by default (OutdoorBuild = 0 in cartographer.txt) until the game checks of issue #89 and #142 confirm the void marker and the
// entry room. Fail open: a missing part, a plan that cannot be made or a level that is not as expected leaves that build to the game,
// with the reason in the log. A Jungle map opens through portal.cpp (issue #144), which calls OutdoorMapOpened.
// Notes and evidence: docs/reverse-engineering/cartographer.md section 8a.
#pragma once

#include "maze_hooks.h"
#include "portal.h"
#include "scan.h"

#include <cstdint>
#include <memory>
#include <vector>

namespace carto {

// The game's level build, as the maze hook and this one call it.
using LevelBuildFn = uint64_t (*)(uint8_t dataFlag, void* level) noexcept;

// Finds the outdoor parts and installs the Act 5 builder hook; when the maze hook did not take the level build (0x3271c0), installs its
// own on it. Call after InstallMazeHooks. Never throws. A missing or refused part turns only the outdoor build off.
void InstallOutdoorHooks(const D2RL::PluginContext* context, const std::vector<SignatureOverride>& overrides, const CodeRange* ranges, int count);

// The outdoor parts and what became of them (for 'cartographer check').
auto OutdoorPartStatuses() -> std::vector<PartStatus>;

// True when every outdoor part is on and the hooks are in (the setting may still be off).
auto OutdoorBuildLive() -> bool;

// The tables the hooks use: the Outdoor slot level (id, origin, declared size), the OutdoorBuild and OutdoorProbe switches, and the shapes.
// Forgets the cached plans. Any thread, any time.
void SetOutdoorTables(const std::shared_ptr<const Tables>& tables);

// An outdoor map is about to be opened through the Outdoor slot: the next build of that level uses it. `seedSalt` is mixed with a job id into the
// layout seed; a non-zero `fixedSeed` is the seed as it is (a client taking the host's).
void OutdoorMapOpened(const MapDef& map, const Tables& tables, uint64_t seedSalt, uint64_t fixedSeed = 0);

// True once the Outdoor slot level's build has started in this game (with or without a map open). A client then keeps the layout it has: the
// host's map seed is not applied (map sync, issue #146).
auto OutdoorSlotRanAny() -> bool;

// The seed of the open outdoor map (what map sync sends the clients), or 0.
auto OutdoorOpenSeed() -> uint64_t;

// Empty when the map just handed to OutdoorMapOpened can be planned on the Outdoor slot (the same cached plan the build uses); otherwise the
// player-facing reason. Call right after OutdoorMapOpened, before the portal opens.
auto OutdoorPlanProblem() -> std::string;

// True when the Outdoor slot level was built with a map open (or is being built) in this game: it holds that layout for the game, so another
// outdoor map cannot be opened until the game is left.
auto OutdoorSlotBuilt() -> bool;

// The tileset of the outdoor map opened and not yet cleared (the job the next slot build would use), or empty. One job serves both outdoor
// slots, so portal.cpp refuses to open a map of the other tileset while this is set (OutdoorOpenConflict).
auto OutdoorPendingTileset() -> std::string;

// The portal did not open after all: forget the map.
void OutdoorMapCancelled();

// Leaving a game: forget the map and the cached plans.
void OutdoorGameLeft();

// The Outdoor slot level id, or -1.
auto OutdoorSlotLevelId() -> int;

// The rooms of the built Outdoor slot level at the centres of the plan's event and boss cells (the exact DRLG rect of one room each, which the
// map events compare with the room the game populates), in the maze's form. Known once the slot build ran as planned; cleared when the game is
// left. Not known when the layout has no such cell. Any thread.
auto OutdoorEventRoom() -> MazeEventRect;
auto OutdoorBossRoom() -> MazeEventRect;

// What the arrival probe found for the local player in this game: 0 not yet, 1 in the planned entrance cell, 2 not in it, 3 could not be told,
// 4 the room was read but no entrance cell was marked (the arrival is left to the game).
// Cleared when the arrival mark is written (ArrangeRooms) and when the game is left. The probe runs when OutdoorProbe or AutoCheck is 1. Read by AutoCheck. Any thread.
auto OutdoorArrivalResult() -> int;

// OutdoorProbe or AutoCheck: where the local player first stands in the Outdoor slot level, compared with the entrance cell (as MazeProbeArrival). Game thread.
auto OutdoorArrivalPending() -> bool;
void OutdoorProbeArrival(void* room, int32_t levelId);

// Called by the level-build hook for every level. Returns true, with the game's result in *result, when it built the Outdoor slot level (the
// caller returns *result); false when it left the build alone (not the slot, the setting is off, a part or the plan is missing, or the level is
// not as expected), so the caller runs the game's own build. `original` is the game's level build, without our hook. Never throws.
auto OutdoorLevelBuild(uint8_t dataFlag, void* level, LevelBuildFn original, uint64_t* result) noexcept -> bool;

}  // namespace carto
