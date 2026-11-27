#pragma once
#include <array>
#include <cstdint>
#include <span>

namespace armory {
// A conservative preflight, not D2R's equipment validator. Inputs must come
// from verified native facts; conditional, negative or unknown bonuses cannot
// be represented as known additive contributions.
struct Attributes { uint32_t strength{}, dexterity{}, level{}, classBit{}; };
struct GearRequirement {
    int slot{};
    uint32_t strength{}, dexterity{}, level{}, classes{};
    int32_t grantsStrength{}, grantsDexterity{};
    bool known{};
};
enum class RequirementResult { Satisfied, Unknown, Illegal, Dependency };
inline RequirementResult CheckRequirements(Attributes base, std::span<const GearRequirement> gear) {
    if (!base.classBit || (base.classBit & (base.classBit-1)) || gear.size()>12) return RequirementResult::Unknown;
    std::array<bool,13> slots{};
    for (const auto& item:gear) {
        if (item.slot<1 || item.slot>12 || slots[item.slot]) return RequirementResult::Illegal;
        slots[item.slot]=true;
        if (!item.known || item.grantsStrength<0 || item.grantsDexterity<0 || !item.classes) return RequirementResult::Unknown;
        if (!(item.classes & base.classBit) || item.level>base.level) return RequirementResult::Illegal;
    }
    // Both sets must be independently valid with common equipment. No item
    // can grant its own prerequisites, or borrow stats from the inactive set.
    for (int set=0;set<2;++set) {
        std::array<bool,13> active{};
        uint64_t str=base.strength,dex=base.dexterity;
        size_t remaining=0;
        for (const auto& item:gear) {
            const bool primary=item.slot==4 || item.slot==5;
            const bool alternate=item.slot==11 || item.slot==12;
            if ((set==0 && alternate) || (set==1 && primary)) active[item.slot]=true;
            else ++remaining;
        }
        while (remaining) {
            bool progress=false;
            for (const auto& item:gear) {
                if (active[item.slot] || item.strength>str || item.dexterity>dex) continue;
                active[item.slot]=true; --remaining; progress=true;
                str+=static_cast<uint32_t>(item.grantsStrength);
                dex+=static_cast<uint32_t>(item.grantsDexterity);
            }
            if (!progress) return RequirementResult::Dependency;
        }
    }
    return RequirementResult::Satisfied;
}
} // namespace armory
