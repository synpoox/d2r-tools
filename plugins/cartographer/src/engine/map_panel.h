// The map panel (issue #308): while the player stands inside a map, a plugin panel down the right side of the screen, under the
// game's own area and game info, in place of the purple terror zone text (hidden while the panel shows), shows the map's name, monster level and rolled
// properties. Outside a map the panel is closed and the game's own HUD is untouched. The panel is a layout the plugin registers
// through the D2RLoader Panel and Resource services (cartographer/map-panel); its text is set through the game's own FindPanel,
// FindChild and SetText (found by byte signature, only called), on the UI thread. A part or service that is missing, or a fault,
// turns the panel off and nothing else. The game's own TerrorZoneInfoText widget and the Overlay service were tried first and
// could not be made to show (docs/reverse-engineering/cartographer/22-map-panel.md); pure text part: src/mappanel. Only the host
// (or single player) knows the properties, so only it shows the panel.
#pragma once

#include "engine/portal.h"

#include <D2RLPlugin/api.h>

#include <string>
#include <vector>

namespace carto {

// Finds the three UI parts, registers the panel layout and the frame callback that opens it. Fails open: with a part or service
// missing the map has no panel. `parts` gets the parts added by this call. Returns whether the panel can show.
auto InstallMapPanel(const D2RL::PluginContext* context, const std::vector<SignatureOverride>& overrides, std::vector<PartStatus>* parts) -> bool;

// Closes the panel and unregisters the callback and the panel.
void UnloadMapPanel() noexcept;

// A map opened: `text` is the panel text, `slotLevel` the level id the map is built in. Any thread.
void MapPanelMapOpened(const std::string& text, int slotLevel);

// The local player's tick (game thread): whether the player stands in the open map's level.
void MapPanelPlayerTick(void* player) noexcept;

// The game was left: forget the map.
void MapPanelGameLeft() noexcept;

auto MapPanelPartStatuses() -> std::vector<PartStatus>;
auto MapPanelSummary() -> std::string;

}  // namespace carto
