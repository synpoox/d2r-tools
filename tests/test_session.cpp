// XpSession tests. Synthetic thresholds only.
#include "bxb/xp_session.h"
#include "check.h"

#include <cstdint>
#include <random>
#include <vector>

using bxb::ExpTable;
using bxb::SessionConfig;
using bxb::Snapshot;
using bxb::XpSession;

namespace {

auto Small() -> ExpTable {
	ExpTable t;
	t.Assign({0, 100, 300, 600, 1000});
	return t;
}

// A big table so tests that need many gains don't hit the max level.
auto Linear(uint32_t levels, uint64_t step) -> ExpTable {
	std::vector<uint64_t> v;
	for (uint32_t i = 0; i < levels; ++i) {
		v.push_back(static_cast<uint64_t>(i) * step);
	}
	ExpTable t;
	t.Assign(std::move(v));
	return t;
}

auto InGame(ExpTable table, uint64_t xp, SessionConfig cfg = {}) -> XpSession {
	XpSession s(cfg);
	s.SetTable(std::move(table));
	s.BeginCharacter("hero", 0.0);
	s.EnterGame(0.0);
	s.Sample(0.0, xp);
	return s;
}

}  // namespace

TEST(session_first_sample_is_baseline) {
	XpSession s = InGame(Small(), 150);
	const Snapshot g = s.Get(0.0);
	CHECK(g.valid);
	CHECK_EQ(g.level, 2u);
	CHECK_NEAR(g.levelProgress, 0.25, 1e-12);
	CHECK_EQ(g.sessionXp, 0);
	CHECK_EQ(g.grossXp, 0u);
	CHECK(!g.live.has_value());
	CHECK(!g.session.has_value());
	CHECK(!g.etaSeconds.has_value());
}

TEST(session_not_valid_without_table_or_sample) {
	XpSession s;
	s.BeginCharacter("hero", 0.0);
	s.EnterGame(0.0);
	CHECK(!s.Get(1.0).valid);
	s.Sample(1.0, 50);
	CHECK(!s.Get(1.0).valid);  // no table yet
	s.SetTable(Small());
	CHECK(s.Get(1.0).valid);
}

TEST(session_level_up_carries_over) {
	// start half way through level 2 (200), end a quarter into level 3 (375)
	XpSession s = InGame(Small(), 200);
	s.Sample(30.0, 290);
	s.Sample(40.0, 375);
	const Snapshot g = s.Get(40.0);
	CHECK_EQ(g.level, 3u);
	CHECK_EQ(g.sessionXp, 175);
	CHECK_NEAR(g.sessionLevels, 0.75, 1e-12);  // 0.5 of level 2 + 0.25 of level 3
	CHECK_NEAR(g.grossLevels, 0.75, 1e-12);
	CHECK_EQ(g.levelUps, 1u);
	CHECK_NEAR(g.levelProgress, 0.25, 1e-12);
	CHECK_EQ(g.remaining, 225u);
}

TEST(session_multiple_levels_in_one_sample) {
	XpSession s = InGame(Small(), 50);       // 1.5
	s.Sample(10.0, 800);                     // 4.5
	const Snapshot g = s.Get(10.0);
	CHECK_EQ(g.levelUps, 3u);
	CHECK_NEAR(g.sessionLevels, 3.0, 1e-12);
	CHECK_EQ(g.sessionXp, 750);
}

TEST(session_rate_not_shown_before_60s) {
	XpSession s = InGame(Linear(100, 1000), 0);
	s.Sample(10.0, 600);
	CHECK(!s.Get(59.999).live.has_value());
	CHECK(!s.Get(59.999).session.has_value());
	CHECK(!s.Get(59.999).etaSeconds.has_value());
	const Snapshot g = s.Get(60.0);
	CHECK(g.live.has_value());
	CHECK(g.session.has_value());
	CHECK_NEAR(g.live->xpPerHour, 600.0 * 60.0, 1e-6);   // 600 XP in 60 s
	CHECK_NEAR(g.session->xpPerHour, 36000.0, 1e-6);
	CHECK_NEAR(g.live->levelsPerHour, 0.6 * 60.0, 1e-9);
	// ETA = (1000 - 600) / (600 per minute) = 40 s
	CHECK(g.etaSeconds.has_value());
	CHECK_NEAR(*g.etaSeconds, 40.0, 1e-9);
}

