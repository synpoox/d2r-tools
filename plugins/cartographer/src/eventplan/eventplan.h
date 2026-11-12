// Cartographer event plan (phase 6a): turns a decided event (src/events) into what the engine part carries out, and holds the
// small decisions around it that need no game: which monster a Spawn name means, where the event room is, where the
// monsters stand, and what the entry banner shows and for how long. Pure C++ on plain structs: no engine, no SDK, no floats.
// The engine part (src/engine/event_apply.cpp) reads the game and calls these.
#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
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

// --- Placement -----------------------------------------------------------------------------------------------------

struct Rect {
	int x = 0, y = 0, w = 0, h = 0;
};
struct Spot {
	int x = 0, y = 0;
};

// Up to `n` distinct spots inside `room`, kept kEdgeInset clear of its edge where the room is big enough: the centre
// first, then a grid around it, kSpotGap apart. Same inputs, same spots. Fewer than `n` when the room is too small.
constexpr int kEdgeInset = 2;
constexpr int kSpotGap = 2;
std::vector<Spot> placements(const Rect &room, int n, uint64_t seed);

// --- The event room ------------------------------------------------------------------------------------------------

// Which room an event spawns in. A maze map knows the room from its plan (`planned`: the DRLG rect of the plan's event
// cell). A preset map has no plan: the first room populated in the level is the entry room, and the event room is the first
// room populated after it whose centre is at least `minDistance` away (Chebyshev, in the rect's own units: DRLG tiles).
// It fires once.
constexpr int kMinEventDistance = 24;
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
};

// A Boss: the named boss at the room centre, plus `minions` of its class around it. A Horde: `hordeSize` of the named
// monster (a superunique spawns as its class: it can only exist once per game). Other kinds give a problem (phase 6b).
SpawnPlan planEvent(const events::Event &event, const Resolved &spawn, const Rect &room, uint64_t seed);

// The game keeps one bit per superuniques row that says it was spawned in this game: bit (row & 7) of byte (row >> 3).
constexpr int kMaxSuperRow = 0xFFFF;
bool superSeen(const uint8_t *bitmap, int row);

// --- Entry message -------------------------------------------------------------------------------------------------

// The lines shown on arrival: the map's name, then the event's Name and Message when an event fired and is applied.
std::vector<std::string> entryLines(const std::string &mapName, const events::Event *event);

// True once each time the player goes from outside the map's level to inside it (and again after leaving and coming back).
class EntryTracker {
public:
	bool update(bool inMap);
	void reset() { inside_ = false; }

private:
	bool inside_ = false;
};

// The banner's opacity (0..255) `elapsedMs` after it started: fades in, holds, fades out, then 0 for good.
constexpr int kBannerFadeInMs = 400;
constexpr int kBannerHoldMs = 4500;
constexpr int kBannerFadeOutMs = 1200;
constexpr int kBannerTotalMs = kBannerFadeInMs + kBannerHoldMs + kBannerFadeOutMs;
int bannerAlpha(int elapsedMs);

// The event stream's seed from the map's seed (salted, so it never shares draws with the stats).
uint64_t eventSeed(uint64_t mapSeed);

} // namespace cartographer::eventplan
