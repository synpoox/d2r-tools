#undef NDEBUG  // asserts are the test, also in Release
#include "bars_logic.h"

#include <cassert>
#include <cmath>

int main() {
	using namespace pb;

	const Settings defaults = ParseSettings(nullptr);
	assert(defaults.hideOrbs && !defaults.below && !defaults.showText && defaults.barWidth == 90.0f);

	const Settings s = ParseSettings(
		"# comment\r\n[player-bars]\r\nhide_orbs = false\r\nposition=\"bottom\"\n  bar_width = 120\n"
		"health_color = \"FF0080\"\nmana_color = \"nope\"\nmana_height = -4\nunknown = 3\noffset_y = 90 # up\n");
	assert(!s.hideOrbs && s.below);
	assert(!ParseSettings("position = \"top\"").below);
	assert(s.barWidth == 120.0f && s.offsetY == 90.0f);
	assert(s.manaHeight == 7.0f);  // bad value keeps the default
	assert(s.healthColor.r == 1.0f && s.healthColor.g == 0.0f && std::fabs(s.healthColor.b - 128.0f / 255.0f) < 0.001f);
	assert(s.manaColor.b == defaults.manaColor.b);

	assert(Fraction(50, 100) == 0.5f);
	assert(Fraction(150, 100) == 1.0f);
	assert(Fraction(-1, 100) == 0.0f);
	assert(Fraction(10, 0) == 0.0f);

	assert(FilledSegments(100, 100) == Segments);
	assert(FilledSegments(50, 100) == Segments / 2);
	assert(FilledSegments(1, 100000) == 1);
	assert(FilledSegments(0, 100) == 0);

	assert(PanelForShift(0) == 0 && PanelForShift(1) == 1 && PanelForShift(2) == 2 && PanelForShift(3) == 0);

	Pattern p {};
	assert(ParsePattern("48 ?? 05", &p) && p.size == 3 && p.wild[1]);
	assert(!ParsePattern("?? 48", &p));
	assert(!ParsePattern("4", &p));
	const uint8_t data[] = {0x00, 0x48, 0x11, 0x05, 0x48, 0x22, 0x05};
	(void)ParsePattern("48 ?? 05", &p);
	size_t at = 0;
	assert(FindPattern(data, sizeof(data), p, &at) == 2 && at == 1);
	(void)ParsePattern("11 05", &p);
	assert(FindPattern(data, sizeof(data), p, &at) == 1 && at == 2);
	return 0;
}