TEST(session_death_loss_does_not_corrupt_rates) {
	XpSession s = InGame(Linear(100, 1000), 5000);
	s.Sample(60.0, 6000);    // +1000
	s.Sample(120.0, 5500);   // death: -500
	const Snapshot g = s.Get(120.0);
	CHECK_EQ(g.lostXp, 500u);
	CHECK_EQ(g.losses, 1u);
	CHECK_EQ(g.grossXp, 1000u);
	CHECK_EQ(g.sessionXp, 500);                 // net
	CHECK_NEAR(g.session->xpPerHour, 1000.0 * 30.0, 1e-6);  // gains only: 1000 XP / 2 min
	CHECK(g.live->xpPerHour >= 0.0);
	CHECK(g.etaSeconds.has_value());
	CHECK(*g.etaSeconds > 0.0);
	CHECK_EQ(g.remaining, 500u);
}

TEST(session_death_below_start_gives_negative_net_but_positive_eta) {
	XpSession s = InGame(Linear(100, 1000), 5500);
	s.Sample(10.0, 5600);
	s.Sample(20.0, 5000);    // lost more than gained, back to the level start
	const Snapshot g = s.Get(90.0);
	CHECK_EQ(g.sessionXp, -500);
	CHECK_NEAR(g.sessionLevels, -0.5, 1e-12);
	CHECK(g.session->xpPerHour > 0.0);
	CHECK(g.etaSeconds.has_value());
	CHECK(*g.etaSeconds > 0.0);
	CHECK_EQ(g.level, 6u);
}

TEST(session_death_across_level_boundary) {
	// D2 never de-levels on death, but the tracker must not misbehave if XP drops below a threshold
	XpSession s = InGame(Small(), 310);
	s.Sample(10.0, 290);
	const Snapshot g = s.Get(10.0);
	CHECK_EQ(g.level, 2u);
	CHECK_EQ(g.lostXp, 20u);
	CHECK_EQ(g.levelUps, 0u);
	CHECK_EQ(g.grossXp, 0u);
}

TEST(session_idle_rate_decays_in_window) {
	SessionConfig cfg;
	cfg.windowSeconds = 600;
	XpSession s = InGame(Linear(100, 100000), 0, cfg);
	s.Sample(100.0, 6000);
	CHECK_NEAR(s.Get(600.0).live->xpPerHour, 6000.0 * 6.0, 1e-6);   // 6000 in 600 s
	CHECK_NEAR(s.Get(650.0).live->xpPerHour, 6000.0 * 6.0, 1e-6);   // window (50, 650] still holds the gain
	const Snapshot late = s.Get(1000.0);
	CHECK(late.live.has_value());
	CHECK_NEAR(late.live->xpPerHour, 0.0, 1e-9);
	CHECK(!late.etaSeconds.has_value());          // zero live rate -> "--"
	CHECK_NEAR(late.session->xpPerHour, 6000.0 * 3.6, 1e-6);   // 6000 / 1000 s
}

TEST(session_window_edge_is_exclusive_at_start) {
	SessionConfig cfg;
	cfg.windowSeconds = 600;
	XpSession s = InGame(Linear(100, 100000), 0, cfg);
	s.Sample(100.0, 6000);
	// window for t=700 is (100, 700]: the gain at exactly 100 is outside
	CHECK_NEAR(s.Get(700.0).live->xpPerHour, 0.0, 1e-9);
	// one microsecond earlier it is inside
	CHECK_NEAR(s.Get(699.999).live->xpPerHour, 6000.0 * 3600.0 / 600.0, 1e-6);
}

TEST(session_window_longer_than_session_uses_session_time) {
	SessionConfig cfg;
	cfg.windowSeconds = 3600;
	XpSession s = InGame(Linear(100, 100000), 0, cfg);
	s.Sample(30.0, 1000);
	const Snapshot g = s.Get(120.0);
	CHECK_NEAR(g.live->xpPerHour, 1000.0 * 30.0, 1e-6);  // over 120 s, not 3600 s
	CHECK_NEAR(g.live->xpPerHour, g.session->xpPerHour, 1e-9);
}

TEST(session_window_clamped_to_min_data) {
	SessionConfig cfg;
	cfg.windowSeconds = 5;
	XpSession s(cfg);
	CHECK_NEAR(s.Config().windowSeconds, 60.0, 1e-12);
	cfg.windowSeconds = -1;
	cfg.minDataSeconds = -3;
	s.SetConfig(cfg);
	CHECK(s.Config().windowSeconds >= 1.0);
	CHECK_NEAR(s.Config().minDataSeconds, 0.0, 1e-12);
}

