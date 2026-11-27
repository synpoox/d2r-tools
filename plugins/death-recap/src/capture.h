#pragma once
#include "telemetry.h"
#include <atomic>

namespace recap {
// One authoritative producer and one UI consumer. A dropped event leaves a
// sequence gap; the death marker is retried by the producer until delivered.
template<size_t N> class Feed {
	std::array<Event, N> events_ {};
	std::atomic<size_t> read_ {0}, write_ {0};
public:
	bool Push(const Event& e) noexcept {
		const size_t w = write_.load(std::memory_order_relaxed), next = (w + 1) % N;
		if (next == read_.load(std::memory_order_acquire)) return false;
		events_[w] = e;
		write_.store(next, std::memory_order_release);
		return true;
	}
	bool Pop(Event& e) noexcept {
		const size_t r = read_.load(std::memory_order_relaxed);
		if (r == write_.load(std::memory_order_acquire)) return false;
		e = events_[r];
		read_.store((r + 1) % N, std::memory_order_release);
		return true;
	}
};
inline Amount Confirm(int32_t n) noexcept { return {n, Evidence::Confirmed}; }
struct Endpoints { int32_t life = 0, mana = 0, maxLife = 0, maxMana = 0; };
inline Event Sample(const Endpoints& p) noexcept {
	Event e;
	e.kind = Kind::Sample;
	e.life.after = Confirm(p.life); e.mana.after = Confirm(p.mana);
	e.maxLife = Confirm(p.maxLife); e.maxMana = Confirm(p.maxMana);
	return e;
}
// Only an isolated, single resource writer establishes gross amounts. Regen
// commits combine effects, including poison, so retain endpoints only there.
inline Resource Transition(int32_t before, int32_t after, bool isolated) noexcept {
	Resource r {Confirm(before), Confirm(after), {}, {}};
	const int64_t delta = int64_t(after) - before;
	if (isolated && delta >= -INT32_MAX && delta <= INT32_MAX) {
		r.lost = Confirm(static_cast<int32_t>(std::max<int64_t>(0, -delta)));
		r.restored = Confirm(static_cast<int32_t>(std::max<int64_t>(0, delta)));
	}
	return r;
}
class DeathLatch {
	bool dead_ = false;
public:
	void Reset() noexcept { dead_ = false; }
	bool ConfirmTransition(uint32_t before, uint32_t after) noexcept {
		if (dead_ || before == 0 || before == 17 || (after != 0 && after != 17)) return false;
		dead_ = true;
		return true;
	}
};
}
