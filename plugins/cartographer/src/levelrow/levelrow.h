// A map's monster columns written into the game (issue #156). Pure C++, no engine headers. Two places are written when a map
// opens, and the originals are put back afterwards:
//   - the slot level's row of the levels table: `Level`, the six monster level fields (read live by the game on each call);
//   - the slot level's own per-game region record (`*(game + 0x1d0)` indexed by level): `Density` (MonDen), `Uniques`
//     (MonUMin / MonUMax) and `Monsters` (the monster group list). The game copies these from the row once, when the game is
//     created, and its spawn code reads only the record.
// `Champions` has no field in either place (the champion roll is fixed in the game's code), so it is not applied.
// This file holds the layouts, the value mapping and the bookkeeping, including the undo and revert decisions; the engine part
// (src/engine/level_row.cpp) finds the memory and does the writes. Layouts (build 93847) and the evidence:
// docs/reverse-engineering/cartographer/15-map-monster-columns.md.
#pragma once

#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace cartographer::levelrow {

// --- The levels row (build 93847, from the levels.txt loader's column descriptors) ---
// Sizes: MonLvl* 2 bytes (signed), MonDen* 4 bytes, MonUMin/MonUMax* 1 byte.
inline constexpr int RowIdOffset = 0x04;                   // levels.txt Id, 2 bytes: the guard that the row is the slot level's
inline constexpr int RowByte0dOffset = 0x0d;               // 1 byte, copied by the game into the record's first byte
inline constexpr int MonLvlOffsets[6] = {0x18, 0x1a, 0x1c, 0x1e, 0x20, 0x22};  // MonLvl, (N), (H), MonLvlEx, (N), (H)
inline constexpr int MonDenOffsets[3] = {0x24, 0x28, 0x2c};                    // MonDen, (N), (H)
inline constexpr int MonUMinOffsets[3] = {0x30, 0x31, 0x32};                   // MonUMin, (N), (H)
inline constexpr int MonUMaxOffsets[3] = {0x33, 0x34, 0x35};                   // MonUMax, (N), (H)
inline constexpr size_t RowSpan = 0x38;                    // the bytes from the row start the plugin reads (through MonUMax(H))

// --- The region record (build 93847, from the region builder 0x508750 and the readers 0x419d20, 0x419e80, 0x50acf0) ---
inline constexpr int RecByte0Offset = 0x00;                // copy of row + 0x0d
inline constexpr int RecGroupCountOffset = 0x10;           // monster groups held (byte, at most 13)
inline constexpr int RecGroupWeightOffset = 0x11;          // sum of the groups' weights (byte)
inline constexpr int RecGroupCount2Offset = 0x12;          // equals RecGroupCountOffset
inline constexpr int RecGroupsOffset = 0x14;               // the groups, 0x34 bytes each
inline constexpr int RecGroupStride = 0x34;
inline constexpr int RecGroupRowDelta = 0;                 // word: the monstats row
inline constexpr int RecGroupWeightDelta = 2;              // byte: the monstats weight (monstats + 0x64)
inline constexpr int RecGroupMembersDelta = 3;             // byte: how many pack members the game added to this group
inline constexpr int RecMaxGroups = 13;
inline constexpr int RecDensityOffset = 0x2b8;             // int32: MonDen of the game's difficulty
inline constexpr int RecUMinOffset = 0x2bc;                // byte: MonUMin of the game's difficulty
inline constexpr int RecUMaxOffset = 0x2bd;                // byte: MonUMax of the game's difficulty
inline constexpr int RecRoomsSeenOffset = 0x04;            // int32: rooms the game has activated (rises by one per room)
inline constexpr int RecRoomsOffset = 0x0c;                // int32: rooms in the whole level (0 until the first room is populated)
inline constexpr int RecUniqueCountOffset = 0x2c8;         // byte: unique packs made so far
inline constexpr size_t RecSpan = 0x2d0;                   // the bytes from the record start the plugin reads

inline constexpr int MaxWeightSum = 255;                   // the group weight sum is one byte in the record
inline constexpr int MaxMonsters = 10;                     // maps.txt Monsters: at most 10 entries
inline constexpr int LastVanillaLevel = 137;               // Colossal Summit, the last vanilla 3.3.0 level; the slots start above it

