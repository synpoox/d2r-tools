#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "mappanel.h"

using namespace cartographer::mappanel;

TEST_CASE("known stats read in the game's wording")
{
	CHECK(statLine({true, "item_maxhp_percent", 30}) == "Monsters: +30% Life");
	CHECK(statLine({false, "fireresist", -20}) == "You: -20% Fire Resistance");
	CHECK(statLine({true, "lightresist", 0}) == "Monsters: +0% Lightning Resistance");
	CHECK(statLine({false, "hpregen", -15}) == "You: -15% Life Regeneration");
}

TEST_CASE("an unknown stat is tidied and shows the plain number")
{
	CHECK(statLabel("item_goldbonus") == "Goldbonus");
	CHECK(statLabel("some_new_stat") == "Some new stat");
	CHECK(statLine({true, "item_magicbonus", 25}) == "Monsters: +25 Magicbonus");
	CHECK(statLine({false, "strength", -5}) == "You: -5 Strength");
}

TEST_CASE("the panel text is the name, the level, then the properties")
{
	const std::vector<Stat> stats = {{true, "damagepercent", 40}, {false, "coldresist", -10}};
	CHECK(panelText("Shorelines", 85, stats) == "Shorelines\nMonster level 85\nMonsters: +40% Damage\nYou: -10% Cold Resistance");
}

TEST_CASE("a map without properties or a level still has a name")
{
	CHECK(panelText("Shorelines", 85, {}) == "Shorelines\nMonster level 85");
	CHECK(panelText("Shorelines", 0, {}) == "Shorelines");
	CHECK(panelText("", 0, {}) == "Map");
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
