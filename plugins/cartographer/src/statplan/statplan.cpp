#include "statplan.h"

#include <algorithm>
#include <climits>
#include <cstdio>
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

constexpr int kVelocityStatId = 67; // velocitypercent, the engine's fixed id (0x43)

} // namespace

std::string StatIds::nameOf(int id) const
{
	for (const auto &e : byName) {
		if (e.second == id) return e.first;
	}
	return "";
}

std::string StatIds::descKey(std::string_view name) const
{
	for (const auto &p : descKeys)
		if (p.first == name) return p.second;
	return "";
}

std::string StatIds::descNegKey(std::string_view name) const
{
	for (const auto &p : descNegKeys)
		if (p.first == name) return p.second;
	return "";
}

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
	const int cDesc = table.Column("descstrpos");
	const int cDescNeg = table.Column("descstrneg");
	// The stat id is the row number: the game counts data rows from 0. A column that starts with '*' (such as
	// *ID) is a comment to the game, and vanilla has one wrong (row 212 says 213), so it is not read.
	int ordinal = 0;
	for (const carto::TsvRow &row : table.rows) {
		const int id = ordinal++;
		const std::string name = table.Cell(row, cStat);
		if (name.empty()) continue;
		out.byName.emplace_back(name, id);
		if (const std::string key = table.Cell(row, cDesc); !key.empty()) out.descKeys.emplace_back(name, key);
		if (const std::string key = table.Cell(row, cDescNeg); !key.empty()) out.descNegKeys.emplace_back(name, key);
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
	std::vector<long long> monsterSums;
	std::vector<long long> playerSums;
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
		std::vector<Entry> &list = p.target == Target::Monsters ? plan.monsters : plan.players;
		std::vector<long long> &sums = p.target == Target::Monsters ? monsterSums : playerSums;
		// Two properties of one stat on one target become one entry holding the sum: each entry goes to SetListStat, which may set the
		// value instead of adding to it (not checked in game), and the map panel shows the sum.
		size_t at = 0;
		while (at < list.size() && list[at].stat != id) ++at;
		if (at < list.size()) {
			sums[at] += p.value;
			continue;
		}
		list.push_back({p.stat, id, 0});
		sums.push_back(p.value);
	}
	// The sums become the entries' values: an entry that adds up to 0 is dropped (and logged), a monster speed is cut once, on the sum, and
	// the player list is capped after the merge, so a stat that adds up to 0 does not use up a slot.
	const auto finish = [&plan](Target target, std::vector<Entry> &list, const std::vector<long long> &sums) {
		std::vector<Entry> kept;
		for (size_t i = 0; i < list.size(); ++i) {
			long long total = sums[i];
			total = total > INT_MAX ? INT_MAX : (total < INT_MIN ? INT_MIN : total);
			Entry e = list[i];
			e.value = static_cast<int>(total);
			if (e.value == 0) {
				plan.skipped.push_back({target, e.name, "its properties add up to 0, which changes nothing"});
				continue;
			}
			if (target == Target::Players && kept.size() >= kMaxPlayerStats) {
				plan.skipped.push_back({target, e.name, "the game sends at most " + std::to_string(kMaxPlayerStats) + " stats of a list to the character sheet"});
				continue;
			}
			if (target == Target::Monsters && (e.name == "velocitypercent" || e.stat == kVelocityStatId) && e.value < kMinMonsterSpeed) {
				plan.notes.push_back("'" + e.name + "' " + std::to_string(e.value) + " on monsters is cut to " + std::to_string(kMinMonsterSpeed) + " (the map's own value; what -100 or lower does to monsters has not been checked).");
				e.value = kMinMonsterSpeed;
			}
			kept.push_back(e);
		}
		list = std::move(kept);
	};
	finish(Target::Monsters, plan.monsters, monsterSums);
	finish(Target::Players, plan.players, playerSums);
	return plan;
}

