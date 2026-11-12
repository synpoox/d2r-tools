// AutoCheck (issue #173): with AutoCheck=1 in cartographer.txt, a Hell game runs the Cartographer game check by itself and writes one
// "AUTOCHECK PASS|FAIL <item>" line per item and a final "AUTOCHECK DONE" to the plugin log, so a check needs no screenshots. For test
// slots only. It opens AutoCheckMap through the same code path as using its item (no item needed), waits for the player to be in the
// portal level, logs the arrival, moves the player into the plan's boss room so the map Boss spawns, and reads back what the map's
// monster settings and stat changes did. Off (the default) it does nothing, and nothing here runs for a player.
//
// Fails open: a part that is not found fails its own item and the rest go on. Two steps have no native call yet (walking into the
// portal and leaving the game); they are left to whoever runs the check and logged as such (docs/reverse-engineering/cartographer.md
// section 16). The pure parts (checklist, result lines, setting, stand point) are in src/autocheck.
#pragma once

#include "engine/portal.h"
#include "engine/scan.h"

#include <D2RLPlugin/api.h>

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace carto {

// Finds the native parts AutoCheck calls (the unit mover and the two room parts) by signature. Never throws. Call after InstallPortal's parts are resolved.
void InstallAutoCheck(const D2RL::PluginContext* context, const std::vector<SignatureOverride>& overrides, const CodeRange* ranges, int count);

// The tables (settings and maps). Any thread, any time.
void AutoCheckSetTables(const std::shared_ptr<const Tables>& tables);

// The local player is ready in a new game / the game was left. Cheap when AutoCheck is off.
void AutoCheckGameEntered();
void AutoCheckGameLeft();

// The portal's player tick (game thread, after the game's own tick): runs the next step. Cheap when AutoCheck is off.
void AutoCheckTick(void* game, void* unit, uint32_t localPlayer) noexcept;

// event_apply.cpp: the map Boss was (or was not) spawned. `asSuper`: spawned as a superunique; `problem`: why not, when not.
void AutoCheckNoteBoss(const std::string& name, bool spawned, bool asSuper, const std::string& problem) noexcept;

}  // namespace carto
