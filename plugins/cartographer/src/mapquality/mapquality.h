// The quality a map item may have. Maps are normal, rare or unique: a map the game rolls as magic becomes normal.
// Pure C++ (no engine, no SDK). The hook that applies it: src/engine/map_quality.cpp; notes:
// docs/reverse-engineering/cartographer/26-map-quality.md.
#pragma once

#include <cstdint>
#include <string>

namespace cartographer::mapquality {

// The game's item quality numbers (the value the quality roll returns and an item's quality field holds).
inline constexpr int Low = 1;
inline constexpr int Normal = 2;
inline constexpr int Superior = 3;
inline constexpr int Magic = 4;
inline constexpr int Set = 5;
inline constexpr int Rare = 6;
inline constexpr int Unique = 7;
inline constexpr int Crafted = 8;

// The quality a roll of `rolled` gives for an item: a map that rolled magic is normal; every other roll, and every other
// item, keeps its own quality.
int mapQuality(int rolled, bool isMap);

// Whether the game's magic fallback (taken when a set, rare or unique maker fails; it forces quality 4) must be replaced by the
// normal one for this item: true for a map, false for everything else.
bool useNormalFallback(bool isMap);

// The map rolls the item maker has seen, by the quality they ended with (index = the quality number, 0 and 9 unused), as one
// line that adds up: every quality is named and the total is the sum, so the line shows whether any roll went unseen.
std::string rollCounts(const std::uint32_t (&byQuality)[10], std::uint32_t magicMadeNormal);

}  // namespace cartographer::mapquality
