// A console request to open a map, waiting for the game thread to serve it. Pure C++ (no Windows) so the tests can run
// it. The request carries a time limit: if the game never serves it (no game, no matching player), it expires and the
// player is told, rather than a stale request opening a portal minutes later.
#pragma once

#include <cstdint>
#include <string>
#include <utility>

namespace carto {

class OpenRequest {
public:
	enum class State { None, Expired, Ready };

	struct Taken {
		State state = State::None;
		std::string map;
	};

	// Replaces any earlier request.
	void Post(const std::string& map, uint64_t nowMs) {
		map_ = map;
		postedMs_ = nowMs;
		pending_ = true;
	}

	// Removes and returns the request: Ready with the map name, Expired when it waited longer than `ttlMs`, None when
	// there was none.
	auto Take(uint64_t nowMs, uint64_t ttlMs) -> Taken {
		Taken out;
		if (!pending_) {
			return out;
		}
		pending_ = false;
		out.state = nowMs - postedMs_ > ttlMs ? State::Expired : State::Ready;
		out.map = std::move(map_);
		map_.clear();
		return out;
	}

	void Clear() {
		pending_ = false;
		map_.clear();
	}

	auto Pending() const -> bool {
		return pending_;
	}

	// True when there is a request that has waited longer than `ttlMs`.
	auto Stale(uint64_t nowMs, uint64_t ttlMs) const -> bool {
		return pending_ && nowMs - postedMs_ > ttlMs;
	}

private:
	std::string map_;
	uint64_t postedMs_ = 0;
	bool pending_ = false;
};

}  // namespace carto
