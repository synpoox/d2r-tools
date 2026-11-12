#include "statplan.h"

#include <algorithm>
#include <cstdlib>
#include <cstring>

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
	// The stat id is the row number: the game counts data rows from 0. A column that starts with '*' (such as
	// *ID) is a comment to the game, and vanilla has one wrong (row 212 says 213), so it is not read.
	int ordinal = 0;
	for (const carto::TsvRow &row : table.rows) {
		const int id = ordinal++;
		const std::string name = table.Cell(row, cStat);
		if (name.empty()) continue;
		out.byName.emplace_back(name, id);
	}
	return out;
}

// Stats whose value needs a skill, state or monster type number (the stat's layer): the rows of the vanilla 3.3.0
// itemstatcost.txt that have a Save Param Bits value. The map table has no way to give one yet, and the plugin sets
// layer 0, which would name skill, state or type 0. Matched by name or by row id, like the other refusals.
static bool needsParam(std::string_view stat, int id)
{
	static const char *const kNames[] = {"item_addclassskills", "item_nonclassskill", "state", "item_singleskill", "item_elemskill", "item_aura", "item_reanimate",
	                                     "attack_vs_montype", "damage_vs_montype", "item_addskill_tab", "item_skillonattack", "item_skillonkill", "item_skillondeath",
	                                     "item_skillonhit", "item_skillonlevelup", "item_skillongethit", "item_charged_skill"};
	static const int kIds[] = {83, 97, 98, 107, 126, 151, 155, 179, 180, 188, 195, 196, 197, 198, 199, 201, 204};
	for (const char *n : kNames)
		if (stat == n) return true;
	for (const int i : kIds)
		if (id == i) return true;
	return false;
}

