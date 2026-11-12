#include "level_row.h"

#include "engine/event_apply.h"
#include "engine/game_memory.h"
#include "engine/map_names.h"
#include "levelrow.h"
#include "log.h"

#include <cstdint>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

namespace carto {

namespace {

namespace lr = cartographer::levelrow;

std::mutex g_lock;  // the state below; no SDK call is made while holding it
lr::Ledger g_ledger;
bool g_stuck = false;  // a restore failed: no further write for the rest of the process
std::mutex g_noteLock;  // g_lastNote only
std::string g_lastNote = "no map opened yet";

void Note(const std::string& text) {
	std::lock_guard<std::mutex> lock(g_noteLock);
	g_lastNote = text;
}

// A row this plugin wrote into.
struct Target {
	int level;
	const uint8_t* row;
};
std::vector<Target> g_targets;

// One field of one BeforeOpen write: what it was set to, what the row held before, and what the ledger knew.
struct Op {
	lr::Field field;                // the value written
	uint32_t previous = 0;          // what the game's row held before the write
	bool inLedger = false;          // the ledger already had this field (so it keeps its original)
	uint32_t original = 0;          // that ledger original
};

// The last write of LevelRowBeforeOpen, so a failed open can undo just that.
struct LastWrite {
	bool valid = false;
	int level = 0;
	std::vector<Op> ops;
};
LastWrite g_last;

auto FieldAt(uint8_t* row, uint16_t offset) -> uint8_t* {
	return row + offset;
}

auto RowOfTarget(int level) -> uint8_t* {
	for (const Target& t : g_targets) {
		if (t.level == level) {
			return const_cast<uint8_t*>(t.row);
		}
	}
	return nullptr;
}

void SetTarget(int level, const uint8_t* row) {
	for (Target& t : g_targets) {
		if (t.level == level) {
			t.row = row;
			return;
		}
	}
	g_targets.push_back(Target{level, row});
}

void DropTarget(int level) {
	for (auto it = g_targets.begin(); it != g_targets.end(); ++it) {
		if (it->level == level) {
			g_targets.erase(it);
			return;
		}
	}
}

auto WriteField(uint8_t* row, uint16_t offset, uint8_t size, uint32_t value) noexcept -> bool {
	return WriteGame(FieldAt(row, offset), &value, size);
}

// Puts one level's fields back, through the row pointer kept at write time (the game's row getter is not called: at process exit
// its tables may be gone). Memory is checked first, and a field is touched only while it still holds the value this plugin
// wrote. g_lock must be held.
void RestoreLevelLocked(int level) noexcept {
	const std::vector<lr::Ledger::Entry> entries = g_ledger.EntriesOf(level);
	uint8_t* row = RowOfTarget(level);
	if (row == nullptr || !Readable(row, lr::RowSpan)) {
		plog::Warn("map monsters: level %d row is %s; its %zu field(s) were not put back", level, row == nullptr ? "not recorded" : "no longer readable", entries.size());
		for (const lr::Ledger::Entry& e : entries) {
			g_ledger.Forget(level, e.offset);
		}
		DropTarget(level);
		return;
	}
	int back = 0, moved = 0, failed = 0;
	for (const lr::Ledger::Entry& e : entries) {
		const uint32_t mask = e.size >= 4 ? 0xffffffffu : (1u << (e.size * 8)) - 1u;
		const uint32_t now = lr::Load(row, e.offset, e.size);
		if ((now & mask) != (e.written & mask)) {
			++moved;  // changed by someone else: not ours to put back
		} else if (WriteField(row, e.offset, e.size, e.original)) {
			++back;
		} else {
			++failed;
		}
		g_ledger.Forget(level, e.offset);
	}
	if (failed > 0) {
		g_stuck = true;
		plog::Error("map monsters: level %d: %d field(s) could not be put back; no further map is written this session", level, failed);
	}
	plog::Info("map monsters: level %d: %d field(s) put back%s", level, back, moved > 0 ? ", the rest were changed by someone else and left as they are" : "");
	DropTarget(level);
}

void RestoreAllLocked() noexcept {
	try {
		for (const int level : g_ledger.Levels()) {
			RestoreLevelLocked(level);
		}
		g_ledger.Clear();
		g_targets.clear();
	} catch (...) {
		plog::Error("map monsters: restoring the level row failed");
	}
}

// Puts the row and the ledger back to what they were before the ops. g_lock must be held; `row` is readable.
void UndoOpsLocked(int level, uint8_t* row, const std::vector<Op>& ops) noexcept {
	for (auto it = ops.rbegin(); it != ops.rend(); ++it) {
		const Op& op = *it;
		if (!WriteField(row, op.field.offset, op.field.size, op.previous)) {
			g_stuck = true;
			plog::Error("map monsters: level %d field 0x%x could not be put back after a failed open", level, op.field.offset);
		}
		if (!op.inLedger) {
			g_ledger.Forget(level, op.field.offset);
		} else if (!g_ledger.SetWritten(level, op.field.offset, op.previous)) {
			g_ledger.Record(level, lr::Field{op.field.offset, op.field.size, op.previous}, op.original);  // it had been forgotten by a revert
		}
	}
}

void UndoLastLocked() noexcept {
	const LastWrite last = g_last;
	g_last = LastWrite();
	if (!last.valid) {
		return;
	}
	uint8_t* row = RowOfTarget(last.level);
	if (row == nullptr || !Readable(row, lr::RowSpan)) {
		g_stuck = true;
		plog::Error("map monsters: level %d row is not readable; the failed open's values could not be put back", last.level);
		return;
	}
	UndoOpsLocked(last.level, row, last.ops);
	if (!g_ledger.Has(last.level)) {
		DropTarget(last.level);
	}
}

auto ListText(const std::vector<int>& rows) -> std::string {
	std::string s;
	for (const int row : rows) {
		s += (s.empty() ? "" : ",") + std::to_string(row);
	}
	return s;
}

}  // namespace

auto LevelRowBeforeOpen(const MapDef& map, int level, uint32_t dataFlag) noexcept -> bool {
	try {
		const auto skip = [&](const char* why) {
			plog::Warn("map monsters: '%s' keeps the slot level's own monsters: %s", map.name.c_str(), why);
			Note(std::string("'") + map.name + "' was not applied: " + why);
			return false;
		};
		{
			std::lock_guard<std::mutex> lock(g_lock);
			g_last = LastWrite();
			if (g_stuck) {
				return skip("an earlier restore failed, so the row is left alone");
			}
		}
		uint8_t* row = LevelsRowOf(dataFlag, level);
		if (row == nullptr) {
			return skip("the levels row was not found (the LevelsRow part is off or the level is missing)");
		}
		// The SDK is asked outside the lock.
		std::vector<int> monsterRows;
		std::string monsterWhy;
		if (!map.monsters.empty() && !GameMonsterRows(dataFlag, map.monsters, &monsterRows, &monsterWhy)) {
			plog::Warn("map monsters: '%s' keeps the slot level's own monster list: %s", map.name.c_str(), monsterWhy.c_str());
			monsterRows.clear();
		}
		std::lock_guard<std::mutex> lock(g_lock);
		if (!Readable(row, lr::RowSpan)) {
			return skip("the levels row cannot be read");
		}
		if (lr::RowId(row) != level) {
			return skip("the row's Id is not the slot level's (a different build?)");
		}
		// The row's original values: a copy of the row with the plugin's earlier writes taken out.
		uint8_t original[lr::RowSpan];
		std::memcpy(original, row, sizeof(original));
		for (const lr::Ledger::Entry& e : g_ledger.EntriesOf(level)) {
			lr::Store(original, lr::Field{e.offset, e.size, e.original});
		}
		const lr::Snapshot snapshot = lr::SnapshotOf(original);
		const lr::Verdict verdict = lr::Check(snapshot);
		for (const std::string& why : verdict.why) {
			plog::Warn("map monsters: level %d: %s; that part is off for this open", level, why.c_str());
		}
		lr::Request request;
		request.level = map.level;
		request.monsterRows = monsterRows;
		request.density = map.density;
		request.champions = map.champions;
		request.uniques = map.uniques;
		const std::vector<lr::Field> plan = lr::Plan(request, snapshot, verdict.on);

		// A field written for an earlier map that this map does not set goes back to its original.
		std::vector<Op> ops;
		for (const lr::Field& f : plan) {
			Op op;
			op.field = f;
			op.previous = lr::Load(row, f.offset, f.size);
			op.inLedger = g_ledger.Original(level, f.offset, &op.original);
			ops.push_back(op);
		}
		std::vector<Op> reverts;
		for (const lr::Ledger::Entry& e : g_ledger.EntriesOf(level)) {
			bool planned = false;
			for (const lr::Field& f : plan) {
				planned = planned || f.offset == e.offset;
			}
			if (!planned) {
				Op op;
				op.field = lr::Field{e.offset, e.size, e.original};
				op.previous = lr::Load(row, e.offset, e.size);
				op.inLedger = true;
				op.original = e.original;
				reverts.push_back(op);
			}
		}
		if (verdict.on.density && map.density > 0 && snapshot.monDen[0] == 0 && snapshot.monDen[1] == 0 && snapshot.monDen[2] == 0) {
			plog::Info("map monsters: '%s': Density %d%% adds nothing, the slot level's MonDen is 0 in all three difficulties", map.name.c_str(), map.density);
		}
		if (verdict.on.packs && lr::PackPercent(map.champions, map.uniques) > 0 && snapshot.uMin[0] + snapshot.uMax[0] + snapshot.uMin[1] + snapshot.uMax[1] + snapshot.uMin[2] + snapshot.uMax[2] == 0) {
			plog::Info("map monsters: '%s': Champions / Uniques add nothing, the slot level's MonUMin and MonUMax are 0", map.name.c_str());
		}
		if (ops.empty() && reverts.empty()) {
			Note("'" + map.name + "' changed nothing: the row already holds what it asks for");
			plog::Info("map monsters: '%s': level %d row %p already holds the map's values", map.name.c_str(), level, static_cast<const void*>(row));
			return false;
		}
		plog::Info("map monsters: level %d row %p: '%s' sets %zu field(s) (Level %d, %zu listed monster(s)%s, Density %d%%, Champions %d%%, Uniques %d%%); %zu earlier field(s) go back to the original",
		           level, static_cast<const void*>(row), map.name.c_str(), ops.size(), lr::ClampLevel(map.level), monsterRows.size(),
		           monsterRows.empty() ? "" : (" rows " + ListText(monsterRows)).c_str(), map.density, map.champions, map.uniques, reverts.size());
		for (const Op& op : ops) {
			if (op.field.offset < lr::ListOffsets[0]) {
				plog::Info("map monsters:   0x%02x: %u -> %u", op.field.offset, op.previous, op.field.value);
			}
		}

		SetTarget(level, row);
		g_last.level = level;
		g_last.valid = true;
		bool ok = true;
		std::vector<Op> done;
		const auto apply = [&](Op& op, bool revert) {
			if (!WriteField(row, op.field.offset, op.field.size, op.field.value)) {
				return false;
			}
			if (revert) {
				g_ledger.Forget(level, op.field.offset);
			} else if (!g_ledger.Record(level, op.field, op.previous)) {
				// existing entry: written updated
			}
			done.push_back(op);
			return true;
		};
		for (Op& op : ops) {
			if (!op.inLedger) {
				// a new field: its original is what the row holds now
				op.original = op.previous;
			}
			ok = ok && apply(op, false);
		}
		for (Op& op : reverts) {
			ok = ok && apply(op, true);
		}
		g_last.ops = done;
		if (!ok) {
			plog::Warn("map monsters: writing the row failed; putting the previous values back");
			UndoLastLocked();
			Note("'" + map.name + "' was not applied: writing the row failed");
			return false;
		}
		Note("'" + map.name + "' set " + std::to_string(ops.size()) + " field(s) in level " + std::to_string(level));
		return true;
	} catch (...) {
		plog::Error("map monsters: failed for '%s'; the map keeps the slot level's own monsters", map.name.c_str());
		return false;
	}
}

void LevelRowOpenFailed(int level) noexcept {
	try {
		std::lock_guard<std::mutex> lock(g_lock);
		if (g_last.valid && g_last.level == level) {
			UndoLastLocked();
			plog::Info("map monsters: the portal did not open; level %d's row is as before", level);
		}
	} catch (...) {
		plog::Error("map monsters: undoing the level row failed");
	}
}

void LevelRowRestore() noexcept {
	LevelRowUnload();
}

void LevelRowUnload() noexcept {
	try {
		std::lock_guard<std::mutex> lock(g_lock);
		g_last = LastWrite();
		RestoreAllLocked();
	} catch (...) {
		plog::Error("map monsters: restoring the level row failed");
	}
}

auto LevelRowSummary() -> std::string {
	bool partOn = false;
	for (const PartStatus& part : MapNamesPartStatuses()) {
		partOn = part.on;
	}
	std::string last;
	{
		std::lock_guard<std::mutex> noteLock(g_noteLock);
		last = g_lastNote;
	}
	std::lock_guard<std::mutex> lock(g_lock);
	if (!partOn) {
		return "Map monsters: off (the LevelsRow part was not found)";
	}
	if (g_stuck) {
		return "Map monsters: off until the game restarts (a restore of the slot level's row failed)";
	}
	return "Map monsters: on, Level, Monsters, Density, Champions and Uniques are written into the slot level's row when a map opens (written, not verified in game); last: " + last;
}

}  // namespace carto