TEST(session_rolling_window_matches_brute_force) {
	SessionConfig cfg;
	cfg.windowSeconds = 300;
	XpSession s = InGame(Linear(1000, 10000), 0, cfg);
	std::mt19937 rng(1234);
	std::uniform_real_distribution<double> dt(0.1, 20.0);
	std::uniform_int_distribution<int> gain(0, 500);
	std::vector<std::pair<double, uint64_t>> gains;  // (t, amount)
	double t = 0.0;
	uint64_t xp = 0;
	for (int i = 0; i < 3000; ++i) {
		t += dt(rng);
		const int g = gain(rng);
		if (g < 50) {
			const uint64_t loss = std::min<uint64_t>(xp, 300);  // occasional death
			xp -= loss;
		} else {
			xp += static_cast<uint64_t>(g);
			gains.emplace_back(t, static_cast<uint64_t>(g));
		}
		s.Sample(t, xp);
		if (i % 97 == 0 && t >= 60.0) {
			const double query = t + dt(rng);
			const double start = query - std::min(300.0, query);
			double sum = 0.0;
			for (const auto& [gt, amount] : gains) {
				if (gt > start && gt <= query) {
					sum += static_cast<double>(amount);
				}
			}
			const double expected = sum * 3600.0 / std::min(300.0, query);
			CHECK_NEAR(s.Get(query).live->xpPerHour, expected, 1e-6 * std::max(1.0, expected));
		}
	}
}

TEST(session_max_level) {
	XpSession s = InGame(Small(), 900);
	s.Sample(30.0, 1000);
	s.Sample(90.0, 1000);
	const Snapshot g = s.Get(90.0);
	CHECK(g.atMaxLevel);
	CHECK_EQ(g.level, 5u);
	CHECK_EQ(g.remaining, 0u);
	CHECK_NEAR(g.levelProgress, 1.0, 1e-12);
	CHECK(!g.etaSeconds.has_value());
	CHECK(g.live.has_value());                      // XP/hour still shown
}

TEST(session_zero_rate_eta_unavailable) {
	XpSession s = InGame(Small(), 150);
	const Snapshot g = s.Get(120.0);
	CHECK(g.live.has_value());
	CHECK_NEAR(g.live->xpPerHour, 0.0, 1e-12);
	CHECK(!g.etaSeconds.has_value());
}

TEST(session_time_counts_only_in_game) {
	XpSession s = InGame(Linear(100, 100000), 0);
	s.Sample(50.0, 500);
	s.LeaveGame(100.0);
	CHECK_NEAR(s.Get(5000.0).activeSeconds, 100.0, 1e-12);   // frozen while out of game
	s.EnterGame(5000.0);
	CHECK_NEAR(s.Get(5030.0).activeSeconds, 130.0, 1e-12);
	s.Sample(5030.0, 500);        // first read of the new game: baseline
	s.Sample(5040.0, 1500);       // +1000 at active 140
	const Snapshot g = s.Get(5040.0);
	CHECK_EQ(g.grossXp, 1500u);
	CHECK_NEAR(g.session->xpPerHour, 1500.0 * 3600.0 / 140.0, 1e-6);
	CHECK_NEAR(g.live->xpPerHour, 1500.0 * 3600.0 / 140.0, 1e-6);
}

TEST(session_xp_change_between_games_is_not_counted) {
	XpSession s = InGame(Linear(100, 100000), 1000);
	s.Sample(10.0, 2000);
	s.LeaveGame(20.0);
	s.Sample(30.0, 99999);        // out of game: ignored
	s.EnterGame(40.0);
	s.Sample(41.0, 0);            // stats not loaded yet: baseline only, no loss
	CHECK_EQ(s.Get(41.0).lostXp, 0u);
	CHECK_EQ(s.Get(41.0).sessionXp, 1000);   // net kept: the start moved with the baseline
	s.Sample(42.0, 2000);         // real value arrives: counts as a gain of 2000 from the bad 0 read?
	// No: the 0 was the baseline, so 0 -> 2000 is a +2000 gain. The plugin must only sample once the player is
	// ready (LocalPlayerReady). This test pins the behaviour so a change is deliberate.
	CHECK_EQ(s.Get(42.0).grossXp, 3000u);
}

TEST(session_rejoin_same_xp_continues) {
	XpSession s = InGame(Small(), 150);
	s.Sample(10.0, 200);
	s.LeaveGame(20.0);
	s.EnterGame(100.0);
	s.Sample(100.0, 200);
	s.Sample(110.0, 250);
	const Snapshot g = s.Get(110.0);
	CHECK_EQ(g.sessionXp, 100);
	CHECK_EQ(g.grossXp, 100u);
	CHECK_EQ(g.lostXp, 0u);
	CHECK_NEAR(g.activeSeconds, 30.0, 1e-12);
}

