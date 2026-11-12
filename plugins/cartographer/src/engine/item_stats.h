// The map item's own stats (ItemStats=1): reads the stat list of the item that is being used, before the game consumes it.
// Two engine parts, both only called and found by byte signature at load: the finder for a unit's list by flags and the
// list copy the game's own client sender uses. A part that is missing leaves the mode off (a normal draw). Notes:
// docs/reverse-engineering/cartographer.md section 14.
#pragma once

#include "engine/portal.h"

#include <D2RLPlugin/api.h>

#include <string>
#include <vector>

namespace carto {

// Finds the two parts. Call once at load. Returns whether the item's stats can be read; `parts` gets the parts added by this call.
auto InstallItemStats(const D2RL::PluginContext* context, const std::vector<SignatureOverride>& overrides, std::vector<PartStatus>* parts) -> bool;

// The stats on the base magic list of `item` (an item unit), in the list's order. Empty when a part is off, the item has no such list or
// the list is empty; `note` then says why when there is something to say. At most 16 stats are read (the game's own copy limit).
// Game thread only.
auto ReadMapItemStats(void* item, std::string* note) noexcept -> std::vector<cartographer::statplan::ItemStat>;

auto ItemStatsPartStatuses() -> std::vector<PartStatus>;

}  // namespace carto
