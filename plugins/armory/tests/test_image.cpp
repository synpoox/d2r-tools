#include "../src/native_image.h"
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <cstring>
using namespace armory::native;
void Check(bool ok) { if (!ok) std::abort(); }
bool Read(void* data,uint32_t at,std::span<uint8_t> out) {
    const auto& bytes=*static_cast<const std::vector<uint8_t>*>(data);
    if (at>bytes.size() || out.size()>bytes.size()-at) return false;
    std::memcpy(out.data(),bytes.data()+at,out.size()); return true;
}
int main(int argc,char** argv) {
    std::vector<uint8_t> bad(64); Check(!ResolveImage(1,Read,&bad).valid);
    if (argc==2) {
        std::ifstream input(argv[1],std::ios::binary); Check(static_cast<bool>(input));
        std::vector<uint8_t> bytes(std::istreambuf_iterator<char>{input},{});
        constexpr uintptr_t base=0x100000000ull;
        auto result=ResolveImage(base,Read,&bytes); Check(result.valid && result.stamp==0x6ab3782c);
        for (auto address:result.addresses) Check(address>=base && address<base+result.size);
        auto relocated=ResolveImage(base+0x10000000,Read,&bytes); Check(relocated.valid);
        for (size_t i=0;i<result.addresses.size();++i) Check(relocated.addresses[i]-result.addresses[i]==0x10000000);
        // Flip one signature byte, then require no fallback to its original address.
        bytes[result.addresses[0]-base]^=1;
        auto changed=ResolveImage(base,Read,&bytes); Check(changed.valid && !changed.addresses[0]);
    }
    std::cout<<"Armory image resolver checks passed\n";
}
