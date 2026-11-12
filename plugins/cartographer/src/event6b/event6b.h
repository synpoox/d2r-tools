// Cartographer events, phase 6b (Shrine, Boss Drop, Ambush, Treasure): the small decisions that need no game. Pure C++ on plain
// values: no engine, no SDK. The engine part (src/engine/event_6b.cpp) reads the game and calls these.
// Notes and evidence: docs/reverse-engineering/cartographer.md section 13.
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "events.h"

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

// --- Which engine path an event takes -----------------------------------------------------------------------------------

enum class Route : uint8_t {
	None,             // not one of the five kinds
	Boss,             // the named boss and its minions
	Horde,            // Count of the named monster
	Ambush,           // waves of Count x Spawn, started when the player reaches the event room
	TreasureMonster,  // Treasure whose Spawn is a monster: it flees, and its death rolls Drop
	TreasureChest,    // Treasure whose Spawn is not a monster (an objects.txt name): a chest that rolls Drop when opened
	Shrine,           // an objects.txt row placed at a free spot
};
// `spawnKind` is what the monster lookup made of Spawn (a monstats Id or a superuniques name). A Treasure whose Spawn is not a monster
// is a chest; whether its objects.txt name is valid is decided later, against the game's row count.
Route routeEvent(events::Kind kind, events::NameKind spawnKind);
const char *routeName(Route route);

// --- Ambush waves ---------------------------------------------------------------------------------------------------

// Bookkeeping for an Ambush: wait for the player in the event room, spawn a wave, count its kills by unit id, give the wave a
// time-out, spawn the next one after a short gap, and finish after the last. Plain values; the caller gives the clock (ms).
constexpr uint64_t kWaveTimeoutMs = 90000;  // a guess: a wave not cleared by then counts as done (needs-game item 9)
constexpr uint64_t kWaveGapMs = 1500;       // between a cleared wave and the next
enum class WaveEnd : uint8_t { None, Cleared, TimedOut, Empty };  // how the last wave ended; Empty: the game made none of its units
const char *waveEndName(WaveEnd end);

class WaveTracker {
public:
	enum class Step : uint8_t {
		Wait,       // nothing to do
		SpawnWave,  // spawn the next wave now, then call waveSpawned() with the ids
		Finished,   // every wave is done (or the event was abandoned)
	};
	enum class Phase : uint8_t { Idle, Waiting, Spawning, Fighting, Gap, Done };

	// Starts a new Ambush of `waves` waves (at least 1).
	void arm(int waves, uint64_t timeoutMs = kWaveTimeoutMs, uint64_t gapMs = kWaveGapMs);
	void reset();
	// Called every tick: `inRoom` is whether the local player is in the event room.
	Step step(bool inRoom, uint64_t nowMs);
	// After SpawnWave: the ids of the units that were made. None at all abandons the event (the room cannot take the spawn).
	void waveSpawned(const std::vector<uint32_t> &ids, uint64_t nowMs);
	// A monster died; true when it belonged to the current wave.
	bool onDeath(uint32_t id);

	Phase phase() const { return phase_; }
	bool active() const { return phase_ != Phase::Idle && phase_ != Phase::Done; }
	int wavesStarted() const { return started_; }
	int wavesTotal() const { return waves_; }
	size_t remaining() const { return pending_.size(); }
	WaveEnd lastEnd() const { return lastEnd_; }

private:
	Phase phase_ = Phase::Idle;
	int waves_ = 0;
	int started_ = 0;
	uint64_t timeoutMs_ = kWaveTimeoutMs;
	uint64_t gapMs_ = kWaveGapMs;
	uint64_t deadline_ = 0;
	uint64_t gapUntil_ = 0;
	std::vector<uint32_t> pending_;
	WaveEnd lastEnd_ = WaveEnd::None;
};

// --- Treasure chest ---------------------------------------------------------------------------------------------------

// The operate hook (0x592c40) sees every object use. The chest rolls its Drop once, when its mode changed during the call.
enum class ChestReason : uint8_t { Rolled, NotArmed, NotTracked, NotObject, AlreadyRolled, NotOpened };
struct ChestInput {
	bool armed = false;    // a Treasure chest with a resolved Drop is armed
	bool tracked = false;  // the object's id is the chest the event placed
	bool object = false;   // the unit's type is 2
	bool rolled = false;   // the Drop was rolled already
	int32_t modeBefore = 0;
	int32_t modeAfter = 0;
};
ChestReason decideChest(const ChestInput &input);
const char *chestReasonName(ChestReason reason);

// --- Treasure monster flee ----------------------------------------------------------------------------------------------

// What the two checks 0x597030 makes first said about the target (-1: that check was not run). The flee setup returns at once when
// `blocked` (0x34c2c0) is nonzero or `targetOk` (0x34c730) is zero.
std::string describeFleeChecks(int blocked, int targetOk);

}  // namespace cartographer::event6b
