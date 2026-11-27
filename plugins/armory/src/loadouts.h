#pragma once
#include "model.h"
#include <string>
#include <string_view>
#include <utility>

namespace armory {
inline constexpr size_t MaxLoadouts = 64, MaxName = 64, MaxScope = 512, MaxFile = 256 * 1024;
struct NamedLoadout { std::string name; Loadout slots; bool operator==(const NamedLoadout&) const = default; };
struct Book { std::string scope; std::vector<NamedLoadout> entries; bool operator==(const Book&) const = default; };

inline bool ValidText(std::string_view s, size_t max) {
    return !s.empty() && s.size() <= max && std::none_of(s.begin(), s.end(), [](unsigned char c) { return c < 32 || c == 127; });
}
inline bool ValidSlots(const Loadout& slots) {
    if (slots.empty() || slots.size() > 12) return false;
    for (const auto& [slot, ref] : slots) {
        if (slot < 1 || slot > 12 || (ref && !ref->code)) return false;
        if (ref) for (const auto& [other, candidate] : slots)
            if (other != slot && candidate && *candidate == *ref) return false;
    }
    return true;
}
inline bool ValidBook(const Book& book) {
    if (!ValidText(book.scope, MaxScope) || book.entries.size() > MaxLoadouts) return false;
    std::set<std::string> names;
    for (const auto& e : book.entries)
        if (!ValidText(e.name, MaxName) || !ValidSlots(e.slots) || !names.insert(e.name).second) return false;
    return true;
}
inline auto Find(Book& b, std::string_view name) {
    return std::find_if(b.entries.begin(), b.entries.end(), [&](const NamedLoadout& e) { return e.name == name; });
}
inline bool Add(Book& b, std::string name, Loadout slots) {
    if (!ValidBook(b) || !ValidText(name, MaxName) || !ValidSlots(slots) || b.entries.size() == MaxLoadouts || Find(b, name) != b.entries.end()) return false;
    b.entries.push_back({std::move(name), std::move(slots)});
    return true;
}
inline bool Rename(Book& b, std::string_view name, std::string replacement) {
    if (!ValidBook(b) || !ValidText(replacement, MaxName)) return false;
    auto it = Find(b, name);
    if (it == b.entries.end()) return false;
    if (it->name == replacement) return true;
    if (Find(b, replacement) != b.entries.end()) return false;
    it->name = std::move(replacement);
    return true;
}
inline bool Update(Book& b, std::string_view name, Loadout slots) {
    if (!ValidBook(b) || !ValidSlots(slots)) return false;
    auto it = Find(b, name);
    if (it == b.entries.end()) return false;
    it->slots = std::move(slots);
    return true;
}
inline bool Erase(Book& b, std::string_view name) {
    if (!ValidBook(b)) return false;
    auto it = Find(b, name);
    if (it == b.entries.end()) return false;
    b.entries.erase(it);
    return true;
}
inline std::optional<Loadout> Capture(const Snapshot& s, uint16_t mask) {
    if (!s.authoritative || !s.idle || !Valid(s) || !mask || (mask & ~0x0fffu)) return std::nullopt;
    Loadout result;
    for (int slot = 1; slot <= 12; ++slot) if (mask & (1u << (slot - 1))) {
        auto it = std::find_if(s.items.begin(), s.items.end(), [&](const Item& a) { return a.place == Place{-1, slot, 0}; });
        if (it == s.items.end()) result.emplace(slot, std::nullopt);
        else {
            if (std::count_if(s.items.begin(), s.items.end(), [&](const Item& a) { return a.fingerprint == it->fingerprint; }) != 1) return std::nullopt;
            result.emplace(slot, it->fingerprint);
        }
    }
    return ValidSlots(result) ? std::optional<Loadout>(std::move(result)) : std::nullopt;
}
inline bool Equipped(const Snapshot& s, const Loadout& slots) {
    if (!ValidSlots(slots)) return false;
    uint16_t mask = 0;
    for (const auto& [slot, ref] : slots) { (void)ref; mask |= static_cast<uint16_t>(1u << (slot - 1)); }
    auto current = Capture(s, mask);
    return current && *current == slots;
}
// Multiple partial loadouts may match. Never invent a single active selection.
inline std::vector<size_t> Active(const Book& b, const Snapshot& s) {
    std::vector<size_t> result;
    if (ValidBook(b)) for (size_t i = 0; i < b.entries.size(); ++i) if (Equipped(s, b.entries[i].slots)) result.push_back(i);
    return result;
}
inline std::optional<size_t> Cycle(const Book& b, std::optional<size_t> current, bool forward) {
    if (!ValidBook(b) || b.entries.empty()) return std::nullopt;
    const auto n = b.entries.size();
    if (!current || *current >= n) return forward ? 0 : n - 1;
    return forward ? (*current + 1) % n : (*current + n - 1) % n;
}

// Versioned, bounded, little-endian wire format. No runtime handles or locations.
namespace wire {
inline void Put(std::string& out, uint32_t n) { for (unsigned i = 0; i < 4; ++i) out.push_back(static_cast<char>((n >> (8*i)) & 255)); }
inline void Text(std::string& out, std::string_view s) { Put(out, static_cast<uint32_t>(s.size())); out.append(s); }
struct Reader {
    std::string_view bytes;
    bool ok = true;
    uint32_t Get() {
        if (bytes.size() < 4) { ok = false; return 0; }
        uint32_t n = 0;
        for (unsigned i = 0; i < 4; ++i) n |= uint32_t(static_cast<unsigned char>(bytes[i])) << (8*i);
        bytes.remove_prefix(4); return n;
    }
    std::string Text(size_t max) {
        auto n = Get();
        if (!ok || n > max || n > bytes.size()) { ok = false; return {}; }
        std::string s(bytes.substr(0, n)); bytes.remove_prefix(n); return s;
    }
};
inline void WriteFingerprint(std::string& out, const Fingerprint& f) {
    for (auto n : {f.code, f.generationSeed, f.itemSeed, f.quality, f.level, f.record}) Put(out, n);
    for (auto n : f.prefixes) Put(out, n);
    for (auto n : f.suffixes) Put(out, n);
    Put(out, f.ethereal ? 1 : 0);
}
inline Fingerprint ReadFingerprint(Reader& r) {
    Fingerprint f{};
    f.code = r.Get(); f.generationSeed = r.Get(); f.itemSeed = r.Get(); f.quality = r.Get(); f.level = r.Get(); f.record = r.Get();
    for (auto& n : f.prefixes) n = r.Get();
    for (auto& n : f.suffixes) n = r.Get();
    auto eth = r.Get(); if (eth > 1) r.ok = false; f.ethereal = eth == 1;
    return f;
}
} // namespace wire
inline std::optional<std::string> Encode(const Book& b) {
    if (!ValidBook(b)) return std::nullopt;
    std::string out = "ARMY";
    wire::Put(out, 1); wire::Text(out, b.scope); wire::Put(out, static_cast<uint32_t>(b.entries.size()));
    for (const auto& e : b.entries) {
        wire::Text(out, e.name); wire::Put(out, static_cast<uint32_t>(e.slots.size()));
        for (const auto& [slot, ref] : e.slots) {
            wire::Put(out, slot); wire::Put(out, ref ? 1 : 0);
            if (ref) wire::WriteFingerprint(out, *ref);
        }
    }
    return out;
}
inline std::optional<Book> Decode(std::string_view bytes, std::string_view expectedScope) {
    if (!ValidText(expectedScope, MaxScope) || bytes.size() > MaxFile || !bytes.starts_with("ARMY")) return std::nullopt;
    wire::Reader r{bytes.substr(4)};
    if (r.Get() != 1) return std::nullopt;
    Book b{r.Text(MaxScope), {}};
    if (b.scope != expectedScope) return std::nullopt;
    auto n = r.Get(); if (!r.ok || n > MaxLoadouts) return std::nullopt;
    for (uint32_t i = 0; i < n; ++i) {
        auto name = r.Text(MaxName); auto count = r.Get(); Loadout slots;
        if (!r.ok || !count || count > 12) return std::nullopt;
        for (uint32_t j = 0; j < count; ++j) {
            auto slot = r.Get(), present = r.Get();
            if (!r.ok || slot < 1 || slot > 12 || present > 1) return std::nullopt;
            std::optional<Fingerprint> ref;
            if (present) ref = wire::ReadFingerprint(r);
            if (!r.ok || !slots.emplace(static_cast<int>(slot), ref).second) return std::nullopt;
        }
        if (!Add(b, std::move(name), std::move(slots))) return std::nullopt;
    }
    return r.ok && r.bytes.empty() ? std::optional<Book>(std::move(b)) : std::nullopt;
}
} // namespace armory
