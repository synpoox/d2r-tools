#pragma once
#include "loadouts.h"
namespace armory {
// The authenticated peer and player are supplied by the host's SDK lookup,
// never by the packet. Revision and session token come from the host.
struct EquipRequest {
    uint64_t session{}, nonce{}, revision{};
    Loadout target;
    bool operator==(const EquipRequest&) const = default;
};
inline constexpr size_t MaxRequestBytes = 1024;
namespace wire {
inline void Put64(std::string& out,uint64_t n) { Put(out,static_cast<uint32_t>(n)); Put(out,static_cast<uint32_t>(n>>32)); }
inline uint64_t Get64(Reader& r) { const uint64_t low=r.Get(); return low | (uint64_t(r.Get())<<32); }
}
inline std::optional<std::string> EncodeRequest(const EquipRequest& request) {
    if (!request.session || !request.nonce || !ValidSlots(request.target)) return std::nullopt;
    std::string out="ARQ1";
    wire::Put64(out,request.session); wire::Put64(out,request.nonce); wire::Put64(out,request.revision);
    wire::Put(out,static_cast<uint32_t>(request.target.size()));
    for (const auto& [slot,ref]:request.target) {
        wire::Put(out,slot); wire::Put(out,ref?1:0);
        if (ref) wire::WriteFingerprint(out,*ref);
    }
    return out.size()<=MaxRequestBytes ? std::optional<std::string>(std::move(out)) : std::nullopt;
}
inline std::optional<EquipRequest> DecodeRequest(std::string_view bytes) {
    if (bytes.size()>MaxRequestBytes || !bytes.starts_with("ARQ1")) return std::nullopt;
    wire::Reader r{bytes.substr(4)};
    EquipRequest out{wire::Get64(r),wire::Get64(r),wire::Get64(r),{}};
    auto count=r.Get();
    if (!r.ok || !out.session || !out.nonce || !count || count>12) return std::nullopt;
    for (uint32_t i=0;i<count;++i) {
        auto slot=r.Get(),present=r.Get();
        if (!r.ok || slot<1 || slot>12 || present>1) return std::nullopt;
        std::optional<Fingerprint> ref;
        if (present) ref=wire::ReadFingerprint(r);
        if (!r.ok || !out.target.emplace(static_cast<int>(slot),ref).second) return std::nullopt;
    }
    return r.ok && r.bytes.empty() && ValidSlots(out.target) ? std::optional<EquipRequest>(std::move(out)) : std::nullopt;
}
struct PeerSession {
    uint64_t token{}, lastNonce{}, lastAttemptMs{};
    bool attempted{};
};
enum class Admission { Ready, WrongSession, Replay, RateLimited, Stale, Rejected };
struct HostDecision { Admission status; std::optional<Plan> plan; };
// This is admission and planning only. The caller must serialize by authenticated
// player, obtain a fresh authoritative snapshot and use a real native validator.
// Disconnect removes the PeerSession; no queued plan survives a session change.
inline HostDecision HostPrepare(PeerSession& peer,const EquipRequest& request,const Snapshot& state,Validate validate,uint64_t nowMs) {
    auto deny=[](Admission a){return HostDecision{a,std::nullopt};};
    if (!peer.token || peer.token!=request.session) return deny(Admission::WrongSession);
    if (!request.nonce || request.nonce<=peer.lastNonce) return deny(Admission::Replay);
    peer.lastNonce=request.nonce;
    constexpr uint64_t MinIntervalMs=250;
    if (peer.attempted && (nowMs<peer.lastAttemptMs || nowMs-peer.lastAttemptMs<MinIntervalMs)) return deny(Admission::RateLimited);
    peer.attempted=true; peer.lastAttemptMs=nowMs;
    if (request.revision!=state.revision) return deny(Admission::Stale);
    if (!ValidSlots(request.target)) return deny(Admission::Rejected);
    auto plan=Prepare(state,request.target,validate);
    if (plan.error!=Error::None) return {Admission::Rejected,std::move(plan)};
    return {Admission::Ready,std::move(plan)};
}
} // namespace armory
