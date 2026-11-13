// Cartographer event plan (phase 6a): turns a decided event (src/events) into what the engine part carries out, and holds the
// small decisions around it that need no game: which monster a Spawn name means, where the event room is, where the
// monsters stand, and what the entry banner shows and for how long. Pure C++ on plain structs: no engine, no SDK, no floats.
// The engine part (src/engine/event_apply.cpp) reads the game and calls these.
#pragma once
#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "events.h"

namespace cartographer::eventplan {

// --- Spawn names ---------------------------------------------------------------------------------------------------

// monstats.txt Id by row, and superuniques.txt Superunique and Class by row. The row index is what the engine is asked for.
struct SpawnTable {
	struct Super {
		std::string name;
		std::string cls;  // the monstats Id the superunique is made from
	};
	std::vector<std::string> monsters;  // an empty cell keeps its row
	std::vector<Super> supers;
	bool loaded() const { return !monsters.empty(); }
};

struct Resolved {
	events::NameKind kind = events::NameKind::Unresolved;  // Monster or Unique when found
	int row = -1;       // monstats row (Monster) or superuniques row (Unique)
	int classRow = -1;  // monstats row to spawn: the monster itself, or a superunique's Class (-1: unknown)
};

// `spawn` against monstats Id first, then superuniques Name; exact match first, then ASCII case-insensitive. A name in
// both tables is the monster. Not found: kind Unresolved.
Resolved resolveSpawn(const SpawnTable &table, std::string_view spawn);

// --- Spawn names from the game's loaded tables (docs/reverse-engineering/cartographer/11-spawn-names.md) ---------------------

// The compiled monstats row is 0x1fc bytes with the `Id` inline at +2 (47 bytes, NUL-terminated; the u16 at +0 is the row's own
// index, seen in game); a superuniques row is 0x3c.
constexpr uint32_t kMonStatsRowSize = 0x1fc;
constexpr uint32_t kSuperRowSize = 0x3c;
constexpr size_t kMonStatsIdOffset = 2;
constexpr size_t kMonStatsIdSize = 47;

// `name` against the NUL-terminated text in the first `fieldSize` bytes of `field`, ASCII case-insensitive. An empty name or
// empty field never matches; a field with no NUL within `fieldSize` is not text and never matches.
bool idMatches(const char *field, size_t fieldSize, std::string_view name);

// The text of an `Id` field (up to its NUL), or nullopt when it has no NUL within `fieldSize`.
std::optional<std::string_view> idText(const char *field, size_t fieldSize);

// The first row of `rowCount` rows of `rowSize` bytes whose inline `Id` matches `name`, or -1. Rows are read-only.
int findMonsterRow(const uint8_t *rows, uint64_t rowCount, uint32_t rowSize, std::string_view name);

// What the SDK table view says about one table beside what the game's own table base (`dt`) holds for it.
struct TableFacts {
	const void *viewRows = nullptr;
	uint32_t viewCount = 0;
	uint32_t viewRowSize = 0;
	const void *dtRows = nullptr;  // *(dt + vector offset)
	uint64_t dtCount = 0;          // the 64-bit count beside it
};

// Empty when the view is the game's own table for this data flag: the row size is the expected one, the pointers are equal
// and not null, and the 64-bit count equals the SDK's 32-bit one. Else why not.
std::string checkTable(const TableFacts &facts, uint32_t wantRowSize);

// What looking every monstats `Id` up through the game's name map gave: each non-empty `Id` should map to a row that carries it.
// A repeated `Id` (any case) is a mod mistake; which copy the game's map keeps is not known, so either copy's row is accepted
// (`idRowAccepted`) rather than turning the route off for the whole game on one odd row.
struct RoundTrip {
	bool mapNull = false;
	int checked = 0;
	int wrong = 0;
	int firstWrongRow = -1;  // the first row whose `Id` came back as another index
	int firstWrongGot = 0;
};
// Empty when the map can be trusted for name lookups. Else why not.
std::string checkRoundTrip(const RoundTrip &trip);

// True when the game's lookup `got` is one of `rowsWithId`, the rows that carry the looked-up `Id` (one row, or several for a repeat).
bool idRowAccepted(const std::vector<int> &rowsWithId, int got);

// The lookup result for superuniques: exactly -1 is the game's miss; any other negative number or a row past the table is
// not a row either (`rowCount` is the table's). Returns the row, or -1.
int superRowFromLookup(int32_t result, uint64_t rowCount);

// The monstats Id a superuniques row (the txt's) names as its Class, or empty when the row is out of range.
std::string_view superClassOf(const SpawnTable &table, int superRow);

// Does the Class the game's tables gave for the superunique `name` (`gameClassId`, the `Id` at the game's monstats row) agree with
// the mod's txt `Class` name (case-insensitive)? Names are compared, never row numbers.
// `NoReference`: the txt does not have the name, or it has no Class, or `gameClassId` is empty, so there is nothing to compare.
enum class ClassCheck { NoReference, Agrees, Differs };
ClassCheck checkSuperClass(const SpawnTable &table, std::string_view name, std::string_view gameClassId);

// What the self-check decided for one (table revision, data flag).
struct RouteVerdict {
	bool monsters = false;  // the SDK monstats table is the game's table for this data flag
	bool supers = false;    // the superuniques name map may be called
	uint32_t bank = 0;      // the SDK Bank whose views matched
	std::string why;        // what is off, and why (empty when everything is on)
};
struct RouteKey {
	uint64_t revision = 0;  // the SDK TableView revision (changes on every table load)
	uint32_t dataFlag = 0;
	bool operator==(const RouteKey &o) const { return revision == o.revision && dataFlag == o.dataFlag; }
};
// Verdicts by (view revision, data flag). A newer revision drops the older ones: a table load invalidates everything.
class RouteCache {
public:
	const RouteVerdict *find(const RouteKey &key) const;
	void store(const RouteKey &key, RouteVerdict verdict);
	size_t size() const { return entries_.size(); }

private:
	std::vector<std::pair<RouteKey, RouteVerdict>> entries_;
};

// --- Placement -----------------------------------------------------------------------------------------------------

struct Rect {
	int x = 0, y = 0, w = 0, h = 0;
};
struct Spot {
	int x = 0, y = 0;
};

// Up to `n` distinct spots inside `room`: the centre first, then a grid around it, kSpotGap apart and kEdgeInset clear of the edge where the room is
// big enough. Only when that holds fewer than `n` is the whole layout redone at kTightInset/kTightGap (a Horde of 6 in an 8 x 8 tile room
// becomes a 3 x 3 block around the centre, not 4 spread out plus 2 extra). Same inputs, same spots. Fewer than `n` when the room is too small.
// kEdgeInset and kSpotGap are in tiles; `scale` is how many of the room's own units make a tile (1 for a rect in tiles,
// 5 for the server room's rect), so the spacing stays the same on the ground.
constexpr int kEdgeInset = 2;
constexpr int kSpotGap = 2;
// When the room cannot hold `n` spots at that spacing (an outdoor event room is one 8 x 8 tile room), the spots are packed one tile apart.
constexpr int kTightInset = 1;
constexpr int kTightGap = 1;
std::vector<Spot> placements(const Rect &room, int n, uint64_t seed, int scale = 1);

// Where the map Boss may be tried (issue #183): the centre first, then the tiles of the room one tile clear of its edge (where the room
// is big enough), nearest the centre first, at most kBossSpots. A fail-open backstop only: the game's own free-spot search (SpawnMonster
// p7 = 0xa) walks rings 3, 6, ... 30 subtiles out from any start, on a ring grid 3 subtiles apart, so starts 5 subtiles apart (9 in all: the
// centre and its 8 neighbour tiles, every offset mod 3) together reach every subtile but the 9 starts themselves (for a ~40 x 40 maze
// room; an outdoor clearing may be larger than the rings reach). The refusals seen in #183 were the stale room of #184.
constexpr int kBossSpots = 9;
std::vector<Spot> bossSpots(const Rect &room, int scale = 1);

// How many of the server room's units make one DRLG tile: server.w / drlg.w when the server rect is a whole multiple of the
// DRLG rect in both directions (5 for subtiles), else 1.
int spotScale(const Rect &server, const Rect &drlg);

// Why spotScale gave what it gave, for the probe log: a scale of 1 alone is ambiguous.
enum class ScaleKind { Same, Multiple, NotAMultiple };
ScaleKind scaleKind(const Rect &server, const Rect &drlg);
const char *scaleKindName(ScaleKind kind);

// --- The event room ------------------------------------------------------------------------------------------------

// Which room an event spawns in. A maze map knows the room from its plan (`planned`: the DRLG rect of the plan's event
// cell). A preset map has no plan: the first room populated in the level is the entry room, and the event room is the first
// room populated after it whose centre is at least `minDistance` away (Chebyshev, in the rect's own units: DRLG tiles); the
// caller gives `presetEventDistance` there. It fires once.
constexpr int kMinEventDistance = 24;
// The distance for a level of `levelW` x `levelH` tiles: a third of the larger side, from 1 up to kMinEventDistance, so a small
// map still has a room that qualifies. A size that is not known (no side above 0) gives kMinEventDistance.
int minEventDistance(int levelW, int levelH);
// The room of `rooms` (the level's whole room list, DRLG rects in tiles) whose centre is farthest from `entry`'s centre: the largest
// Chebyshev distance, then the largest squared straight distance, then the earlier in the list. A room with the entry's own rect, and
// a room with no size, are skipped. Empty when no other room exists. Same inputs, same room.
std::optional<Rect> farthestRoom(const Rect &entry, const std::vector<Rect> &rooms);
// The distance a preset map's event room must reach: `minEventDistance(levelW, levelH)`, capped at the Chebyshev distance of the farthest
// room of `rooms` from `entry`, so at least one room always qualifies and the event lands where players walk, not only in the farthest
// corner. With no other room in the list (or an unreadable one: pass an empty list) it is `minEventDistance` alone.
int presetEventDistance(int levelW, int levelH, const Rect &entry, const std::vector<Rect> &rooms);
class EventRoomTracker {
public:
	explicit EventRoomTracker(std::optional<Rect> planned = std::nullopt, int minDistance = kMinEventDistance);
	// A room of the level was populated. True when this is the event room (only ever true once).
	bool onPopulated(const Rect &room);
	bool fired() const { return fired_; }
	bool planned() const { return planned_.has_value(); }
	int populated() const { return count_; }

private:
	std::optional<Rect> planned_;
	std::optional<Rect> entry_;
	int minDistance_;
	int count_ = 0;
	bool fired_ = false;
};

// Which room the map's own Boss goes in, and what happens when the game refuses it there. Pure: the engine keeps the rooms' game
// pointers by `indexOf` and does the spawning; this decides the order, so the Boss always ends up somewhere (no give-up).
//   1. `cell`: the plan's boss room, then the other rooms of its cell (a maze cell has several engine rooms), in order. Each is waited
//      for until it is populated, so the Boss is not put anywhere else while the cell may still take it.
//   2. `others`: every other room of the level, farthest from the entrance first, the entrance room last (`entry`). Once the cell is
//      spent they are tried as they are populated: the populated ones at once, the farthest first; one that populates later the moment it does.
//   3. The last resort: when every populated room has refused, the entrance room is tried again with `forcing()` (the game's own spot
//      test for collision skipped), and any room that populates after that is tried normally. There is no give-up.
// A room the list does not know (the room list could not be read) is `adopt`ed as a populated candidate at the end.
class BossRoomPicker {
public:
	enum class Tier { Cell, Other, Forced };
	BossRoomPicker(std::vector<Rect> cell, std::vector<Rect> others = {}, std::optional<Rect> entry = std::nullopt);
	// The candidate's index for a rect, -1 when the rect is not one of them.
	int indexOf(const Rect &room) const;
	// A room the list does not know, populated: added as the last candidate. Returns its index (the existing one when already known).
	int adopt(const Rect &room);
	// A room of the level was populated. True when the Boss should be tried in it now (`current()` is then its index). In the cell it is
	// the current candidate only; after the cell any room not yet refused counts. A later cell room is remembered for `refused`.
	bool onPopulated(const Rect &room);
	// The Boss was placed: no more tries.
	void finish() { done_ = true; }
	// The try in `current()` failed (or its room cannot be read: that counts as a refusal). Returns the index to try next, now, or -1: then
	// wait (`currentRoom()` is the cell room waited for, null after the cell: the next room to populate is tried at once).
	int refused();
	// The tier of the room being tried or waited for.
	Tier tier() const { return forcing_ ? Tier::Forced : (current_ < cellCount_ ? Tier::Cell : Tier::Other); }
	bool forcing() const { return forcing_; }
	// The cell room being waited for, when there is one.
	const Rect *currentRoom() const { return !done_ && !forcing_ && current_ < cellCount_ ? &ranked_[current_] : nullptr; }
	bool done() const { return done_; }
	size_t size() const { return ranked_.size(); }
	size_t current() const { return current_; }
	size_t cellSize() const { return cellCount_; }
	const Rect &room(size_t i) const { return ranked_[i]; }

private:
	int nextUntried();
	std::vector<Rect> ranked_;
	std::vector<bool> populated_;
	std::vector<bool> refused_;
	size_t cellCount_ = 0;
	size_t current_ = 0;
	std::optional<size_t> entry_;  // the index of the entrance room, the forced try's room
	bool forcing_ = false;
	bool forced_ = false;  // the forced try was made
	bool done_ = false;
};

// The rooms other than `entry` and `exclude`, farthest first from `entry` (Chebyshev distance of the centres, then the larger squared
// distance, then list order). The order of `BossRoomPicker`'s `others`.
std::vector<Rect> roomsFarthestFirst(const Rect &entry, const std::vector<Rect> &rooms, const std::vector<Rect> &exclude);

// What one try of the Boss in one room came to.
enum class BossTry { Made, Refused, Stop };
// What the engine loop did, the picker's decisions run against a `tryRoom` that does the spawn. `tries` lists every room tried, in order, with
// its tier; `placed`: the Boss is made; `stopped`: the loop was told to stop (the game was left); `waiting`: no room is ready now (`waitRoom` is the
// cell room waited for, when there is one; else the next room to populate takes it). Pure, so the engine's loop is tested as it runs.
struct BossDrive {
	struct Step {
		size_t index = 0;
		BossRoomPicker::Tier tier = BossRoomPicker::Tier::Cell;
	};
	std::vector<Step> tries;
	bool placed = false;
	bool stopped = false;
	bool waiting = false;
	std::optional<Rect> waitRoom;
};
BossDrive driveBoss(BossRoomPicker &picker, size_t first, const std::function<BossTry(size_t index, BossRoomPicker::Tier tier)> &tryRoom);

// --- What is spawned -----------------------------------------------------------------------------------------------

struct Spawn {
	bool superUnique = false;  // true: spawn superuniques row `row`; false: monstats row `row`
	int row = -1;
	int classRow = -1;  // for a superunique: the monstats row used when it cannot be spawned as one
	Spot spot;
};

struct SpawnPlan {
	std::vector<Spawn> spawns;
	std::string problem;  // empty when usable
	int requested = 0;    // how many monsters the event asked for (Count, 1 + Minions, wave size); spawns.size() is less when the room has no more spots
};

// A Boss: the named boss at the room centre, plus `minions` of its class around it. A Horde: `hordeSize` of the named
// monster (a superunique spawns as its class: it can only exist once per game). An Ambush: one wave, `waveSize` of the named monster
// (the engine part spawns it once per wave). A Treasure monster: the one monster (the first spawn, a superunique if named). A Shrine and a
// Treasure chest are placed by the engine part, not planned here: a Shrine gives a problem.
// `room` is the rect the spots are placed in (the server room's, which is what the spawn code takes), `scale` its units per tile.
SpawnPlan planEvent(const events::Event &event, const Resolved &spawn, const Rect &room, uint64_t seed, int scale = 1);

// The map's own Boss: one monster, the centre spot first and then bossSpots' others for the caller to try in order (a superunique when the name is one; the engine part falls back to its Class when the
// game has spawned it already). No minions, no Drop: those belong to the Boss event. A name that is neither a monstats Id nor a superunique, or a
// superunique whose Class is not in monstats, gives a problem.
SpawnPlan planMapBoss(const std::string &name, const Resolved &boss, const Rect &room, int scale = 1);

// The game keeps one bit per superuniques row that says it was spawned in this game: bit (row & 7) of byte (row >> 3).
constexpr int kMaxSuperRow = 0xFFFF;
bool superSeen(const uint8_t *bitmap, int row);

// --- Entry message -------------------------------------------------------------------------------------------------

// The lines shown on arrival: the map's name, then the event's Name and Message when an event is given. The caller gives the
// event only once its room is chosen and something spawned, so a banner never announces an event that did not happen.
std::vector<std::string> entryLines(const std::string &mapName, const events::Event *event);

// Which lines a banner shows: the event's Name and Message, only once `announced`, never the map's name (the game's own "Entering
// <name>" is the title; `mapName` and `late` are kept for the callers). An empty result shows nothing.
std::vector<std::string> bannerLines(const std::string &mapName, const events::Event *event, bool announced, bool late);

// True once each time the player goes from outside the map's level to inside it (and again after leaving and coming back).
class EntryTracker {
public:
	bool update(bool inMap);
	bool inside() const { return inside_; }
	void reset() { inside_ = false; }

private:
	bool inside_ = false;
};

// The banner's opacity (0..255) `elapsedMs` after it started: fades in, holds, fades out, then 0 for good.
constexpr int kBannerTotalMs = 6100;  // how long the banner panel stays open
// Whether the banner is still up this long (ms) after it started.
bool bannerVisible(int64_t elapsedMs);

// The event stream's seed from the map's seed (salted, so it never shares draws with the stats).
uint64_t eventSeed(uint64_t mapSeed);

} // namespace cartographer::eventplan
