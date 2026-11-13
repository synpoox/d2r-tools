// The quality a map item may have. Maps are normal, rare or unique: a map the game rolls as magic becomes normal.
// Pure C++ (no engine, no SDK). The hook that applies it: src/engine/map_quality.cpp; notes:
// docs/reverse-engineering/cartographer/23-map-quality.md.
#pragma once

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

}  // namespace cartographer::mapquality
