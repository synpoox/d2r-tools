// A map's monster columns written into the slot level's own row of the game's levels table (issue #156). Pure C++, no engine
// headers. When a map opens, `Level` goes into the six monster level fields, `Monsters` into the monster lists and NumMon,
// `Density` scales MonDen, and `Champions` / `Uniques` scale MonUMin / MonUMax; the originals are put back afterwards. This
// file holds the row layout, the value mapping and the bookkeeping; the engine part (src/engine/level_row.cpp) finds the row
// and does the writes. Layout (build 93847, from the levels.txt loader's column descriptors) and the evidence:
// docs/reverse-engineering/cartographer.md section 10b.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace cartographer::levelrow {

// Offsets in a levels row (build 93847). Sizes: MonLvl* 2 bytes (signed), MonDen* 4 bytes, MonUMin/MonUMax* 1 byte, NumMon 1 byte,
// monster list entries 2 bytes (a monstats row index).
inline constexpr int RowIdOffset = 0x04;                   // levels.txt Id, 2 bytes: the guard that the row is the slot level's
inline constexpr int MonLvlOffsets[6] = {0x18, 0x1a, 0x1c, 0x1e, 0x20, 0x22};  // MonLvl, (N), (H), MonLvlEx, (N), (H)
inline constexpr int MonDenOffsets[3] = {0x24, 0x28, 0x2c};                    // MonDen, (N), (H)
inline constexpr int MonUMinOffsets[3] = {0x30, 0x31, 0x32};                   // MonUMin, (N), (H)
inline constexpr int MonUMaxOffsets[3] = {0x33, 0x34, 0x35};                   // MonUMax, (N), (H)
inline constexpr int NumMonOffset = 0x3a;
inline constexpr int ListOffsets[3] = {0x3e, 0x70, 0xa2};  // mon1, nmon1, umon1; 25 entries each, 2 bytes apart
inline constexpr int ListSlots = 25;
inline constexpr int MaxMonsters = 10;                     // maps.txt Monsters: at most 10 entries
inline constexpr size_t RowSpan = 0xd4;                    // the bytes from the row start through umon25: what Snapshot reads

inline constexpr int MaxLevel = 255;
inline constexpr int MaxPercent = 1000;
inline constexpr int MaxRow = 32766;  // a monster list entry is a 16-bit signed row index

struct Field {
	uint16_t offset = 0;
	uint8_t size = 0;    // 1, 2 or 4
	uint32_t value = 0;  // little-endian, the low `size` bytes
};

// The four things a map can change, each switched on or off on its own.
struct Parts {
	bool level = true;
	bool monsters = true;
	bool density = true;
	bool packs = true;  // Champions and Uniques: the game has one MonUMin / MonUMax pair for all special packs
};

// What the row holds (values the scaling and the sanity check read).
struct Snapshot {
	int32_t monLvl[6] = {};
	int32_t monDen[3] = {};
	int32_t uMin[3] = {};
	int32_t uMax[3] = {};
	int32_t numMon = 0;
};

// What a map asks for. monsterRows are monstats row indexes in the order of maps.txt `Monsters` (empty: leave the lists).
struct Request {
	int level = 85;
	std::vector<int> monsterRows;
	int density = 0, champions = 0, uniques = 0;
};

// The 1 to 255 monster level.
auto ClampLevel(int level) -> int;

// base plus `percent` percent of it, rounded half up, at most `maxValue`. A base of 0 or less, or a percent of 0 or less, comes
// back unchanged.
auto ScalePercent(int64_t base, int percent, int64_t maxValue) -> int64_t;

// The one percent for the shared MonUMin / MonUMax pair: the average of Champions and Uniques (each 0 to 1000), rounded up.
// The levels table has no separate champion and unique count; the game picks the pack kind itself.
auto PackPercent(int champions, int uniques) -> int;

// Reads the row bytes (at least RowSpan of them) into a snapshot.
auto SnapshotOf(const uint8_t* row) -> Snapshot;
// The levels.txt Id stored in the row.
auto RowId(const uint8_t* row) -> int;

// Which parts the row's current values allow: a part whose fields hold values no levels row has (a wrong offset or another
// build) is switched off and `why` says so. A value outside the range of vanilla and modded tables is the signal, not a proof.
struct Verdict {
	Parts on;
	std::vector<std::string> why;
};
auto Check(const Snapshot& snapshot) -> Verdict;

// The fields to write for `request`, only those whose value would change, for the parts in `allowed`. Monsters: empty when the
// list is empty, longer than MaxMonsters or holds a row outside 0..MaxRow. `snapshot` must hold the row's original values.
auto Plan(const Request& request, const Snapshot& snapshot, const Parts& allowed) -> std::vector<Field>;

// Reads and writes one field of a row buffer (little-endian).
auto Load(const uint8_t* row, int offset, int size) -> uint32_t;
void Store(uint8_t* row, const Field& field);

// The original and last-written value of every field the plugin wrote, per level, so the fields can be put back.
class Ledger {
public:
	struct Entry {
		int level = 0;
		uint16_t offset = 0;
		uint8_t size = 0;
		uint32_t original = 0;
		uint32_t written = 0;
	};

	// `field` is about to be written to `level`, and `current` is what the row holds now. The first record of a field keeps
	// `current` as its original; later ones only update what was written. Returns true when the field was new.
	auto Record(int level, const Field& field, uint32_t current) -> bool;
	// Sets what a field was last written with (after a failed open put an earlier value back). False when not recorded.
	auto SetWritten(int level, int offset, uint32_t value) -> bool;
	// Drops one field (it holds its original again). False when not recorded.
	auto Forget(int level, int offset) -> bool;
	auto Has(int level) const -> bool;
	auto Original(int level, int offset, uint32_t* out) const -> bool;
	auto EntriesOf(int level) const -> std::vector<Entry>;
	auto Levels() const -> std::vector<int>;
	void Clear() { entries_.clear(); }
	auto Count() const -> size_t { return entries_.size(); }

private:
	std::vector<Entry> entries_;
};

}  // namespace cartographer::levelrow
