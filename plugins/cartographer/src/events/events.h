// Cartographer map events (phase 6a). Pure C++: no engine headers, no SDK, no floats, no <random>, no parsing.
//
// Input: already-parsed mapevents.txt rows (plain structs), the effective EventChance, a map seed, and the layout's
// reserved event room (from src/layout). Output: whether an event fires, which row, of which of the five kinds, and its
// parameters and message text. The decision depends on (rows, chance, seed, room) only; it uses the layout library's
// Pcg32 on its own stream (seed salted, so it never shares draws with the layout generator).
#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "layout.h"

namespace cartographer::events {

constexpr int kDefaultEventChance = 20; // cartographer.txt EventChance default
constexpr int kDefaultCount = 1;        // mapevents.txt Count default
constexpr int kDefaultWaves = 3;        // mapevents.txt Waves default (Ambush)
constexpr int kDefaultWeight = 1;       // mapevents.txt Weight default
constexpr int kMaxCount = 200;          // sanity caps, so a typo cannot ask for a million monsters
constexpr int kMaxWaves = 50;
constexpr int kMaxWeight = 1000000;     // Weight cap, the same cap as mapevents.txt weights; the draw total is 64-bit
// Weight: 0 = off (never drawn, no message), negative = error (row skipped), above kMaxWeight = used as kMaxWeight
// with a warning (see warnRow).

enum class Kind : uint8_t { Invalid, Boss, Ambush, Horde, Treasure, Shrine };
constexpr int kKindCount = 5;

// Case-insensitive (ASCII, independent of the C locale); nullopt for anything that is not one of the five kinds.
std::optional<Kind> parseKindName(std::string_view name);
// The kind a mistyped name is most likely meant to be ("Bos" -> Boss), or nullopt when nothing is close.
std::optional<Kind> suggestKind(std::string_view name);
const char *kindName(Kind kind);

// What a `Spawn` name refers to. mapevents.txt's Spawn column holds a monstats Id, a superuniques Name or an objects
// Name, and nothing in the text says which. OPEN PLAN QUESTION (plan §3.3 mixes them): the core keeps the kind next to
// the name and lets a caller that has the game tables fill it in. Unresolved is accepted everywhere and passed through
// as Unresolved so a later step can resolve it; a resolved kind that does not suit the event kind is rejected (a Shrine
// needs an Object; Boss, Ambush and Horde need a Monster or Unique; Treasure takes any).
enum class NameKind : uint8_t { Unresolved, Monster, Unique, Object };

// One parsed row of mapevents.txt. Blank cells arrive as nullopt / empty and get the table defaults.
struct Row {
	std::string name;          // required
	Kind kind = Kind::Invalid; // required
	std::string kindText;      // the raw Kind cell, kept so a typo can get a "did you mean" (read when kind is Invalid)
	int line = 0;              // the row's line in the file, set by the parser; 0 = unknown (index + 2 is used)
	std::string spawn;         // required
	NameKind spawnKind = NameKind::Unresolved;
	std::optional<int> count;
	std::optional<int> waves;
	std::string drop;    // treasureclassex name; optional
	std::string message; // optional
	std::optional<int> weight;
};

// Plain-English problem with one row (empty when usable). `line` only appears in the message.
// Waves is only checked on Ambush rows; Weight 0 is not a problem (the row is off).
std::string validateRow(const Row &row, int line);
// Every problem in a table, one string per problem. Empty means every row is usable. Uses Row::line when set.
std::vector<std::string> validateRows(const std::vector<Row> &rows);
// A note about a usable row (empty when none): a Weight above kMaxWeight (clamped), or a Treasure with a blank Drop.
std::string warnRow(const Row &row, int line);
// Every warning in a table, one string per note, for the loader and `cartographer check`. Rows with errors are skipped.
std::vector<std::string> warnRows(const std::vector<Row> &rows);
// An EventChance outside 0..100 (empty when fine). The functions below clamp either way.
std::string validateEventChance(int chance);
// The chance a map uses: its own EventChance, else the cartographer.txt value, clamped to 0..100.
int effectiveEventChance(std::optional<int> perMap, int globalDefault = kDefaultEventChance);

// How a Treasure event shows up, from the Spawn name's kind.
enum class TreasureForm : uint8_t { None, Monster, Chest, Unresolved };

// A decided event, ready for whatever later spawns it. Fields that do not apply to the kind are zero/empty.
struct Event {
	Kind kind = Kind::Invalid;
	int rowIndex = -1; // index into the rows given to decide()
	std::string name;
	std::string message; // the row's Message (may be empty)
	std::string text;    // Name, then a line break and Message when there is one: what the player is shown
	std::string spawn;
	NameKind spawnKind = NameKind::Unresolved;
	std::string drop;
	layout::Point room; // the reserved event room (macro-cell)
	int roomIdx = -1;   // index into Layout::mainPath, or -1 when decided from a bare Point

	int minions = 0;  // Boss: minions with the boss
	int waveSize = 0; // Ambush: monsters per wave
	int waves = 0;    // Ambush: number of waves
	int hordeSize = 0; // Horde: monsters packed into the room
	TreasureForm treasure = TreasureForm::None; // Treasure only
};

// The event for (rows, seed), or nullopt. Skips unusable rows. Never throws on bad input.
//   * `room` null (a layout with no event room) gives no event.
//   * Order: roll `chance` percent, then draw one usable row by Weight. Nothing is drawn when the roll fails.
std::optional<Event> decide(const std::vector<Row> &rows, int chance, uint64_t seed, const layout::Point *room,
                            int roomIdx = -1);
// The same, taking the seed and the reserved event room from a generated layout (Layout::mainPath[eventIdx]).
std::optional<Event> decide(const std::vector<Row> &rows, int chance, const layout::Layout &layout);

// One-line description for logs: "Boss 'The Drowned King' (Bishibosh) minions 6 at (3,2)".
std::string describe(const Event &event);

} // namespace cartographer::events
