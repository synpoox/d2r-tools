#include "bxb/xp_session.h"

#include <algorithm>

namespace bxb {

XpSession::XpSession(SessionConfig config) {
	SetConfig(config);
}

void XpSession::SetConfig(const SessionConfig& config) {
	config_ = config;
	config_.minDataSeconds = std::max(0.0, config_.minDataSeconds);
	config_.windowSeconds = std::max(config_.windowSeconds, std::max(config_.minDataSeconds, 1.0));
}

void XpSession::SetTable(ExpTable table) {
	table_ = std::move(table);
}

void XpSession::Clear() {
	activeBefore_ = 0.0;
	haveStart_ = false;
	baselineNext_ = true;
	startXp_ = lastXp_ = 0;
	grossXp_ = 0;
	grossLevels_ = 0.0;
	lostXp_ = 0;
	losses_ = 0;
	levelUps_ = 0;
	gains_.clear();
}

auto XpSession::BeginCharacter(const std::string& key, double now) -> bool {
	if (key == character_) {
		return false;
	}
	character_ = key;
	Clear();
	++resets_;
	if (inGame_) {
		enteredAt_ = now;  // the new character's session starts now
	}
	return true;
}

void XpSession::EnterGame(double now) {
	if (inGame_) {
		return;
	}
	if (config_.scope == SessionScope::Game) {
		Clear();  // normally already clear from LeaveGame; also covers a missed leave
	}
	inGame_ = true;
	enteredAt_ = now;
	baselineNext_ = true;
}

void XpSession::LeaveGame(double now) {
	if (!inGame_) {
		return;
	}
	activeBefore_ += std::max(0.0, now - enteredAt_);
	inGame_ = false;
	if (config_.scope == SessionScope::Game) {
		Clear();
		++resets_;
	}
}

void XpSession::Reset(double now) {
	Clear();
	++resets_;
	if (inGame_) {
		enteredAt_ = now;
	}
}

auto XpSession::ActiveSeconds(double now) const noexcept -> double {
	return activeBefore_ + (inGame_ ? std::max(0.0, now - enteredAt_) : 0.0);
}

auto XpSession::Window() const noexcept -> double {
	return config_.windowSeconds;
}

void XpSession::Sample(double now, uint64_t xp) {
	if (!inGame_) {
		return;
	}
	if (!haveStart_) {
		haveStart_ = true;
		startXp_ = lastXp_ = xp;
		baselineNext_ = false;
		return;
	}
	if (baselineNext_) {
		// first read of a new game: XP did not change while out of game, and a read before the character's stats
		// arrive must not count as a loss and a gain. Keep the session's net figures honest by moving the start.
		if (xp != lastXp_) {
			startXp_ = startXp_ + xp - lastXp_;  // unsigned wrap is exact modulo 2^64 and cancels in xp - startXp_
		}
		lastXp_ = xp;
		baselineNext_ = false;
		return;
	}
	if (xp == lastXp_) {
		return;
	}
	if (xp < lastXp_) {
		lostXp_ += lastXp_ - xp;
		++losses_;
		lastXp_ = xp;
		return;
	}
	const double t = ActiveSeconds(now);
	const uint32_t before = table_.LevelForXp(lastXp_);
	const uint32_t after = table_.LevelForXp(xp);
	if (after > before) {
		levelUps_ += after - before;
	}
	grossXp_ += xp - lastXp_;
	grossLevels_ += std::max(0.0, table_.Position(xp) - table_.Position(lastXp_));
	lastXp_ = xp;
	gains_.push_back({t, grossXp_, grossLevels_});
	// keep the last gain at or before the window start: it is the window's starting value
	const double windowStart = t - Window();
	while (gains_.size() >= 2 && gains_[1].t <= windowStart) {
		gains_.pop_front();
	}
}

auto XpSession::RateOver(double gross, double grossLevels, double fromGross, double fromLevels, double seconds) const
	-> Rate {
	Rate r;
	if (seconds > 0.0) {
		r.xpPerHour = std::max(0.0, gross - fromGross) * 3600.0 / seconds;
		r.levelsPerHour = std::max(0.0, grossLevels - fromLevels) * 3600.0 / seconds;
	}
	return r;
}

auto XpSession::Get(double now) const -> Snapshot {
	Snapshot s;
	s.inGame = inGame_;
	s.activeSeconds = ActiveSeconds(now);
	s.maxLevel = table_.MaxLevel();
	s.grossXp = grossXp_;
	s.grossLevels = grossLevels_;
	s.lostXp = lostXp_;
	s.losses = losses_;
	s.levelUps = levelUps_;
	if (!table_.Valid() || !haveStart_) {
		return s;
	}
	s.valid = true;
	s.xp = lastXp_;
	s.level = table_.LevelForXp(lastXp_);
	s.atMaxLevel = s.level >= s.maxLevel;
	s.levelProgress = table_.Progress(lastXp_, s.level);
	s.remaining = table_.Remaining(lastXp_, s.level);
	s.sessionXp = static_cast<int64_t>(lastXp_ - startXp_);
	s.sessionLevels = table_.Position(lastXp_) - table_.Position(startXp_);

	const double t = s.activeSeconds;
	if (t >= config_.minDataSeconds && t > 0.0) {
		s.session = RateOver(static_cast<double>(grossXp_), grossLevels_, 0.0, 0.0, t);

		// gross at the window start = the last gain at or before it (0 when the session is younger than the window)
		const double span = std::min(Window(), t);
		const double windowStart = t - span;
		double fromGross = 0.0;
		double fromLevels = 0.0;
		for (const Point& p : gains_) {
			if (p.t > windowStart) {
				break;
			}
			fromGross = static_cast<double>(p.gross);
			fromLevels = p.grossLevels;
		}
		s.live = RateOver(static_cast<double>(grossXp_), grossLevels_, fromGross, fromLevels, span);
	}

	const std::optional<Rate>& etaRate = config_.etaFromSessionRate ? s.session : s.live;
	if (!s.atMaxLevel && s.remaining > 0 && etaRate.has_value() && etaRate->xpPerHour > 0.0) {
		s.etaSeconds = static_cast<double>(s.remaining) / (etaRate->xpPerHour / 3600.0);
	}
	return s;
}

}  // namespace bxb
