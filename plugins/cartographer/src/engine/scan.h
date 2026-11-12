// Finding engine code by byte signature in the game image: the exe's executable sections, a unique-match search and a
// check that an address starts a function. Used by the map stats part (stat_apply.cpp). portal.cpp keeps its own copy
// of the same three helpers; moving it onto this header is a cleanup for a change that already touches portal.cpp.
#pragma once

#include "core/sig.h"

#include <windows.h>

#include <cstddef>
#include <cstdint>
#include <string>

namespace carto::scan {

struct CodeRange {
	const uint8_t* start;
	size_t size;
};

inline auto CodeRanges(uintptr_t exeBase, CodeRange* out, int max) noexcept -> int {
	const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(exeBase);
	if (exeBase == 0 || dos->e_magic != IMAGE_DOS_SIGNATURE) {
		return 0;
	}
	const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(exeBase + static_cast<uintptr_t>(dos->e_lfanew));
	if (nt->Signature != IMAGE_NT_SIGNATURE) {
		return 0;
	}
	const IMAGE_SECTION_HEADER* section = IMAGE_FIRST_SECTION(nt);
	int n = 0;
	for (WORD i = 0; i < nt->FileHeader.NumberOfSections && n < max; ++i, ++section) {
		if ((section->Characteristics & IMAGE_SCN_MEM_EXECUTE) != 0 && section->Misc.VirtualSize > 0) {
			out[n++] = {reinterpret_cast<const uint8_t*>(exeBase + section->VirtualAddress), section->Misc.VirtualSize};
		}
	}
	return n;
}

// The one place `signature` matches. Fails (with the reason) when it matches nowhere or more than once.
inline auto FindUnique(const CodeRange* ranges, int count, const char* signature, Pattern& pattern, std::string* why) noexcept -> const uint8_t* {
	if (!ParsePattern(signature, &pattern)) {
		*why = "the signature is not valid (hex bytes and ?? separated by spaces, not starting with ??)";
		return nullptr;
	}
	const uint8_t* found = nullptr;
	int matches = 0;
	for (int i = 0; i < count && matches < 2; ++i) {
		size_t at = 0;
		const int n = FindPattern(ranges[i].start, ranges[i].size, pattern, &at);
		if (n > 0 && matches == 0) {
			found = ranges[i].start + at;
		}
		matches += n;
	}
	if (matches != 1) {
		*why = matches == 0 ? "the signature matches nowhere in this game's code" : "the signature matches more than once in this game's code";
		return nullptr;
	}
	return found;
}

// True when `address` is the first byte of a function in the exe's .pdata. *known: whether that could be checked.
inline auto IsFunctionEntry(uintptr_t exeBase, const uint8_t* address, bool* known) noexcept -> bool {
	*known = false;
	const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(exeBase);
	if (exeBase == 0 || dos->e_magic != IMAGE_DOS_SIGNATURE) {
		return false;
	}
	const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(exeBase + static_cast<uintptr_t>(dos->e_lfanew));
	const IMAGE_DATA_DIRECTORY& dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXCEPTION];
	if (dir.VirtualAddress == 0 || dir.Size < sizeof(RUNTIME_FUNCTION)) {
		return false;
	}
	*known = true;
	const auto* table = reinterpret_cast<const RUNTIME_FUNCTION*>(exeBase + dir.VirtualAddress);
	const size_t n = dir.Size / sizeof(RUNTIME_FUNCTION);
	const auto rva = static_cast<DWORD>(reinterpret_cast<uintptr_t>(address) - exeBase);
	size_t lo = 0, hi = n;
	while (lo < hi) {
		const size_t mid = lo + (hi - lo) / 2;
		if (table[mid].BeginAddress == rva) {
			return true;
		}
		if (table[mid].BeginAddress < rva) {
			lo = mid + 1;
		} else {
			hi = mid;
		}
	}
	return false;
}

}  // namespace carto::scan
