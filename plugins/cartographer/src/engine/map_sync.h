// Map sync in the game (phase 7, part 1): when the host opens a map it tells each joined TCP/IP client which map is open (the
// maps.txt row, the slot level and the layout seed), over the SDK's NetworkService. A client connects to the host after it enters a
// game, says hello, checks what it is sent against its own maps.txt and keeps it. Part 2: the client then applies the map's name
// (MapNames, in its own copy of the slot level's row) and, for a Cave or Crypt map, the host's layout seed (the maze job of its next
// slot build), each only where it fits the client's own tables. Stats and events stay with the host. No new engine address and no hook of its own: everything here goes through the SDK, so a
// missing or older NetworkService only turns the sync off (logged) and the game runs as before. Single player and a host with no client
// send nothing. Written, not verified in a game: docs/reverse-engineering/cartographer.md section 12.
#pragma once

#include "mapsync.h"
#include "portal.h"

#include <D2RLPlugin/api.h>

#include <memory>
#include <optional>
#include <string>

namespace carto {

// Registers the channel. Never throws. Returns whether the sync is on; when not, why is logged and in MapSyncSummary().
auto InstallMapSync(const D2RL::PluginContext* context) -> bool;

// The names of maps.txt by row, for the client's check. Call whenever the tables are set. Any thread.
void MapSyncSetTables(const std::shared_ptr<const Tables>& tables);

// A map was opened (the portal is open) on the host or in single player. `map` points into `tables.maps`. `slotLevel` is the level the
// portal leads to, `openings` the game's opening count after the open, `seed` the maze layout seed (0 for a preset map). `outdoor` marks an outdoor map: `seed` is then the outdoor layout seed (OutdoorOpenSeed()); the portal passes it (issue #144).
// Game thread. Sends to the clients that said hello; does nothing else.
void MapSyncMapOpened(const MapDef& map, const Tables& tables, int slotLevel, int openings, bool maze, uint64_t seed, bool outdoor = false);

// LocalPlayerReady: a game was entered. Starts the client's connect loop (it stops by itself on a host, where a server tick shows up).
void MapSyncGameEntered();

// GameLeft: forget the game's peers, the open map and the connect loop.
void MapSyncGameLeft();

// The map the host last told this client about, with its name from this client's maps.txt. Empty on a host, in single player,
// or before the host has said anything. For the later parts that apply it.
struct RemoteMap {
	cartographer::mapsync::MapIdentity identity;
	std::string name;
};
auto MapSyncRemoteMap() -> std::optional<RemoteMap>;

// For 'cartographer check': one line saying whether the sync is on and what it holds.
auto MapSyncSummary() -> std::string;

// Unload: turns the sync off and unregisters the channel when the SDK offers it. Never throws.
void UnloadMapSync() noexcept;

}  // namespace carto
