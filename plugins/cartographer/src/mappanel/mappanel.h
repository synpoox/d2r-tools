// Cartographer map panel text: what the side-of-screen panel says while the player is inside a map (the map's
// monster level and the rolled properties, grouped like item properties). Pure C++ on plain structs: no engine, no SDK. The engine part
// (src/engine/map_panel.cpp) puts the text into the plugin's panel.
#pragma once
#include <string>
#include <vector>

namespace cartographer::mappanel {

// One rolled property as the panel shows it.
struct Stat {
	bool monsters = true; // false: it is on the player
	std::string name;     // an itemstatcost Stat name
	int value = 0;
	std::string gameText; // the game's own text for the stat ("+%d%% Faster Run/Walk"), "" when it could not be read
	std::string gameTextNeg; // the game's text for a negative value (descstrneg), "" when none: gameText is used
};

// The panel text in two colours. The panel's text widget prints the game's colour codes ("ÿc3") as plain characters (seen in game,
// build 93847, D2RLoader 1.3.1-beta), so the colour comes from two text boxes on top of each other: `gold` has the header lines
// and `blue` the property lines, each with an empty line where the other has text, so every line lands in the same place.
struct PanelText {
	std::string gold;
	std::string blue;
	bool empty() const { return gold.empty() && blue.empty(); }
};

// A game string with the value put in: "%d" and "%i" are the number, "%+d" the number with its sign, "%%" a percent sign. A '+'
// in front of "%d" is dropped for a negative value. "" when the text does not have exactly one number placeholder or has another
// conversion such as "%s" (the caller then falls back).
std::string formatGameText(const std::string &text, int value);

// A readable name for an itemstatcost Stat name that has no entry in the fallback table: underscores turned into spaces and a
// leading "item " dropped, first letter upper case.
std::string statLabel(const std::string &stat);

// The property text without the group: "+30% Life", "-20% Fire Resistance". The stat's game text (the negative one for a negative
// value, when there is one) when it has one with a number in it and keeps the percent sign a fallback entry has, else the fallback table's wording, else "+25 Magic bonus" (the plain number and the tidied name).
std::string statLine(const Stat &stat);

// The whole panel text, lines joined by newlines: "Monster level N" (left out when level is 0 or below), then "Monsters have" and its
// property lines, then "Players have" and its lines (a group with no property is left out). Header lines are in `gold`, property
// lines in `blue`. `density` is the map's total Density (maps.txt plus the affixes' bonus), a monster value: "+N% Monster Density"
// ends the Monsters group (left out at 0). Magic find is a player stat from the affixes (maps.txt has no MF column), so stats of one
// name and group are added up into one line. The map's name is not in it: the game shows it above Difficulty. Both empty when
// there is nothing to show.
PanelText panelText(int monsterLevel, const std::vector<Stat> &stats, int density = 0);

// The most lines panelText can give: the level, "Monsters have", one line for each monster-side map stat (statplan monsterStats(): 7), the
// Density, "Players have" and the most player stats the game sends (statplan kMaxPlayerStats: 16).
constexpr int kMaxMonsterLines = 7;  // statplan monsterStats().size(); a test ties the two
constexpr int kMaxPlayerLines = 16;  // statplan kMaxPlayerStats
constexpr int kMaxLines = 1 + 1 + kMaxMonsterLines + 1 + 1 + kMaxPlayerLines;

// The panel's layout, in layout units (twice the 1080p pixels): where it starts, the height of one text line at the panel's font size
// (measured live) and the most blank lines of padding it puts in front of the text.
constexpr int kPanelTop = 270;
constexpr int kLineUnits = 44;
constexpr int kMaxPadLines = 8;

// The number of lines in a panel text (0 for an empty one).
int lineCount(const std::string &text);

// The two panel texts (header lines and property lines) have the same line count, an empty line where the other has text: one text takes the
// non-empty line of each (the gold one when both have text), for the game's single-colour terror zone field.
std::string mergeLines(const std::string &gold, const std::string &blue);

// The height of the panel's text boxes, in layout units, so the longest panel text still fits under `maxPadLines` blank lines of padding.
constexpr int panelHeight(int lineUnits, int maxPadLines)
{
	return (maxPadLines + kMaxLines) * lineUnits;
}

// Blank lines to put in front of the panel text so it starts under the game's terror zone text: the text ends at `textBottom` (0 or less:
// no zone text), the panel starts at `panelTop`, a line of the panel text is `lineUnits` high (all in layout units), at most `maxLines`.
int padLines(int textBottom, int panelTop, int lineUnits, int maxLines);

} // namespace cartographer::mappanel
