#include "bxb/exp_table.h"

#include <algorithm>

namespace bxb {

auto ExpTable::Assign(std::vector<uint64_t> levelStarts, std::string* error) -> bool {
	if (levelStarts.empty()) {
		if (error != nullptr) {
			*error = "experience table is empty";
		}
		return false;
	}
	for (size_t i = 1; i < levelStarts.size(); ++i) {
		if (levelStarts[i] < levelStarts[i - 1]) {
			if (error != nullptr) {
				*error = "experience table decreases at level " + std::to_string(i + 1);
			}
			return false;
		}
	}
	starts_ = std::move(levelStarts);
	return true;
}

auto ExpTable::LevelStart(uint32_t level) const noexcept -> uint64_t {
	if (starts_.empty()) {
		return 0;
	}
	level = std::clamp<uint32_t>(level, 1, MaxLevel());
	return starts_[level - 1];
}

auto ExpTable::LevelForXp(uint64_t xp) const noexcept -> uint32_t {
	if (starts_.empty()) {
		return 0;
	}
	// last level whose start is <= xp
	const auto it = std::upper_bound(starts_.begin(), starts_.end(), xp);
	const auto n = static_cast<uint32_t>(it - starts_.begin());
	return std::max<uint32_t>(n, 1);
}

auto ExpTable::Progress(uint64_t xp, uint32_t level) const noexcept -> double {
	if (starts_.empty() || level >= MaxLevel()) {
		return starts_.empty() ? 0.0 : 1.0;
	}
	level = std::max<uint32_t>(level, 1);
	const uint64_t lo = starts_[level - 1];
	const uint64_t hi = starts_[level];
	if (hi <= lo) {
		return 1.0;  // zero-width level: complete as soon as it is reached
	}
	if (xp <= lo) {
		return 0.0;
	}
	if (xp >= hi) {
		return 1.0;
	}
	return static_cast<double>(xp - lo) / static_cast<double>(hi - lo);
}

auto ExpTable::Remaining(uint64_t xp, uint32_t level) const noexcept -> uint64_t {
	if (starts_.empty() || level == 0 || level >= MaxLevel()) {
		return 0;
	}
	const uint64_t next = starts_[level];
	return xp >= next ? 0 : next - xp;
}

auto ExpTable::Position(uint64_t xp) const noexcept -> double {
	const uint32_t level = LevelForXp(xp);
	if (level == 0) {
		return 0.0;
	}
	if (level >= MaxLevel()) {
		return static_cast<double>(MaxLevel());
	}
	return static_cast<double>(level) + Progress(xp, level);
}

}  // namespace bxb
