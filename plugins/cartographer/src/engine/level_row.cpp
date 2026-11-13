#include "level_row.h"

#include "engine/event_apply.h"
#include "engine/game_memory.h"
#include "engine/map_names.h"
#include "levelrow.h"
#include "log.h"

#include <cstdint>
#include <atomic>
#include <cstring>
#include <mutex>
#include <string>
#include <vector>

namespace carto {

namespace {

namespace lr = cartographer::levelrow;

constexpr int GameDifficultyOffset = 0x104;  // uint8 on the game: 0 Normal, 1 Nightmare, 2 Hell
constexpr int GameRegionsOffset = 0x1d0;     // pointer on the game: the region records, one pointer per level (0x508740)

// A memory area this plugin writes into (the levels row, or the region record), with what it wrote there.
struct Target {
	int level;
	uint8_t* base;
};

struct Space {
	const char* what;  // for the log
	size_t span;
	lr::Ledger ledger;
	std::vector<Target> targets;
	// The last write of LevelRowBeforeOpen, so a failed open can undo just that.
	bool lastValid = false;
	int lastLevel = 0;
	std::vector<lr::Op> lastOps;
};

std::mutex g_lock;  // the state below; no SDK call is made while holding it
Space g_row{"row", lr::RowSpan, {}, {}};
Space g_rec{"record", lr::RecSpan, {}, {}};
bool g_stuck = false;  // a restore failed: no further write for the rest of the process
std::mutex g_noteLock;  // g_lastNote only
std::string g_lastNote = "no map opened yet";

void Note(const std::string& text) {
	std::lock_guard<std::mutex> lock(g_noteLock);
	g_lastNote = text;
}

auto BaseOf(const Space& space, int level) -> uint8_t* {
	for (const Target& t : space.targets) {
		if (t.level == level) {
			return t.base;
		}
	}
	return nullptr;
}

void SetTarget(Space& space, int level, uint8_t* base) {
	for (Target& t : space.targets) {
		if (t.level == level) {
			t.base = base;
			return;
		}
	}
	space.targets.push_back(Target{level, base});
}

void DropTarget(Space& space, int level) {
	for (auto it = space.targets.begin(); it != space.targets.end(); ++it) {
		if (it->level == level) {
			space.targets.erase(it);
			return;
		}
	}
}

auto WriterFor(uint8_t* base) -> lr::WriteFn {
	return [base](const lr::Field& field) { return WriteGame(base + field.offset, &field.value, field.size); };
}

// Puts one level's fields back, through the pointer kept at write time (the game's row getter is not called: at process exit its
// tables may be gone). Memory is checked first, and a field is touched only while it still holds the value this plugin wrote.
// g_lock must be held.
void RestoreLevelLocked(Space& space, int level) noexcept {
	uint8_t* base = BaseOf(space, level);
	if (base == nullptr || !Readable(base, space.span)) {
		plog::Warn("map monsters: level %d %s is %s; its %zu field(s) were not put back", level, space.what, base == nullptr ? "not recorded" : "no longer readable",
		           space.ledger.EntriesOf(level).size());
		space.ledger.ForgetLevel(level);
		DropTarget(space, level);
		return;
	}
	const lr::RestoreResult result = lr::RestoreLevel(space.ledger, level, base, WriterFor(base));
	if (result.failed > 0) {
		g_stuck = true;
		plog::Error("map monsters: level %d %s: %d field(s) could not be put back; no further map is written this session", level, space.what, result.failed);
	}
	plog::Info("map monsters: level %d %s: %d field(s) put back%s", level, space.what, result.back,
	           result.moved > 0 ? ", the rest were changed by someone else and left as they are" : "");
	DropTarget(space, level);
}

void RestoreRowsLocked() noexcept {
	try {
		for (const int level : g_row.ledger.Levels()) {
			RestoreLevelLocked(g_row, level);
		}
		g_row.ledger.Clear();
		g_row.targets.clear();
	} catch (...) {
		plog::Error("map monsters: restoring the level row failed");
	}
}

// The region record is the game's own and goes with the game: nothing is written at GameLeft or unload (the memory may already be
// freed), the fields are only forgotten. The next game builds a fresh record from the row.
void ForgetRecordsLocked() noexcept {
	try {
		for (const int level : g_rec.ledger.Levels()) {
			plog::Info("map monsters: level %d record: %zu field(s) forgotten (the record goes with the game)", level, g_rec.ledger.EntriesOf(level).size());
		}
		g_rec.ledger.Clear();
		g_rec.targets.clear();
	} catch (...) {
		plog::Error("map monsters: forgetting the region record failed");
	}
}

// Undoes the last write of one space. g_lock must be held.
void UndoLastLocked(Space& space) noexcept {
	const bool valid = space.lastValid;
	const int level = space.lastLevel;
	const std::vector<lr::Op> ops = space.lastOps;
	space.lastValid = false;
	space.lastOps.clear();
	if (!valid) {
		return;
	}
	uint8_t* base = BaseOf(space, level);
	if (base == nullptr || !Readable(base, space.span)) {
		g_stuck = true;
		plog::Error("map monsters: level %d %s is not readable; the failed open's values could not be put back", level, space.what);
		return;
	}
	if (lr::Undo(space.ledger, level, ops, WriterFor(base)) > 0) {
		g_stuck = true;
		plog::Error("map monsters: level %d %s: a field could not be put back after a failed open", level, space.what);
	}
	if (!space.ledger.Has(level)) {
		DropTarget(space, level);
	}
}

auto ListText(const std::vector<int>& rows) -> std::string {
	std::string s;
	for (const int row : rows) {
		s += (s.empty() ? "" : ",") + std::to_string(row);
	}
	return s;
}

// The slot level's region record of this game, or null with `why`. The array and the record are checked for memory only; whether
// the record belongs to the level is decided by lr::CheckRecord against the row.
auto RecordOf(const void* game, int level, int64_t levelCount, std::string* why) noexcept -> uint8_t* {
	const uint8_t* g = static_cast<const uint8_t*>(game);
	if (g == nullptr) {
		*why = "no game";
		return nullptr;
	}
	if (!Readable(g + GameRegionsOffset, sizeof(void*))) {
		*why = "the game's region array pointer cannot be read";
		return nullptr;
	}
	const uint8_t* array = nullptr;
	std::memcpy(&array, g + GameRegionsOffset, sizeof(array));
	if (array == nullptr || level <= 0 || level >= levelCount || !Readable(array + static_cast<size_t>(level) * sizeof(void*), sizeof(void*))) {
		*why = "the game's region array has no entry for the level (past the levels count or not readable)";
		return nullptr;
	}
	uint8_t* record = nullptr;
	std::memcpy(&record, array + static_cast<size_t>(level) * sizeof(void*), sizeof(record));
	if (record == nullptr || !Readable(record, lr::RecSpan)) {
		*why = "the level's region record is not there";
		return nullptr;
	}
	return record;
}

std::atomic<bool> g_probe{false};
int g_probeLevel = 0;  // game thread only
int g_probeRooms = 0;
int g_probeUniques = 0;

}  // namespace

void LevelRowProbeSet(bool on) noexcept {
	g_probe.store(on);
	g_probeLevel = 0;
}

auto LevelRowProbeOn() noexcept -> bool {
	return g_probe.load();
}

void LevelRowProbeRoom(const void* game, int level, uint32_t dataFlag, const char* where) noexcept {
	if (!g_probe.load()) {
		return;
	}
	try {
		std::string why;
		int64_t levelCount = 0;
		if (!GameLevelCount(dataFlag, &levelCount, &why)) {
			plog::Info("record probe: level count unreadable: %s", why.c_str());
			return;
		}
		const uint8_t* r = RecordOf(game, level, levelCount, &why);
		if (r == nullptr) {
			plog::Info("record probe: no record: %s", why.c_str());
			return;
		}
		if (!Readable(r + 0x2c8, 1)) {
			return;
		}
		if (g_probeLevel != level) {
			g_probeLevel = level;
			g_probeRooms = 0;
			g_probeUniques = 0;
		}
		int32_t f4 = 0, f8 = 0, fc = 0, den = 0;
		std::memcpy(&f4, r + 4, 4);
		std::memcpy(&f8, r + 8, 4);
		std::memcpy(&fc, r + 0xc, 4);
		std::memcpy(&den, r + 0x2b8, 4);
		const int uniques = r[0x2c8];
		plog::Info("record probe: room #%d level %d %s: +4=%d +8=%d +0xc=%d uniqueCount(+0x2c8)=%d (+%d this room) MonUMin=%d MonUMax=%d MonDen=%d", ++g_probeRooms, level, where, f4, f8, fc,
		           uniques, uniques - g_probeUniques, r[0x2bc], r[0x2bd], den);
		g_probeUniques = uniques;
	} catch (...) {
	}
}

auto LevelRowBeforeOpen(const MapDef& map, int level, int slotLevel, const void* game, uint32_t dataFlag) noexcept -> bool {
	try {
		const auto skip = [&](const char* why) {
			plog::Warn("map monsters: '%s' keeps the slot level's own monsters: %s", map.name.c_str(), why);
			Note(std::string("'") + map.name + "' was not applied: " + why);
			return false;
		};
		{
			std::lock_guard<std::mutex> lock(g_lock);
			g_row.lastValid = g_rec.lastValid = false;
			g_row.lastOps.clear();
			g_rec.lastOps.clear();
			if (g_stuck) {
				return skip("an earlier restore failed, so the game's memory is left alone");
			}
		}
		if (!lr::SlotAllowed(level, slotLevel)) {
			return skip("the level is not the map's slot above the last vanilla level (137), so nothing is written");
		}
		uint8_t* row = LevelsRowOf(dataFlag, level);
		if (row == nullptr) {
			return skip("the levels row was not found (the LevelsRow part is off or the level is missing)");
		}
		// The SDK is asked outside the lock.
		std::vector<int> monsterRows, monsterWeights;
		std::string monsterWhy;
		if (!map.monsters.empty() && !GameMonsterRows(dataFlag, map.monsters, &monsterRows, &monsterWeights, &monsterWhy)) {
			plog::Warn("map monsters: '%s' keeps the slot level's own monster list: %s", map.name.c_str(), monsterWhy.c_str());
			monsterRows.clear();
			monsterWeights.clear();
		}
		std::string recordWhy;
		int64_t levelCount = 0;
		uint8_t* record = nullptr;
		if (GameLevelCount(dataFlag, &levelCount, &recordWhy)) {
			record = RecordOf(game, level, levelCount, &recordWhy);
		}
		const int difficulty = game == nullptr || !Readable(static_cast<const uint8_t*>(game) + GameDifficultyOffset, 1) ? -1 : static_cast<const uint8_t*>(game)[GameDifficultyOffset];

		std::lock_guard<std::mutex> lock(g_lock);
		if (!Readable(row, lr::RowSpan)) {
			return skip("the levels row cannot be read");
		}
		if (lr::RowId(row) != level) {
			return skip("the row's Id is not the slot level's (a different build?)");
		}
		lr::Request request;
		request.level = map.level;
		request.monsterRows = monsterRows;
		request.monsterWeights = monsterWeights;
		request.density = map.density;
		request.uniques = map.uniques;
		if (const std::string refusal = lr::MonstersRefusal(request); !refusal.empty()) {
			plog::Warn("map monsters: '%s' keeps the slot level's own monster list: %s", map.name.c_str(), refusal.c_str());
			request.monsterRows.clear();
			request.monsterWeights.clear();
		}
		if (map.champions > 0) {
			plog::Info("map monsters: '%s': Champions %d%% is not applied (the game has no field for it)", map.name.c_str(), map.champions);
		}

		// --- The row: Level. The row's original values are a copy of the row with the plugin's earlier writes taken out.
		uint8_t rowOriginal[lr::RowSpan];
		lr::OriginalImage(g_row.ledger, level, row, sizeof(rowOriginal), rowOriginal);
		const lr::Snapshot rowSnap = lr::SnapshotOf(rowOriginal);
		const lr::RowVerdict rowVerdict = lr::CheckRow(rowSnap);
		for (const std::string& why : rowVerdict.why) {
			plog::Warn("map monsters: level %d: %s; that part is off for this open", level, why.c_str());
		}
		const lr::Prepared rowPrep = lr::Prepare(g_row.ledger, level, row, lr::PlanRow(request, rowSnap, rowVerdict.level));

		// --- The record: Density, Uniques, Monsters.
		lr::Prepared recPrep;
		lr::RecordSnapshot recSnap;
		lr::Parts recOn;
		if (record == nullptr) {
			plog::Warn("map monsters: level %d: Density, Uniques and Monsters are off for this open: %s", level, recordWhy.c_str());
		} else if (difficulty < 0) {
			plog::Warn("map monsters: level %d: Density, Uniques and Monsters are off for this open: the game's difficulty cannot be read", level);
		} else {
			// A record from an earlier game (the game's memory was reused) no longer holds what the ledger wrote: forget that ledger.
			if (g_rec.ledger.Has(level) && (BaseOf(g_rec, level) != record || !g_rec.ledger.AllHold(level, record))) {
				plog::Info("map monsters: level %d: the region record is not the one written before; its old ledger is dropped", level);
				g_rec.ledger.ForgetLevel(level);
				DropTarget(g_rec, level);
			}
			uint8_t recOriginal[lr::RecSpan];
			lr::OriginalImage(g_rec.ledger, level, record, sizeof(recOriginal), recOriginal);
			recSnap = lr::RecordSnapshotOf(recOriginal);
			const lr::RecordVerdict verdict = lr::CheckRecord(recSnap, rowSnap, difficulty);
			for (const std::string& why : verdict.why) {
				plog::Warn("map monsters: level %d: %s; that part is off for this open", level, why.c_str());
			}
			recOn = verdict.on;
			recPrep = lr::Prepare(g_rec.ledger, level, record, lr::PlanRecord(request, recSnap, recOn));
			if (recOn.density && map.density > 0 && recSnap.density == 0) {
				plog::Info("map monsters: '%s': Density %d%% adds nothing, the slot level's MonDen is 0 in this difficulty", map.name.c_str(), map.density);
			}
			if (recOn.uniques && map.uniques > 0 && recSnap.uMax == 0) {
				plog::Info("map monsters: '%s': Uniques %d%% adds nothing, the slot level's MonUMax is 0 in this difficulty", map.name.c_str(), map.uniques);
			}
		}

		const size_t rowCount = rowPrep.ops.size() + rowPrep.reverts.size();
		const size_t recCount = recPrep.ops.size() + recPrep.reverts.size();
		if (rowCount == 0 && recCount == 0) {
			Note("'" + map.name + "' changed nothing: the game already holds what it asks for");
			plog::Info("map monsters: '%s': level %d already holds the map's values", map.name.c_str(), level);
			return false;
		}
		plog::Info("map monsters: level %d (row %p, record %p): '%s' sets %zu row and %zu record field(s) (Level %d, %zu listed monster(s)%s, Density %d%%, Uniques %d%%); %zu earlier field(s) go back to the original",
		           level, static_cast<const void*>(row), static_cast<const void*>(record), map.name.c_str(), rowPrep.ops.size(), recPrep.ops.size(),
		           lr::ClampLevel(map.level), monsterRows.size(), monsterRows.empty() ? "" : (" rows " + ListText(monsterRows)).c_str(), map.density, map.uniques,
		           rowPrep.reverts.size() + recPrep.reverts.size());
		for (const lr::Op& op : rowPrep.ops) {
			plog::Info("map monsters:   row 0x%02x: %u -> %u", op.field.offset, op.previous, op.field.value);
		}
		for (const lr::Op& op : recPrep.ops) {
			if (op.field.offset >= lr::RecDensityOffset || op.field.offset < lr::RecGroupsOffset) {
				plog::Info("map monsters:   record 0x%03x: %u -> %u", op.field.offset, op.previous, op.field.value);
			}
		}

		// Each space is written, and undone, on its own: a failure in one leaves the other as it is.
		const auto write = [&](Space& space, uint8_t* base, const lr::Prepared& prepared, size_t count) {
			if (count == 0) {
				return true;
			}
			SetTarget(space, level, base);
			space.lastLevel = level;
			space.lastValid = true;
			std::vector<lr::Op> done;
			const bool ok = lr::Apply(space.ledger, level, prepared, WriterFor(base), &done);
			space.lastOps = done;
			if (!ok) {
				plog::Warn("map monsters: writing the %s failed; putting the previous values back", space.what);
				UndoLastLocked(space);
			}
			return ok;
		};
		const bool rowOk = write(g_row, row, rowPrep, rowCount);
		const bool recOk = record == nullptr ? true : write(g_rec, record, recPrep, recCount);
		if (!rowOk || !recOk) {
			Note("'" + map.name + "' was partly not applied: writing the " + std::string(!rowOk ? "row" : "record") + " failed");
		} else {
			Note("'" + map.name + "' set " + std::to_string(rowPrep.ops.size()) + " row and " + std::to_string(recPrep.ops.size()) + " record field(s) in level " + std::to_string(level));
		}
		return (rowOk && rowCount > 0) || (recOk && recCount > 0);
	} catch (...) {
		plog::Error("map monsters: failed for '%s'; the map keeps the slot level's own monsters", map.name.c_str());
		return false;
	}
}

void LevelRowOpenFailed(int level) noexcept {
	try {
		std::lock_guard<std::mutex> lock(g_lock);
		bool undone = false;
		for (Space* space : {&g_row, &g_rec}) {
			if (space->lastValid && space->lastLevel == level) {
				UndoLastLocked(*space);
				undone = true;
			}
		}
		if (undone) {
			plog::Info("map monsters: the portal did not open; level %d is as before", level);
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
		g_row.lastValid = g_rec.lastValid = false;
		g_row.lastOps.clear();
		g_rec.lastOps.clear();
		RestoreRowsLocked();
		ForgetRecordsLocked();
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
	return "Map monsters: on, Level is written into the slot level's row and Density, Uniques and Monsters into its region record when a map opens; Champions is not applied (written, not verified in game); last: " + last;
}

}  // namespace carto
