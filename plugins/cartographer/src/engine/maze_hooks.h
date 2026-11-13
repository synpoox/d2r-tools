// Maze steering for the slot level: hooks D2R's level build (0x3271c0), maze walk (0x3e96d0), special-room placer
// (0x3eb850) and ring grower (0x3e9820), found by byte signature, and builds the maze a map's layout asks for instead of
// the random one. Only the Maze slot level is touched; every other level runs the game's own code. All four hooks go in
// at load and pass straight through until every part is on; a part that is missing or refused turns only the maze steering
// off (every other level keeps working). If the plan cannot be built or the engine refuses a step, that build falls back to
// the game's own maze with the reason logged. Notes: docs/reverse-engineering/cartographer.md (index): sections 1, 1a and 5.
#pragma once

#include "portal.h"
#include "scan.h"

#include "eventplan/eventplan.h"

#include <memory>
#include <vector>

namespace carto {

// Finds the maze parts and installs the four hooks. Never throws. `ranges` are the exe's executable sections.
void InstallMazeHooks(const D2RL::PluginContext* context, const std::vector<SignatureOverride>& overrides, const CodeRange* ranges, int count);

// True once InstallMazeHooks reached the level build hook (0x3271c0), whether or not it went in: the outdoor build shares it, or stays off.
auto MazeLevelBuildHookTried() -> bool;

// True when every maze part is on and the hooks are in: a Cave or Crypt map can be opened.
auto MazeSteeringLive() -> bool;

// The maze parts and what became of them (for 'cartographer check').
auto MazePartStatuses() -> std::vector<PartStatus>;

// The tables the hooks use: the Maze slot level and the MazeProbe switch. Any thread, any time.
void SetMazeTables(const std::shared_ptr<const Tables>& tables);

// A maze map is about to be opened through the Maze slot: the next build of that level uses it. Call before the portal opens.
void MazeMapOpened(const MapDef& map, const Tables& tables, uint64_t seedSalt);

// A TCP/IP client was told the host's map (map sync, phase 7 part 2): the next slot build on this client uses the host's layout seed with this
// client's own row of maps.txt. False, and nothing changed, when the seed is 0, the maze steering is off, or the slot level was already built
// here (with or without a map open): it is not rebuilt. Never call it on a host or in single player.
auto MazeRemoteMapOpened(const MapDef& map, const Tables& tables, uint64_t hostSeed) -> bool;

// True once the Maze slot level was built, or is being built, in this game, whether or not a map was open then.
auto MazeSlotRanAny() -> bool;

// The data flag the game last called the level build with in this game, or -1 before any build. On a remote client every build is the
// client's own, so this is the flag its tables are looked up with. The map sync's client apply reads it for the levels row.
auto MazeLevelDataFlag() -> int;

// The layout seed of the map opened last (the seed MazeMapOpened chose), or 0 when none is open. For the map sync (host to client).
auto MazeOpenSeed() -> uint64_t;

// The portal did not open after all: forget the map.
void MazeMapCancelled();

// Leaving a game: forget the map and its plan.
void MazeGameLeft();

// The local player's room was seen in the Maze slot level (probe logging only; does nothing unless MazeProbe is on).
void MazeProbeArrival(void* room, int32_t levelId);

// What the arrival probe found for the local player in this game: 0 not yet, 1 in the planned entrance cell, 2 not in it, 3 could not be told.
// The probe runs when MazeProbe or AutoCheck is 1. Read by AutoCheck.
auto MazeArrivalResult() -> int;

// The tileset of the maze map that is open and not yet built, or empty. The maze slots share one job, so portal.cpp refuses a map of another
// tileset while this is set (MazeOpenConflict).
auto MazePendingTileset() -> std::string;

// True while a slot build is running, or once one ran for an opened map in this game; no map is opened through the slot then
// (the level can be torn down and rebuilt, so a later open could mix plans) until the game is left.
auto MazeSlotBuilt() -> bool;

// True while the arrival probe still has something to log (MazeProbe on, a slot, not logged yet). Cheap; the tick asks first.
auto MazeArrivalPending() -> bool;

// True when the map's tileset is one the maze steering builds (Cave, Crypt): it opens through its tileset's slot (MapSlotName).
auto MapIsMaze(const MapDef& map) -> bool;

// Empty, or why the map's slot level cannot build it (the level has another LevelType than the tileset needs).
auto MazeTilesetProblem(const MapDef& map, const Tables& tables) -> std::string;

// True when `level` is the slot level of any maze tileset.
auto IsMazeSlotLevel(int level) -> bool;

// The plan's event room in the built slot level, as the game's own room rect (tiles; the DRLG room's x, y, width, height).
// Known once the slot build has run for the opened map; cleared when a map is opened and when the game is left.
struct MazeRoomRect {
	int32_t x = 0, y = 0, w = 0, h = 0;
};
struct MazeEventRect {
	bool known = false;
	int32_t levelId = 0, x = 0, y = 0, w = 0, h = 0;
	std::vector<MazeRoomRect> more;  // the plan cell's other rooms, nearest the centre first: the fallbacks when this one cannot host the Boss
};
auto MazeEventRoom() -> MazeEventRect;

// The plan's boss room (the far cell of the main path), in the same form and with the same lifetime. Not known when the layout has no boss room.
auto MazeBossRoom() -> MazeEventRect;

// The DRLG rects (tiles) of every room in `level`'s room list (level +0x10 first room, room +0x48 next), read through the guarded
// walker. False, and `out` left empty, when the list cannot be read (no level, no first room, an unreadable room, or more than the
// sanity bound). Game thread.
auto LevelRoomRects(const void* level, std::vector<cartographer::eventplan::Rect>* out) noexcept -> bool;

}  // namespace carto
