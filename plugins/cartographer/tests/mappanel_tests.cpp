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

TEST_CASE("a failed terror zone text call keeps the previous hidden state")
{
	CHECK(hiddenAfter(false, true, false));
	CHECK(hiddenAfter(false, true, true));
	CHECK_FALSE(hiddenAfter(false, false, false));
	CHECK(hiddenAfter(false, false, true));  // an earlier hide in the same map still counts
	CHECK_FALSE(hiddenAfter(true, true, true));
	CHECK_FALSE(hiddenAfter(true, true, false));
	CHECK(hiddenAfter(true, false, true));   // a failed restore stays marked hidden
	CHECK_FALSE(hiddenAfter(true, false, false));
}
