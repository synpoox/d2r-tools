// Map items are never magic: hooks the game's item quality roll and turns a magic result for a map item into normal.
// Found by byte signature at load; a part that is missing leaves the hook out and the game rolls as it always did.
// Notes: docs/reverse-engineering/cartographer/23-map-quality.md.
#pragma once

#include "engine/portal.h"

#include <D2RLPlugin/api.h>

#include <string>
#include <vector>

namespace carto {

// Finds the parts and installs the hook. Call once at load. Returns whether the hook is live; `parts` gets the parts of this call.
auto InstallMapQuality(const D2RL::PluginContext* context, const std::vector<SignatureOverride>& overrides, std::vector<PartStatus>* parts) -> bool;

auto MapQualityPartStatuses() -> std::vector<PartStatus>;

// One line for 'cartographer check': whether magic maps are blocked, and how many the hook has turned normal this run.
auto MapQualitySummary() -> std::string;

}  // namespace carto
