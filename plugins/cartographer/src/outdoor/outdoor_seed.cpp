#include "outdoor_seed.h"

#include "rng.h"

namespace cartographer::outdoor {

SlotRngSeed slotRngSeedFor(uint64_t mapSeed, int32_t levelId)
{
	const uint64_t mixed = layout::splitmix64(mapSeed ^ (static_cast<uint64_t>(static_cast<uint32_t>(levelId)) << 32) ^ 0x4f5554444f4f52ULL);
	SlotRngSeed s;
	s.first = static_cast<int32_t>(static_cast<uint32_t>(mixed ^ (mixed >> 32)));
	return s;
}

} // namespace cartographer::outdoor
