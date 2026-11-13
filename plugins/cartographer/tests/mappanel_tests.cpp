#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "mappanel.h"

using namespace cartographer::mappanel;

static const std::string B = kBlue;
static const std::string G = kGold;

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

TEST_CASE("an unknown stat is tidied and shows the plain number")
{
	CHECK(statLabel("item_goldbonus") == "Goldbonus");
	CHECK(statLabel("some_new_stat") == "Some new stat");
	CHECK(statLine({true, "some_new_stat", 25}) == "+25 Some new stat");
	CHECK(statLine({false, "weird", -5}) == "-5 Weird");
}

TEST_CASE("the panel text groups the lines under Monsters have and Players have, properties in blue")
{
	const std::vector<Stat> stats = {{true, "damagepercent", 40}, {false, "coldresist", -10}, {true, "item_maxhp_percent", 30}};
	CHECK(panelText(85, stats) == G + "Monster level 85\n" + G + "Monsters have\n" + B + "+40% Damage\n" + B + "+30% Life\n" + G + "Players have\n" + B + "-10% Cold Resistance");
}

TEST_CASE("a group with no properties is left out and the map name is never shown")
{
	CHECK(panelText(85, {{false, "coldresist", -10}}) == G + "Monster level 85\n" + G + "Players have\n" + B + "-10% Cold Resistance");
	CHECK(panelText(85, {}) == G + "Monster level 85");
	CHECK(panelText(0, {{true, "damagepercent", 40}}) == G + "Monsters have\n" + B + "+40% Damage");
	CHECK(panelText(0, {}).empty());
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
