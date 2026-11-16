#include "openextra.h"

#include <algorithm>

namespace cartographer::openextra {

int pickExtraMap(const std::vector<Candidate>& rows, const std::vector<int>& takenSlotLevels, int sourceSlotLevel, int charLevel, uint64_t seed) {
	std::vector<int> eligible;
	for (size_t i = 0; i < rows.size(); ++i) {
		const Candidate& row = rows[i];
		if (!row.usable || row.slotLevel <= 0 || row.slotLevel == sourceSlotLevel || row.minCharLevel > charLevel) {
			continue;
		}
		if (std::find(takenSlotLevels.begin(), takenSlotLevels.end(), row.slotLevel) != takenSlotLevels.end()) {
			continue;
		}
		eligible.push_back(static_cast<int>(i));
	}
	if (eligible.empty()) {
		return -1;
	}
	return eligible[seed % eligible.size()];
}

}  // namespace cartographer::openextra
