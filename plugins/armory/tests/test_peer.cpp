#include "../src/peer_request.h"
#include <cstdlib>
#include <iostream>
using namespace armory;
void Check(bool ok){if(!ok) std::abort();}
bool Allow(const Snapshot&,const Snapshot&){return true;}
int main(){
    Fingerprint a{1},b{2};
    Snapshot state{42,true,true,true,{{0,2,1}},{{1,a,{-1,1,0},1,1},{2,b,{0,0,0},1,1}}};
    auto original=state.items;
    EquipRequest req{123,1,42,{{1,b}}};
    auto bytes=EncodeRequest(req); Check(bytes && DecodeRequest(*bytes)==req);
    for(size_t i=0;i<bytes->size();++i) Check(!DecodeRequest(std::string_view(*bytes).substr(0,i)));
    Check(!DecodeRequest(*bytes+"x")); Check(!DecodeRequest(std::string(1025,'x')));
    PeerSession peer{124};
    Check(HostPrepare(peer,req,state,Allow,1000).status==Admission::WrongSession);
    peer.token=123;
    auto first=HostPrepare(peer,req,state,Allow,1000); Check(first.status==Admission::Ready && first.plan->moves.size()==2);
    Check(state.items==original && state.revision==42);
    Check(HostPrepare(peer,req,state,Allow,1300).status==Admission::Replay);
    ++req.nonce; Check(HostPrepare(peer,req,state,Allow,1100).status==Admission::RateLimited);
    ++req.nonce; --req.revision; Check(HostPrepare(peer,req,state,Allow,1300).status==Admission::Stale);
    ++req.nonce; ++req.revision; state.town=false; Check(HostPrepare(peer,req,state,Allow,1600).status==Admission::Rejected);
    ++req.nonce; state.town=true; Check(HostPrepare(peer,req,state,nullptr,1900).status==Admission::Rejected);
    ++req.nonce; req.target={{1,Fingerprint{3}}}; Check(HostPrepare(peer,req,state,Allow,2200).status==Admission::Rejected);
    peer={456}; ++req.nonce; Check(HostPrepare(peer,req,state,Allow,2500).status==Admission::WrongSession);
    std::cout<<"Armory host request checks passed\n";
}
