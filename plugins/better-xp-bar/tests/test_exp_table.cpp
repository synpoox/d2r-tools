// ExpTable tests. Synthetic thresholds only: no game data in the tests.
#include "bxb/exp_table.h"
#include "check.h"

using bxb::ExpTable;

namespace {
// level 1 at 0, 2 at 100, 3 at 300, 4 at 600, 5 (max) at 1000
auto Small() -> ExpTable {
	ExpTable t;
	t.Assign({0, 100, 300, 600, 1000});
	return t;
}
}  // namespace

TEST(table_rejects_empty_and_decreasing) {
	ExpTable t;
	std::string err;
	CHECK(!t.Assign({}, &err));
	CHECK(!err.empty());
	CHECK(!t.Assign({0, 100, 50}, &err));
	CHECK(err.find("level 3") != std::string::npos);
	CHECK(!t.Valid());
	CHECK_EQ(t.LevelForXp(5), 0u);
	CHECK_EQ(t.Progress(5, 1), 0.0);
}

TEST(table_level_for_xp_boundaries) {
	const ExpTable t = Small();
	CHECK_EQ(t.MaxLevel(), 5u);
	CHECK_EQ(t.LevelForXp(0), 1u);
	CHECK_EQ(t.LevelForXp(99), 1u);
	CHECK_EQ(t.LevelForXp(100), 2u);  // exactly at the threshold = the new level
	CHECK_EQ(t.LevelForXp(299), 2u);
	CHECK_EQ(t.LevelForXp(300), 3u);
	CHECK_EQ(t.LevelForXp(1000), 5u);
	CHECK_EQ(t.LevelForXp(5000000000ULL), 5u);  // beyond the table (and beyond 32 bits)
}

TEST(table_progress_and_remaining) {
	const ExpTable t = Small();
	CHECK_NEAR(t.Progress(150, 2), 0.25, 1e-12);  // (150-100)/(300-100)
	CHECK_NEAR(t.Progress(100, 2), 0.0, 1e-12);
	CHECK_NEAR(t.Progress(299, 2), 199.0 / 200.0, 1e-12);
	CHECK_EQ(t.Remaining(150, 2), 150u);
	CHECK_EQ(t.Remaining(1000, 5), 0u);       // max level
	CHECK_NEAR(t.Progress(1000, 5), 1.0, 1e-12);
	CHECK_EQ(t.Remaining(400, 2), 0u);        // stale level argument: past the threshold
	CHECK_NEAR(t.Progress(400, 2), 1.0, 1e-12);
}

TEST(table_position_matches_level_equivalents) {
	const ExpTable t = Small();
	CHECK_NEAR(t.Position(200), 2.5, 1e-12);
	CHECK_NEAR(t.Position(375), 3.25, 1e-12);
	// "half of level 2 plus a quarter of level 3" = 0.75
	CHECK_NEAR(t.Position(375) - t.Position(200), 0.75, 1e-12);
	CHECK_NEAR(t.Position(1000), 5.0, 1e-12);
	CHECK_NEAR(t.Position(9999), 5.0, 1e-12);
}

TEST(table_zero_width_level) {
	ExpTable t;
	CHECK(t.Assign({0, 100, 100, 200}));
	CHECK_EQ(t.LevelForXp(100), 3u);  // both thresholds reached
	CHECK_NEAR(t.Progress(100, 2), 1.0, 1e-12);
	CHECK_NEAR(t.Position(150), 3.5, 1e-12);
}

TEST(table_single_level) {
	ExpTable t;
	CHECK(t.Assign({0}));
	CHECK_EQ(t.LevelForXp(12345), 1u);
	CHECK_EQ(t.Remaining(12345, 1), 0u);
	CHECK_NEAR(t.Progress(12345, 1), 1.0, 1e-12);
}
