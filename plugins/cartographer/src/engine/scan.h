// Finding engine functions by byte signature: shared by the portal hooks and the maze hooks. A part is one function (or a
// hooked entry); its address is found once at load, never by a fixed address. Notes: docs/reverse-engineering/cartographer.md (the index).
#pragma once

#include "portal.h"

#include <D2RLPlugin/api.h>

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace carto {

struct CodeRange {
	const uint8_t* start;
	size_t size;
};

// The executable sections of the exe at `exeBase` (up to `max`).
auto CodeRanges(uintptr_t exeBase, CodeRange* out, int max) noexcept -> int;

template <typename T>
auto At(const void* base, int offset) noexcept -> T {
	T value{};
	std::memcpy(&value, static_cast<const uint8_t*>(base) + offset, sizeof(T));
	return value;
}

// One part: a function (or a hooked entry) found by signature. Signatures are anchored at the function's first byte.
// The signatures are in core/part_signatures.h (build 93847); the part table in docs/reverse-engineering/cartographer.md
// records each one, and signatures.txt can replace them without a rebuild.
struct Part {
	const char* name;
	const char* signature;  // nullptr: none; only a signatures.txt line can then turn the part on
	uint32_t hookSize;      // entry bytes copied for an inline hook; 0: only called
	bool required;
	const uint8_t* address;
	PartStatus status;
	uint8_t guard[32];  // the signature's own first hookSize bytes: what D2RLoader re-checks at the entry
	uint32_t guardExact = 0;  // leading bytes of guard the signature pinned with no wildcard (hookSize once resolved, else 0)
};

// What to do with the result of InstallInlineHook for `part`: true only when the hook is installed and the loader returned the
// original. An installed hook with no original is taken out again by writing the guard's exact original bytes back (only when
// they are all known; see hookguard::canRestoreHook), so the game's own function runs; the part is then off. Sets the part's
// status and logs. Call it at install time, before the game thread can be inside the function.
auto HookOutcome(Part& part, bool installed, bool haveOriginal) noexcept -> bool;

// Finds one part by its signature (or the one from signatures.txt) and checks its guards; sets its address and status.
void ResolvePart(const D2RL::PluginContext* context, const std::vector<SignatureOverride>& overrides, const CodeRange* ranges, int count, Part& part);

}  // namespace carto
