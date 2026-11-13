#include "mappanel.h"

#include <cctype>

namespace cartographer::mappanel {

namespace {

struct Known {
	const char *stat;
	const char *text; // the game's wording with the number in: "%+d" is signed, "%%" a percent sign
};

// The fallback when the game's own text for a stat cannot be read. Anything else is shown with a tidied stat name and its plain number.
const Known kKnown[] = {
	{"item_maxhp_percent", "%+d%% Life"},
	{"damagepercent", "%+d%% Damage"},
	{"fireresist", "%+d%% Fire Resistance"},
	{"coldresist", "%+d%% Cold Resistance"},
	{"lightresist", "%+d%% Lightning Resistance"},
	{"poisonresist", "%+d%% Poison Resistance"},
	{"magicresist", "%+d%% Magic Resistance"},
	{"hpregen", "%+d%% Life Regeneration"},
	{"toblock", "%+d%% Chance to Block"},
	{"velocitypercent", "%+d%% Movement Speed"},
	{"item_fastermovevelocity", "%+d%% Faster Run/Walk"},
	{"item_fasterattackrate", "%+d%% Increased Attack Speed"},
	{"item_fastercastrate", "%+d%% Faster Cast Rate"},
	{"item_fastergethitrate", "%+d%% Faster Hit Recovery"},
	{"item_magicbonus", "%+d%% Better Chance of Getting Magic Items"},
	{"item_goldbonus", "%+d%% Extra Gold from Monsters"},
	{"item_armor_percent", "%+d%% Enhanced Defense"},
	{"item_maxdamage_percent", "%+d%% Enhanced Damage"},
	{"item_lifeleech", "%+d%% Life Stolen per Hit"},
	{"item_manaleech", "%+d%% Mana Stolen per Hit"},
	{"item_absorbfire_percent", "Fire Absorb %+d%%"},
	{"item_absorbcold_percent", "Cold Absorb %+d%%"},
	{"item_absorblight_percent", "Lightning Absorb %+d%%"},
	{"maxfireresist", "%+d%% to Maximum Fire Resistance"},
	{"maxcoldresist", "%+d%% to Maximum Cold Resistance"},
	{"maxlightresist", "%+d%% to Maximum Lightning Resistance"},
	{"maxpoisonresist", "%+d%% to Maximum Poison Resistance"},
	{"strength", "%+d to Strength"},
	{"dexterity", "%+d to Dexterity"},
	{"vitality", "%+d to Vitality"},
	{"energy", "%+d to Energy"},
	{"maxhp", "%+d to Life"},
	{"maxmana", "%+d to Mana"},
	{"armorclass", "%+d Defense"},
	{"tohit", "%+d to Attack Rating"},
};

const Known *find(const std::string &stat)
{
	for (const Known &k : kKnown)
		if (stat == k.stat) return &k;
	return nullptr;
}

std::string bluePropertyLine(const Stat &stat)
{
	return std::string(kBlue) + statLine(stat);
}

} // namespace

std::string formatGameText(const std::string &text, int value)
{
	std::string out;
	bool number = false;
	for (size_t i = 0; i < text.size(); ++i) {
		if (text[i] != '%') {
			out.push_back(text[i]);
			continue;
		}
		const std::string rest = text.substr(i + 1, 2);
		if (rest.compare(0, 1, "%") == 0) {
			out.push_back('%');
			i += 1;
		} else if (rest == "+d" || rest == "+i") {
			if (value >= 0) out.push_back('+');
			out += std::to_string(value);
			number = true;
			i += 2;
		} else if (rest.compare(0, 1, "d") == 0 || rest.compare(0, 1, "i") == 0) {
			if (value < 0 && !out.empty() && out.back() == '+') out.pop_back();  // "+%d" with a negative value
			out += std::to_string(value);
			number = true;
			i += 1;
		} else {
			out.push_back('%');
		}
	}
	return number ? out : std::string();
}

std::string statLabel(const std::string &stat)
{
	std::string out;
	for (char c : stat) out.push_back(c == '_' ? ' ' : c);
	if (out.rfind("item ", 0) == 0) out.erase(0, 5);
	if (!out.empty()) out[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(out[0])));
	return out;
}

std::string statLine(const Stat &stat)
{
	if (!stat.gameText.empty()) {
		std::string text = formatGameText(stat.gameText, stat.value);
		if (!text.empty()) return text;
	}
	if (const Known *k = find(stat.name)) return formatGameText(k->text, stat.value);
	std::string out;
	if (stat.value >= 0) out += '+';
	out += std::to_string(stat.value);
	out += ' ';
	out += statLabel(stat.name);
	return out;
}

std::string panelText(int monsterLevel, const std::vector<Stat> &stats)
{
	std::string out;
	const auto line = [&out](const std::string &text) {
		if (!out.empty()) out += '\n';
		out += text;
	};
	if (monsterLevel > 0) line(std::string(kGold) + "Monster level " + std::to_string(monsterLevel));
	for (const bool monsters : {true, false}) {
		bool header = false;
		for (const Stat &s : stats) {
			if (s.monsters != monsters) continue;
			if (!header) line(std::string(kGold) + (monsters ? "Monsters have" : "Players have"));
			header = true;
			line(bluePropertyLine(s));
		}
	}
	return out;
}

bool hiddenAfter(bool visible, bool succeeded, bool wasHidden) {
	if (!succeeded) {
		return wasHidden;
	}
	return !visible;
}

} // namespace cartographer::mappanel
