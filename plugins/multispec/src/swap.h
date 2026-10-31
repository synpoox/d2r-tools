// The swap algorithm (docs/design.md section 4), over an abstract engine so tests can run it with a fake one.
//
// The real engine (spec_engine.cpp) calls D2R's own functions: its skills-only respec for Reset and the add-skill
// packet handler's checks plus the spend-one-point function for CanSpend / Spend. Everything here runs inside one
// game-thread callback, so D2R never saves a half-swapped character.
#pragma once

#include "spec_model.h"

#include <cstddef>
#include <cstdint>

namespace ms {

class Engine {
public:
	virtual ~Engine() = default;
	// The live character (class skill list, hard levels, unspent points).
	virtual auto Read(LiveChar* out) noexcept -> bool = 0;
	// Every class skill back to 0, its points unspent (D2R's respec).
	virtual void Reset() noexcept = 0;
	// D2R would let the player put one more point into class skill `index` now (class, level, prerequisites, stat
	// requirements, max level, a cost of exactly one point).
	virtual auto CanSpend(int32_t index) noexcept -> bool = 0;
	// Spend one point on class skill `index` the way D2R does. False when D2R refused.
	virtual auto Spend(int32_t index) noexcept -> bool = 0;
	// Puts `slot` on the left or right mouse button the way D2R does; nothing when the character cannot use that skill.
	virtual void Select(bool left, const SkillSlot& slot) noexcept = 0;
};

class Store {
public:
	virtual ~Store() = default;
	virtual auto Write(const CharSpecs& specs) noexcept -> bool = 0;
};

enum class SwapResult {
	Done,        // the character has spec `to`
	Refused,     // nothing changed (bad spec number, invalid spec, file not writable, ...)
	RolledBack,  // spec `to` could not be applied; the old spec is back
	LeftReset,   // neither could be applied: every point is unspent, no spec lost; the file stays in recovery
};
auto SwapResultName(SwapResult r) noexcept -> const char*;

struct SwapReport {
	SwapResult result;
	int32_t from;
	int32_t to;
	int32_t pointsUsed;  // in the spec the character has now
	int32_t unspent;
	char detail[512];    // why it was refused or failed
};

// Swaps `file` (already reconciled with `live`) from its active spec to spec `to` (1-based). `configCount` limits the
// usable spec numbers.
auto Swap(Engine& engine, Store& store, CharSpecs* file, const LiveChar& live, int32_t to, int32_t configCount,
          SwapReport* report) noexcept -> SwapResult;

// Applies `target` levels to the live character: Reset, then replay point by point. Exposed for tests.
auto ApplyLevels(Engine& engine, int32_t total, const uint8_t* target, int32_t count, char* detail, size_t size) noexcept
	-> bool;

}  // namespace ms
