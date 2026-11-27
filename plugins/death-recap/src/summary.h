#pragma once
#include "telemetry.h"
#include <map>
#include <tuple>

namespace recap {

// Measured subtotal, not a complete total unless coverage says so.
struct Measure {
	int64_t total = 0;
	uint64_t known = 0, unknown = 0;
	void Add(const Amount& amount) noexcept {
		if (amount.Confirmed() && amount.value >= 0) {
			total += amount.value;
			++known;
		} else {
			++unknown;
		}
	}
};
using UnitKey = std::optional<std::tuple<uint32_t, uint32_t, uint32_t>>;
inline UnitKey Key(const Field<Unit>& field) noexcept {
	if (!field.Confirmed()) return std::nullopt;
	return std::tuple{field.value.type, field.value.id, field.value.classId};
}
struct Summary {
	uint64_t damageEvents = 0;
	Measure pre, post, actualLifeLost, actualManaLost, overkill;
	std::array<Measure, static_cast<size_t>(Element::Count)> preByElement {}, postByElement {};
	std::map<UnitKey, Measure> lossByAttacker, lossByOrigin;
	std::map<std::optional<uint32_t>, Measure> lossBySkill;
};

// Off the capture path. Do not distribute actual life loss across elements:
// pre/post elemental amounts do not establish how much life each removed.
inline Summary Summarize(const Report& report) {
	Summary s;
	for (const auto& e : report.timeline) {
		if (e.kind != Kind::Damage) continue;
		++s.damageEvents;
		const Amount life = GrossKnown(e.life) ? e.life.lost : Amount{};
		const Amount mana = GrossKnown(e.mana) ? e.mana.lost : Amount{};
		s.pre.Add(e.damage.pre);
		s.post.Add(e.damage.post);
		s.overkill.Add(e.damage.overkill);
		s.actualLifeLost.Add(life);
		s.actualManaLost.Add(mana);
		for (size_t i = 0; i < s.preByElement.size(); ++i) {
			s.preByElement[i].Add(e.damage.preByElement[i]);
			s.postByElement[i].Add(e.damage.postByElement[i]);
		}
		s.lossByAttacker[Key(e.attacker)].Add(life);
		s.lossByOrigin[Key(e.origin)].Add(life);
		const auto skill = e.skill.Confirmed() ? std::optional<uint32_t>(e.skill.value) : std::nullopt;
		s.lossBySkill[skill].Add(life);
	}
	return s;
}

}  // namespace recap
