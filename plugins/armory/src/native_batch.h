#pragma once
#include "model.h"
#include <cstddef>
#include <span>
#include <type_traits>
namespace armory::native {
// Static shape corroborated by the location constructors and validator. This
// is deliberately data-only: it grants no permission to invoke the mover.
struct Location {
    uint32_t kind{}, page{};
    uint16_t x{}, y{};
    uint32_t body{};
    bool operator==(const Location&) const = default;
};
static_assert(std::is_standard_layout_v<Location> && sizeof(Location)==16);
static_assert(offsetof(Location,kind)==0 && offsetof(Location,page)==4 && offsetof(Location,x)==8 && offsetof(Location,y)==10 && offsetof(Location,body)==12);
inline std::optional<Location> EncodeProbeLocation(Place p) {
    // The first probe is restricted to normal inventory and primary body slots.
    if (p.page==0 && p.x>=0 && p.x<16 && p.y>=0 && p.y<16)
        return Location{0,0,static_cast<uint16_t>(p.x),static_cast<uint16_t>(p.y),0};
    if (p.page==-1 && p.x>=1 && p.x<=10 && p.y==0)
        return Location{1,255,static_cast<uint16_t>(p.x),0,static_cast<uint32_t>(p.x)};
    return std::nullopt;
}
struct LiveId { uint64_t modelId; uint32_t runtimeId; };
struct ProbeBatch {
    std::array<Location,2> sources{}, destinations{};
    std::array<uint32_t,2> expectedIds{};
    // A nonzero native flag bypasses at least one requirements check. Never set it.
    static constexpr uint8_t flag=0;
};
// Prepare only the two-item exchange shape for a disposable test character.
// Validation and actual ownership are still the caller's responsibility.
inline std::optional<ProbeBatch> EncodePair(const Plan& plan, std::span<const LiveId> ids) {
    if (plan.error!=Error::None || plan.moves.size()!=2 || ids.size()!=2) return std::nullopt;
    const auto& a=plan.moves[0]; const auto& b=plan.moves[1];
    if (!a.id || !b.id || a.id==b.id || a.from!=b.to || a.to!=b.from || a.from==a.to ||
        !((a.from.page==-1 && a.to.page==0) || (a.from.page==0 && a.to.page==-1))) return std::nullopt;
    // Exclude hand slots from the initial proof: paired-hand state and
    // weapon-set effects are not yet verified. A ring/armor swap is sufficient.
    const auto slot=a.from.page==-1?a.from.x:a.to.x;
    if (slot==4 || slot==5) return std::nullopt;
    ProbeBatch result{};
    for (size_t i=0;i<2;++i) {
        const auto& move=plan.moves[i];
        auto from=EncodeProbeLocation(move.from),to=EncodeProbeLocation(move.to);
        if (!from || !to) return std::nullopt;
        auto id=std::find_if(ids.begin(),ids.end(),[&](const LiveId& v){return v.modelId==move.id;});
        if (id==ids.end() || id->runtimeId==0xffffffffu) return std::nullopt;
        result.sources[i]=*from; result.destinations[i]=*to; result.expectedIds[i]=id->runtimeId;
    }
    if (result.expectedIds[0]==result.expectedIds[1]) return std::nullopt;
    return result;
}
} // namespace armory::native
