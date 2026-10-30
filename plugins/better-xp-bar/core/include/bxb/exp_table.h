// Level thresholds: the experience at which each character level starts.
//
// Pure C++, no game or Windows dependency. The numbers are never written into this code: the plugin reads them from
// the running game's experience table (D2RLoader DataTableService) and hands them over here; the unit tests use
// synthetic tables.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace bxb {

class ExpTable {
public:
	ExpTable() = default;

	// levelStarts[i] = experience at which level i+1 starts (levelStarts[0] is level 1, normally 0).
	// The table's size is the maximum level. Fails (returns false, `error` says why) when it is empty or decreasing.
	auto Assign(std::vector<uint64_t> levelStarts, std::string* error = nullptr) -> bool;

	[[nodiscard]] auto Valid() const noexcept -> bool { return !starts_.empty(); }
	[[nodiscard]] auto MaxLevel() const noexcept -> uint32_t { return static_cast<uint32_t>(starts_.size()); }

	// Experience at which `level` starts (clamped to 1..MaxLevel).
	[[nodiscard]] auto LevelStart(uint32_t level) const noexcept -> uint64_t;

	// The level a character with `xp` experience has (1..MaxLevel). 0 when the table is not valid.
	[[nodiscard]] auto LevelForXp(uint64_t xp) const noexcept -> uint32_t;

	// Fraction of `level` done at `xp`, in [0, 1]. Level % = (xp - start[L]) / (start[L+1] - start[L]).
	// At the maximum level (no next threshold) it is 1.
	[[nodiscard]] auto Progress(uint64_t xp, uint32_t level) const noexcept -> double;

	// Experience still needed for level + 1. 0 at the maximum level or past the threshold.
	[[nodiscard]] auto Remaining(uint64_t xp, uint32_t level) const noexcept -> uint64_t;

	// Position on a continuous level scale: level + Progress (for example 80.5 = half way through level 80).
	// Differences between two positions are "level-equivalents" (half of level 80 plus a quarter of 81 = 0.75).
	[[nodiscard]] auto Position(uint64_t xp) const noexcept -> double;

private:
	std::vector<uint64_t> starts_;
};

}  // namespace bxb