inline constexpr int MaxLevel = 255;
inline constexpr int MaxRow = 32766;  // a monster row is a 16-bit signed index

struct Field {
	uint16_t offset = 0;
	uint8_t size = 0;    // 1, 2 or 4
	uint32_t value = 0;  // little-endian, the low `size` bytes
};

// The parts of the record a map can change, each switched on or off on its own.
struct Parts {
	bool level = true;     // row
	bool monsters = true;  // record
	bool density = true;   // record
	bool uniques = true;   // record
};

// What the row holds (values the sanity check and the record match read).
struct Snapshot {
	uint8_t byte0d = 0;
	int32_t monLvl[6] = {};
	int32_t monDen[3] = {};
	int32_t uMin[3] = {};
	int32_t uMax[3] = {};
};

// What the record holds.
struct RecordSnapshot {
	uint8_t firstByte = 0;
	int32_t density = 0;
	int32_t uMin = 0, uMax = 0;
	int32_t groupCount = 0, groupWeight = 0, groupCount2 = 0;
};

// What a map asks for. monsterRows are monstats row indexes in the order of maps.txt `Monsters`, monsterWeights the monstats
// weight (byte at monstats + 0x64) of each (empty rows: leave the groups). Champions is not here: it has no field.
struct Request {
	int level = 85;
	std::vector<int> monsterRows;
	std::vector<int> monsterWeights;
	int density = 0, uniques = 0;
};

// How many unique packs the map may make over the whole level (Uniques scaling applied to the record's MonUMax), 0 when it has none,
// `Uniques` is 0 (that map is not paced) or the part is not allowed. The pace (PaceBudget) spreads this number over the rooms.
auto UniqueTotal(const Request& request, const RecordSnapshot& record, bool uniquesAllowed) -> int;

// The unique packs allowed after `seen` of `rooms` rooms: `total` times the share of rooms done, less half of a pack's extra units
// (`packUnits` is how many units one unique pack adds to the game's count, 1 when not known), rounded up, at least 1 and at most
// `total`. The game adds a whole pack once it lets one through, so without the lag the count passes the line by up to a pack and runs
// out of budget before the last rooms. `rooms` of 0 or less (not known yet) gives 1; `seen` past `rooms` gives `total`. A `total` of 0
// or less gives 0.
auto PaceBudget(int total, int seen, int rooms, int packUnits = 1) -> int;

// The units one unique pack adds, from `units` added over `events` packs (a room's populate that raised the count): the mean rounded,
// at least 1; 1 when nothing was seen yet.
auto PackUnitsEstimate(int events, int units) -> int;

// The 1 to 255 monster level.
auto ClampLevel(int level) -> int;

// base plus `percent` percent of it, rounded half up, at most `maxValue`. A base of 0 or less, or a percent of 0 or less, comes
// back unchanged.
auto ScalePercent(int64_t base, int percent, int64_t maxValue) -> int64_t;

// Whether `level` may be written: above the last vanilla level and the slot this map opens through.
auto SlotAllowed(int level, int slotLevel) -> bool;

// Reads the row bytes (at least RowSpan of them) into a snapshot.
auto SnapshotOf(const uint8_t* row) -> Snapshot;
// The levels.txt Id stored in the row.
auto RowId(const uint8_t* row) -> int;
// Reads the record bytes (at least RecSpan of them) into a snapshot.
auto RecordSnapshotOf(const uint8_t* record) -> RecordSnapshot;

struct RowVerdict {
	bool level = true;
	std::vector<std::string> why;
};
// A row whose monster level fields hold values no levels row has (a wrong offset or another build) switches Level off.
auto CheckRow(const Snapshot& snapshot) -> RowVerdict;

struct RecordVerdict {
	Parts on;  // `level` is not used here
	std::vector<std::string> why;
};
// Whether the record is the one the game built from this row for `difficulty` (0 to 2): the copied fields must agree, else every
// record part is off. A value outside the range of vanilla and modded tables switches its part off. `row` and `record` must hold
// the originals (with the plugin's earlier writes taken out).
auto CheckRecord(const RecordSnapshot& record, const Snapshot& row, int difficulty) -> RecordVerdict;

