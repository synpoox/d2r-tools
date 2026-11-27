#pragma once
#include "capture.h"

namespace recap {
// Display-only fixture: never reads or mutates a game unit, enters the native
// feed, or goes through the history/storage path.
inline Report PreviewReport(bool longNames = false) {
	Rolling<128> rolling;
	constexpr uint64_t session = 0x50524556;
	rolling.Reset(session, 0);
	Endpoints p {1000 * 256, 500 * 256, 1000 * 256, 500 * 256};
	uint64_t sequence = 0, fatal = 0;
	auto push = [&](Event e, uint64_t ms) { e.session = session; e.sequence = ++sequence; e.ms = ms; rolling.Push(e); };
	push(Sample(p), 0);
	constexpr int losses[] {160, 210, 280, 190};
	for (unsigned i = 1; i <= 40; ++i) {
		const uint64_t ms = i * 250;
		if (i == 13 || i == 27) {
			Event e; e.kind = Kind::Healing; e.cause = Evidence::Confirmed;
			e.life = Transition(p.life, p.life + 100 * 256, true); p.life += 100 * 256;
			e.mana = Transition(p.mana, p.mana, true); e.maxLife = Confirm(p.maxLife); e.maxMana = Confirm(p.maxMana);
			push(e, ms);
		}
		if (i % 8 == 0) {
			const int32_t lost = i == 40 ? p.life : losses[i / 8 - 1] * 256;
			Event e; e.kind = Kind::Damage; e.cause = Evidence::Confirmed;
			e.attacker = {{1, 1, 1}, Evidence::Confirmed}; e.dataBank = {3, Evidence::Confirmed};
			e.life = Transition(p.life, p.life - lost, true); p.life -= lost;
			e.mana = Transition(p.mana, p.mana - 50 * 256, true); p.mana -= 50 * 256;
			e.maxLife = Confirm(p.maxLife); e.maxMana = Confirm(p.maxMana);
			e.damage.post = Confirm(lost + (i == 40 ? 70 * 256 : 0)); e.damage.pre = Confirm(e.damage.post.value + 40 * 256);
			e.damage.overkill = Confirm(i == 40 ? 70 * 256 : 0);
			for (size_t n = 0; n < 6; ++n)
				e.damage.preByElement[n] = e.damage.postByElement[n] = Confirm(0);
			constexpr int weights[]{11, 20, 64, 0, 5, 0};
			int32_t preRemaining = e.damage.pre.value, postRemaining = e.damage.post.value;
			for (size_t n = 0; n < 5; ++n) {
				const int32_t pre = n == 4 ? preRemaining : e.damage.pre.value * weights[n] / 100;
				const int32_t post = n == 4 ? postRemaining : e.damage.post.value * weights[n] / 100;
				e.damage.preByElement[n] = Confirm(pre);
				e.damage.postByElement[n] = Confirm(post);
				preRemaining -= pre;
				postRemaining -= post;
			}
			push(e, ms); fatal = sequence;
		}
		push(Sample(p), ms);
	}
	auto death = Sample(p); death.kind = Kind::Death; death.cause = Evidence::Confirmed;
	death.fatalSequence = {fatal, Evidence::Confirmed}; push(death, 10000);
	auto report = *rolling.Freeze();
	report.names.push_back(
		{{1, 1, 1},
		 3,
		 longNames ? "Ancient Guardian of the Forgotten Sanctuary Beyond the Northern Mountains - localized name stress fixture" : "Preview attacker"});
	if (longNames) report.areaName = "The Ancient Sanctuary of the Forgotten Guardians Beyond the Northern Mountains - localized area stress fixture";
	return report;
}
}
