#include "mapquality.h"

namespace cartographer::mapquality {

int mapQuality(int rolled, bool isMap)
{
	return isMap && rolled == Magic ? Normal : rolled;
}

bool useNormalFallback(bool isMap)
{
	return isMap;
}

std::string rollCounts(const std::uint32_t (&byQuality)[10], std::uint32_t magicMadeNormal)
{
	static const char* const names[] = {"low", "normal", "superior", "magic", "set", "rare", "unique", "crafted"};
	std::uint32_t total = 0;
	std::string out;
	for (int quality = Low; quality <= Crafted; ++quality) {
		total += byQuality[quality];
		out += ", " + std::to_string(byQuality[quality]) + " " + names[quality - Low];
		if (quality == Normal) {
			out += " (" + std::to_string(magicMadeNormal) + " of them rolled magic)";
		}
	}
	return std::to_string(total) + " map rolls: " + out.substr(2);
}

}  // namespace cartographer::mapquality
