#include "../src/native_batch.h"
#include <cstdlib>
#include <iostream>
using namespace armory;
using namespace armory::native;
void Check(bool ok) { if (!ok) std::abort(); }
int main() {
    Check(EncodeProbeLocation({0,2,3})==Location{0,0,2,3,0});
    Check(EncodeProbeLocation({-1,6,0})==Location{1,255,6,0,6});
    Check(!EncodeProbeLocation({4,2,3})); Check(!EncodeProbeLocation({-1,11,0}));
    Check(!EncodeProbeLocation({0,16,0})); Check(!EncodeProbeLocation({0,-1,0}));
    Plan plan{Error::None,1,{{1,{-1,6,0},{0,2,3}},{2,{0,2,3},{-1,6,0}}}};
    std::array<LiveId,2> ids{{{2,20},{1,10}}};
    auto batch=EncodePair(plan,ids); Check(batch && batch->expectedIds[0]==10 && batch->expectedIds[1]==20 && batch->flag==0);
    Check(batch->sources[0]==batch->destinations[1] && batch->sources[1]==batch->destinations[0]);
    ids[0].runtimeId=10; Check(!EncodePair(plan,ids)); ids[0].runtimeId=20;
    ids[0].modelId=3; Check(!EncodePair(plan,ids)); ids[0].modelId=2;
    plan.moves[0].from.x=4; plan.moves[1].to.x=4; Check(!EncodePair(plan,ids));
    plan.moves[0].from.x=6; plan.moves[1].to.x=6; plan.moves[1].from.x=1; Check(!EncodePair(plan,ids));
    std::cout<<"Armory native batch shape checks passed\n";
}
