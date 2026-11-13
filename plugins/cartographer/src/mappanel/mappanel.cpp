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

} // namespace

std::string formatGameText(const std::string &text, int value)
{
	std::string out;
	int numbers = 0;
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
			++numbers;
			i += 2;
		} else if (rest.compare(0, 1, "d") == 0 || rest.compare(0, 1, "i") == 0) {
			if (value < 0 && !out.empty() && out.back() == '+') out.pop_back();  // "+%d" with a negative value
			out += std::to_string(value);
			++numbers;
			i += 1;
		} else {
			return std::string();  // %s or another conversion: the game fills it from something this panel does not have
		}
	}
	return numbers == 1 ? out : std::string();  // two numbers would show the one value twice
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
	const Known *known = find(stat.name);
	const std::string *sources[] = {(stat.value < 0 && !stat.gameTextNeg.empty()) ? &stat.gameTextNeg : nullptr, &stat.gameText};
	for (const std::string *source : sources) {
		if (source == nullptr || source->empty()) continue;
		std::string text = formatGameText(*source, stat.value);
		// A percent stat whose game text lost its percent sign (the game adds it by descfunc): the fallback has it.
		const bool losesPercent = known != nullptr && std::string(known->text).find("%%") != std::string::npos && text.find('%') == std::string::npos;
		if (!text.empty() && !losesPercent) return text;
	}
	if (known != nullptr) return formatGameText(known->text, stat.value);
	std::string out;
	if (stat.value >= 0) out += '+';
	out += std::to_string(stat.value);
	out += ' ';
	out += statLabel(stat.name);
	return out;
}

std::string densityLine(int density)
{
	return density == 0 ? std::string() : formatGameText("%+d%% Monster Density", density);
}

std::vector<Stat> mergedStats(const std::vector<Stat> &stats)
{
	std::vector<Stat> out;
	for (const Stat &s : stats) {
		bool merged = false;
		for (Stat &o : out) {
			if (o.monsters == s.monsters && o.name == s.name) {
				o.value += s.value;
				merged = true;
				break;
			}
		}
		if (!merged) out.push_back(s);
	}
	return out;
}

PanelText panelText(int monsterLevel, const std::vector<Stat> &rolled, int density)
{
	const std::vector<Stat> stats = mergedStats(rolled);
	PanelText out;
	bool any = false;
	const auto line = [&out, &any](const std::string &header, const std::string &property) {
		if (any) {
			out.gold += '\n';
			out.blue += '\n';
		}
		any = true;
		out.gold += header;
		out.blue += property;
	};
	if (monsterLevel > 0) line("Monster level " + std::to_string(monsterLevel), "");
	for (const bool monsters : {true, false}) {
		bool header = false;
		for (const Stat &s : stats) {
			if (s.monsters != monsters) continue;
			if (!header) line(monsters ? "Monsters have" : "Players have", "");
			header = true;
			line("", statLine(s));
		}
		if (monsters && density != 0) {
			if (!header) line("Monsters have", "");
			line("", densityLine(density));
		}
	}
	return out;
}

int lineCount(const std::string &text)
{
	if (text.empty()) return 0;
	int lines = 1;
	for (char c : text)
		if (c == '\n') ++lines;
	return lines;
}

int padLines(int textBottom, int panelTop, int lineUnits, int maxLines)
{
	if (textBottom <= panelTop || lineUnits <= 0) {
		return 0;
	}
	const int lines = (textBottom - panelTop + lineUnits - 1) / lineUnits;
	return lines < maxLines ? lines : maxLines;
}

} // namespace cartographer::mappanel
