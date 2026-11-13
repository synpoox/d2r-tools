// Cartographer map panel text: what the side-of-screen panel says while the player is inside a map (the map's name, its
// monster level and the rolled properties). Pure C++ on plain structs: no engine, no SDK. The engine part
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
};

// A readable name for an itemstatcost Stat name: the common map properties get their game wording ("Life", "Fire
// Resistance"); any other stat has its underscores turned into spaces and a leading "item " dropped, first letter upper case.
std::string statLabel(const std::string &stat);

// One line for a property: "Monsters: +30% Life" or "You: -20% Fire Resistance". Percent stats (resistances, life and damage
// percent) get a % sign; the others show the plain number.
std::string statLine(const Stat &stat);

// The whole panel text, lines joined by '\n': the map name, "Monster level N" (left out when level is 0 or below), then one
// line per property (in the order given). A map without properties shows the first two lines only. An empty name shows "Map".
std::string panelText(const std::string &mapName, int monsterLevel, const std::vector<Stat> &stats);

// Whether the game's terror zone text counts as hidden after a call that asked for it to be `visible` and `succeeded` or not. A failed
// restore leaves it hidden (so it is tried again); a failed hide leaves it shown.
bool hiddenAfter(bool visible, bool succeeded);

} // namespace cartographer::mappanel
