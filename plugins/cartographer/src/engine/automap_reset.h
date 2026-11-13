// A fresh slot map starts with an empty revealed automap (issue #306 review). The game keeps each character's revealed cells per layer and per game seed in
// the character's map memory file and reads them back when the layer becomes current (0xd5710). A Cave or Crypt slot map shares its layer with a game level
// and, in single player, the seed carries over, so the cells of an earlier map would draw on a new, different maze. The build of a slot map arms a skip of
// that one read; the leave and return of a town trip, which read the cells the same game saved, are untouched.
// Notes: docs/reverse-engineering/cartographer/22-automap-memory.md.
#pragma once

#include "scan.h"

#include <vector>

namespace carto {

// Finds the restore function by signature and hooks it (17 bytes). Never throws; off with a logged reason when the part is not found or refused.
void InstallAutomapReset(const D2RL::PluginContext* context, const std::vector<SignatureOverride>& overrides, const CodeRange* ranges, int count);

// The slot map is being built: the next time the game makes another layer current and reads its cells back, skip that read. Any thread. No effect when the part is off.
void AutomapResetArm() noexcept;

// A game was left, or the map did not open: forget a skip that was not used.
void AutomapResetDisarm() noexcept;

auto AutomapResetPartStatus() -> PartStatus;

}  // namespace carto
