#include "events.h"

#include <algorithm>
#include <utility>
#include <string>

#include "rng.h"

namespace cartographer::events {

namespace {

constexpr uint64_t kEventSalt = 0x4D41504556454E54ull; // "MAPEVENT": keeps this stream apart from the layout's

// ASCII-only, so the result never depends on the C locale.
char lower(char c) { return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c; }
bool space(char c) { return c == ' ' || (c >= '\t' && c <= '\r'); }

bool iequals(std::string_view a, std::string_view b)
{
	if (a.size() != b.size()) return false;
	for (size_t i = 0; i < a.size(); ++i)
		if (lower(a[i]) != lower(b[i])) return false;
	return true;
}

bool blank(const std::string &s)
{
	for (char c : s)
		if (!space(c)) return false;
	return true;
}

std::string_view trim(std::string_view s)
{
	while (!s.empty() && space(s.front())) s.remove_prefix(1);
	while (!s.empty() && space(s.back())) s.remove_suffix(1);
	return s;
}

// Edit distance, case-insensitive; the names are short.
size_t distance(std::string_view a, std::string_view b)
{
	std::vector<size_t> prev(b.size() + 1), cur(b.size() + 1);
	for (size_t j = 0; j <= b.size(); ++j) prev[j] = j;
	for (size_t i = 1; i <= a.size(); ++i) {
		cur[0] = i;
		for (size_t j = 1; j <= b.size(); ++j) {
			const size_t sub = prev[j - 1] + (lower(a[i - 1]) == lower(b[j - 1]) ? 0 : 1);
			cur[j] = std::min(std::min(prev[j], cur[j - 1]) + 1, sub);
		}
		std::swap(prev, cur);
	}
	return prev[b.size()];
}

// Uniform in [0, n) for a 64-bit n. Below 2^32 this is Pcg32::below; above it, two words are combined and rejected the
// same way, so there is no modulo bias.
uint64_t below64(layout::Pcg32 &rng, uint64_t n)
{
	if (n <= 0xFFFFFFFFull) return rng.below(static_cast<uint32_t>(n));
	const uint64_t threshold = (0ull - n) % n;
	for (;;) {
		const uint64_t r = (static_cast<uint64_t>(rng.next()) << 32) | rng.next();
		if (r >= threshold) return r % n;
	}
}

int countOf(const Row &r) { return r.count.value_or(kDefaultCount); }
int wavesOf(const Row &r) { return r.waves.value_or(kDefaultWaves); }
int weightOf(const Row &r)
{
	const int w = r.weight.value_or(kDefaultWeight);
	return w > kMaxWeight ? kMaxWeight : w;
}
int lineOf(const Row &r, size_t index) { return r.line > 0 ? r.line : static_cast<int>(index) + 2; } // line 1 is the header

} // namespace

std::optional<Kind> parseKindName(std::string_view name)
{
	name = trim(name);
	static const Kind all[] = {Kind::Boss, Kind::Ambush, Kind::Horde, Kind::Treasure, Kind::Shrine};
	for (Kind k : all)
		if (iequals(name, kindName(k))) return k;
	return std::nullopt;
}

std::optional<Kind> suggestKind(std::string_view name)
{
	name = trim(name);
	if (name.empty()) return std::nullopt;
	static const Kind all[] = {Kind::Boss, Kind::Ambush, Kind::Horde, Kind::Treasure, Kind::Shrine};
	std::optional<Kind> best;
	size_t bestD = 3; // at most 2 edits
	for (Kind k : all) {
		const std::string_view full = kindName(k);
		size_t d = distance(name, full);
		// A shortened word ("Treas") is close too.
		if (name.size() >= 3 && name.size() < full.size() && iequals(name, full.substr(0, name.size()))) d = 1;
		if (d < bestD) {
			bestD = d;
			best = k;
		}
	}
	return best;
}

const char *kindName(Kind kind)
{
	switch (kind) {
	case Kind::Boss: return "Boss";
	case Kind::Ambush: return "Ambush";
	case Kind::Horde: return "Horde";
	case Kind::Treasure: return "Treasure";
	case Kind::Shrine: return "Shrine";
	default: return "Invalid";
	}
}

std::string validateRow(const Row &row, int line)
{
	const std::string at = "mapevents.txt line " + std::to_string(line) + ": ";
	if (blank(row.name)) return at + "the Name is empty. Name is shown to the player when the event starts.";
	if (row.kind == Kind::Invalid) {
		if (!blank(row.kindText)) {
			std::string m = at + "'" + row.name + "' has Kind '" + std::string(trim(row.kindText)) + "', which is not a Kind.";
			if (const auto hint = suggestKind(row.kindText)) m += std::string(" Did you mean ") + kindName(*hint) + "?";
			return m + " Use Boss, Ambush, Horde, Treasure or Shrine.";
		}
		return at + "'" + row.name + "' needs a Kind: Boss, Ambush, Horde, Treasure or Shrine.";
	}
	if (blank(row.spawn)) return at + "'" + row.name + "' needs a Spawn (a monster, unique monster or object name).";
	if (row.count && (*row.count < 1 || *row.count > kMaxCount))
		return at + "'" + row.name + "' has Count " + std::to_string(*row.count) + ". Use 1 to " +
		       std::to_string(kMaxCount) + ".";
	if (row.kind == Kind::Ambush && row.waves && (*row.waves < 1 || *row.waves > kMaxWaves))
		return at + "'" + row.name + "' has Waves " + std::to_string(*row.waves) + ". Use 1 to " +
		       std::to_string(kMaxWaves) + ".";
	if (row.weight && *row.weight < 0)
		return at + "'" + row.name + "' has Weight " + std::to_string(*row.weight) +
		       ". Use 0 to switch the row off, or 1 or more.";
	if (row.kind == Kind::Shrine && (row.spawnKind == NameKind::Monster || row.spawnKind == NameKind::Unique))
		return at + "'" + row.name + "' is a Shrine, so Spawn must be an object, not a monster.";
	if ((row.kind == Kind::Boss || row.kind == Kind::Ambush || row.kind == Kind::Horde) &&
	    row.spawnKind == NameKind::Object)
		return at + "'" + row.name + "' is a " + kindName(row.kind) + ", so Spawn must be a monster, not an object.";
	return {};
}

std::vector<std::string> validateRows(const std::vector<Row> &rows)
{
	std::vector<std::string> errors;
	for (size_t i = 0; i < rows.size(); ++i) {
		std::string e = validateRow(rows[i], lineOf(rows[i], i));
		if (!e.empty()) errors.push_back(std::move(e));
	}
	return errors;
}

std::string warnRow(const Row &row, int line)
{
	const std::string at = "mapevents.txt line " + std::to_string(line) + ": ";
	if (row.weight && *row.weight > kMaxWeight)
		return at + "'" + row.name + "' has Weight " + std::to_string(*row.weight) + ", above " +
		       std::to_string(kMaxWeight) + ". It is used as " + std::to_string(kMaxWeight) + ".";
	if (row.kind == Kind::Treasure && blank(row.drop))
		return at + "'" + row.name + "' is a Treasure with no Drop, so it drops nothing special. Fill in Drop.";
	return {};
}

std::vector<std::string> warnRows(const std::vector<Row> &rows)
{
	std::vector<std::string> out;
	for (size_t i = 0; i < rows.size(); ++i) {
		if (!validateRow(rows[i], 0).empty()) continue;
		std::string w = warnRow(rows[i], lineOf(rows[i], i));
		if (!w.empty()) out.push_back(std::move(w));
	}
	return out;
}

std::string validateEventChance(int chance)
{
	if (chance < 0 || chance > 100) return "EventChance " + std::to_string(chance) + " is not a percent. Use 0 to 100.";
	return {};
}

int effectiveEventChance(std::optional<int> perMap, int globalDefault)
{
	const int c = perMap.value_or(globalDefault);
	return c < 0 ? 0 : (c > 100 ? 100 : c);
}

std::optional<Event> decide(const std::vector<Row> &rows, int chance, uint64_t seed, const layout::Point *room,
                            int roomIdx)
{
	if (!room) return std::nullopt;

	// Drawable rows (usable, Weight above 0) and their total weight. Weights are capped and the total is 64-bit.
	const auto drawable = [](const Row &r) { return weightOf(r) > 0 && validateRow(r, 0).empty(); };
	uint64_t total = 0;
	for (const Row &r : rows)
		if (drawable(r)) total += static_cast<uint64_t>(weightOf(r));
	if (total == 0) return std::nullopt;

	layout::Pcg32 rng(layout::splitmix64(seed ^ kEventSalt));
	if (!rng.percent(chance)) return std::nullopt;

	uint64_t pick = below64(rng, total);
	const Row *row = nullptr;
	int rowIndex = -1;
	for (size_t i = 0; i < rows.size(); ++i) {
		if (!drawable(rows[i])) continue;
		const uint64_t w = static_cast<uint64_t>(weightOf(rows[i]));
		if (pick < w) {
			row = &rows[i];
			rowIndex = static_cast<int>(i);
			break;
		}
		pick -= w;
	}
	if (!row) return std::nullopt; // not reached (pick < total, so a row always matches); kept as a guard

	Event e;
	e.kind = row->kind;
	e.rowIndex = rowIndex;
	e.name = row->name;
	e.message = row->message;
	e.text = blank(row->message) ? row->name : row->name + "\n" + row->message;
	e.spawn = row->spawn;
	e.spawnKind = row->spawnKind;
	e.drop = row->drop;
	e.room = *room;
	e.roomIdx = roomIdx;
	switch (row->kind) {
	case Kind::Boss: e.minions = countOf(*row); break;
	case Kind::Ambush:
		e.waveSize = countOf(*row);
		e.waves = wavesOf(*row);
		break;
	case Kind::Horde: e.hordeSize = countOf(*row); break;
	case Kind::Treasure:
		e.treasure = row->spawnKind == NameKind::Object       ? TreasureForm::Chest
		             : row->spawnKind == NameKind::Unresolved ? TreasureForm::Unresolved
		                                                      : TreasureForm::Monster;
		break;
	default: break;
	}
	return e;
}

std::optional<Event> decide(const std::vector<Row> &rows, int chance, const layout::Layout &layout)
{
	if (layout.eventIdx < 0 || layout.eventIdx >= static_cast<int>(layout.mainPath.size())) return std::nullopt;
	return decide(rows, chance, layout.seed, &layout.mainPath[static_cast<size_t>(layout.eventIdx)], layout.eventIdx);
}

std::string describe(const Event &e)
{
	std::string s = kindName(e.kind);
	s += " '" + e.name + "' (" + e.spawn + ")";
	switch (e.kind) {
	case Kind::Boss: s += " minions " + std::to_string(e.minions); break;
	case Kind::Ambush: s += " " + std::to_string(e.waves) + " waves of " + std::to_string(e.waveSize); break;
	case Kind::Horde: s += " x" + std::to_string(e.hordeSize); break;
	default: break;
	}
	s += " at (" + std::to_string(e.room.x) + "," + std::to_string(e.room.y) + ")";
	return s;
}

} // namespace cartographer::events
