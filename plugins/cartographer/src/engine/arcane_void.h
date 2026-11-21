// Serves the Arcane void room's DS1 and HD preset (maze/void_room.h, issue #566) through the SDK's resource service while the plugin loads.
// Without the service, or without install.ps1's lvlprest row, Arcane maps build as before: cells with no room, which block missiles.
#pragma once

#include <D2RLPlugin/api.h>

namespace carto {

void InstallArcaneVoid(const D2RL::PluginContext* context);

}  // namespace carto
