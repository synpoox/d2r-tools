// Cartographer events, phase 6b (Shrine and Boss Drop): the small decisions that need no game. Pure C++ on plain values:
// no engine, no SDK. The engine part (src/engine/event_6b.cpp) reads the game and calls these.
// Notes and evidence: docs/reverse-engineering/cartographer.md section 13.
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace cartographer::event6b {

// --- Objects: a Shrine's Spawn name to an objects.txt row -------------------------------------------------------------

// The row index of a compiled objects row is its line order in objects.txt. A name is a valid Spawn only when exactly one line
// has it; the file's line count must also equal the game's compiled row count (`*(dt+0x1530)`), else line order is not known
// to be the row index and a name is refused. A Spawn that is only digits is a row index (what the game's own debug handler
// takes), allowed when it is below the game's row count and no line is named with those digits.
enum class ObjectResult : uint8_t {
	Found,
	NotFound,       // no line has this name (and the Spawn is not a row index)
	Duplicate,      // two or more lines have this name; `rows` lists them
	NoTable,        // objects.txt could not be read, so there are no names
	CountMismatch,  // the file's line count differs from the game's row count: names are not trusted
	BadRow,         // a numeric Spawn at or past the game's row count
	NoGameCount,    // the game's row count is not known (0): nothing is checked against it
};

struct ObjectLookup {
	ObjectResult result = ObjectResult::NotFound;
	int row = -1;           // the objects row when Found
	bool numeric = false;   // Found by a numeric Spawn, not by name
	std::vector<int> rows;  // Duplicate: every line with the name
};

// `names`: the Name cell of every objects.txt line in order (an empty cell keeps its line). `gameRows`: the game's compiled row
// count. Exact (case-sensitive) matches are counted first; with none, ASCII case-insensitive ones.
ObjectLookup resolveObject(const std::vector<std::string> &names, std::string_view spawn, uint64_t gameRows);

// One plain-English line for the log: why `spawn` was or was not accepted.
std::string describeObjectLookup(const ObjectLookup &lookup, std::string_view spawn, size_t fileLines, uint64_t gameRows);

// True when the whole text is a decimal number (no sign, no space), at most 9 digits.
bool isRowNumber(std::string_view text, int *value);

// --- Treasure class names ---------------------------------------------------------------------------------------------

// The game's name map (`*(dt+0xd70)`) gives a treasure class index, or exactly -1 for a miss; the vector it indexes
// (`*(dt+0xd78)`, count `*(dt+0xd80)`) is what the roller reads. Index 0 is refused: the roller's lookup (`0x397290`) returns no
// class for it. The index must fit the 16 bits the roller takes. Returns the index, or -1 (with `why`).
int tcIndexFromLookup(int32_t found, uint64_t tcCount, std::string *why);

// --- Boss Drop --------------------------------------------------------------------------------------------------------

enum class DropAction : uint8_t {
	Skip,  // not our unit, or nothing to do
	Roll,  // roll the Drop class once more
};
enum class DropReason : uint8_t {
	Rolled,
	NotArmed,       // no Boss event with a usable Drop on this map
	NotTracked,     // another unit died
	NotMonster,     // the dying unit is not a monster
	AlreadyDropped, // the extra roll was already made for this boss
	NoKiller,       // the death info has no killer pointer: the roller is not called with an unknown argument
};
struct DropInput {
	bool armed = false;     // a Boss event with a resolved Drop treasure class is armed
	bool tracked = false;   // the dying unit's id is the boss the event spawned
	bool monster = false;   // the dying unit's type is 1
	bool dropped = false;   // the extra roll was made already
	bool hasKiller = false; // the death info's killer pointer is not null
};
struct DropDecision {
	DropAction action = DropAction::Skip;
	DropReason reason = DropReason::NotArmed;
};
DropDecision decideDrop(const DropInput &input);
const char *dropReasonName(DropReason reason);

// --- Finding a call target ---------------------------------------------------------------------------------------------

// `0x402f50` has no signature of its own (it is byte-identical to a sibling for 452 bytes), so it is found as the target of the call
// at a known offset inside the object spawn handler. `code` holds `size` bytes; the call is at `site`. Returns the target's
// offset from `code` (it may lie outside the buffer), or `kNoTarget` when the byte at `site` is not E8 or the call does not fit.
constexpr int64_t kNoTarget = INT64_MIN;
int64_t callTargetOffset(const uint8_t *code, size_t size, size_t site);

}  // namespace cartographer::event6b
