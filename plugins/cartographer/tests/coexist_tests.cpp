// Host tests for the core-maps notice. No game, no SDK.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <string>

#include "core/coexist.h"

TEST_CASE("no notice when core-maps is not loaded") {
	CHECK(carto::CoreMapsNotice(false).empty());
}

TEST_CASE("the notice names core-maps and says to use one") {
	const std::string text = carto::CoreMapsNotice(true);
	CHECK(text.find("core-maps") != std::string::npos);
	CHECK(text.find("Remove one") != std::string::npos);
}
