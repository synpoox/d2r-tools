// The map item and the red portal: hooks D2R's item spell check and use, and on item spell 16 gates the map (Hell,
// Harrogath, one per game, a free spot) and opens a red portal to the slot level. Every address is found by byte
// signature at load; a part that finds no single match stays off. The two item hooks go in at load whenever their own
// parts were found and pass straight through to the game until every part the portal needs is on. Notes: docs/reverse-engineering/cartographer.md.
#pragma once

#include "core/tables.h"
#include "statplan.h"

#include <D2RLPlugin/api.h>

#include <memory>
#include <string>
#include <vector>

namespace carto {

struct Tables {
	std::vector<MapDef> maps;
	Settings settings;
	std::vector<Slot> slots;
	// Optional tables. The pool is applied in game (not yet verified there); shapes and events are loaded and checked only.
	cartographer::layout::ShapeTable shapes;
	std::vector<cartographer::stats::PoolRow> pool;  // empty: the built-in pool
	std::vector<cartographer::events::Row> events;
	cartographer::statplan::StatIds statIds;  // itemstatcost.txt: stat name -> id, for the map stats
};

// One engine part and what became of it.
struct PartStatus {
	std::string name;
	bool on = false;
	std::string detail;
};

// Signatures given in signatures.txt (Part, Signature) replace the built-in ones.
struct SignatureOverride {
	std::string part, signature;
};

// The tables the hooks use. Safe to call from any thread, at any time.
void SetTables(std::shared_ptr<const Tables> tables);

// Called once, on the first LocalPlayerReady event (only registered when the PlayerTick part was found). Set before InstallPortal.
void SetFirstGameStartNotify(void (*notify)() noexcept);

// Finds the parts and installs the hooks. Returns whether the portal is live; `parts` says why not.
auto InstallPortal(const D2RL::PluginContext* context, const std::vector<SignatureOverride>& overrides, std::vector<PartStatus>* parts) -> bool;

// `cartographer check` calls it. The hooks are written at load only; this never writes code. It re-scans all the called
// parts (using the signature overrides read at load) and turns the portal on when every required part is on. When the
// item hooks were not installed at load (NeedsRestart), it changes nothing. Returns whether the portal is live.
auto RescanPortal() -> bool;

// True when a hooked part was missing or refused at load: only a restart can fix that.
auto NeedsRestart() -> bool;

enum class OpenStart {
	Posted,      // the game thread opens it, through the gates, on the next frame
	NoSuchMap,
	NotInGame,   // no local player yet
	Unavailable, // the PlayerTick part is missing or refused: there is no direct open
	NoServer,    // no server tick has run in this game (a game joined over TCP/IP): only the host can open it
};

// 'cartographer open <Name>': asks the game to open the map for the local player without an item. Safe from any thread.
// `mapName` (optional) gets the map's own name, as written in maps.txt, when the request was posted. A stale earlier request
// that this one replaces is reported to the player.
auto RequestOpen(const std::string& name, std::string* mapName = nullptr) -> OpenStart;

auto PortalLive() -> bool;
auto PartStatuses() -> std::vector<PartStatus>;

}  // namespace carto
