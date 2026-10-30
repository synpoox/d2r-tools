// XP session metrics: session XP (flat and level-equivalents), XP/hour (rolling window and whole session), session
// length and time to the next level.
//
// Pure C++, no game or Windows dependency. Time is passed in by the caller (seconds on any monotonic clock), so the
// tests drive it directly.
//
// Rules (see README "Metrics"):
//  - Scope (SessionConfig::scope):
//      Game (default): a session is one game. It starts when the game is entered and is cleared when the game is left
//      (back to the main menu / lobby); the next game starts a new one.
//      Character: a session belongs to one character and survives leaving and joining games; it resets when the
//      character changes (BeginCharacter with another key).
//    In both scopes Reset() (the hotkey / command) starts a new session at once.
//  - Session length counts only time between EnterGame and LeaveGame.
//  - Session XP (flat) = current XP - XP at session start: net, so a death's loss shows. Session XP % = the same in
//    level-equivalents: Position(now) - Position(start) (half of level 80 plus a quarter of level 81 = 0.75).
//  - XP/hour is NET: a death's XP loss lowers it (and can make it negative for a while). Gains and losses are also
//    counted separately (grossXp, lostXp) for the status command.
//  - The live XP/hour is over the last Config::windowSeconds of in-game time (default 600); the session XP/hour is
//    over the whole session. Both are "not available" until Config::minDataSeconds (60) of in-game time exist.
//  - Time to level = XP still needed / live XP per second; not available when that rate is zero or negative, or at the
//    maximum level, so it is never negative.
//  - The first XP sample of every game only sets the baseline: a change between games (or a not-yet-loaded 0 read)
//    is never counted as gained or lost.
#pragma once

#include "bxb/exp_table.h"

#include <cstdint>
#include <deque>
#include <optional>
#include <string>

namespace bxb {

enum class SessionScope {
	Game,       // one session per game (default)
	Character,  // one session per character, across games
};

struct SessionConfig {
	SessionScope scope = SessionScope::Game;
	double windowSeconds = 600.0;  // rolling window for the live XP/hour (clamped to >= minDataSeconds)
	double minDataSeconds = 60.0;  // no rate before this much in-game time is covered
	bool etaFromSessionRate = false;  // false: time to level from the live rate; true: from the session average
};

struct Rate {
	double xpPerHour = 0.0;      // net: negative when losses outweigh gains in the period
	double levelsPerHour = 0.0;  // net level-equivalents per hour (x100 = "% per hour")
};

struct Snapshot {
	bool valid = false;            // a table and at least one XP sample exist
	uint64_t xp = 0;               // current total experience
	uint32_t level = 0;            // level from the table for `xp`
	uint32_t maxLevel = 0;
	bool atMaxLevel = false;
	double levelProgress = 0.0;    // [0, 1] of the current level
	uint64_t remaining = 0;        // XP to the next level (0 at the maximum)
	int64_t sessionXp = 0;         // net: xp - session start xp (may be negative after a death)
	double sessionLevels = 0.0;    // net level-equivalents gained (x100 = session %)
	uint64_t grossXp = 0;          // gains only
	double grossLevels = 0.0;
	uint64_t lostXp = 0;           // losses (deaths) during the session
	uint32_t losses = 0;           // number of XP drops seen
	uint32_t levelUps = 0;
	double activeSeconds = 0.0;    // in-game time this session
	bool inGame = false;
	std::optional<Rate> live;      // rolling window; empty = "--"
	std::optional<Rate> session;   // whole session; empty = "--"
	std::optional<double> etaSeconds;  // empty = "--"
};

class XpSession {
public:
	explicit XpSession(SessionConfig config = {});

	void SetConfig(const SessionConfig& config);
	[[nodiscard]] auto Config() const noexcept -> const SessionConfig& { return config_; }

	// Replace the level thresholds (after a table load). Keeps the session; level-equivalents use the new table.
	void SetTable(ExpTable table);
	[[nodiscard]] auto Table() const noexcept -> const ExpTable& { return table_; }

	// Identifies the character (any stable key). A different key resets the session (in both scopes).
	// Returns true when it reset. A reset while in a game starts the session clock at `now`.
	auto BeginCharacter(const std::string& key, double now) -> bool;

	// Game scope: EnterGame starts a fresh session, LeaveGame clears it. Character scope: time pauses between games.
	void EnterGame(double now);
	void LeaveGame(double now);

	// Start a new session now (hotkey / command). The next sample becomes the baseline.
	void Reset(double now);

	// Current total experience of the character, read while in a game. Ignored outside a game.
	void Sample(double now, uint64_t xp);

	[[nodiscard]] auto Get(double now) const -> Snapshot;

	[[nodiscard]] auto ActiveSeconds(double now) const noexcept -> double;
	[[nodiscard]] auto CharacterKey() const noexcept -> const std::string& { return character_; }
	[[nodiscard]] auto Resets() const noexcept -> uint32_t { return resets_; }

private:
	struct Point {
		double t;          // active seconds at the change
		int64_t net;       // cumulative net XP change of the session after it
		double netLevels;  // cumulative net level-equivalents after it
	};

	void Clear();
	auto Window() const noexcept -> double;
	auto RateOver(double net, double netLevels, double fromNet, double fromLevels, double seconds) const -> Rate;

	SessionConfig config_;
	ExpTable table_;
	std::string character_;

	// time
	double activeBefore_ = 0.0;  // in-game seconds of finished games this session
	double enteredAt_ = 0.0;
	bool inGame_ = false;

	// xp
	bool haveStart_ = false;
	bool baselineNext_ = true;  // next sample only sets `last_`
	uint64_t startXp_ = 0;
	uint64_t lastXp_ = 0;
	uint64_t grossXp_ = 0;
	double grossLevels_ = 0.0;
	int64_t netXp_ = 0;         // counted changes only (baselines excluded)
	double netLevels_ = 0.0;
	uint64_t lostXp_ = 0;
	uint32_t losses_ = 0;
	uint32_t levelUps_ = 0;
	uint32_t resets_ = 0;
	std::deque<Point> changes_;  // one entry per XP change (gain or loss), oldest first, pruned to the window
};

}  // namespace bxb
