#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "mappanel.h"

using namespace cartographer::mappanel;

TEST_CASE("known stats read in the game's wording from the fallback table")
{
	CHECK(statLine({true, "item_maxhp_percent", 30}) == "+30% Life");
	CHECK(statLine({false, "fireresist", -20}) == "-20% Fire Resistance");
	CHECK(statLine({true, "lightresist", 0}) == "+0% Lightning Resistance");
	CHECK(statLine({true, "item_fastermovevelocity", 16}) == "+16% Faster Run/Walk");
	CHECK(statLine({true, "item_magicbonus", 31}) == "+31% Better Chance of Getting Magic Items");
}

TEST_CASE("the game's own text wins over the fallback")
{
	CHECK(statLine({true, "item_magicbonus", 31, "%+d%% More Magic Find"}) == "+31% More Magic Find");
	CHECK(statLine({true, "item_goldbonus", 12, "+%d%% Gold"}) == "+12% Gold");
	CHECK(statLine({false, "fireresist", -20, "Fire Resist %+d%%"}) == "Fire Resist -20%");
	CHECK(statLine({false, "fireresist", -20, "+%d%% Fire Resist"}) == "-20% Fire Resist");
}

TEST_CASE("a game text with no number is not used")
{
	CHECK(formatGameText("to Strength", 5).empty());
	CHECK(statLine({false, "weird", 5, "to Weird"}) == "+5 Weird");
	CHECK(formatGameText("%d%% 100%%", 7) == "7% 100%");
}

TEST_CASE("a game text with a %s or two numbers is not used")
{
	CHECK(formatGameText("%+d to %s", 5).empty());
	CHECK(formatGameText("%d%% to %d", 5).empty());
	CHECK(statLine({true, "item_magicbonus", 9, "%d to %s"}) == "+9% Better Chance of Getting Magic Items");
	CHECK(statLine({true, "weird", 9, "%d to %s"}) == "+9 Weird");
}

TEST_CASE("the negative game text is used for a negative value")
{
	CHECK(statLine({false, "fireresist", -20, "%+d%% Fire Resist", "Fire Weakness %d%%"}) == "Fire Weakness -20%");
	CHECK(statLine({false, "fireresist", 20, "%+d%% Fire Resist", "Fire Weakness %d%%"}) == "+20% Fire Resist");
	CHECK(statLine({false, "fireresist", -20, "%+d%% Fire Resist", "to Weakness"}) == "-20% Fire Resist");  // no number: the positive text
}

TEST_CASE("a percent stat keeps its percent sign when the game text has none")
{
	CHECK(statLine({true, "item_fastermovevelocity", 16, "%+d Faster Run/Walk"}) == "+16% Faster Run/Walk");
	CHECK(statLine({true, "strength", 5, "%+d to Strength"}) == "+5 to Strength");
	CHECK(statLine({true, "weird", 5, "%+d Weird"}) == "+5 Weird");
}

TEST_CASE("an unknown stat is tidied and shows the plain number")
{
	CHECK(statLabel("item_goldbonus") == "Goldbonus");
	CHECK(statLabel("some_new_stat") == "Some new stat");
	CHECK(statLine({true, "some_new_stat", 25}) == "+25 Some new stat");
	CHECK(statLine({false, "weird", -5}) == "-5 Weird");
}

TEST_CASE("the panel text puts the headers in gold and the property lines in blue, line for line")
{
	const std::vector<Stat> stats = {{true, "damagepercent", 40}, {false, "coldresist", -10}, {true, "item_maxhp_percent", 30}};
	const PanelText t = panelText(85, stats);
	CHECK(t.gold == "Monster level 85\nMonsters have\n\n\nPlayers have\n");
	CHECK(t.blue == "\n\n+40% Damage\n+30% Life\n\n-10% Cold Resistance");
}

TEST_CASE("the panel text has no colour codes (the text widget prints them)")
{
	const PanelText t = panelText(85, {{true, "damagepercent", 40}});
	CHECK(t.gold.find("\xC3\xBF") == std::string::npos);
	CHECK(t.blue.find("\xC3\xBF") == std::string::npos);
}

TEST_CASE("a group with no properties is left out and the map name is never shown")
{
	CHECK(panelText(85, {{false, "coldresist", -10}}).gold == "Monster level 85\nPlayers have\n");
	CHECK(panelText(85, {{false, "coldresist", -10}}).blue == "\n\n-10% Cold Resistance");
	CHECK(panelText(85, {}).gold == "Monster level 85");
	CHECK(panelText(85, {}).blue.empty());
	CHECK(panelText(0, {{true, "damagepercent", 40}}).gold == "Monsters have\n");
	CHECK(panelText(0, {{true, "damagepercent", 40}}).blue == "\n+40% Damage");
	CHECK(panelText(0, {}).empty());
}

TEST_CASE("the panel text starts under the game's terror zone text")
{
	CHECK(padLines(0, 270, 44, 8) == 0);     // no zone text
	CHECK(padLines(270, 270, 44, 8) == 0);   // it ends where the panel starts
	CHECK(padLines(100, 270, 44, 8) == 0);   // it ends above the panel
	CHECK(padLines(271, 270, 44, 8) == 1);   // a part of a line rounds up
	CHECK(padLines(416, 270, 44, 8) == 4);   // 146 units: 3.3 lines
	CHECK(padLines(2000, 270, 44, 8) == 8);  // capped
	CHECK(padLines(416, 270, 0, 8) == 0);    // no line height known
}
