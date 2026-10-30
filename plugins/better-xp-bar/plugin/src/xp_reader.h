// Reads the local player's level, experience and class, and the experience table, from the running game.
// Uses only plugin/src/d2r.h (evidence: docs/reverse-engineering/better-xp-bar.md). Call on D2R's UI thread only.
//
// NOT RUN IN GAME YET: every function here "needs live check" (TESTING.md).
#pragma once

#include "bxb/exp_table.h"

#include <D2RLPlugin/api.h>

#include <cstdint>
#include <string>

namespace xp_reader {

// Checks the image bytes of everything used (d2r::Checks) and binds the functions. Report: "ok" or the first mismatch.
auto Install(const D2RL::PluginContext* context, char* report, size_t size) noexcept -> bool;
auto Installed() noexcept -> bool;

struct Reading {
	bool ok = false;        // a player unit exists and the values look sane
	uint32_t level = 0;     // base stat 12
	uint64_t xp = 0;        // stat 13 as unsigned 32-bit
	int32_t classId = -1;
	uint32_t dataFlag = 0;
};

// needs live check: values equal the character screen's.
auto ReadPlayer() noexcept -> Reading;

// Builds the table for (dataFlag, classId) from the game's experience rows. needs live check: logged values.
// `info` gets a one-line description (counts, max level, first thresholds) for the log.
auto ReadTable(uint32_t dataFlag, int32_t classId, bxb::ExpTable* table, std::string* info) -> bool;

}  // namespace xp_reader
