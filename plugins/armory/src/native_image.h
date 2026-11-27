#pragma once
#include "native_signatures.h"
#include <algorithm>
#include <limits>

namespace armory::native {
// The reader copies memory from a module-relative offset. A live reader must
// refuse unreadable ranges, and callers must corroborate executable protection.
using ReadImage = bool (*)(void*, uint32_t, std::span<uint8_t>);
struct ResolvedImage {
    bool valid{};
    uint32_t stamp{}, size{};
    std::array<uintptr_t, Signatures.size()> addresses{};
};
inline uint32_t U32(std::span<const uint8_t> s, size_t at) {
    return uint32_t(s[at]) | (uint32_t(s[at+1]) << 8) | (uint32_t(s[at+2]) << 16) | (uint32_t(s[at+3]) << 24);
}
inline uint16_t U16(std::span<const uint8_t> s, size_t at) {
    return static_cast<uint16_t>(uint16_t(s[at]) | (uint16_t(s[at+1]) << 8));
}
inline bool Range(uint32_t offset, uint32_t size, uint32_t limit) {
    return offset <= limit && size <= limit - offset;
}
inline ResolvedImage ResolveImage(uintptr_t base, ReadImage read, void* opaque) {
    constexpr uint32_t MaxImage = 512u * 1024u * 1024u;
    if (!base || !read || base > std::numeric_limits<uintptr_t>::max() - MaxImage) return {};
    std::array<uint8_t,64> dos{};
    if (!read(opaque, 0, dos) || U16(dos,0) != 0x5a4d) return {};
    const auto pe = U32(dos,60);
    if (pe < dos.size() || pe > 65536) return {};
    std::array<uint8_t,24> coff{};
    if (!read(opaque,pe,coff) || U32(coff,0)!=0x4550 || U16(coff,4)!=0x8664) return {};
    const auto count=U16(coff,6), optSize=U16(coff,20);
    if (!count || count>96 || optSize<144 || optSize>4096) return {};
    std::vector<uint8_t> opt(optSize);
    if (!read(opaque,pe+24,opt) || U16(opt,0)!=0x20b || U32(opt,108)<4) return {};
    const auto size=U32(opt,56), table=pe+24+optSize;
    if (!size || size>MaxImage || !Range(table,uint32_t(count)*40,size)) return {};
    const auto exc=U32(opt,136), excSize=U32(opt,140);
    if (!exc || !excSize || excSize%12 || excSize>16u*1024u*1024u || !Range(exc,excSize,size)) return {};
    std::vector<uint8_t> sections(size_t(count)*40), entries(excSize);
    if (!read(opaque,table,sections) || !read(opaque,exc,entries)) return {};
    uint32_t code=0, length=0;
    for (uint16_t i=0;i<count;++i) {
        size_t at=size_t(i)*40;
        if (std::string_view(reinterpret_cast<const char*>(sections.data()+at),8)!=std::string_view(".text\0\0\0",8)) continue;
        if (code || !(U32(sections,at+36)&0x20000000u)) return {};
        length=U32(sections,at+8); code=U32(sections,at+12);
    }
    if (!code || !length || length>128u*1024u*1024u || !Range(code,length,size)) return {};
    std::vector<uint8_t> text(length);
    if (!read(opaque,code,text)) return {};
    ResolvedImage result{true,U32(coff,8),size,{}};
    for (size_t i=0;i<Signatures.size();++i) {
        const auto* hit=ResolveUnique(text,Signatures[i].pattern);
        if (!hit) continue;
        auto start=code+static_cast<uint32_t>(hit-text.data());
        for (size_t at=0;at<entries.size();at+=12) {
            if (U32(entries,at)==start && U32(entries,at+4)>start && U32(entries,at+4)<=code+length) {
                result.addresses[i]=base+start; break;
            }
        }
    }
    return result;
}
} // namespace armory::native
