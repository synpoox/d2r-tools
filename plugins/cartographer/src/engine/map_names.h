// The map's own Name on the red portal tooltip, the automap title and the entry banner (issue #90). The plugin registers a
// string file (one name text and one entry text per map) in its own string namespace through the SDK's resource service,
// and when a map opens writes that map's two keys into the slot level's row of the game's levels table (LevelName and
// LevelEntry, each a char[40]), then puts the originals back on leaving the game and on unload. Off by default
// (MapNames=1 in cartographer.txt). Written, not verified in a game. Notes: docs/reverse-engineering/cartographer.md section 10.
#pragma once

#include "engine/portal.h"

#include <D2RLPlugin/api.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace carto {

// Finds the levels row part and the SDK services, and registers the strings for the tables set so far. Call after SetTables.
// Fails open: with a part or service missing the map keeps the slot level's own name. `parts` gets the part added by this call.
auto InstallMapNames(const D2RL::PluginContext* context, const std::vector<SignatureOverride>& overrides, std::vector<PartStatus>* parts) -> bool;

// The part and what the setting does now, for 'cartographer check'.
auto MapNamesPartStatuses() -> std::vector<PartStatus>;
auto MapNamesSummary() -> std::string;

// The tables changed (load or reload): registers the strings again when the map names changed. Any thread.
void MapNamesSetTables(const std::shared_ptr<const Tables>& tables);

// Called on the game thread just before the game opens the portal for `map` through slot level `level`. Writes the map's
// keys into the slot row when the setting is on, every check passed and the engine part is on; otherwise changes nothing.
// `dataFlag` is the game's data flag (the byte at game + 0x106). Returns whether the row was changed.
auto MapNamesBeforeOpen(const MapDef& map, const Tables& tables, int level, uint32_t dataFlag) noexcept -> bool;

// Puts the original LevelName and LevelEntry back in every row changed. Safe to call at any time, any number of times: on a
// map that did not open, on GameLeft and on unload.
void MapNamesRestore() noexcept;

}  // namespace carto
