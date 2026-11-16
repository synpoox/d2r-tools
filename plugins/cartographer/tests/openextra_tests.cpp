// Host tests for the open-extra pick (issue #485). No game, no SDK: the code is pure C++.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include "openextra.h"

#include <set>
#include <vector>

using namespace cartographer::openextra;

namespace {

// Slots 202 (Cave), 203 (Crypt), 206 (Jungle), 207 (Snow).
std::vector<Candidate> rows() {
	return {
	    {202, 1, true},
	    {203, 1, true},
	    {206, 1, true},
	    {207, 1, true},
	};
}

}  // namespace

TEST_CASE("open-extra: the source slot is never picked")
{
	for (uint64_t seed = 0; seed < 64; ++seed) {
		const int index = pickExtraMap(rows(), {202}, 202, 99, seed);
		REQUIRE(index >= 0);
		CHECK(rows()[index].slotLevel != 202);
	}
}

TEST_CASE("open-extra: built or taken slots are never picked")
{
	std::set<int> seen;
	for (uint64_t seed = 0; seed < 64; ++seed) {
		const int index = pickExtraMap(rows(), {202, 203}, 202, 99, seed);
		REQUIRE(index >= 0);
		seen.insert(rows()[index].slotLevel);
	}
	CHECK(seen == std::set<int>{206, 207});
}

TEST_CASE("open-extra: nothing left gives -1")
{
	CHECK(pickExtraMap(rows(), {202, 203, 206, 207}, 202, 99, 1) == -1);
	auto unusable = rows();
	unusable[1].usable = false;
	unusable[2].usable = false;
	unusable[3].usable = false;
	CHECK(pickExtraMap(unusable, {202}, 202, 99, 1) == -1);
	CHECK(pickExtraMap({}, {}, 202, 99, 1) == -1);
}

TEST_CASE("open-extra: MinCharLevel keeps a row out")
{
	auto leveled = rows();
	leveled[2].minCharLevel = 90;  // slot 206 needs character level 90
	for (uint64_t seed = 0; seed < 64; ++seed) {
		CHECK(pickExtraMap(leveled, {202, 203}, 202, 89, seed) == 3);
	}
}

TEST_CASE("open-extra: the same seed gives the same pick")
{
	CHECK(pickExtraMap(rows(), {202}, 202, 99, 12345) == pickExtraMap(rows(), {202}, 202, 99, 12345));
}
