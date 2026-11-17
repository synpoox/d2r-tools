// A map's own Hue and Saturation (issue #442): while the player is in the map, the renderer's colour-grading look-up table (LUT) for the
// level is replaced by a plugin-made one (the identity table pushed through one hue/saturation matrix; src/lutgrade). The renderer
// keeps a per-frame list of (LUT, weight) pairs that the level's transition code fills; the hook on the list's submit function swaps the
// game's LUT for ours and keeps the game's weight, so the picture is graded exactly as stock apart from the hue and saturation. Outside the
// map nothing is swapped, and the list is rebuilt every frame, so nothing needs restoring. Parts are found by byte signature; a part that is
// missing leaves hue and saturation off (a warning in the log) and nothing else. Notes: docs/reverse-engineering/cartographer/50-colour-grading.md.
#pragma once

#include "engine/portal.h"
#include "lutgrade.h"

#include <D2RLPlugin/api.h>

#include <vector>

namespace carto {

// Finds the parts and installs the two hooks. Call once at load. Returns whether the swap is live; `parts` gets the part statuses.
auto InstallLutGrade(const D2RL::PluginContext* context, const std::vector<SignatureOverride>& overrides, std::vector<PartStatus>* parts) -> bool;

// The grade to apply to the level's picture now, or nullptr for none (the tileset's own colours). Any thread; cheap.
void LutGradeSet(const cartographer::lutgrade::Grade* grade) noexcept;

// True when the swap hooks are installed.
auto LutGradeLive() noexcept -> bool;

}  // namespace carto
