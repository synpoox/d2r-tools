#include "levelrow.h"

#include <algorithm>
#include <cstring>

namespace cartographer::levelrow {

namespace {

constexpr int64_t MaxDensity = 0x7fffffff;  // MonDen is a 32-bit signed value
constexpr int64_t MaxPackCount = 255;       // MonUMin / MonUMax are bytes
constexpr int PlausibleLevel = 255;
constexpr int PlausibleDensity = 100000;    // the vanilla tables top out at 2200
constexpr int PlausiblePacks = 100;         // ... and at 15
constexpr int PlausibleNumMon = ListSlots;

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

void Add(std::vector<Field>& out, int offset, int size, int64_t value, uint32_t current) {
	const uint32_t mask = size >= 4 ? 0xffffffffu : (1u << (size * 8)) - 1u;
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

auto PackPercent(int champions, int uniques) -> int {
	const int c = std::clamp(champions, 0, MaxPercent);
	const int u = std::clamp(uniques, 0, MaxPercent);
	return (c + u + 1) / 2;
}

auto Load(const uint8_t* row, int offset, int size) -> uint32_t {
	uint32_t value = 0;
	std::memcpy(&value, row + offset, static_cast<size_t>(size));  // little-endian host (x64)
	return value;
}

void Store(uint8_t* row, const Field& field) {
	std::memcpy(row + field.offset, &field.value, field.size);
}

auto SnapshotOf(const uint8_t* row) -> Snapshot {
	Snapshot s;
	for (int i = 0; i < 6; ++i) {
		s.monLvl[i] = SignedLoad(row, MonLvlOffsets[i], 2);
	}
	for (int i = 0; i < 3; ++i) {
		s.monDen[i] = SignedLoad(row, MonDenOffsets[i], 4);
		s.uMin[i] = SignedLoad(row, MonUMinOffsets[i], 1);
		s.uMax[i] = SignedLoad(row, MonUMaxOffsets[i], 1);
	}
	s.numMon = SignedLoad(row, NumMonOffset, 1);
	return s;
}

auto RowId(const uint8_t* row) -> int {
	return SignedLoad(row, RowIdOffset, 2);
}

auto Check(const Snapshot& s) -> Verdict {
	Verdict v;
	for (const int32_t lvl : s.monLvl) {
		if (lvl < 0 || lvl > PlausibleLevel) {
			v.on.level = false;
			v.why.push_back("Level: a monster level field holds " + std::to_string(lvl) + ", not a level");
			break;
		}
	}
	if (s.numMon < 0 || s.numMon > PlausibleNumMon) {
		v.on.monsters = false;
		v.why.push_back("Monsters: NumMon holds " + std::to_string(s.numMon) + ", more than " + std::to_string(PlausibleNumMon));
	}
	for (const int32_t den : s.monDen) {
		if (den < 0 || den > PlausibleDensity) {
			v.on.density = false;
			v.why.push_back("Density: MonDen holds " + std::to_string(den) + ", not a density");
			break;
		}
	}
	for (int i = 0; i < 3; ++i) {
		if (s.uMin[i] > PlausiblePacks || s.uMax[i] > PlausiblePacks) {
			v.on.packs = false;
			v.why.push_back("Champions / Uniques: MonUMin / MonUMax hold " + std::to_string(std::max(s.uMin[i], s.uMax[i])) + ", not a pack count");
			break;
		}
	}
	return v;
}

auto Plan(const Request& r, const Snapshot& s, const Parts& allowed) -> std::vector<Field> {
	std::vector<Field> out;
	if (allowed.level) {
		const int level = ClampLevel(r.level);
		for (int i = 0; i < 6; ++i) {
			Add(out, MonLvlOffsets[i], 2, level, static_cast<uint32_t>(s.monLvl[i]));
		}
	}
	if (allowed.monsters && !r.monsterRows.empty() && r.monsterRows.size() <= static_cast<size_t>(MaxMonsters) &&
	    std::all_of(r.monsterRows.begin(), r.monsterRows.end(), [](int row) { return row >= 0 && row <= MaxRow; })) {
		out.push_back(Field{static_cast<uint16_t>(NumMonOffset), 1, static_cast<uint32_t>(r.monsterRows.size())});
		for (const int base : ListOffsets) {
			for (size_t i = 0; i < r.monsterRows.size(); ++i) {
				out.push_back(Field{static_cast<uint16_t>(base + 2 * static_cast<int>(i)), 2, static_cast<uint32_t>(r.monsterRows[i])});
			}
		}
	}
	if (allowed.density && r.density > 0) {
		const int percent = std::min(r.density, MaxPercent);
		for (int i = 0; i < 3; ++i) {
			Add(out, MonDenOffsets[i], 4, ScalePercent(s.monDen[i], percent, MaxDensity), static_cast<uint32_t>(s.monDen[i]));
		}
	}
	if (allowed.packs) {
		const int percent = PackPercent(r.champions, r.uniques);
		if (percent > 0) {
			for (int i = 0; i < 3; ++i) {
				Add(out, MonUMinOffsets[i], 1, ScalePercent(s.uMin[i], percent, MaxPackCount), static_cast<uint32_t>(s.uMin[i]));
				Add(out, MonUMaxOffsets[i], 1, ScalePercent(s.uMax[i], percent, MaxPackCount), static_cast<uint32_t>(s.uMax[i]));
			}
		}
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

}  // namespace cartographer::levelrow
