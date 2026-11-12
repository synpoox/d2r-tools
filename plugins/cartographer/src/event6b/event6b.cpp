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

ObjectLookup resolveObject(const std::vector<std::string> &names, std::string_view spawn, uint64_t gameRows) {
	ObjectLookup out;
	if (spawn.empty()) {
		return out;
	}
	if (gameRows == 0) {
		out.result = ObjectResult::NoGameCount;
		return out;
	}
	const bool namesTrusted = !names.empty() && names.size() == gameRows;
	if (namesTrusted) {
		std::vector<int> rows = linesNamed(names, spawn, true);
		if (rows.empty()) {
			rows = linesNamed(names, spawn, false);
		}
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
	out.result = names.empty() ? ObjectResult::NoTable : !namesTrusted ? ObjectResult::CountMismatch : ObjectResult::NotFound;
	return out;
}

std::string describeObjectLookup(const ObjectLookup &lookup, std::string_view spawn, size_t fileLines, uint64_t gameRows) {
	const std::string name(spawn);
	switch (lookup.result) {
	case ObjectResult::Found:
		return lookup.numeric ? "objects row " + std::to_string(lookup.row) + " from the number in Spawn (the game has " + std::to_string(gameRows) + " rows)"
		                      : "objects row " + std::to_string(lookup.row) + " by its line in objects.txt (the game has " + std::to_string(gameRows) + " rows)";
	case ObjectResult::NotFound:
		return "no line of objects.txt is named '" + name + "'; give the objects row number in Spawn instead";
	case ObjectResult::Duplicate: {
		std::string list;
		for (size_t i = 0; i < lookup.rows.size() && i < 8; ++i) {
			list += (i ? ", " : "") + std::to_string(lookup.rows[i]);
		}
		return "'" + name + "' names " + std::to_string(lookup.rows.size()) + " lines of objects.txt (rows " + list + (lookup.rows.size() > 8 ? ", ..." : "") +
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

} // namespace cartographer::event6b
