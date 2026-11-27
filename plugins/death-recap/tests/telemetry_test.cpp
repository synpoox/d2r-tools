#include "telemetry.h"
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
	CHECK(b.Push(E(5, 11000, Kind::Death)));
	CHECK(!b.Push(E(6, 11001, Kind::Death)));
	auto r = b.Freeze();
	CHECK(r && r->timeline.size() == 5 && r->Complete());
	CHECK(r->life.lost == 1150 && r->life.restored == 150 && r->mana.lost == 150 && r->mana.restored == 50);
	CHECK(b.Freeze(1000)->timeline.size() == 2);
	b.Reset(7, 0);
	CHECK(b.Push(E(1, 100, Kind::Sample, {{}, C(1000), {}, {}})));
	CHECK(b.Push(E(2, 200, Kind::Damage, {C(900), C(700), {}, {}})));
	CHECK(!b.Push(E(2, 200, Kind::Damage)));
	auto uncertain = E(3, 300, Kind::Death); uncertain.cause = Evidence::Inferred;
	CHECK(!b.Push(uncertain));
	CHECK(b.Push(E(3, 300, Kind::Death)));
	r = b.Freeze();
	CHECK(r->life.Reconciled() && !r->life.Complete() && r->life.lost == 0 && r->life.unresolvedNet == -300);
	CHECK(r->rejected == 2);
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
