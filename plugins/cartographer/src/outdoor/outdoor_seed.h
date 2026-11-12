// The level RNG seed of an outdoor map's slot level, as a pure function of the map's layout seed (issue #146).
//
// The host builds the slot level on its server DRLG and every client on its own, and a remote client only learns the layout seed
// (map sync). The plan already depends on (tileset, shape, size, seed, origin, limits, options) only. The one other input of the
// level build is the level RNG, which draws each piece's DS1 file when the plan leaves it to the engine (pinFiles = false). The
// game's own seed for it is int32(drlg seed base) + level id, which comes from the act seed; this derives it from the map's seed
// instead, so the built level depends on nothing the client could not know. Evidence: docs/reverse-engineering/cartographer.md section 8a.
#pragma once
#include <cstdint>

namespace cartographer::outdoor {

// The pair the engine stores at level +0x1e4 / +0x1e8 (the reseed 0x367110 stores `edx` and 0x29a).
struct SlotRngSeed {
	int32_t first = 0;
	int32_t second = 0x29a;
};

// splitmix64 over the map seed and the level id, folded to 32 bits. Same inputs, same pair, on every machine.
SlotRngSeed slotRngSeedFor(uint64_t mapSeed, int32_t levelId);

} // namespace cartographer::outdoor