std::string refusedReason(Target target, std::string_view stat, int id)
{
	static const char *const kNames[] = {"statpts", "newskills", "hitpoints", "mana", "stamina", "level", "experience", "gold", "goldbank"};
	static const int kIds[] = {4, 5, 6, 8, 10, 12, 13, 14, 15};
	bool listed = false;
	for (const char *n : kNames) listed = listed || stat == n;
	for (const int i : kIds) listed = listed || id == i;
	if (needsParam(stat, id))
		return "'" + std::string(stat) + "' is refused: it needs a skill or state number, and a map stat cannot give one yet.";
	if (!listed) return "";
	const bool isLevel = stat == "level" || id == 12;
	if (target == Target::Monsters) {
		if (isLevel) return "'" + std::string(stat) + "' is refused on monsters: the game writes the monster's level itself while it sets the monster up.";
		const bool current = stat == "hitpoints" || stat == "mana" || stat == "stamina" || id == 6 || id == 8 || id == 10;
		if (!current) return ""; // a monster is never saved
		return "'" + std::string(stat) + "' is refused on monsters: it is the current value, which the game rewrites on every hit and regeneration, so the map's amount would be added again each time. Use the maximum stat instead.";
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

PlayerAction decidePlayer(bool planHasPlayers, bool inMap, bool hasList, bool settingOn)
{
	const bool wanted = planHasPlayers && inMap && settingOn;
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

int PlayerLists::stateOf(uint32_t unit) const
{
	for (const Slot &s : slots_)
		if (s.list != 0 && s.unit == unit) return s.state;
	return 0;
}

bool PlayerLists::add(uint32_t unit, uintptr_t list, int state)
{
	if (list == 0) return false;
	for (Slot &s : slots_) {
		if (s.list != 0 && s.unit == unit) {
			s.list = list;
			s.state = state;
			return true;
		}
	}
	for (Slot &s : slots_) {
		if (s.list == 0) {
			s = {unit, list, state};
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

// --- The state for the player's list ---------------------------------------------------------------------------------------

namespace {

int16_t i16At(const uint8_t *row, size_t off)
{
	int16_t v;
	std::memcpy(&v, row + off, sizeof(v));
	return v;
}

bool flagBit(const uint8_t *row, int bit)
{
	return (row[0x10 + (bit >> 3)] & (1u << (bit & 7))) != 0;
}

bool namedBySkill(const uint8_t *skills, uint32_t skillCount, int state)
{
	static const size_t offsets[] = {0xa0, 0xa2, 0xb4, 0x2d2, 0x2d4, 0x2d6};
	for (uint32_t i = 0; i < skillCount; ++i) {
		for (const size_t off : offsets) {
			const int16_t v = i16At(skills + static_cast<size_t>(i) * kSkillRowSize, off);
			if (v >= 0 && v == state) return true;
		}
	}
	return false;
}

// A state row nothing uses: every column of the compiled row (cartographer.md section 7) is empty. Lookup columns are empty
// when negative (overlays, stat, skill, missile) or 0 or negative (sounds, itemtype, cltevent: the dump does not
// say which one the loader writes); gfxclass is a plain word, empty when 0; setfunc and remfunc are empty when 0 or -1; clteventfunc, cltactivefunc, srvactivefunc, group, bytes (colour, light, gfxtype, itemtrans, canstack) and the
// vfxweaponstate dword are empty when 0; the whole flag field is zero.
bool candidate(const uint8_t *row)
{
	for (int i = 0; i < 8; ++i) {
		if (row[0x10 + i] != 0) return false; // every flag bit clear, so also nosend (0) and disguise (17)
	}
	for (size_t off = 2; off <= 0xe; off += 2) {
		if (i16At(row, off) >= 0) return false;
	}
	if (i16At(row, 0x18) >= 0 || i16At(row, 0x38) >= 0 || i16At(row, 0x3a) >= 0) return false; // stat, skill, missile
	for (const size_t off : {size_t(0x1a), size_t(0x1c)}) {
		const int16_t f = i16At(row, off);
		if (f != 0 && f != -1) return false; // setfunc, remfunc: 0 and -1 both do nothing (entry 0 of the tables is null, -1 fails the bound)
	}
	for (const size_t off : {size_t(0x32), size_t(0x34), size_t(0x36)}) {
		if (i16At(row, off) != 0) return false; // clteventfunc, cltactivefunc, srvactivefunc
	}
	if (i16At(row, 0x1e) != 0) return false; // group
	for (const size_t off : {size_t(0x26), size_t(0x28), size_t(0x2a), size_t(0x30)}) {
		if (i16At(row, off) > 0) return false; // onsound, offsound, itemtype, cltevent
	}
	if (i16At(row, 0x2e) != 0) return false; // gfxclass: a plain word (type 3), empty is 0
	for (const size_t off : {size_t(0x20), size_t(0x21), size_t(0x22), size_t(0x23), size_t(0x24), size_t(0x2c), size_t(0x2d), size_t(0x3c)}) {
		if (row[off] != 0) return false; // colorpri, colorshift, light r/g/b, itemtrans, gfxtype, canstack
	}
	uint32_t vfx;
	std::memcpy(&vfx, row + 0x40, sizeof(vfx));
	return vfx == 0; // vfxweaponstate
}

} // namespace

std::string checkTableView(const void *viewRows, uint32_t viewCount, uint32_t viewRowSize, const void *dtRows, uint64_t dtCount,
                           uint32_t wantRowSize)
{
	if (viewRowSize != wantRowSize) {
		return "the row size is " + std::to_string(viewRowSize) + ", not " + std::to_string(wantRowSize);
	}
	if (viewRows == nullptr || dtRows == nullptr) return "a row pointer is null";
	if (viewRows != dtRows) return "the SDK rows are not the game's rows";
	if (dtCount != viewCount) {
		return "the row counts differ (game " + std::to_string(dtCount) + ", SDK " + std::to_string(viewCount) + ")";
	}
	return "";
}

bool isStaticState(int s)
{
	return s == 0 || s == 6 || s == 7 || s == 11 || s == 18 || s == 22 || s == 24 || s == 52 || s == 62 || s == 92 || (s >= 100 && s <= 107) || s == 109 ||
	       s == 118 || s == 154 || s == 164 || (s >= 165 && s <= 172) || s == 184 || s == 185 || s == 186 || s == 189 || s == 209 ||
	       s == 219;
}

StatePick pickPlayerState(const uint8_t *states, uint32_t count, const uint8_t *skills, uint32_t skillCount)
{
	StatePick pick;
	if (states == nullptr || (skills == nullptr && skillCount != 0)) return pick;
	for (uint32_t s = 1; s < count; ++s) {
		if (isStaticState(static_cast<int>(s)) || !candidate(states + static_cast<size_t>(s) * kStateRowSize) ||
		    namedBySkill(skills, skillCount, static_cast<int>(s))) {
			continue;
		}
		pick.state = static_cast<int>(s);
		return pick;
	}
	return pick;
}

} // namespace cartographer::statplan
