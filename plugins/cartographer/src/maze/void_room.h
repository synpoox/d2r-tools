// The Arcane void room (issue #566): a 12-tile maze room with no floor and no walls. A grid cell with no room blocks missiles (the collision
// test gives 0x27 there, block-missile 0x4 included); the blank tiles of a room whose lvlprest row has FillBlanks block walking only, which is how
// the void inside Arcane's own pieces lets missiles and Teleport over it. The plan fills the free cells of an Arcane map's box with this room.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace cartographer::maze {

// Resource paths: the DS1 is install.ps1's lvlprest File1 (core/tables.h ArcaneVoidFile) under data/global/tiles/, and the HD preset is found by the
// same stem under data/hd/env/preset/.
inline constexpr const char *kVoidRoomDs1Path = "data/global/tiles/d2rloader/cartographer/arcanevoid.ds1";
inline constexpr const char *kVoidRoomPresetPath = "data/hd/env/preset/d2rloader/cartographer/arcanevoid.json";

// A version 18 DS1 of 13 x 13 cells (12 tiles plus the shared edge), act 2, one wall and one floor layer, all blank but the two hidden corner
// floor tiles every Arcane piece has (cells 0 and 168: c2 00 02 80), no objects and no paths.
std::vector<uint8_t> voidRoomDs1();

// An HD preset with no entities and no terrain model, on the Arcane biome: the room draws nothing, like the void around Arcane's platforms.
std::string voidRoomPreset();

} // namespace cartographer::maze
