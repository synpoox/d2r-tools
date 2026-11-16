// cartographer.open-extra.v1: the pick of the second map (issue #485). Pure C++, no engine code: the engine gives one candidate per
// maps.txt row, and this chooses one.
#pragma once

#include <cstdint>
#include <vector>

namespace cartographer::openextra {

// One maps.txt row, in table order. `usable` is false when the row cannot open now (its slot is not installed, refused or steered off).
struct Candidate {
	int slotLevel = -1;
	int minCharLevel = 1;
	bool usable = false;
};

// The index of the row to open, or -1 when none is eligible. Eligible: usable, slotLevel > 0, slotLevel is neither the source's
// nor in takenSlotLevels, and minCharLevel <= charLevel. The pick is eligible[seed % eligible.size()].
int pickExtraMap(const std::vector<Candidate>& rows, const std::vector<int>& takenSlotLevels, int sourceSlotLevel, int charLevel, uint64_t seed);

}  // namespace cartographer::openextra
