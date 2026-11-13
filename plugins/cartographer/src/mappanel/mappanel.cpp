#include "mappanel.h"

#include <cctype>

namespace cartographer::mappanel {

namespace {

struct Known {
	const char *stat;
	const char *label;
	bool percent;
};

// Stat names the built-in pool and the examples use. Anything else is shown with a tidied stat name and its plain number.
const Known kKnown[] = {
	{"item_maxhp_percent", "Life", true},
	{"damagepercent", "Damage", true},
	{"fireresist", "Fire Resistance", true},
	{"coldresist", "Cold Resistance", true},
	{"lightresist", "Lightning Resistance", true},
	{"poisonresist", "Poison Resistance", true},
	{"magicresist", "Magic Resistance", true},
	{"hpregen", "Life Regeneration", true},
	{"toblock", "Chance to Block", true},
	{"velocitypercent", "Movement Speed", true},
};

const Known *find(const std::string &stat)
{
	for (const Known &k : kKnown)
		if (stat == k.stat) return &k;
	return nullptr;
}

} // namespace

std::string statLabel(const std::string &stat)
{
	if (const Known *k = find(stat)) return k->label;
	std::string out;
	for (char c : stat) out.push_back(c == '_' ? ' ' : c);
	if (out.rfind("item ", 0) == 0) out.erase(0, 5);
	if (!out.empty()) out[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(out[0])));
	return out;
}

std::string statLine(const Stat &stat)
{
	const Known *k = find(stat.name);
	const bool percent = k != nullptr && k->percent;
	std::string out = stat.monsters ? "Monsters: " : "You: ";
	if (stat.value >= 0) out += '+';
	out += std::to_string(stat.value);
	if (percent) out += '%';
	out += ' ';
	out += statLabel(stat.name);
	return out;
}

std::string panelText(const std::string &mapName, int monsterLevel, const std::vector<Stat> &stats)
{
	std::string out = mapName.empty() ? "Map" : mapName;
	if (monsterLevel > 0) out += "\nMonster level " + std::to_string(monsterLevel);
	for (const Stat &s : stats) out += "\n" + statLine(s);
	return out;
}

} // namespace cartographer::mappanel
