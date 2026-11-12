#include "statplan.h"

#include <algorithm>
#include <cstdlib>

#include "../core/tsv.h"

namespace cartographer::statplan {

namespace {

// Edit distance, for the "did you mean" suggestion only; names are short.
size_t editDistance(std::string_view a, std::string_view b)
{
	std::vector<size_t> prev(b.size() + 1), cur(b.size() + 1);
	for (size_t j = 0; j <= b.size(); ++j) prev[j] = j;
	for (size_t i = 1; i <= a.size(); ++i) {
		cur[0] = i;
		for (size_t j = 1; j <= b.size(); ++j) {
			const size_t sub = prev[j - 1] + (a[i - 1] == b[j - 1] ? 0 : 1);
			cur[j] = std::min({prev[j] + 1, cur[j - 1] + 1, sub});
		}
		std::swap(prev, cur);
	}
	return prev[b.size()];
}

bool parseInt(const std::string &s, int *out)
{
	if (s.empty()) return false;
	char *end = nullptr;
	const long v = std::strtol(s.c_str(), &end, 10);
	if (end == s.c_str() || *end != '\0' || v < -1000000 || v > 1000000) return false;
	*out = static_cast<int>(v);
	return true;
}

const char *targetWord(Target t)
{
	return t == Target::Monsters ? "monsters" : "players";
}

std::string signedValue(int v)
{
	return (v > 0 ? "+" : "") + std::to_string(v);
}

// SplitMix64 finaliser.
uint64_t mix(uint64_t x)
{
	x += 0x9E3779B97F4A7C15ull;
	x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ull;
	x = (x ^ (x >> 27)) * 0x94D049BB133111EBull;
	return x ^ (x >> 31);
}

} // namespace

int StatIds::find(std::string_view name) const
{
	for (const auto &p : byName)
		if (p.first == name) return p.second;
	return -1;
}

std::string StatIds::suggest(std::string_view name) const
{
	std::string best;
	size_t bestDistance = 3; // at most two edits away
	const std::string wanted = carto::Lower(name);
	for (const auto &p : byName) {
		const size_t d = editDistance(wanted, carto::Lower(p.first));
		if (d < bestDistance) {
			bestDistance = d;
			best = p.first;
		}
	}
	return best;
}

StatIds parseItemStatCost(std::string_view text)
{
	StatIds out;
	const carto::Tsv table = carto::ParseTsv(text);
	const int cStat = table.Column("Stat");
	if (cStat < 0) return out;
	out.loaded = true;
	const int cId = table.Column("*ID");
	int ordinal = 0; // the game counts data rows from 0
	for (const carto::TsvRow &row : table.rows) {
		const int rowId = ordinal++;
		const std::string name = table.Cell(row, cStat);
		if (name.empty()) continue;
		int id = rowId;
		if (cId >= 0) {
			int explicitId = 0;
			if (parseInt(table.Cell(row, cId), &explicitId)) id = explicitId;
		}
		out.byName.emplace_back(name, id);
	}
	return out;
}

std::string refusedReason(Target target, std::string_view stat, int id)
{
	static const char *const kNames[] = {"statpts", "newskills", "hitpoints", "mana", "stamina", "level", "experience", "gold", "goldbank"};
	static const int kIds[] = {4, 5, 6, 8, 10, 12, 13, 14, 15};
	bool listed = false;
	for (const char *n : kNames) listed = listed || stat == n;
	for (const int i : kIds) listed = listed || id == i;
	if (!listed) return "";
	const bool isLevel = stat == "level" || id == 12;
	if (target == Target::Monsters) {
		if (!isLevel) return ""; // a monster is never saved
		return "'" + std::string(stat) + "' is refused on monsters: the game writes the monster's level itself while it sets the monster up.";
	}
	return "'" + std::string(stat) + "' is refused on players: the game reads it as a total and writes it back as the saved base value, so the map's value would stay in the character.";
}

Plan buildPlan(const std::vector<Property> &properties, const StatIds &ids, bool playerStats)
{
	Plan plan;
	for (const Property &p : properties) {
		const auto skip = [&](const std::string &why) { plan.skipped.push_back({p.target, p.stat, why}); };
		if (p.target == Target::Players && !playerStats) {
			skip("PlayerStats is 0 in cartographer.txt, so nothing is put on the player (monsters still get their stats)");
			continue;
		}
		if (!ids.loaded) {
			skip("the game's itemstatcost.txt could not be read, so the stat id is not known");
			continue;
		}
		const int id = ids.find(p.stat);
		if (id < 0) {
			const std::string near = ids.suggest(p.stat);
			skip("'" + p.stat + "' is not a Stat in itemstatcost.txt." + (near.empty() ? "" : " Did you mean '" + near + "'?"));
			continue;
		}
		if (id > kMaxStatId) {
			skip("its stat id " + std::to_string(id) + " is above " + std::to_string(kMaxStatId));
			continue;
		}
		const std::string refused = refusedReason(p.target, p.stat, id);
		if (!refused.empty()) {
			skip(refused);
			continue;
		}
		if (p.value == 0) {
			skip("it rolled 0, which changes nothing");
			continue;
		}
		Entry e{p.stat, id, p.value};
		(p.target == Target::Monsters ? plan.monsters : plan.players).push_back(e);
	}
	return plan;
}

std::vector<PoolIssue> checkPoolStats(const std::vector<PoolRow> &pool, const StatIds &ids)
{
	std::vector<PoolIssue> out;
	if (!ids.loaded) return out;
	for (size_t i = 0; i < pool.size(); ++i) {
		const PoolRow &r = pool[i];
		if (r.weight <= 0) continue; // an off or erroring row is never drawn
		const int id = ids.find(r.stat);
		if (id >= 0) {
			const std::string refused = refusedReason(r.target, r.stat, id);
			if (!refused.empty())
				out.push_back({false, i, "row " + std::to_string(i + 1) + " (" + r.stat + ", " + targetWord(r.target) + "): " + refused + " If it is drawn it is skipped."});
			continue;
		}
		const std::string near = ids.suggest(r.stat);
		out.push_back({false, i,
			"row " + std::to_string(i + 1) + " (" + r.stat + "): Stat is not in itemstatcost.txt." +
				(near.empty() ? "" : " Did you mean '" + near + "'?") + " If it is drawn it is skipped."});
	}
	return out;
}

std::string describe(const Plan &plan)
{
	std::string out;
	const auto list = [&](Target t, const std::vector<Entry> &entries) {
		if (entries.empty()) return;
		if (!out.empty()) out += "; ";
		out += targetWord(t);
		out += ":";
		for (size_t i = 0; i < entries.size(); ++i)
			out += std::string(i ? ", " : " ") + entries[i].name + "(" + std::to_string(entries[i].stat) + ")=" + signedValue(entries[i].value);
	};
	list(Target::Monsters, plan.monsters);
	list(Target::Players, plan.players);
	return out.empty() ? "no stats" : out;
}

uint64_t mixSeed(uint64_t clock, uint32_t playerId, uint32_t openings)
{
	return mix(mix(clock) ^ ((static_cast<uint64_t>(playerId) << 32) | openings));
}

PlayerAction decidePlayer(bool planHasPlayers, bool inMap, bool hasList)
{
	const bool wanted = planHasPlayers && inMap;
	if (wanted && !hasList) return PlayerAction::Apply;
	if (!wanted && hasList) return PlayerAction::Remove;
	return PlayerAction::None;
}

bool monsterGetsStats(bool planActive, int levelId, int slotLevel)
{
	return planActive && slotLevel > 0 && levelId == slotLevel;
}

uintptr_t PlayerLists::find(uint32_t unit) const
{
	for (const Slot &s : slots_)
		if (s.list != 0 && s.unit == unit) return s.list;
	return 0;
}

bool PlayerLists::add(uint32_t unit, uintptr_t list)
{
	if (list == 0) return false;
	for (Slot &s : slots_) {
		if (s.list != 0 && s.unit == unit) {
			s.list = list;
			return true;
		}
	}
	for (Slot &s : slots_) {
		if (s.list == 0) {
			s = {unit, list};
			return true;
		}
	}
	return false;
}

void PlayerLists::remove(uint32_t unit)
{
	for (Slot &s : slots_)
		if (s.list != 0 && s.unit == unit) s = {};
}

void PlayerLists::clear()
{
	for (Slot &s : slots_) s = {};
}

size_t PlayerLists::size() const
{
	size_t n = 0;
	for (const Slot &s : slots_) n += s.list != 0 ? 1 : 0;
	return n;
}

} // namespace cartographer::statplan
