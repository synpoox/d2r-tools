#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <map>
#include <optional>
#include <set>
#include <vector>

namespace armory {

// Persistent identity candidate; the adapter must establish the stability of
// these fields across save/reload. Runtime ids and locations are not saved.
struct Fingerprint {
    uint32_t code{}, generationSeed{}, itemSeed{}, quality{}, level{}, record{};
    std::array<uint32_t, 3> prefixes{}, suffixes{};
    bool ethereal{};
    auto operator==(const Fingerprint&) const -> bool = default;
};
struct Place {
    // page == -1: equipment slot x; otherwise a storage grid.
    int page{}, x{}, y{};
    auto operator==(const Place&) const -> bool = default;
};
struct Item {
    uint64_t id{};
    Fingerprint fingerprint{};
    Place place{};
    int width{}, height{};
    auto operator==(const Item&) const -> bool = default;
};
struct Grid { int page{}, width{}, height{}; };
struct Snapshot {
    uint64_t revision{};
    bool authoritative{}, town{}, idle{};
    std::vector<Grid> grids;
    std::vector<Item> items;
};
// Missing slot means keep; present null means empty; present fingerprint means equip.
using Loadout = std::map<int, std::optional<Fingerprint>>;
enum class Error { None, UnsafeContext, Invalid, Missing, Ambiguous, NoSpace, Unverified, Stale };
struct Move { uint64_t id; Place from, to; };
struct Plan { Error error; uint64_t revision; std::vector<Move> moves; };
using Validate = bool (*)(const Snapshot& before, const Snapshot& after);

inline bool Fits(const Item& item, const Grid& grid, int x, int y) {
    return item.width > 0 && item.height > 0 && x >= 0 && y >= 0 &&
        item.width <= grid.width && item.height <= grid.height &&
        x <= grid.width - item.width && y <= grid.height - item.height;
}
inline bool Overlap(const Item& a, const Item& b) {
    if (a.place.page != b.place.page) return false;
    if (a.place.page == -1) return a.place.x == b.place.x;
    return a.place.x < b.place.x + b.width && b.place.x < a.place.x + a.width &&
        a.place.y < b.place.y + b.height && b.place.y < a.place.y + a.height;
}
inline bool Valid(const Snapshot& s) {
    std::set<int> pages;
    for (const auto& g : s.grids)
        if (g.page < 0 || g.width < 1 || g.height < 1 || g.width > 16 || g.height > 16 || !pages.insert(g.page).second) return false;
    std::set<uint64_t> ids;
    for (size_t i = 0; i < s.items.size(); ++i) {
        const auto& a = s.items[i];
        if (!a.id || !ids.insert(a.id).second || a.width < 1 || a.height < 1 || a.width > 16 || a.height > 16) return false;
        if (a.place.page == -1) {
            if (a.place.x < 1 || a.place.x > 12 || a.place.y != 0) return false;
        } else {
            auto g = std::find_if(s.grids.begin(), s.grids.end(), [&](const Grid& v) { return v.page == a.place.page; });
            if (g == s.grids.end() || !Fits(a, *g, a.place.x, a.place.y)) return false;
        }
        for (size_t j = 0; j < i; ++j) if (Overlap(a, s.items[j])) return false;
    }
    return true;
}
// Bounded backtracking over outgoing items after all incoming cells are freed.
// No native mutation occurs here. Exhausting the search budget rejects safely.
inline bool Pack(std::vector<Item>& placed, const std::vector<Item>& outgoing,
                 const std::vector<Grid>& grids, size_t index, unsigned& budget) {
    if (index == outgoing.size()) return true;
    for (const auto& g : grids) for (int y = 0; y < g.height; ++y) for (int x = 0; x < g.width; ++x) {
        if (!budget) return false;
        --budget;
        auto a = outgoing[index];
        if (!Fits(a, g, x, y)) continue;
        a.place = {g.page, x, y};
        if (std::any_of(placed.begin(), placed.end(), [&](const Item& b) { return Overlap(a, b); })) continue;
        placed.push_back(a);
        if (Pack(placed, outgoing, grids, index + 1, budget)) return true;
        placed.pop_back();
    }
    return false;
}
inline Plan Prepare(const Snapshot& before, const Loadout& target, Validate validate) {
    auto fail = [&](Error e) { return Plan{e, before.revision, {}}; };
    if (!before.authoritative || !before.town || !before.idle) return fail(Error::UnsafeContext);
    if (!Valid(before)) return fail(Error::Invalid);
    if (!validate) return fail(Error::Unverified);
    std::map<uint64_t, int> selected;
    for (const auto& [slot, ref] : target) {
        if (slot < 1 || slot > 12) return fail(Error::Invalid);
        if (!ref) continue;
        uint64_t id = 0;
        for (const auto& a : before.items) if (a.fingerprint == *ref) {
            if (id) return fail(Error::Ambiguous);
            id = a.id;
        }
        if (!id) return fail(Error::Missing);
        if (!selected.emplace(id, slot).second) return fail(Error::Invalid);
    }
    auto after = before;
    after.items.clear();
    std::vector<Item> outgoing;
    for (auto a : before.items) {
        auto next = selected.find(a.id);
        if (next != selected.end()) {
            // A partial loadout cannot silently clear an unspecified slot.
            if (a.place.page == -1 && a.place.x != next->second && !target.contains(a.place.x)) return fail(Error::Invalid);
            a.place = {-1, next->second, 0};
            after.items.push_back(a);
        } else if (a.place.page == -1 && target.contains(a.place.x)) {
            outgoing.push_back(a);
        } else after.items.push_back(a);
    }
    std::stable_sort(outgoing.begin(), outgoing.end(), [](const Item& a, const Item& b) {
        return a.width * a.height > b.width * b.height;
    });
    unsigned budget = 200000;
    if (!Pack(after.items, outgoing, before.grids, 0, budget)) return fail(Error::NoSpace);
    if (!Valid(after) || !validate(before, after)) return fail(Error::Invalid);
    Plan result{Error::None, before.revision, {}};
    for (const auto& a : before.items) {
        auto b = std::find_if(after.items.begin(), after.items.end(), [&](const Item& v) { return v.id == a.id; });
        if (b == after.items.end()) return fail(Error::Invalid);
        if (!(a.place == b->place)) result.moves.push_back({a.id, a.place, b->place});
    }
    return result;
}

// This is a model-only transaction, NOT a native engine adapter. Copy then
// publish, only after validation; the caller owns synchronization.
inline Error ApplyModel(Snapshot& state, const Plan& plan, Validate validate) {
    if (plan.error != Error::None) return plan.error;
    if (!state.authoritative || !state.town || !state.idle) return Error::UnsafeContext;
    if (state.revision != plan.revision) return Error::Stale;
    if (!validate) return Error::Unverified;
    auto next = state;
    std::set<uint64_t> moved;
    for (const auto& m : plan.moves) {
        auto a = std::find_if(next.items.begin(), next.items.end(), [&](const Item& v) { return v.id == m.id; });
        if (a == next.items.end() || !moved.insert(m.id).second || !(a->place == m.from)) return Error::Stale;
        a->place = m.to;
    }
    if (!Valid(state) || !Valid(next) || !validate(state, next)) return Error::Invalid;
    if (!plan.moves.empty()) ++next.revision;
    state = std::move(next);
    return Error::None;
}
} // namespace armory
