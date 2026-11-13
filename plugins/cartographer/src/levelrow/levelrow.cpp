#include "levelrow.h"

#include <algorithm>
#include <cstring>
#include <string>

namespace cartographer::levelrow {

namespace {

constexpr int64_t MaxDensity = 0x7fffffff;  // MonDen is a 32-bit signed value
constexpr int64_t MaxPackCount = 255;       // MonUMin / MonUMax are bytes
constexpr int PlausibleLevel = 255;
constexpr int PlausibleDensity = 100000;    // the vanilla tables top out at 2200
constexpr int PlausiblePacks = 100;         // ... and at 15

auto SignedLoad(const uint8_t* row, int offset, int size) -> int32_t {
	const uint32_t raw = Load(row, offset, size);
	if (size == 2) {
		return static_cast<int16_t>(raw);
	}
	if (size == 4) {
		return static_cast<int32_t>(raw);
	}
	return static_cast<int32_t>(raw);  // one byte: unsigned
}

auto MaskOf(int size) -> uint32_t {
	return size >= 4 ? 0xffffffffu : (1u << (size * 8)) - 1u;
}

void Add(std::vector<Field>& out, int offset, int size, int64_t value, uint32_t current) {
	const uint32_t mask = MaskOf(size);
	const uint32_t wanted = static_cast<uint32_t>(value) & mask;
	if (wanted == (current & mask)) {
		return;  // already holds it
	}
	Field f;
	f.offset = static_cast<uint16_t>(offset);
	f.size = static_cast<uint8_t>(size);
	f.value = wanted;
	out.push_back(f);
}

void AddAlways(std::vector<Field>& out, int offset, int size, uint32_t value) {
	out.push_back(Field{static_cast<uint16_t>(offset), static_cast<uint8_t>(size), value & MaskOf(size)});
}

}  // namespace

auto ClampLevel(int level) -> int {
	return std::clamp(level, 1, MaxLevel);
}

auto ScalePercent(int64_t base, int percent, int64_t maxValue) -> int64_t {
	if (base <= 0 || percent <= 0) {
		return base;
	}
	const int64_t scaled = base + (base * percent + 50) / 100;
	return std::min(scaled, std::max(maxValue, base));
}

auto SlotAllowed(int level, int slotLevel) -> bool {
	return level > LastVanillaLevel && level == slotLevel;
}

auto Load(const uint8_t* buffer, int offset, int size) -> uint32_t {
	uint32_t value = 0;
	std::memcpy(&value, buffer + offset, static_cast<size_t>(size));  // little-endian host (x64)
	return value;
}

void Store(uint8_t* buffer, const Field& field) {
	std::memcpy(buffer + field.offset, &field.value, field.size);
}

auto SnapshotOf(const uint8_t* row) -> Snapshot {
	Snapshot s;
	s.byte0d = row[RowByte0dOffset];
	for (int i = 0; i < 6; ++i) {
		s.monLvl[i] = SignedLoad(row, MonLvlOffsets[i], 2);
	}
	for (int i = 0; i < 3; ++i) {
		s.monDen[i] = SignedLoad(row, MonDenOffsets[i], 4);
		s.uMin[i] = SignedLoad(row, MonUMinOffsets[i], 1);
		s.uMax[i] = SignedLoad(row, MonUMaxOffsets[i], 1);
	}
	return s;
}

auto RowId(const uint8_t* row) -> int {
	return SignedLoad(row, RowIdOffset, 2);
}

auto RecordSnapshotOf(const uint8_t* record) -> RecordSnapshot {
	RecordSnapshot s;
	s.firstByte = record[RecByte0Offset];
	s.density = SignedLoad(record, RecDensityOffset, 4);
	s.uMin = SignedLoad(record, RecUMinOffset, 1);
	s.uMax = SignedLoad(record, RecUMaxOffset, 1);
	s.groupCount = SignedLoad(record, RecGroupCountOffset, 1);
	s.groupWeight = SignedLoad(record, RecGroupWeightOffset, 1);
	s.groupCount2 = SignedLoad(record, RecGroupCount2Offset, 1);
	return s;
}

auto CheckRow(const Snapshot& s) -> RowVerdict {
	RowVerdict v;
	for (const int32_t lvl : s.monLvl) {
		if (lvl < 0 || lvl > PlausibleLevel) {
			v.level = false;
			v.why.push_back("Level: a monster level field holds " + std::to_string(lvl) + ", not a level");
			break;
		}
	}
	return v;
}

auto CheckRecord(const RecordSnapshot& r, const Snapshot& row, int difficulty) -> RecordVerdict {
	RecordVerdict v;
	const auto allOff = [&](const std::string& why) {
		v.on.monsters = v.on.density = v.on.uniques = false;
		v.why.push_back(why);
	};
	if (difficulty < 0 || difficulty > 2) {
		allOff("the game's difficulty is " + std::to_string(difficulty) + ", not 0 to 2");
		return v;
	}
	if (r.firstByte != row.byte0d || r.density != row.monDen[difficulty] || r.uMin != row.uMin[difficulty] || r.uMax != row.uMax[difficulty]) {
		allOff("the region record does not hold what the levels row gives (MonDen " + std::to_string(r.density) + " against " +
		       std::to_string(row.monDen[difficulty]) + ", MonUMin " + std::to_string(r.uMin) + " against " + std::to_string(row.uMin[difficulty]) +
		       ", MonUMax " + std::to_string(r.uMax) + " against " + std::to_string(row.uMax[difficulty]) + "): not this level's record, or another build");
		return v;
	}
	if (r.density < 0 || r.density > PlausibleDensity) {
		v.on.density = false;
		v.why.push_back("Density: MonDen holds " + std::to_string(r.density) + ", not a density");
	}
	if (r.uMin > PlausiblePacks || r.uMax > PlausiblePacks) {
		v.on.uniques = false;
		v.why.push_back("Uniques: MonUMin / MonUMax hold " + std::to_string(std::max(r.uMin, r.uMax)) + ", not a pack count");
	}
	if (r.groupCount < 0 || r.groupCount > RecMaxGroups || r.groupCount2 != r.groupCount) {
		v.on.monsters = false;
		v.why.push_back("Monsters: the group counts are " + std::to_string(r.groupCount) + " and " + std::to_string(r.groupCount2) + ", not equal and at most " +
		                std::to_string(RecMaxGroups));
	}
	return v;
}

auto PlanRow(const Request& r, const Snapshot& s, bool level) -> std::vector<Field> {
	std::vector<Field> out;
	if (level) {
		const int clamped = ClampLevel(r.level);
		for (int i = 0; i < 6; ++i) {
			Add(out, MonLvlOffsets[i], 2, clamped, static_cast<uint32_t>(s.monLvl[i]));
		}
	}
	return out;
}

auto MonstersRefusal(const Request& r) -> std::string {
	const size_t n = r.monsterRows.size();
	if (n == 0) {
		return "";
	}
	if (n > static_cast<size_t>(MaxMonsters)) {
		return "more than " + std::to_string(MaxMonsters) + " monsters";
	}
	if (r.monsterWeights.size() != n) {
		return "a monster has no weight";
	}
	if (!std::all_of(r.monsterRows.begin(), r.monsterRows.end(), [](int row) { return row >= 0 && row <= MaxRow; })) {
		return "a monster row is outside 0 to " + std::to_string(MaxRow);
	}
	if (!std::all_of(r.monsterWeights.begin(), r.monsterWeights.end(), [](int w) { return w >= 0 && w <= 255; })) {
		return "a monster weight is outside 0 to 255";
	}
	int sum = 0;
	for (const int w : r.monsterWeights) {
		sum += w;
	}
	if (sum == 0) {
		return "the monsters' weights are all 0, so the game would pick none";
	}
	if (sum > MaxWeightSum) {
		return "the monsters' weights add up to " + std::to_string(sum) + ", above " + std::to_string(MaxWeightSum) + " (the game keeps the sum in a byte)";
	}
	return "";
}

auto PlanRecord(const Request& r, const RecordSnapshot& s, const Parts& allowed) -> std::vector<Field> {
	std::vector<Field> out;
	const size_t n = r.monsterRows.size();
	if (allowed.monsters && n > 0 && MonstersRefusal(r).empty()) {
		uint32_t sum = 0;
		for (const int w : r.monsterWeights) {
			sum += static_cast<uint32_t>(w);
		}
		AddAlways(out, RecGroupCountOffset, 1, static_cast<uint32_t>(n));
		AddAlways(out, RecGroupWeightOffset, 1, sum);
		AddAlways(out, RecGroupCount2Offset, 1, static_cast<uint32_t>(n));
		for (size_t i = 0; i < n; ++i) {
			const int base = RecGroupsOffset + RecGroupStride * static_cast<int>(i);
			AddAlways(out, base + RecGroupRowDelta, 2, static_cast<uint32_t>(r.monsterRows[i]));
			AddAlways(out, base + RecGroupWeightDelta, 1, static_cast<uint32_t>(r.monsterWeights[i]));
			AddAlways(out, base + RecGroupMembersDelta, 1, 0);
		}
		// The slots past the list keep the level's old groups. The game appends a group for a class that is not listed (a
		// spawned Treasure monster, an event monster) in the first of them and adds members only up to a limit that counts
		// what is already there, so a stale members byte leaves the new group the old group's pieces (a bPiece assert when
		// the unit is sent to the client).
		for (size_t i = n; i < static_cast<size_t>(RecMaxGroups); ++i) {
			AddAlways(out, RecGroupsOffset + RecGroupStride * static_cast<int>(i) + RecGroupMembersDelta, 1, 0);
		}
	}
	if (allowed.density && r.density > 0) {
		const int percent = std::min(r.density, MaxPercent);
		Add(out, RecDensityOffset, 4, ScalePercent(s.density, percent, MaxDensity), static_cast<uint32_t>(s.density));
	}
	if (allowed.uniques && r.uniques > 0) {
		const int percent = std::min(r.uniques, MaxPercent);
		// MonUMin stays as the level has it: below it the game makes a pack unique with a chance that grows with the rooms done
		// (0x419d20), so a raised minimum is used up at the first rooms; only MonUMax is raised, and it is spent at a flat chance per pack.
		Add(out, RecUMaxOffset, 1, ScalePercent(s.uMax, percent, MaxPackCount), static_cast<uint32_t>(s.uMax));
	}
	return out;
}

auto Ledger::Record(int level, const Field& field, uint32_t current) -> bool {
	for (Entry& e : entries_) {
		if (e.level == level && e.offset == field.offset) {
			e.written = field.value;
			return false;
		}
	}
	Entry e;
	e.level = level;
	e.offset = field.offset;
	e.size = field.size;
	e.original = current;
	e.written = field.value;
	entries_.push_back(e);
	return true;
}

auto Ledger::SetWritten(int level, int offset, uint32_t value) -> bool {
	for (Entry& e : entries_) {
		if (e.level == level && e.offset == offset) {
			e.written = value;
			return true;
		}
	}
	return false;
}

auto Ledger::Forget(int level, int offset) -> bool {
	for (auto it = entries_.begin(); it != entries_.end(); ++it) {
		if (it->level == level && it->offset == offset) {
			entries_.erase(it);
			return true;
		}
	}
	return false;
}

void Ledger::ForgetLevel(int level) {
	entries_.erase(std::remove_if(entries_.begin(), entries_.end(), [level](const Entry& e) { return e.level == level; }), entries_.end());
}

auto Ledger::Has(int level) const -> bool {
	return std::any_of(entries_.begin(), entries_.end(), [level](const Entry& e) { return e.level == level; });
}

auto Ledger::Original(int level, int offset, uint32_t* out) const -> bool {
	for (const Entry& e : entries_) {
		if (e.level == level && e.offset == offset) {
			*out = e.original;
			return true;
		}
	}
	return false;
}

auto Ledger::EntriesOf(int level) const -> std::vector<Entry> {
	std::vector<Entry> out;
	for (const Entry& e : entries_) {
		if (e.level == level) {
			out.push_back(e);
		}
	}
	return out;
}

auto Ledger::Levels() const -> std::vector<int> {
	std::vector<int> out;
	for (const Entry& e : entries_) {
		if (std::find(out.begin(), out.end(), e.level) == out.end()) {
			out.push_back(e.level);
		}
	}
	return out;
}

auto Ledger::AllHold(int level, const uint8_t* buffer) const -> bool {
	for (const Entry& e : entries_) {
		if (e.level == level && (Load(buffer, e.offset, e.size) & MaskOf(e.size)) != (e.written & MaskOf(e.size))) {
			return false;
		}
	}
	return true;
}

void OriginalImage(const Ledger& ledger, int level, const uint8_t* buffer, size_t span, uint8_t* out) {
	std::memcpy(out, buffer, span);
	for (const Ledger::Entry& e : ledger.EntriesOf(level)) {
		Store(out, Field{e.offset, e.size, e.original});
	}
}

auto Prepare(const Ledger& ledger, int level, const uint8_t* buffer, const std::vector<Field>& plan) -> Prepared {
	Prepared out;
	for (const Field& f : plan) {
		Op op;
		op.field = f;
		op.previous = Load(buffer, f.offset, f.size);
		op.inLedger = ledger.Original(level, f.offset, &op.original);
		if (!op.inLedger) {
			if ((op.previous & MaskOf(f.size)) == (f.value & MaskOf(f.size))) {
				continue;  // a new field that already holds the value: nothing to write or to put back
			}
			op.original = op.previous;
		}
		out.ops.push_back(op);
	}
	for (const Ledger::Entry& e : ledger.EntriesOf(level)) {
		const bool planned = std::any_of(plan.begin(), plan.end(), [&](const Field& f) { return f.offset == e.offset; });
		if (!planned) {
			Op op;
			op.field = Field{e.offset, e.size, e.original};
			op.previous = Load(buffer, e.offset, e.size);
			op.inLedger = true;
			op.original = e.original;
			out.reverts.push_back(op);
		}
	}
	return out;
}

auto Apply(Ledger& ledger, int level, const Prepared& prepared, const WriteFn& write, std::vector<Op>* done) -> bool {
	for (const Op& op : prepared.ops) {
		if (!write(op.field)) {
			return false;
		}
		ledger.Record(level, op.field, op.previous);
		done->push_back(op);
	}
	for (const Op& op : prepared.reverts) {
		if (!write(op.field)) {
			return false;
		}
		ledger.Forget(level, op.field.offset);
		done->push_back(op);
	}
	return true;
}

auto Undo(Ledger& ledger, int level, const std::vector<Op>& done, const WriteFn& write) -> int {
	int failed = 0;
	for (auto it = done.rbegin(); it != done.rend(); ++it) {
		const Op& op = *it;
		if (!write(Field{op.field.offset, op.field.size, op.previous})) {
			++failed;
		}
		if (!op.inLedger) {
			ledger.Forget(level, op.field.offset);
		} else if (!ledger.SetWritten(level, op.field.offset, op.previous)) {
			ledger.Record(level, Field{op.field.offset, op.field.size, op.previous}, op.original);  // it had been forgotten by a revert
		}
	}
	return failed;
}

auto RestoreLevel(Ledger& ledger, int level, const uint8_t* buffer, const WriteFn& write) -> RestoreResult {
	RestoreResult result;
	for (const Ledger::Entry& e : ledger.EntriesOf(level)) {
		const uint32_t mask = MaskOf(e.size);
		if ((Load(buffer, e.offset, e.size) & mask) != (e.written & mask)) {
			++result.moved;  // changed by someone else: not ours to put back
		} else if (write(Field{e.offset, e.size, e.original})) {
			++result.back;
		} else {
			++result.failed;
		}
		ledger.Forget(level, e.offset);
	}
	return result;
}

}  // namespace cartographer::levelrow
