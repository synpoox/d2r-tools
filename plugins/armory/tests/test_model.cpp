#include "../src/model.h"
#include <cstdlib>
#include <iostream>
using namespace armory;
void Check(bool v) { if (!v) std::abort(); }
bool Allow(const Snapshot&, const Snapshot&) { return true; }
bool Reject(const Snapshot&, const Snapshot&) { return false; }
int main() {
    Fingerprint a{1}, b{2};
    Snapshot s{1, true, true, true, {{0, 2, 1}}, {{1, a, {-1, 1, 0}, 2, 1}, {2, b, {0, 0, 0}, 2, 1}}};
    const auto original = s;
    Loadout target{{1, b}};
    auto p = Prepare(s, target, Allow);
    Check(p.error == Error::None && p.moves.size() == 2 && s.items == original.items);
    Check(ApplyModel(s, p, Reject) == Error::Invalid && s.items == original.items && s.revision == 1);
    Check(ApplyModel(s, p, Allow) == Error::None && s.items[0].place == Place{0, 0, 0} && s.items[1].place == Place{-1, 1, 0});
    Check(ApplyModel(s, p, Allow) == Error::Stale);
    s = original;
    Check(Prepare(s, target, nullptr).error == Error::Unverified);
    Check(Prepare(s, {{1, Fingerprint{3}}}, Allow).error == Error::Missing);
    Check(Prepare(s, {{1, b}, {2, b}}, Allow).error == Error::Invalid);
    Check(Prepare(s, {{2, a}}, Allow).error == Error::Invalid);
    Check(Prepare(s, {{1, std::nullopt}}, Allow).error == Error::NoSpace);
    s.town = false;
    Check(Prepare(s, target, Allow).error == Error::UnsafeContext);
    s = original;
    s.grids.push_back({1, 2, 1});
    s.items.push_back({3, b, {1, 0, 0}, 2, 1});
    Check(Prepare(s, target, Allow).error == Error::Ambiguous);
    s = original;
    s.items[0].width = 3;
    Check(Prepare(s, target, Allow).error == Error::NoSpace);
    std::cout << "Armory model checks passed\n";
}