// The row field writes for `request` (Level), only those whose value would change.
auto PlanRow(const Request& request, const Snapshot& snapshot, bool level) -> std::vector<Field>;
// The record field writes for `request` for the parts in `allowed`. Scaling uses the record's original values. Monsters: empty when
// the list is empty, longer than MaxMonsters, holds a row outside 0..MaxRow or lacks a weight (0..255) per row, or the weights sum to 0 or above MaxWeightSum (MonstersRefusal).
auto PlanRecord(const Request& request, const RecordSnapshot& snapshot, const Parts& allowed) -> std::vector<Field>;

// Why the Monsters list of `request` is not written (empty when it is fine or the list is empty): too many entries, a row outside
// 0..MaxRow, a missing or out-of-range weight, weights that are all 0 or add up to more than MaxWeightSum.
auto MonstersRefusal(const Request& request) -> std::string;

// Reads and writes one field of a buffer (little-endian).
auto Load(const uint8_t* buffer, int offset, int size) -> uint32_t;
void Store(uint8_t* buffer, const Field& field);

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

	// `field` is about to be written to `level`, and `current` is what the buffer holds now. The first record of a field keeps
	// `current` as its original; later ones only update what was written. Returns true when the field was new.
	auto Record(int level, const Field& field, uint32_t current) -> bool;
	// Sets what a field was last written with (after a failed open put an earlier value back). False when not recorded.
	auto SetWritten(int level, int offset, uint32_t value) -> bool;
	// Drops one field (it holds its original again). False when not recorded.
	auto Forget(int level, int offset) -> bool;
	// Drops every field of one level.
	void ForgetLevel(int level);
	auto Has(int level) const -> bool;
	auto Original(int level, int offset, uint32_t* out) const -> bool;
	auto EntriesOf(int level) const -> std::vector<Entry>;
	auto Levels() const -> std::vector<int>;
	// True when every recorded field of `level` still holds what was written (so the buffer is the one that was written).
	auto AllHold(int level, const uint8_t* buffer) const -> bool;
	void Clear() { entries_.clear(); }
	auto Count() const -> size_t { return entries_.size(); }

private:
	std::vector<Entry> entries_;
};

// A copy of `buffer` (span bytes) with the ledger's earlier writes for `level` taken out: the values the game had.
void OriginalImage(const Ledger& ledger, int level, const uint8_t* buffer, size_t span, uint8_t* out);

// --- The write choreography (pure: the write itself is a callback) ---
using WriteFn = std::function<bool(const Field&)>;

// One field of one open: what it is set to, what the buffer held before, and what the ledger knew.
struct Op {
	Field field;                // the value written
	uint32_t previous = 0;      // what the buffer held before the write
	bool inLedger = false;      // the ledger already had this field (so it keeps its original)
	uint32_t original = 0;      // that ledger original
};

struct Prepared {
	std::vector<Op> ops;      // the plan's fields that change something
	std::vector<Op> reverts;  // fields an earlier map set that this one does not: they go back to their original
};

// Decides the writes for `plan` against `buffer` and the ledger. A new field that already holds its value is dropped.
auto Prepare(const Ledger& ledger, int level, const uint8_t* buffer, const std::vector<Field>& plan) -> Prepared;

// Writes the ops, then the reverts, through `write`, stopping at the first failure, and keeps the ledger in step. `done` gets
// the ops written (also after a failure). True when all were written.
auto Apply(Ledger& ledger, int level, const Prepared& prepared, const WriteFn& write, std::vector<Op>* done) -> bool;

// Puts back what Apply wrote, newest first, and the ledger as it was. Returns how many writes failed.
auto Undo(Ledger& ledger, int level, const std::vector<Op>& done, const WriteFn& write) -> int;

struct RestoreResult {
	int back = 0;    // fields put back
	int moved = 0;   // fields someone else changed: left as they are
	int failed = 0;  // writes that failed
};
// Puts back every field of `level` that still holds the value the plugin wrote (`buffer` is the memory as it is now), and
// forgets them all.
auto RestoreLevel(Ledger& ledger, int level, const uint8_t* buffer, const WriteFn& write) -> RestoreResult;

}  // namespace cartographer::levelrow
