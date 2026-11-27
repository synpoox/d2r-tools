#include "telemetry.h"
#include "summary.h"
#include <cstdlib>
#include <iostream>

using namespace recap;
#define CHECK(x) do { if (!(x)) { std::cerr << "line " << __LINE__ << ": " << #x << '\n'; std::exit(1); } } while (false)
Amount C(int32_t n) { return {n, Evidence::Confirmed}; }
Resource R(int32_t before, int32_t after, int32_t lost, int32_t restored) {
	return {C(before), C(after), C(lost), C(restored)};
}
Event E(uint64_t seq, uint64_t ms, Kind kind, Resource life = {}, Resource mana = {}) {
	Event e; e.session = 7; e.sequence = seq; e.ms = ms; e.kind = kind;
	e.cause = Evidence::Confirmed; e.life = life; e.mana = mana; return e;
}
int main() {
	Rolling<8> b;
	b.Reset(7, 0);
	CHECK(!b.Freeze());
	CHECK(b.Push(E(1, 1000, Kind::Damage, R(1000, 900, 100, 0), R(500, 350, 150, 0))));
	// Absorption can restore life while the same hit deducts it.
	CHECK(b.Push(E(2, 2000, Kind::Damage, R(900, 850, 100, 50), R(350, 350, 0, 0))));
	CHECK(b.Push(E(3, 3000, Kind::Potion, R(850, 950, 0, 100), R(350, 400, 0, 50))));
	CHECK(b.Push(E(4, 11000, Kind::Damage, R(950, 0, 950, 0), R(400, 400, 0, 0))));
	auto death = E(5, 11000, Kind::Death, {{}, C(0), {}, {}}, {{}, C(400), {}, {}});
	death.fatalSequence = {4, Evidence::Confirmed};
	CHECK(b.Push(death));
	CHECK(!b.Push(E(6, 11001, Kind::Death)));
	auto r = b.Freeze();
	CHECK(r && r->timeline.size() == 5 && r->Complete());
	CHECK(r->life.lost == 1150 && r->life.restored == 150 && r->mana.lost == 150 && r->mana.restored == 50);
	CHECK(b.Freeze(1000)->timeline.size() == 2);
	CHECK(b.Freeze(1000)->boundaryUnconfirmed);
	CHECK(KillingBlow(*r) && KillingBlow(*r)->sequence == 4);
	// A confirmed source groups measured loss; inferred source is unknown.
	r->timeline[0].origin = {{1, 17, 2}, Evidence::Confirmed};
	r->timeline[0].skill = {12, Evidence::Confirmed};
	r->timeline[0].damage.postByElement[0] = C(120);
	r->timeline[1].origin = {{1, 17, 2}, Evidence::Inferred};
	const auto summary = Summarize(*r);
	CHECK(summary.damageEvents == 3 && summary.actualLifeLost.total == 1150);
	CHECK(summary.lossByOrigin.at(std::nullopt).total == 1050);
	CHECK(summary.lossByOrigin.at(Key(r->timeline[0].origin)).total == 100);
	CHECK(summary.lossBySkill.at(12).total == 100);
	CHECK(summary.postByElement[0].total == 120 && summary.postByElement[0].known == 1 && summary.postByElement[0].unknown == 2);
	CHECK(summary.overkill.known == 0 && summary.overkill.unknown == 3);
	b.Reset(7, 0);
	CHECK(b.Push(E(1, 100, Kind::Sample, {{}, C(1000), {}, {}})));
	CHECK(b.Push(E(2, 200, Kind::Damage, {C(900), C(700), {}, {}})));
	CHECK(!b.Push(E(2, 200, Kind::Damage)));
	auto uncertain = E(3, 300, Kind::Death); uncertain.cause = Evidence::Inferred;
	CHECK(!b.Push(uncertain));
	CHECK(b.Push(E(3, 300, Kind::Death)));
	r = b.Freeze();
	CHECK(r->life.Reconciled() && !r->life.Complete() && r->life.lost == 0 && r->life.unresolvedNet == -300);
	CHECK(r->rejected == 2 && !KillingBlow(*r) && r->deathResourcesUnconfirmed);
	CHECK(Summarize(*r).actualLifeLost.unknown == 1);
	b.Reset(7, 0);
	CHECK(b.Push(E(1, 0, Kind::Sample, {{}, C(1000), {}, {}}, {{}, C(500), {}, {}})));
	CHECK(b.Push(E(3, 1, Kind::Death, {{}, C(1000), {}, {}}, {{}, C(500), {}, {}})));
	CHECK(b.Freeze()->sequenceGap && !b.Freeze()->Complete());
	Rolling<2> tiny;
	tiny.Reset(7, 0);
	CHECK(tiny.Push(E(1, 1, Kind::Damage)));
	CHECK(tiny.Push(E(2, 2, Kind::Damage)));
	CHECK(tiny.Push(E(3, 3, Kind::Death)));
	CHECK(tiny.Freeze()->windowTruncated);
	tiny.Reset(7, 0);
	CHECK(tiny.Push(E(1, 1, Kind::Damage)));
	CHECK(tiny.Push(E(2, 20000, Kind::Damage)));
	CHECK(tiny.Push(E(3, 20001, Kind::Death)));
	CHECK(!tiny.Freeze()->windowTruncated);
	CHECK(!tiny.Freeze()->Complete());
	CHECK(r->timeline.size() == 3); // report owns its data independently
	std::cout << "telemetry tests passed\n";
}
