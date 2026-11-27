#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace recap {

// All resource and damage quantities are raw 1/256 units. Unknown is not zero.
enum class Evidence : uint8_t { Unavailable, Inferred, Confirmed };
template<class T> struct Field {
	T value {};
	Evidence evidence = Evidence::Unavailable;
	bool Confirmed() const noexcept { return evidence == Evidence::Confirmed; }
};
using Amount = Field<int32_t>;
enum class Kind : uint8_t { Damage, Healing, Potion, Regen, Cost, State, Sample, Death, Other };
enum class Element : uint8_t { Physical, Fire, Lightning, Cold, Poison, Magic, Count };
struct Unit {
	uint32_t type = 0, id = 0, classId = 0;
};
struct Resource {
	Amount before, after, lost, restored;
};
struct Damage {
	Amount pre, post, overkill;
	std::array<Amount, static_cast<size_t>(Element::Count)> preByElement {}, postByElement {};
};
struct Defense {
	std::array<Field<int32_t>, 16> stats {};
	std::array<uint32_t, 16> states {};
	uint8_t stateCount = 0;
	Evidence stateEvidence = Evidence::Unavailable;
	bool statesTruncated = false;
};
struct Event {
	uint64_t session = 0, sequence = 0, ms = 0;
	Kind kind = Kind::Other;
	Evidence cause = Evidence::Unavailable;
	Field<Unit> attacker, origin, missile;
	Field<uint32_t> skill;
	std::array<Field<uint32_t>, 16> modifiers {};
	Resource life, mana;
	Damage damage;
	Defense defense;
};
struct Accounting {
	std::optional<int32_t> first, last;
	int64_t lost = 0, restored = 0, unresolvedNet = 0;
	uint64_t unresolvedTransitions = 0, missingEndpoints = 0;
	bool Reconciled() const noexcept {
		return first && last && int64_t(*last) - *first == restored - lost + unresolvedNet;
	}
	bool Complete() const noexcept {
		return Reconciled() && unresolvedTransitions == 0 && missingEndpoints == 0;
	}
};
struct Report {
	uint64_t session = 0, deathMs = 0, windowStartMs = 0;
	bool windowTruncated = false;
	uint64_t rejected = 0;
	std::vector<Event> timeline;
	Accounting life, mana;
	bool Complete() const noexcept {
		return !windowTruncated && rejected == 0 && life.Complete() && mana.Complete();
	}
};

// Only confirmed endpoints participate in accounting. A net change does not prove
// gross loss/healing; even a zero net change can hide simultaneous changes.
inline void Account(Accounting& a, const Resource& r, Kind kind) noexcept {
	auto gap = [&](int32_t value) {
		if (!a.first) a.first = value;
		if (a.last && *a.last != value) {
			a.unresolvedNet += int64_t(value) - *a.last;
			++a.unresolvedTransitions;
		}
		a.last = value;
	};
	if (r.before.Confirmed()) gap(r.before.value);
	if (!r.after.Confirmed()) {
		if (kind != Kind::State && kind != Kind::Death) ++a.missingEndpoints;
		return;
	}
	if (!r.before.Confirmed()) {
		if (kind != Kind::Sample) ++a.missingEndpoints;
		gap(r.after.value);
		return;
	}
	const int64_t net = int64_t(r.after.value) - r.before.value;
	if (r.lost.Confirmed() && r.restored.Confirmed() && r.lost.value >= 0 && r.restored.value >= 0 &&
	    int64_t(r.restored.value) - r.lost.value == net) {
		a.lost += r.lost.value;
		a.restored += r.restored.value;
	} else {
		a.unresolvedNet += net;
		++a.unresolvedTransitions;
	}
	a.last = r.after.value;
}

// One producer per life/session. The producer must canonicalize nested engine
// callbacks to one monotonically sequenced event per committed transition.
// Reset and Freeze must run on that same thread or after producer quiescence.
// No allocation, locking, pointers to engine objects, or I/O on Push.
template<size_t Capacity> class Rolling {
	static_assert(Capacity > 0);
	std::array<Event, Capacity> entries_ {};
	size_t head_ = 0, count_ = 0;
	uint64_t session_ = 0, start_ = 0, lastMs_ = 0, lastSeq_ = 0, rejected_ = 0;
	std::optional<uint64_t> overwrittenMs_;
	bool active_ = false, dead_ = false;
public:
	void Reset(uint64_t session, uint64_t ms) noexcept {
		head_ = count_ = 0;
		session_ = session; start_ = lastMs_ = ms; lastSeq_ = rejected_ = 0;
		overwrittenMs_.reset(); active_ = true; dead_ = false;
	}
	bool Push(const Event& e) noexcept {
		if (!active_ || dead_ || e.session != session_) return false;
		if (e.sequence <= lastSeq_ || e.ms < lastMs_ ||
		    (e.kind == Kind::Death && e.cause != Evidence::Confirmed)) {
			++rejected_;
			return false;
		}
		if (count_ == Capacity) {
			overwrittenMs_ = entries_[head_].ms;
			head_ = (head_ + 1) % Capacity;
			--count_;
		}
		entries_[(head_ + count_) % Capacity] = e;
		++count_; lastSeq_ = e.sequence; lastMs_ = e.ms;
		dead_ = e.kind == Kind::Death;
		return true;
	}
	std::optional<Report> Freeze(uint64_t windowMs = 10000) const {
		if (!dead_) return std::nullopt;
		windowMs = std::clamp<uint64_t>(windowMs, 1000, 60000);
		Report r;
		r.session = session_; r.deathMs = lastMs_;
		r.windowStartMs = lastMs_ >= windowMs ? lastMs_ - windowMs : 0;
		r.windowTruncated = start_ > r.windowStartMs || (overwrittenMs_ && *overwrittenMs_ >= r.windowStartMs);
		r.rejected = rejected_;
		r.timeline.reserve(count_);
		for (size_t i = 0; i < count_; ++i) {
			const auto& e = entries_[(head_ + i) % Capacity];
			if (e.ms < r.windowStartMs) continue;
			r.timeline.push_back(e);
			Account(r.life, e.life, e.kind);
			Account(r.mana, e.mana, e.kind);
		}
		return r;
	}
};

}  // namespace recap
