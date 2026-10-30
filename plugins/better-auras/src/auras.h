// Better Auras core: the active-aura manager and the five engine hooks (auras.cpp).
//
// How it uses the engine (build 93847, knowledge/d2r-93847-addresses.md "Auras"):
//  - vanilla keeps the right-skill aura alive with unit timer event 8 keyed -1; selecting another right skill
//    removes that event and the aura's state list (0x438a70);
//  - the same event can be keyed with a skill id instead (0x43b2b0 rightSkill = 0). Such per-skill timers are not
//    touched by a skill switch. A toggled aura is started the vanilla way, then moved to a per-skill timer;
//  - drain and reservation run in the upkeep tick (event 8 handler 0x437460); the reservation is a negative max
//    life/mana stat inside the aura's own state list, so whatever removes the aura (toggle off, death, game end)
//    also releases the reservation.
//
// All state lives on the server (the game host in TCP/IP): the hooked functions are D2Game code. Clients see the
// result through the normal state and stat sync.
#pragma once

#include <D2RLPlugin/api.h>

#include <cstddef>

namespace auras {

enum Feature : int { Toggle, Drain, Reserve, Decouple, FeatureCount };

auto FeatureWord(Feature f) noexcept -> const char*;

// Installs the hooks (once) and turns the feature on. False (report says why) when a hook was refused.
auto Enable(const D2RL::PluginContext* context, Feature f, char* report, size_t reportSize) noexcept -> bool;

// One line per active aura for the status command ("" when none). Called from the console thread.
void Describe(void (*line)(const char* text, void* user), void* user) noexcept;

// Counters for the status command: toggles on/off, refusals, drain stops, re-arms after a skill switch.
void Counters(char* out, size_t size) noexcept;

}  // namespace auras
