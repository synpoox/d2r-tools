#undef NDEBUG  // asserts are the test, also in Release
#include "counter_logic.h"

#include <cassert>
#include <cstring>

namespace {
// An item code as the SDK packs it: first character lowest, padded with spaces.
auto Code(const char* s) -> uint32_t {
	uint32_t v = 0;
	bool ended = false;
	for (int i = 0; i < 4; ++i) {
		ended = ended || s[i] == 0;
		v |= static_cast<uint32_t>(static_cast<uint8_t>(ended ? ' ' : s[i])) << (8 * i);
	}
	return v;
}
}  // namespace

int main() {
	using namespace pc;

	auto sprite = [](const char* code) { return Icons[KindOf(Code(code)) - 1].sprite; };
	assert(std::strcmp(sprite("hp5"), "potion/greater_healing_potion") == 0 && std::strcmp(sprite("hpf"), "potion/full_healing_potion") == 0);
	assert(std::strcmp(sprite("mp1"), "potion/lesser_mana_potion") == 0 && std::strcmp(sprite("rvl"), "potion/full_rejuv_potion") == 0);
	assert(std::strcmp(sprite("yps"), "potion/antidote_potion") == 0 && std::strcmp(sprite("wms"), "potion/thawing_potion") == 0);
	assert(std::strcmp(sprite("tsc"), "scroll/town_portal_scroll") == 0 && std::strcmp(sprite("isc"), "scroll/identify_scroll") == 0);
	assert(KindOf(Code("zzz")) == Other && KindOf(Code("hrb")) == Other && KindOf(Code("hp")) == Other);  // a mod's own item

	assert(RowsOf(4) == 1 && RowsOf(0) == 1 && RowsOf(8) == 2 && RowsOf(12) == 3 && RowsOf(16) == 4 && RowsOf(40) == 4);

	Kind squares[Columns][Rows];
	FillSquares(nullptr, 0, squares);
	assert(squares[0][0] == Empty && CountColumn(squares[3]) == 0);  // empty belt

	// Column 0 full of mixed items (4 rows), column 1 two, column 2 empty, column 3 one; a bad slot is skipped.
	const BeltItem items[] = {{0, Code("hp5")}, {4, Code("mp5")}, {8, Code("tsc")}, {12, Code("zzz")},
	                          {1, Code("mp5")}, {5, Code("mp5")}, {3, Code("rvs")}, {16, Code("hp5")}, {-1, Code("hp5")}};
	FillSquares(items, sizeof(items) / sizeof(items[0]), squares);
	assert(CountColumn(squares[0]) == 4 && CountColumn(squares[1]) == 2 && CountColumn(squares[2]) == 0 && CountColumn(squares[3]) == 1);
	assert(squares[0][0] == KindOf(Code("hp5")) && squares[0][1] == KindOf(Code("mp5")) && squares[0][2] == KindOf(Code("tsc")) &&
	       squares[0][3] == Other);
	assert(squares[1][0] == KindOf(Code("mp5")) && squares[1][2] == Empty && squares[3][0] == KindOf(Code("rvs")));

	Pattern p {};
	assert(ParsePattern("48 8B ?? 05", &p) && p.size == 4 && p.wild[2]);
	assert(!ParsePattern("?? 8B", &p) && !ParsePattern("4", &p));
	const uint8_t code[] = {0x90, 0x48, 0x8B, 0x11, 0x05, 0x48, 0x8B, 0x22, 0x06};
	size_t at = 0;
	assert(ParsePattern("48 8B ?? 05", &p) && FindPattern(code, sizeof(code), p, &at) == 1 && at == 1);
	return 0;
}
