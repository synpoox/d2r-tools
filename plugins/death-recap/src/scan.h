#pragma once
#define NOMINMAX
#include <windows.h>
#include <array>
#include <cstdint>
#include <cstring>
#include <cstdlib>

namespace recap::native {
struct Pattern {
	std::array<int, 256> bytes {};
	size_t size = 0;
	explicit Pattern(const char* s) noexcept {
		while (*s && size < bytes.size()) {
			if (*s == ' ') { ++s; continue; }
			if (*s == '?') { bytes[size++] = -1; while (*s == '?') ++s; }
			else { char* end = nullptr; const long n = std::strtol(s, &end, 16);
				if (end == s || n < 0 || n > 255) { size = 0; return; }
				bytes[size++] = static_cast<int>(n); s = end; }
		}
	}
};
inline const uint8_t* Find(uintptr_t base, const char* sig) noexcept {
	const Pattern p(sig);
	if (!p.size) return nullptr;
	const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
	const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
	const auto* sections = IMAGE_FIRST_SECTION(nt);
	const uint8_t* result = nullptr;
	for (unsigned i = 0; i < nt->FileHeader.NumberOfSections; ++i) {
		const auto& s = sections[i];
		if (!(s.Characteristics & IMAGE_SCN_MEM_EXECUTE) || s.Misc.VirtualSize < p.size) continue;
		const auto* start = reinterpret_cast<const uint8_t*>(base + s.VirtualAddress);
		for (size_t at = 0; at <= s.Misc.VirtualSize - p.size; ++at) {
			size_t n = 0;
			while (n < p.size && (p.bytes[n] < 0 || start[at + n] == p.bytes[n])) ++n;
			if (n != p.size) continue;
			if (result) return nullptr;
			result = start + at;
		}
	}
	return result;
}
inline const uint8_t* Target(const uint8_t* call) noexcept {
	if (!call || *call != 0xe8) return nullptr;
	int32_t rel; std::memcpy(&rel, call + 1, sizeof(rel));
	return call + 5 + rel;
}
inline bool Executable(uintptr_t base, const uint8_t* p) noexcept {
	const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
	const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
	const auto* sections = IMAGE_FIRST_SECTION(nt);
	for (unsigned i = 0; i < nt->FileHeader.NumberOfSections; ++i) {
		const uintptr_t a = base + sections[i].VirtualAddress, b = a + sections[i].Misc.VirtualSize;
		if (reinterpret_cast<uintptr_t>(p) >= a && reinterpret_cast<uintptr_t>(p) < b)
			return (sections[i].Characteristics & IMAGE_SCN_MEM_EXECUTE) != 0;
	}
	return false;
}
}
