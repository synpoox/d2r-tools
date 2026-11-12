#include "events.h"

#include <cctype>
#include <string>

#include "rng.h"

namespace cartographer::events {

namespace {

constexpr uint64_t kEventSalt = 0x4D41504556454E54ull; // "MAPEVENT": keeps this stream apart from the layout's

bool iequals(std::string_view a, std::string_view b)
{
	if (a.size() != b.size()) return false;
	for (size_t i = 0; i < a.size(); ++i)
		if (std::tolower(static_cast<unsigned char>(a[i])) != std::tolower(static_cast<unsigned char>(b[i]))) return false;
	return true;
}

bool blank(const std::string &s)
{
	for (char c : s)
		if (!std::isspace(static_cast<unsigned char>(c))) return false;
	return true;
}

int countOf(const Row &r) { return r.count.value_or(kDefaultCount); }
int wavesOf(const Row &r) { return r.waves.value_or(kDefaultWaves); }
int weightOf(const Row &r) { return r.weight.value_or(kDefaultWeight); }

} // namespace

std::optional<Kind> parseKindName(std::string_view name)
{
	while (!name.empty() && std::isspace(static_cast<unsigned char>(name.front()))) name.remove_prefix(1);
	while (!name.empty() && std::isspace(static_cast<unsigned char>(name.back()))) name.remove_suffix(1);
	static const Kind all[] = {Kind::Boss, Kind::Ambush, Kind::Horde, Kind::Treasure, Kind::Shrine};
	for (Kind k : all)
		if (iequals(name, kindName(k))) return k;
	return std::nullopt;
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
	if (row.kind == Kind::Invalid)
		return at + "'" + row.name + "' needs a Kind: Boss, Ambush, Horde, Treasure or Shrine.";
	if (blank(row.spawn)) return at + "'" + row.name + "' needs a Spawn (a monster, unique monster or object name).";
	if (row.count && (*row.count < 1 || *row.count > kMaxCount))
		return at + "'" + row.name + "' has Count " + std::to_string(*row.count) + ". Use 1 to " +
		       std::to_string(kMaxCount) + ".";
	if (row.waves && (*row.waves < 1 || *row.waves > kMaxWaves))
		return at + "'" + row.name + "' has Waves " + std::to_string(*row.waves) + ". Use 1 to " +
		       std::to_string(kMaxWaves) + ".";
	if (row.weight && (*row.weight < 1 || *row.weight > kMaxWeight))
		return at + "'" + row.name + "' has Weight " + std::to_string(*row.weight) + ". Use 1 to " +
		       std::to_string(kMaxWeight) + ".";
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
		std::string e = validateRow(rows[i], static_cast<int>(i) + 2); // line 1 is the header
		if (!e.empty()) errors.push_back(std::move(e));
	}
	return errors;
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

	// Usable rows and their total weight (saturating, so a huge table cannot wrap).
	uint64_t total = 0;
	for (const Row &r : rows)
		if (validateRow(r, 0).empty()) total += static_cast<uint64_t>(weightOf(r));
	if (total == 0) return std::nullopt;
	if (total > 0xFFFFFFFFull) total = 0xFFFFFFFFull;

	layout::Pcg32 rng(layout::splitmix64(seed ^ kEventSalt));
	if (!rng.percent(chance)) return std::nullopt;

	uint64_t pick = rng.below(static_cast<uint32_t>(total));
	const Row *row = nullptr;
	int rowIndex = -1;
	for (size_t i = 0; i < rows.size(); ++i) {
		if (!validateRow(rows[i], 0).empty()) continue;
		const uint64_t w = static_cast<uint64_t>(weightOf(rows[i]));
		if (pick < w) {
			row = &rows[i];
			rowIndex = static_cast<int>(i);
			break;
		}
		pick -= w;
	}
	if (!row) return std::nullopt; // only when total saturated and the draw fell past the last row

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
