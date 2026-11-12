#include "event6b.h"

#include <cstdint>
#include <cstring>

namespace cartographer::event6b {

namespace {

char lower(char c) { return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c; }

bool equalsNoCase(std::string_view a, std::string_view b) {
	if (a.size() != b.size()) {
		return false;
	}
	for (size_t i = 0; i < a.size(); ++i) {
		if (lower(a[i]) != lower(b[i])) {
			return false;
		}
	}
	return true;
}

std::vector<int> linesNamed(const std::vector<std::string> &names, std::string_view spawn, bool exact) {
	std::vector<int> rows;
	for (size_t i = 0; i < names.size(); ++i) {
		if (!names[i].empty() && (exact ? std::string_view(names[i]) == spawn : equalsNoCase(names[i], spawn))) {
			rows.push_back(static_cast<int>(i));
		}
	}
	return rows;
}

} // namespace

bool isRowNumber(std::string_view text, int *value) {
	if (text.empty() || text.size() > 9) {
		return false;
	}
	int v = 0;
	for (char c : text) {
		if (c < '0' || c > '9') {
			return false;
		}
		v = v * 10 + (c - '0');
	}
	if (value != nullptr) {
		*value = v;
	}
	return true;
}

ObjectLookup resolveObject(const std::vector<std::string> &names, std::string_view spawn, uint64_t gameRows) { return resolveObject({}, names, spawn, gameRows); }

ObjectLookup resolveObject(const std::vector<std::string> &classes, const std::vector<std::string> &names, std::string_view spawn, uint64_t gameRows) {
	ObjectLookup out;
	if (spawn.empty()) {
		return out;
	}
	if (gameRows == 0) {
		out.result = ObjectResult::NoGameCount;
		return out;
	}
	const bool classesTrusted = !classes.empty() && classes.size() == gameRows;
	const bool namesTrusted = !names.empty() && names.size() == gameRows;
	auto pick = [&](const std::vector<std::string> &column) {
		std::vector<int> rows = linesNamed(column, spawn, true);
		if (rows.empty()) {
			rows = linesNamed(column, spawn, false);
		}
		return rows;
	};
	// Class first (every Class is distinct), then Name, which is refused when more than one line has it.
	for (int pass = 0; pass < 2; ++pass) {
		if (!(pass == 0 ? classesTrusted : namesTrusted)) {
			continue;
		}
		std::vector<int> rows = pick(pass == 0 ? classes : names);
		if (rows.size() == 1) {
			out.result = ObjectResult::Found;
			out.row = rows[0];
			return out;
		}
		if (rows.size() > 1) {
			out.result = ObjectResult::Duplicate;
			out.rows = std::move(rows);
			return out;
		}
	}
	int number = 0;
	if (isRowNumber(spawn, &number)) {
		if (static_cast<uint64_t>(number) < gameRows) {
			out.result = ObjectResult::Found;
			out.row = number;
			out.numeric = true;
		} else {
			out.result = ObjectResult::BadRow;
		}
		return out;
	}
	out.result = (names.empty() && classes.empty()) ? ObjectResult::NoTable : !(namesTrusted || classesTrusted) ? ObjectResult::CountMismatch : ObjectResult::NotFound;
	return out;
}

std::vector<std::string> dropMarkerLines(const std::vector<std::string> &classes, const std::vector<std::string> &cells) {
	std::vector<std::string> out;
	const size_t n = classes.size() < cells.size() ? classes.size() : cells.size();
	for (size_t i = 0; i < n; ++i) {
		if (!equalsNoCase(classes[i], "Expansion")) {
			out.push_back(cells[i]);
		}
	}
	return out;
}

std::vector<std::string> objectRowNames(const std::vector<std::string> &classes, const std::vector<std::string> &names) { return dropMarkerLines(classes, names); }

bool spotInBox(const SpotBox &room, int x, int y) { return x >= room.x && x < room.x + room.w && y >= room.y && y < room.y + room.h; }

std::vector<SpotAttempt> freeSpotAttempts(const SpotBox &room, int maxRadius) {
	std::vector<SpotAttempt> out;
	if (room.w <= 0 || room.h <= 0) {
		return out;
	}
	std::vector<int> radii;
	for (int r : {maxRadius, 2, 1}) {
		if (r <= maxRadius && (radii.empty() || radii.back() > r)) {
			radii.push_back(r);
		}
	}
	const int xs[] = {room.x + room.w / 2, room.x + room.w / 4, room.x + room.w * 3 / 4};
	const int ys[] = {room.y + room.h / 2, room.y + room.h / 4, room.y + room.h * 3 / 4};
	std::vector<SpotAttempt> starts = {{xs[0], ys[0], 0}};
	for (int i = 1; i < 3; ++i) {
		for (int j = 1; j < 3; ++j) {
			starts.push_back({xs[i], ys[j], 0});
		}
	}
	for (int r : radii) {
		for (const SpotAttempt &s : starts) {
			out.push_back({s.x, s.y, r});
		}
	}
	return out;
}

std::string describeObjectLookup(const ObjectLookup &lookup, std::string_view spawn, size_t fileLines, uint64_t gameRows) {
	const std::string name(spawn);
	switch (lookup.result) {
	case ObjectResult::Found:
		return lookup.numeric ? "objects row " + std::to_string(lookup.row) + " from the number in Spawn (the game has " + std::to_string(gameRows) + " rows)"
		                      : "objects row " + std::to_string(lookup.row) + " by its line in objects.txt (the game has " + std::to_string(gameRows) + " rows)";
	case ObjectResult::NotFound:
		return "no line of objects.txt has Class or Name '" + name + "'; give the objects row number in Spawn instead";
	case ObjectResult::Duplicate: {
		std::string list;
		for (size_t i = 0; i < lookup.rows.size() && i < 8; ++i) {
			list += (i ? ", " : "") + std::to_string(lookup.rows[i]);
		}
		return "'" + name + "' matches " + std::to_string(lookup.rows.size()) + " lines of objects.txt (rows " + list + (lookup.rows.size() > 8 ? ", ..." : "") +
		       "), so it is not a single object; give the objects row number in Spawn instead";
	}
	case ObjectResult::NoTable:
		return "objects.txt could not be read from the mod's excel folder, so '" + name + "' cannot be turned into a row; give the objects row number in Spawn";
	case ObjectResult::CountMismatch:
		return "objects.txt has " + std::to_string(fileLines) + " lines but the game has " + std::to_string(gameRows) + " objects rows, so line order is not the row index and '" + name +
		       "' is not trusted; give the objects row number in Spawn";
	case ObjectResult::BadRow:
		return "Spawn '" + name + "' is a row number, but the game has only " + std::to_string(gameRows) + " objects rows";
	case ObjectResult::NoGameCount:
		return "the game's objects row count is not known, so '" + name + "' cannot be checked";
	}
	return {};
}

int tcIndexFromLookup(int32_t found, uint64_t tcCount, std::string *why) {
	auto fail = [&](const std::string &text) {
		if (why != nullptr) {
			*why = text;
		}
		return -1;
	};
	if (found == -1) {
		return fail("the game has no treasure class of this name");
	}
	if (found < 0) {
		return fail("the game's name lookup gave " + std::to_string(found) + ", which is neither a row nor its miss (-1)");
	}
	if (found == 0) {
		return fail("the lookup gave index 0, which the roller does not take as a class");
	}
	if (found > 0xFFFF) {
		return fail("the lookup gave " + std::to_string(found) + ", which does not fit the roller's 16-bit index");
	}
	if (tcCount == 0 || static_cast<uint64_t>(found) >= tcCount) {
		return fail("the lookup gave " + std::to_string(found) + ", past the game's " + std::to_string(tcCount) + " treasure classes");
	}
	return found;
}

DropDecision decideDrop(const DropInput &in) {
	auto skip = [](DropReason reason) { return DropDecision{DropAction::Skip, reason}; };
	if (!in.armed) {
		return skip(DropReason::NotArmed);
	}
	if (!in.tracked) {
		return skip(DropReason::NotTracked);
	}
	if (!in.monster) {
		return skip(DropReason::NotMonster);
	}
	if (in.dropped) {
		return skip(DropReason::AlreadyDropped);
	}
	if (!in.hasKiller) {
		return skip(DropReason::NoKiller);
	}
	return {DropAction::Roll, DropReason::Rolled};
}

const char *dropReasonName(DropReason reason) {
	switch (reason) {
	case DropReason::Rolled: return "rolled";
	case DropReason::NotArmed: return "no armed Drop";
	case DropReason::NotTracked: return "not the event's boss";
	case DropReason::NotMonster: return "not a monster";
	case DropReason::AlreadyDropped: return "already rolled for this boss";
	case DropReason::NoKiller: return "the death info has no killer";
	}
	return "";
}

int64_t callTargetOffset(const uint8_t *code, size_t size, size_t site) {
	if (code == nullptr || site > size || size - site < 5 || code[site] != 0xE8) {
		return kNoTarget;
	}
	int32_t rel = 0;
	std::memcpy(&rel, code + site + 1, sizeof(rel));
	return static_cast<int64_t>(site) + 5 + rel;
}

Route routeEvent(events::Kind kind, events::NameKind spawnKind) {
	switch (kind) {
	case events::Kind::Boss: return Route::Boss;
	case events::Kind::Horde: return Route::Horde;
	case events::Kind::Ambush: return Route::Ambush;
	case events::Kind::Shrine: return Route::Shrine;
	case events::Kind::Treasure:
		return spawnKind == events::NameKind::Monster || spawnKind == events::NameKind::Unique ? Route::TreasureMonster : Route::TreasureChest;
	case events::Kind::Invalid: break;
	}
	return Route::None;
}

const char *routeName(Route route) {
	switch (route) {
	case Route::None: return "none";
	case Route::Boss: return "boss";
	case Route::Horde: return "horde";
	case Route::Ambush: return "ambush";
	case Route::TreasureMonster: return "treasure monster";
	case Route::TreasureChest: return "treasure chest";
	case Route::Shrine: return "shrine";
	}
	return "";
}

const char *waveEndName(WaveEnd end) {
	switch (end) {
	case WaveEnd::None: return "none yet";
	case WaveEnd::Cleared: return "cleared";
	case WaveEnd::TimedOut: return "timed out";
	case WaveEnd::Empty: return "the game made none of its units";
	}
	return "";
}

void WaveTracker::arm(int waves, uint64_t timeoutMs, uint64_t gapMs) {
	reset();
	waves_ = waves < 1 ? 1 : waves;
	timeoutMs_ = timeoutMs;
	gapMs_ = gapMs;
	phase_ = Phase::Waiting;
}

void WaveTracker::reset() {
	*this = WaveTracker{};
}

WaveTracker::Step WaveTracker::step(bool inRoom, uint64_t nowMs) {
	switch (phase_) {
	case Phase::Idle:
	case Phase::Spawning:  // waveSpawned() has not come back yet
		return Step::Wait;
	case Phase::Done:
		return Step::Finished;
	case Phase::Waiting:
		if (!inRoom) {
			return Step::Wait;
		}
		phase_ = Phase::Spawning;
		++started_;
		return Step::SpawnWave;
	case Phase::Fighting:
		if (pending_.empty()) {
			lastEnd_ = WaveEnd::Cleared;
		} else if (nowMs >= deadline_) {
			lastEnd_ = WaveEnd::TimedOut;
			pending_.clear();  // a monster that never reports must not hold the next wave
		} else {
			return Step::Wait;
		}
		if (started_ >= waves_) {
			phase_ = Phase::Done;
			return Step::Finished;
		}
		phase_ = Phase::Gap;
		gapUntil_ = nowMs + gapMs_;
		return Step::Wait;
	case Phase::Gap:
		if (!inRoom || nowMs < gapUntil_) {
			return Step::Wait;
		}
		phase_ = Phase::Spawning;
		++started_;
		return Step::SpawnWave;
	}
	return Step::Wait;
}

void WaveTracker::waveSpawned(const std::vector<uint32_t> &ids, uint64_t nowMs) {
	if (phase_ != Phase::Spawning) {
		return;
	}
	if (ids.empty()) {
		lastEnd_ = WaveEnd::Empty;
		phase_ = Phase::Done;
		return;
	}
	pending_ = ids;
	for (const uint32_t dead : early_) {
		for (size_t i = 0; i < pending_.size(); ++i) {
			if (pending_[i] == dead) {
				pending_.erase(pending_.begin() + static_cast<std::ptrdiff_t>(i));
				break;
			}
		}
	}
	early_.clear();
	deadline_ = nowMs + timeoutMs_;
	phase_ = Phase::Fighting;
}

bool WaveTracker::onDeath(uint32_t id) {
	if (phase_ == Phase::Spawning) {
		if (early_.size() < 256) {
			early_.push_back(id);
		}
		return false;
	}
	if (phase_ != Phase::Fighting) {
		return false;
	}
	for (size_t i = 0; i < pending_.size(); ++i) {
		if (pending_[i] == id) {
			pending_.erase(pending_.begin() + static_cast<std::ptrdiff_t>(i));
			return true;
		}
	}
	return false;
}

ChestReason decideChest(const ChestInput &in) {
	if (!in.armed) {
		return ChestReason::NotArmed;
	}
	if (!in.tracked) {
		return ChestReason::NotTracked;
	}
	if (!in.object) {
		return ChestReason::NotObject;
	}
	if (in.rolled) {
		return ChestReason::AlreadyRolled;
	}
	if (in.modeBefore == in.modeAfter) {
		return ChestReason::NotOpened;
	}
	return ChestReason::Rolled;
}

const char *chestReasonName(ChestReason reason) {
	switch (reason) {
	case ChestReason::Rolled: return "rolled";
	case ChestReason::NotArmed: return "no armed chest Drop";
	case ChestReason::NotTracked: return "not the event's chest";
	case ChestReason::NotObject: return "not an object";
	case ChestReason::AlreadyRolled: return "already rolled for this chest";
	case ChestReason::NotOpened: return "the chest's mode did not change, so it was not opened by this call";
	}
	return "";
}

std::string describeFleeChecks(int blocked, int targetOk) {
	std::string text = blocked < 0 ? "0x34c2c0 not run" : blocked != 0 ? "0x34c2c0 says the target is excluded (nonzero)" : "0x34c2c0 passes";
	text += "; ";
	text += targetOk < 0 ? "0x34c730 not run" : targetOk == 0 ? "0x34c730 says the target cannot be scared (zero)" : "0x34c730 passes";
	const bool refused = (blocked > 0) || targetOk == 0;
	text += refused ? ": 0x597030 will return without setting the flee" : blocked < 0 || targetOk < 0 ? ": not fully probed" : ": 0x597030 should set the flee";
	return text;
}

} // namespace cartographer::event6b