TEST(session_reset_on_new_game_option) {
	SessionConfig cfg;
	cfg.resetOnNewGame = true;
	XpSession s = InGame(Small(), 150, cfg);
	s.Sample(10.0, 200);
	s.LeaveGame(20.0);
	s.EnterGame(30.0);
	s.Sample(30.0, 200);
	const Snapshot g = s.Get(30.0);
	CHECK_EQ(g.sessionXp, 0);
	CHECK_NEAR(g.activeSeconds, 0.0, 1e-12);
}

TEST(session_character_switch_resets) {
	XpSession s = InGame(Small(), 150);
	s.Sample(10.0, 200);
	CHECK(!s.BeginCharacter("hero", 10.0));          // same key: no reset
	CHECK_EQ(s.Get(10.0).sessionXp, 50);
	s.LeaveGame(20.0);
	CHECK(s.BeginCharacter("alt", 25.0));
	s.EnterGame(30.0);
	s.Sample(30.0, 700);
	const Snapshot g = s.Get(30.0);
	CHECK_EQ(g.sessionXp, 0);
	CHECK_EQ(g.level, 4u);
	CHECK_NEAR(g.activeSeconds, 0.0, 1e-12);
	CHECK_EQ(s.CharacterKey(), std::string("alt"));
}

TEST(session_manual_reset_in_game) {
	XpSession s = InGame(Linear(100, 100000), 0);
	s.Sample(100.0, 5000);
	s.Reset(100.0);
	CHECK(!s.Get(100.0).valid);           // waits for the next sample as the baseline
	s.Sample(101.0, 5000);
	s.Sample(150.0, 5600);
	const Snapshot g = s.Get(161.0);
	CHECK_EQ(g.sessionXp, 600);
	CHECK_NEAR(g.activeSeconds, 61.0, 1e-12);
	CHECK_NEAR(g.session->xpPerHour, 600.0 * 3600.0 / 61.0, 1e-6);
}

TEST(session_samples_outside_game_ignored) {
	XpSession s;
	s.SetTable(Small());
	s.BeginCharacter("hero", 0.0);
	s.Sample(0.0, 150);
	CHECK(!s.Get(0.0).valid);
}

TEST(session_eta_from_session_rate_option) {
	SessionConfig cfg;
	cfg.windowSeconds = 60;
	cfg.etaFromSessionRate = true;
	XpSession s = InGame(Linear(100, 100000), 0, cfg);
	s.Sample(10.0, 1000);
	const Snapshot g = s.Get(200.0);
	CHECK_NEAR(g.live->xpPerHour, 0.0, 1e-9);     // nothing in the last 60 s
	CHECK(g.etaSeconds.has_value());              // but the session average is used
	CHECK_NEAR(*g.etaSeconds, 99000.0 / (1000.0 / 200.0), 1e-6);
}

TEST(session_table_swap_keeps_session) {
	XpSession s = InGame(Small(), 150);
	s.Sample(10.0, 250);
	ExpTable other;
	other.Assign({0, 200, 400});
	s.SetTable(other);
	const Snapshot g = s.Get(10.0);
	CHECK_EQ(g.sessionXp, 100);
	CHECK_EQ(g.level, 2u);
	CHECK_NEAR(g.levelProgress, 0.25, 1e-12);
}

TEST(session_large_32bit_values) {
	ExpTable t;
	t.Assign({0, 3'000'000'000ULL, 3'600'000'000ULL});
	XpSession s = InGame(t, 3'400'000'000ULL);
	s.Sample(60.0, 3'500'000'000ULL);
	const Snapshot g = s.Get(60.0);
	CHECK_EQ(g.sessionXp, 100'000'000);
	CHECK_EQ(g.remaining, 100'000'000u);  // synthetic values above 2^31
	CHECK_NEAR(*g.etaSeconds, 100'000'000.0 / (100'000'000.0 / 60.0), 1e-6);
}

TEST(session_character_switch_in_game_restarts_clock) {
	XpSession s = InGame(Small(), 150);
	s.Sample(50.0, 200);
	CHECK(s.BeginCharacter("alt", 100.0));
	s.Sample(100.0, 700);
	CHECK_NEAR(s.Get(130.0).activeSeconds, 30.0, 1e-12);
	CHECK_EQ(s.Get(130.0).sessionXp, 0);
	CHECK_EQ(s.Resets(), 2u);  // "hero" + "alt"
}
