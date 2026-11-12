// The map item and the red portal: hooks D2R's item spell check and use, and on item spell 16 gates the map (Hell,
// Harrogath, one per game, a free spot) and opens a red portal to the slot level. Every address is found by byte
// signature at load; a part that finds no single match stays off, and the portal hooks go in only when all the
// parts they need are on. Notes: docs/reverse-engineering/cartographer.md.
#pragma once

#include "core/tables.h"

#include <D2RLPlugin/api.h>

#include <memory>
#include <string>
#include <vector>

namespace carto {

struct Tables {
	std::vector<MapDef> maps;
	Settings settings;
	std::vector<Slot> slots;
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

// Finds the parts and installs the hooks. Returns whether the portal is live; `parts` says why not.
auto InstallPortal(const D2RL::PluginContext* context, const std::vector<SignatureOverride>& overrides, std::vector<PartStatus>* parts) -> bool;

// The map named `name` opens on the next use of a map item. Returns false when there is no such map.
auto ArmMap(const std::string& name) -> bool;

enum class OpenStart {
	Posted,      // the game thread opens it, through the gates, on the next frame
	NoSuchMap,
	NotInGame,   // no local player yet
	Unavailable, // the PlayerTick part is missing or refused: there is no direct open
	NoServer,    // no server tick has run in this game (a game joined over TCP/IP): only the host can open it
};

// 'cartographer open <Name>': asks the game to open the map for the local player without an item. Safe from any thread.
auto RequestOpen(const std::string& name) -> OpenStart;

auto PortalLive() -> bool;
auto PartStatuses() -> std::vector<PartStatus>;

}  // namespace carto