int parsePropertyId(std::string_view text, std::string_view code)
{
	const carto::Tsv table = carto::ParseTsv(text);
	const int cCode = table.Column("code");
	if (cCode < 0) return -1;
	int ordinal = 0;
	for (const carto::TsvRow &row : table.rows) {
		// The "Expansion" row of vanilla properties.txt is a separator the game does not count (seen in game: the map-level row, 285 by row, is 284 to the game).
		if (table.Cell(row, cCode) == "Expansion") continue;
		const int id = ordinal++;
		if (table.Cell(row, cCode) == code) return id;
	}
	return -1;
}

int cubeRoll(int min, int max, uint64_t entropy)
{
	if (max <= min || max - min >= 1000) return min;
	return min + static_cast<int>(mix(entropy) % static_cast<uint64_t>(max - min + 1));
}

int cubeMapLevelTarget(int before, int after, int min, int max, int roll, bool levelKnown)
{
	if (!levelKnown) return after; // the level could not be read: the game's result stands
	if (max < min) max = min;
	const int delta = roll < min ? min : (roll > max ? max : roll);
	if (delta < 1) return after;
	int target = after;
	if (after == before) {
		target = before + delta; // the game did not touch the stat (a map has no list for it to land on)
	} else if (before > 0 && after >= min && after <= max && after < before + min) {
		target = before + after; // the game set the stat to the recipe's value instead of adding it
	}
	if (target < kMinMapLevel) target = kMinMapLevel;
	if (target > kMaxStatValue) target = kMaxStatValue;
	return target;
}

bool cubeLevelKnown(bool itemReadsOk, int inputLevel, bool cubeStepHooked)
{
	return itemReadsOk && (inputLevel >= 0 || !cubeStepHooked);
}

MapLevelPick mapLevelFromItem(const std::vector<ItemStat> &stats, const StatIds &ids, int fallback)
{
	MapLevelPick out;
	out.level = fallback;
	const int id = ids.find(kMapLevelStat);
	if (id < 0) {
		return out;
	}
	for (const ItemStat &s : stats) {
		if (s.stat != id || s.layer != 0) {
			continue;
		}
		if (s.value < kMinMapLevel) {
			out.note = std::string(kMapLevelStat) + " is " + std::to_string(s.value) + "; the map opens at its maps.txt Level " + std::to_string(fallback);
			return out;
		}
		out.fromItem = true;
		out.level = s.value > kMaxMapLevel ? kMaxMapLevel : s.value;
		if (s.value > kMaxMapLevel) {
			out.note = std::string(kMapLevelStat) + " is " + std::to_string(s.value) + "; the map level is cut to " + std::to_string(kMaxMapLevel);
		}
		return out;
	}
	return out;
}

std::vector<ItemStat> withoutMapLevel(const std::vector<ItemStat> &stats, const StatIds &ids)
{
	const int id = ids.find(kMapLevelStat);
	std::vector<ItemStat> out;
	for (const ItemStat &s : stats) {
		if (id < 0 || s.stat != id) {
			out.push_back(s);
		}
	}
	return out;
}

int mapDensityBonus(const std::vector<ItemStat> &stats, const StatIds &ids)
{
	const int id = ids.find(kMapDensityStat);
	if (id < 0) {
		return 0;
	}
	int64_t sum = 0;
	for (const ItemStat &s : stats) {
		if (s.stat == id && s.layer == 0) {
			sum += s.value;
		}
	}
	constexpr int64_t limit = 0x7fffffff;
	return static_cast<int>(sum > limit ? limit : sum < -limit ? -limit : sum);
}

int densityWithBonus(int base, int bonus)
{
	const int64_t total = static_cast<int64_t>(base) + bonus;
	if (total < 0) {
		return 0;
	}
	return total > 0x7fffffff ? 0x7fffffff : static_cast<int>(total);
}

std::vector<ItemStat> withoutMapDensity(const std::vector<ItemStat> &stats, const StatIds &ids)
{
	const int id = ids.find(kMapDensityStat);
	std::vector<ItemStat> out;
	for (const ItemStat &s : stats) {
		if (id < 0 || s.stat != id) {
			out.push_back(s);
		}
	}
	return out;
}

