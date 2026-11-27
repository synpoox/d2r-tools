#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace armory::native {

// Research witnesses, not callable contracts. Never fall back to a fixed RVA.
// Rel32 and RIP-relative displacements are masked; match within executable code.
struct Signature { const char* name; std::string_view pattern; };
inline constexpr std::array Signatures{
    Signature{"Mover", "40 55 53 56 57 41 54 41 56 48 8D AC 24 58 56 FF FF B8 A8 AA 00 00 E8 ?? ?? ?? ?? 48 2B E0"},
    Signature{"Layout", "40 53 55 41 56 48 81 EC 60 01 00 00 48 8B 05 ?? ?? ?? ?? 48 33 C4 48 89 84 24 50 01 00 00 41 0F B6 E8"},
    Signature{"Requirements", "4C 89 4C 24 20 44 89 44 24 18 55 56 57 48 83 EC 60 33 F6 48 8B EA 48 8B F9 4D 85 C9 74 03"},
    Signature{"StatsChanged", "48 89 5C 24 08 57 48 83 EC 20 8B FA 48 8B D9 85 D2 74 05 E8 ?? ?? ?? ?? 48 85 DB 75 36 48 8D 4C 24 38 88 5C 24 38 E8 ?? ?? ?? ?? 84 C0 74 01 CC 85 FF 74 52"},
    Signature{"Activation", "44 89 44 24 18 57 41 57 48 81 EC 88 00 00 00 4C 8B F9 48 8B FA 48 8B CA E8 ?? ?? ?? ?? 85 C0"},
    Signature{"Equip", "40 55 53 56 57 41 56 48 8D 6C 24 C9 48 81 EC E0 00 00 00 48 8B 05 ?? ?? ?? ?? 48 33 C4 48 89 45 0F 49 63 D9"},
    Signature{"Unequip", "40 55 53 56 57 41 56 48 8D AC 24 F0 FE FF FF 48 81 EC 10 02 00 00 48 8B 05 ?? ?? ?? ?? 48 33 C4 48 89 85 E0 00 00 00"},
};

inline int Hex(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}
// The caller must supply an already validated, readable executable section.
// A missing or ambiguous match returns nullptr, never the first candidate.
inline const uint8_t* ResolveUnique(std::span<const uint8_t> text, std::string_view pattern) {
    std::vector<int> bytes;
    while (!pattern.empty()) {
        if (pattern.size() < 2) return nullptr;
        if (pattern.substr(0, 2) == "??") bytes.push_back(-1);
        else {
            const int hi = Hex(pattern[0]), lo = Hex(pattern[1]);
            if (hi < 0 || lo < 0) return nullptr;
            bytes.push_back(hi * 16 + lo);
        }
        if (pattern.size() == 2) break;
        if (pattern[2] != ' ' || pattern.size() == 3) return nullptr;
        pattern.remove_prefix(3);
    }
    if (bytes.empty() || bytes.size() > text.size() || bytes.front() < 0) return nullptr;
    const uint8_t* result = nullptr;
    for (size_t i = 0; i <= text.size() - bytes.size(); ++i) {
        if (text[i] != bytes.front()) continue;
        size_t j = 1;
        for (; j < bytes.size(); ++j)
            if (bytes[j] >= 0 && text[i + j] != bytes[j]) break;
        if (j != bytes.size()) continue;
        if (result) return nullptr;
        result = text.data() + i;
    }
    return result;
}
} // namespace armory::native
