#undef NDEBUG  // asserts are the test, also in Release
#include "dps_logic.h"

#include <cassert>
#include <cmath>
#include <cstring>

int main() {
	using namespace dps;

	const Settings defaults = ParseSettings(nullptr);
	assert(defaults.enabled && defaults.windowSeconds == 5.0f && defaults.offsetY == 830.0f);
	assert(!ParseSettings("enabled = false\n").enabled);
	assert(ParseSettings("enabled = maybe\n").enabled);  // a bad value keeps the default
	const Settings s = ParseSettings("# c\r\n[dps-counter]\r\nwindow_seconds = 10\noffset_x=-40\noffset_y = nope\ntext_size = 500\n");
	assert(s.windowSeconds == 10.0f && s.offsetX == -40.0f);
	assert(s.offsetY == 830.0f && s.textSize == 22.0f);  // bad values keep the defaults

	Window w(5.0f);
	assert(w.PerSecond(0.0) == 0.0);
	w.Add(100.0, 1000);
	assert(w.PerSecond(100.05) == 1000.0);  // under a second of fight: divided by one second
	w.Add(101.0, 1000);
	w.Add(102.0, 1000);
	assert(std::fabs(w.PerSecond(102.0) - 3000.0 / 2.1) < 1.0);  // a fight shorter than the window: over its own length
	w.Add(104.0, 2000);
	assert(std::fabs(w.PerSecond(104.95) - 1000.0) < 1.0);  // 5000 over the full 5 s window
	assert(std::fabs(w.PerSecond(105.5) - 800.0) < 1.0);    // the hit at 100 s has left the window
	assert(w.PerSecond(110.0) == 0.0);                      // a whole window with no damage
	w.Add(200.0, 500);
	assert(w.PerSecond(200.0) == 500.0);  // a new fight starts over
	w.Add(200.0, -5);
	assert(w.PerSecond(200.0) == 500.0);  // healing never counts

	uint8_t record[0x150] {};
	assert(DamageTaken(record) == 0);
	const int32_t hit = 1234 << 8, heal = -256;
	std::memcpy(record + 0x134, &hit, sizeof(hit));
	assert(DamageTaken(record) == 0);  // a miss: result bit 0 clear
	record[4] = 3;
	assert(DamageTaken(record) == 1234 << 8);
	std::memcpy(record + 0x134, &heal, sizeof(heal));
	assert(DamageTaken(record) == 0);

	char text[32];
	FormatDps(0.4, text, sizeof(text));
	assert(text[0] == '\0');
	FormatDps(999.0, text, sizeof(text));
	assert(std::strcmp(text, "DPS: 999") == 0);
	FormatDps(1234567.0, text, sizeof(text));
	assert(std::strcmp(text, "DPS: 1,234,567") == 0);

	Pattern p {};
	assert(ParsePattern("40 53 ?? E8", &p) && p.size == 4 && p.wild[2]);
	assert(!ParsePattern("?? 53", &p) && !ParsePattern("4", &p));
	const uint8_t data[] = {0x00, 0x40, 0x53, 0x11, 0xE8, 0x40, 0x53, 0x22, 0xE9};
	size_t at = 0;
	assert(ParsePattern("40 53 ?? E8", &p) && FindPattern(data, sizeof(data), p, &at) == 1 && at == 1);
	return 0;
}
