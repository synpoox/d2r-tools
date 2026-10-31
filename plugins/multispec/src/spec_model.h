// MultiSpec data model, file format and reconciliation (docs/design.md sections 2, 3 and 5).
//
// Portable C++: no Windows or game calls, so tests/ build and run it on any system.
#pragma once

#include <cstddef>
#include <cstdint>

namespace ms {

inline constexpr int MaxSkills = 128;  // class skills; retail has 30, extended saves more
inline constexpr int MaxSpecs = 99;  // a soft cap: the "spec N" command and the panel's slot take two digits
inline constexpr int MaxName = 32;
inline constexpr int FileVersion = 1;
inline constexpr size_t MaxFileBytes = 65536;  // fits MaxSpecs specs of MaxSkills three-digit levels

enum class SpecState : uint8_t { Ok, Adjusted, Invalid };

struct Spec {
	uint8_t levels[MaxSkills];
	SpecState state;
};

// One character's specs: the content of <name>.multispec.
struct CharSpecs {
	char name[MaxName];
	int32_t charClass;
	int32_t skillCount;
	int16_t skillIds[MaxSkills];  // the class's skills in D2R's order (the save's "if" order)
	int32_t specCount;            // specs stored (may exceed the configured count: extra specs are kept)
	int32_t active;               // 1-based
	Spec specs[MaxSpecs];
	int32_t pendingFrom;          // 0 = no swap in progress
	int32_t pendingTo;
};

// The live character, read from the game.
struct LiveChar {
	char name[MaxName];
	int32_t charClass;
	int32_t skillCount;
	int16_t skillIds[MaxSkills];
	uint8_t levels[MaxSkills];    // hard levels
	int32_t unspent;              // stat 5 newskills (base)
};

auto Crc32(const void* data, size_t size) noexcept -> uint32_t;
auto SumLevels(const uint8_t* levels, int32_t count) noexcept -> int32_t;
auto SameLevels(const uint8_t* a, const uint8_t* b, int32_t count) noexcept -> bool;
auto StateName(SpecState state) noexcept -> const char*;

// Total skill points the character has earned: hard levels plus unspent (skpoints cost 1 per level).
auto TotalPoints(const LiveChar& live) noexcept -> int32_t;

// Writes the file text into `out` (NUL-terminated). Returns its length, or 0 when it does not fit.
auto Serialize(const CharSpecs& specs, char* out, size_t size) noexcept -> size_t;

// Parses file text. False (with a reason) on any error: bad CRC, unknown version, bad counts, junk.
auto Parse(const char* text, size_t size, CharSpecs* out, char* error, size_t errorSize) noexcept -> bool;

// spec_count from the plugin's TOML text ([multispec] spec_count = N); `fallback` when absent or out of 1..MaxSpecs.
auto ConfigSpecCount(const char* toml, int32_t fallback) noexcept -> int32_t;

// "savepath" value from a modinfo.json text ("" when absent). False only when `out` is too small.
auto ModinfoSavePath(const char* json, char* out, size_t size) noexcept -> bool;

// A character name safe as a file name: letters, digits, '-' and '_' (D2R's name rules), 2-15 characters.
auto ValidName(const char* name) noexcept -> bool;

enum class Reconciled {
	Created,          // no file: spec 1 = live, the others empty
	Recreated,        // file was for another character (name or class differ): fresh file
	Unchanged,        // live == the active spec
	ActiveFromMatch,  // live == another stored spec (game not saved after a swap): that one is active now
	AdoptedLive,      // live matches no spec: the player changed the active spec in game; stored
	Recovery,         // a swap did not finish and live matches neither end: specs kept untouched, `spec N` reloads one
};
auto ReconciledName(Reconciled r) noexcept -> const char*;

struct ReconcileReport {
	Reconciled result;
	bool changed;         // the file must be written
	int32_t remapped;     // stored skill ids that differed from the live list (mod data changed)
	int32_t newlyInvalid; // specs that need more points than the character has
};

// Brings `file` in line with the live character (design section 5). `haveFile` false: `file` is ignored and rebuilt.
// `configCount` is the configured number of specs (1..MaxSpecs); missing specs are added empty.
auto Reconcile(CharSpecs* file, bool haveFile, const LiveChar& live, int32_t configCount) noexcept -> ReconcileReport;

}  // namespace ms