const std::vector<MonsterStat> &monsterStats()
{
	static const std::vector<MonsterStat> table = {
		{"cartographer_mon_life", "item_maxhp_percent"},
		{"cartographer_mon_damage", "damagepercent"},
		{"cartographer_mon_fire_res", "fireresist"},
		{"cartographer_mon_cold_res", "coldresist"},
		{"cartographer_mon_light_res", "lightresist"},
		{"cartographer_mon_poison_res", "poisonresist"},
		{"cartographer_mon_speed", "velocitypercent"},
	};
	return table;
}

ItemProperties itemStatsToProperties(const std::vector<ItemStat> &stats, const StatIds &ids)
{
	ItemProperties out;
	for (const ItemStat &s : stats) {
		const std::string label = "item stat " + std::to_string(s.stat) + (s.layer != 0 ? " (layer " + std::to_string(s.layer) + ")" : "");
		if (!ids.loaded) {
			out.dropped.push_back(label + ": the game's itemstatcost.txt could not be read");
			continue;
		}
		if (s.stat < 0 || s.layer != 0) {
			out.dropped.push_back(label + ": a stat with a layer cannot be a map property");
			continue;
		}
		if (s.value == 0) {
			out.dropped.push_back(label + ": its value is 0");
			continue;
		}
		const std::string name = ids.nameOf(s.stat);
		if (name.empty()) {
			out.dropped.push_back(label + ": no itemstatcost row has this id");
			continue;
		}
		const MonsterStat *mon = nullptr;
		for (const MonsterStat &m : monsterStats()) {
			if (name == m.itemStat) {
				mon = &m;
				break;
			}
		}
		if (mon != nullptr) {
			out.properties.push_back({Target::Monsters, mon->applied, s.value});
		} else {
			out.properties.push_back({Target::Players, name, s.value});
		}
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

RemoveAction decideRemoval(bool onChain, bool foundByState, bool ownedByPlugin)
{
	if (!onChain || !ownedByPlugin) return RemoveAction::Forget;
	return foundByState ? RemoveAction::StateOffDetachFree : RemoveAction::DetachFree;
}

bool ownsList(uint32_t listFlags, uint32_t listState, uint32_t storedFlags, uint32_t storedState, int expectedState)
{
	return expectedState > 0 && listFlags == storedFlags && listState == storedState && listState == static_cast<uint32_t>(expectedState);
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

uint32_t PlayerLists::flagsOf(uint32_t unit) const
{
	for (const Slot &s : slots_)
		if (s.list != 0 && s.unit == unit) return s.flags;
	return 0;
}

uint32_t PlayerLists::rawStateOf(uint32_t unit) const
{
	for (const Slot &s : slots_)
		if (s.list != 0 && s.unit == unit) return s.rawState;
	return 0;
}

bool PlayerLists::add(uint32_t unit, uintptr_t list, int state, uint32_t flags, uint32_t rawState)
{
	if (list == 0) return false;
	for (Slot &s : slots_) {
		if (s.list != 0 && s.unit == unit) {
			s.list = list;
			s.state = state;
			s.flags = flags;
			s.rawState = rawState;
			return true;
		}
	}
	for (Slot &s : slots_) {
		if (s.list == 0) {
			s = {unit, list, state, flags, rawState};
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

// How a column looks when it is empty in the compiled row. Settled from the compiler's own output, `states.bin` of D2RLoader 1.3.1
// (build 93847), read against `states.txt`: all 232 rows, every column, empty cell against stored bytes (cartographer/07-map-stats-parts.md).
enum class Empty : uint8_t {
	Zero,      // all bytes 0
	Negative,  // a name lookup that found nothing: -1 (0 is a real row)
	ZeroOrM1,  // setfunc, remfunc: 0 is stored; -1 also does nothing in the engine's dispatch (table entry 0 is null, -1 fails the bound)
	AllOnes,   // itemtrans: the byte 0xff
};

struct Column {
	size_t off;
	size_t width;
	Empty empty;
	const char *name;
};

const Column kColumns[] = {
	{0x02, 2, Empty::Negative, "overlay1"},    {0x04, 2, Empty::Negative, "overlay2"},    {0x06, 2, Empty::Negative, "overlay3"},
	{0x08, 2, Empty::Negative, "overlay4"},    {0x0a, 2, Empty::Negative, "castoverlay"}, {0x0c, 2, Empty::Negative, "removerlay"},
	{0x0e, 2, Empty::Negative, "pgsvoverlay"}, {0x10, 1, Empty::Zero, "flags 0-7"},       {0x11, 1, Empty::Zero, "flags 8-15"},
	{0x12, 1, Empty::Zero, "flags 16-23"},     {0x13, 1, Empty::Zero, "flags 24-31"},     {0x14, 1, Empty::Zero, "flags 32-39"},
	{0x15, 1, Empty::Zero, "flags 40-47"},     {0x16, 1, Empty::Zero, "flags 48-55"},     {0x17, 1, Empty::Zero, "flags 56-63"},
	{0x18, 2, Empty::Negative, "stat"},        {0x1a, 2, Empty::ZeroOrM1, "setfunc"},     {0x1c, 2, Empty::ZeroOrM1, "remfunc"},
	{0x1e, 2, Empty::Zero, "group"},           {0x20, 1, Empty::Zero, "colorpri"},        {0x21, 1, Empty::Zero, "colorshift"},
	{0x22, 1, Empty::Zero, "light-r"},         {0x23, 1, Empty::Zero, "light-g"},         {0x24, 1, Empty::Zero, "light-b"},
	{0x26, 2, Empty::Zero, "onsound"},         {0x28, 2, Empty::Zero, "offsound"},        {0x2a, 2, Empty::Zero, "itemtype"},
	{0x2c, 1, Empty::AllOnes, "itemtrans"},    {0x2d, 1, Empty::Zero, "gfxtype"},         {0x2e, 2, Empty::Zero, "gfxclass"},
	{0x30, 2, Empty::Negative, "cltevent"},    {0x32, 2, Empty::Zero, "clteventfunc"},    {0x34, 2, Empty::Zero, "cltactivefunc"},
	{0x36, 2, Empty::Zero, "srvactivefunc"},   {0x38, 2, Empty::Negative, "skill"},       {0x3a, 2, Empty::Negative, "missile"},
	{0x3c, 1, Empty::Zero, "canstack"},        {0x40, 4, Empty::Zero, "vfxweaponstate"},
};

uint32_t valueAt(const uint8_t *row, const Column &c)
{
	uint32_t v = 0;
	std::memcpy(&v, row + c.off, c.width);
	return v;
}

bool isEmpty(const uint8_t *row, const Column &c)
{
	const uint32_t v = valueAt(row, c);
	switch (c.empty) {
	case Empty::Zero: return v == 0;
	case Empty::Negative: return (v & (1u << (c.width * 8 - 1))) != 0;
	case Empty::ZeroOrM1: return v == 0 || v == 0xffffu;
	case Empty::AllOnes: return v == 0xffu;
	}
	return false;
}

// A state row nothing uses: every column of the compiled row (cartographer/07-map-stats-parts.md) holds its empty value. So no overlay,
// stat, skill, missile, function, group, colour, light, sound, item, graphics or vfx entry, and the whole flag field is zero
// (also nosend (0) and disguise (17)). The sound, itemtype and group columns must be exactly 0 and the other lookups exactly
// negative: cltevent 0 is a real event (`hitbymissile`) and is not taken for empty; sound row 0 is `none`.
bool candidate(const uint8_t *row)
{
	for (const Column &c : kColumns) {
		if (!isEmpty(row, c)) return false;
	}
	return true;
}

int failingColumns(const uint8_t *row)
{
	int n = 0;
	for (const Column &c : kColumns) n += isEmpty(row, c) ? 0 : 1;
	return n;
}

const char *const kFlagNames[] = {"nosend", "aura", "hide", "hidedead", "transform", "pgsv", "active", "remhit", "damblue", "damred", "attblue", "attred",
                                  "curse", "curable", "plrstaydeath", "monstaydeath", "bossstaydeath", "disguise", "restrict", "blue", "armblue", "rfblue",
                                  "rcblue", "rlblue", "rpblue", "stambarblue", "armred", "rfred", "rcred", "rlred", "rpred", "exp", "shatter", "life",
                                  "udead", "green", "nooverlays", "noclear", "bossinv", "meleeonly", "notondead", "sunderfull", "sunder-res-reduce"};

// "state N: K column(s) not empty; first <column> at +0xOO = 0xVV [(flag name)], empty is <rule>"
std::string rejectionText(int state, const uint8_t *row)
{
	int failing = 0;
	const Column *first = nullptr;
	for (const Column &c : kColumns) {
		if (!isEmpty(row, c)) {
			if (first == nullptr) first = &c;
			++failing;
		}
	}
	std::string text = "state " + std::to_string(state) + ": " + std::to_string(failing) + " column(s) not empty";
	if (first == nullptr) return text;
	const uint32_t v = valueAt(row, *first);
	char buf[64];
	std::snprintf(buf, sizeof(buf), "; first %s at +0x%02x = 0x%0*x", first->name, static_cast<unsigned>(first->off), static_cast<int>(first->width * 2), v);
	text += buf;
	if (first->off >= 0x10 && first->off < 0x18) {
		for (size_t bit = 0; bit < 8; ++bit) {
			const size_t idx = (first->off - 0x10) * 8 + bit;
			if ((v & (1u << bit)) != 0 && idx < sizeof(kFlagNames) / sizeof(kFlagNames[0])) {
				text += std::string(" (flag ") + kFlagNames[idx] + ")";
				break;
			}
		}
	}
	text += std::string(", empty is ") + (first->empty == Empty::Negative ? "negative" : first->empty == Empty::AllOnes ? "0xff" : first->empty == Empty::ZeroOrM1 ? "0 or 0xffff" : "0");
	return text;
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

std::vector<std::string> explainNoPick(const uint8_t *states, uint32_t count, const uint8_t *skills, uint32_t skillCount, size_t maxRows)
{
	std::vector<std::string> lines;
	if (states == nullptr) return lines;
	struct Rank {
		int failing;
		uint32_t state;
	};
	std::vector<Rank> ranks;
	uint32_t empty = 0, emptyStatic = 0, emptyNamed = 0, usable = 0;
	for (uint32_t s = 1; s < count; ++s) {
		const uint8_t *row = states + static_cast<size_t>(s) * kStateRowSize;
		const bool fixed = isStaticState(static_cast<int>(s));
		const bool named = skills != nullptr && namedBySkill(skills, skillCount, static_cast<int>(s));
		if (candidate(row)) {
			++empty;
			emptyStatic += fixed ? 1 : 0;
			emptyNamed += (!fixed && named) ? 1 : 0;
			usable += (!fixed && !named) ? 1 : 0;
		} else if (!fixed && !named) {
			ranks.push_back({failingColumns(row), s});
		}
	}
	lines.push_back(std::to_string(empty) + " of " + std::to_string(count > 0 ? count - 1 : 0) + " rows above 0 have every column empty: " + std::to_string(emptyStatic) +
	                " in the static list, " + std::to_string(emptyNamed) + " named by a skills row, " + std::to_string(usable) + " usable");
	std::stable_sort(ranks.begin(), ranks.end(), [](const Rank &a, const Rank &b) { return a.failing < b.failing; });
	for (size_t i = 0; i < ranks.size() && i < maxRows; ++i) {
		lines.push_back(rejectionText(static_cast<int>(ranks[i].state), states + static_cast<size_t>(ranks[i].state) * kStateRowSize));
	}
	return lines;
}

std::vector<ItemStat> decodeItemStatList(const int32_t *buffer, int count, int maxStats, std::string *problem) {
	std::vector<ItemStat> out;
	if (buffer == nullptr || count <= 0) {
		return out;
	}
	const int n = count < maxStats ? count : maxStats;
	for (int i = 0; i < n; ++i) {
		const int32_t *e = buffer + i * 4;
		if (e[3] != 0 || e[0] < 0 || e[0] > 0xffff || e[1] < 0 || e[1] > kMaxStatId) {
			if (problem != nullptr) {
				*problem = "ItemListCopy layout not recognised (entry " + std::to_string(i) + " is " + std::to_string(e[0]) + ", " +
				           std::to_string(e[1]) + ", " + std::to_string(e[2]) + ", " + std::to_string(e[3]) + "); the item's stats are not used";
			}
			return {};
		}
		out.push_back({e[1], e[0], e[2]});
	}
	return out;
}

} // namespace cartographer::statplan
