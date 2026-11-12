#include "game_memory.h"

#include <windows.h>

#include <cstdint>
#include <cstring>

namespace carto {

namespace {

// Splits [address, address + size) into the memory regions it touches. False if any part is not committed (or more than 4 regions).
struct Segment {
	uint8_t* start;
	size_t size;
	DWORD protect;
};
auto SegmentsOf(const void* address, size_t size, Segment (&out)[4], int* count) noexcept -> bool {
	*count = 0;
	auto* cursor = static_cast<uint8_t*>(const_cast<void*>(address));
	uint8_t* const end = cursor + size;
	while (cursor < end) {
		MEMORY_BASIC_INFORMATION info{};
		if (*count >= 4 || VirtualQuery(cursor, &info, sizeof(info)) == 0 || info.State != MEM_COMMIT || (info.Protect & (PAGE_NOACCESS | PAGE_GUARD)) != 0) {
			return false;
		}
		uint8_t* regionEnd = static_cast<uint8_t*>(info.BaseAddress) + info.RegionSize;
		if (regionEnd > end) {
			regionEnd = end;
		}
		out[(*count)++] = {cursor, static_cast<size_t>(regionEnd - cursor), info.Protect};
		cursor = regionEnd;
	}
	return true;
}

}  // namespace

auto Readable(const void* address, size_t size) noexcept -> bool {
	Segment segments[4];
	int count = 0;
	return SegmentsOf(address, size, segments, &count);
}

auto WriteGame(void* dst, const void* src, size_t size) noexcept -> bool {
	Segment segments[4];
	int count = 0;
	if (!SegmentsOf(dst, size, segments, &count)) {
		return false;
	}
	const DWORD writable = PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
	DWORD olds[4] = {};
	bool lifted[4] = {};
	bool ok = true;
	for (int i = 0; i < count && ok; ++i) {
		if ((segments[i].protect & writable) == 0) {
			ok = VirtualProtect(segments[i].start, segments[i].size, PAGE_READWRITE, &olds[i]) != 0;
			lifted[i] = ok;
		}
	}
	if (ok) {
		std::memcpy(dst, src, size);
	}
	for (int i = 0; i < count; ++i) {
		if (lifted[i]) {
			DWORD ignored = 0;
			VirtualProtect(segments[i].start, segments[i].size, olds[i], &ignored);
		}
	}
	return ok;
}

}  // namespace carto
