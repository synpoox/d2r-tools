#include "void_room.h"

namespace cartographer::maze {

std::vector<uint8_t> voidRoomDs1()
{
	constexpr int cells = 13 * 13;
	std::vector<uint8_t> out;
	auto put = [&out](uint32_t v) {
		for (int i = 0; i < 4; ++i) out.push_back(static_cast<uint8_t>(v >> (8 * i)));
	};
	put(18); // version
	put(12); // width - 1
	put(12); // height - 1
	put(1);  // act - 1 (Act 2)
	put(0);  // tag type: no tag layer, no groups
	put(0);  // no editor file list
	put(1);  // wall layers
	put(1);  // floor layers
	for (int layer = 0; layer < 4; ++layer) // wall, its orientation, floor, shadow
		for (int c = 0; c < cells; ++c) put(layer == 2 && (c == 0 || c == cells - 1) ? 0x800200C2u : 0u);
	put(0); // objects
	put(0); // npc paths
	return out;
}

std::string voidRoomPreset()
{
	return R"({"dependencies": {"particles": [], "models": [], "skeletons": [], "animations": [], "textures": [], "physics": [], )"
	       R"("json": [{"path": "data/hd/env/biome/act2_arcane.json"}], "variantdata": [], "objecteffects": [], "other": []}, )"
	       R"("type": "Preset", "name": "preset", "entities": [], "terrain": {}, "biomeFilename": "data/hd/env/biome/act2_arcane.json", )"
	       R"("perTileBiomeOverrides": [], "specialTiles": {}})";
}

} // namespace cartographer::maze
