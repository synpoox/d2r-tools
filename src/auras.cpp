// Better Auras core (auras.h).
#include "auras.h"

#include "aura_api.h"
#include "config.h"
#include "d2r.h"
#include "log.h"

#define NOMINMAX
#include <windows.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <vector>

namespace auras {

namespace {

using aura::api;
using d2r::At;
using d2r::Game;
using d2r::Unit;

bool g_on[FeatureCount] = {};
bool g_hooksTried = false;
bool g_hooksOk = false;
char g_hookReport[256] = "";

aura::AuraTickFn g_tickOriginal = nullptr;
aura::SelectSkillFn g_selectOriginal = nullptr;
aura::StartSkillFn g_startSkillOriginal = nullptr;
aura::UnitDeathFn g_deathOriginal = nullptr;

std::atomic<uint32_t> g_toggledOn {0}, g_toggledOff {0}, g_refused {0}, g_drainStops {0}, g_rearms {0}, g_deaths {0};

// One running aura the plugin looks after. managed = toggled (per-skill timer); otherwise a vanilla right-skill aura
// of a listed skill with toggle = false, which only gets drain and reservation.
struct Active {
	Game* game;
	Unit* unit;  // compared, dereferenced only when the engine hands us this same unit
	int32_t skill;
	int32_t level;
	int32_t state;
	bool managed;
	int32_t reserveStat;  // StatMaxMana / StatMaxLife, 0 = none
	int32_t reserved;     // << 8
	int32_t lastFrame;
	double drainCarry;    // fraction of a point (<< 8 units) not charged yet
	uint32_t order;       // activation order: reservations are released newest first
};
std::vector<Active> g_active;
uint32_t g_order = 0;
// Hooks run on the game thread; the status command reads from the console. Recursive: a hooked function can run
// inside another (a death inside an aura tick), and every hook looks its entry up again after calling the game.
struct CriticalSection {
	CRITICAL_SECTION cs;
	CriticalSection() { InitializeCriticalSection(&cs); }
} g_lock;

struct Lock {
	Lock() { EnterCriticalSection(&g_lock.cs); }
	~Lock() { LeaveCriticalSection(&g_lock.cs); }
};

auto IsPlayer(Unit* unit) -> bool {
	return unit != nullptr && At<int32_t>(unit, d2r::UnitType) == 0;
}

auto Df(Game* game) -> uint32_t {
	return d2r::DataFlag(game);
}

auto Record(Game* game, int32_t skill) -> const uint8_t* {
	return d2r::api.getSkillRecord(Df(game), skill);
}

auto AuraState(const uint8_t* rec) -> int32_t {
	return At<int16_t>(rec, aura::SkAuraState);
}

// "Uses aura functions": the aura bit the engine tests, or one of D2R's aura do-functions; and an aurastate to hang
// the aura's stat list on.
auto IsAuraSkill(const uint8_t* rec) -> bool {
	if (rec == nullptr || AuraState(rec) < 0) {
		return false;
	}
	const uint16_t fn = At<uint16_t>(rec, aura::SkSrvDoFunc);
	return (At<uint8_t>(rec, aura::SkFlags24) & aura::FlagAura) != 0 || fn == 65 || fn == 66 || fn == 81 || fn == 82;
}

auto Level(Unit* unit, int32_t skill) -> int32_t {
	void* entry = api.unitSkill(unit, skill, 0);
	return entry != nullptr ? api.skillLevel(unit, entry, 1) : 0;
}

auto RightSkillId(Unit* unit) -> int32_t {
	void* entry = api.rightSkill(unit);
	return entry != nullptr ? api.skillEntryId(entry, __FILE__, __LINE__) : -1;
}

// The skill can't work here: in town without InTown (vanilla skips the effect there but keeps the timer).
auto TownBlocked(const uint8_t* rec, Unit* unit) -> bool {
	if ((At<uint8_t>(rec, aura::SkFlags25) & aura::FlagInTown) != 0) {
		return false;
	}
	void* room = api.unitRoom(unit);
	return room == nullptr || api.roomIsTown(room) != 0;
}

auto Frame(Game* game) -> int32_t {
	return At<int32_t>(game, aura::GameFrame);
}

auto CurrentStat(int32_t maxStat) -> int32_t {
	return maxStat == aura::StatMaxMana ? aura::StatMana : aura::StatLife;
}

// Drops entries of an earlier game (pointer compare only) and returns this unit's entry for the skill, or null.
auto FindEntry(Game* game, Unit* unit, int32_t skill) -> Active* {
	g_active.erase(std::remove_if(g_active.begin(), g_active.end(), [game](const Active& a) { return a.game != game; }),
	               g_active.end());
	for (Active& a : g_active) {
		if (a.unit == unit && a.skill == skill) {
			return &a;
		}
	}
	return nullptr;
}

// Sum of what this unit's auras reserve from one pool, optionally leaving one entry out.
auto ReservedTotal(Unit* unit, int32_t maxStat, const Active* except) -> int64_t {
	int64_t total = 0;
	for (const Active& a : g_active) {
		if (a.unit == unit && a.reserveStat == maxStat && &a != except) {
			total += a.reserved;
		}
	}
	return total;
}

// The pool's max before any of our reservations (<< 8).
auto BaseMax(Unit* unit, int32_t maxStat) -> int64_t {
	return static_cast<int64_t>(api.stat(unit, maxStat, 0)) + ReservedTotal(unit, maxStat, nullptr);
}

auto ReserveAmount(const config::SkillConfig& cfg, int64_t baseMax) -> int32_t {
	if (cfg.reserveMode == config::ReserveMode::Percent) {
		return static_cast<int32_t>(std::llround(static_cast<double>(baseMax) * std::min(cfg.reserveValue, 100.0) / 100.0));
	}
	return static_cast<int32_t>(std::llround(cfg.reserveValue * 256.0));
}

// Writes the entry's reservation into its aura list and clamps the current value to the new max.
void ApplyReservation(Game* game, Unit* unit, const Active& a) {
	if (a.reserveStat == 0) {
		return;
	}
	void* list = api.stateList(unit, a.state);
	if (list == nullptr) {
		return;
	}
	if (api.listStat(Df(game), list, a.reserveStat, 0) != -a.reserved) {
		api.setListStat(Df(game), list, a.reserveStat, -a.reserved, 0);
	}
	const int32_t cur = CurrentStat(a.reserveStat);
	const int32_t max = api.stat(unit, a.reserveStat, 0);
	if (api.stat(unit, cur, 0) > max) {
		api.setStat(unit, cur, max, 0);
	}
}

// Restarts the right-skill timer of a vanilla aura on the right button: starting a toggled aura goes through the
// vanilla start, which takes over the -1 timer.
void RestoreRightAura(Game* game, Unit* unit) {
	const int32_t right = RightSkillId(unit);
	if (right <= 0) {
		return;
	}
	const config::SkillConfig* cfg = config::Find(right);
	if (cfg != nullptr && cfg->toggle && g_on[Toggle]) {
		return;  // a toggled skill never runs on the -1 timer
	}
	const uint8_t* rec = Record(game, right);
	const int32_t level = Level(unit, right);
	if (rec != nullptr && (At<uint8_t>(rec, aura::SkFlags24) & aura::FlagAura) != 0 && level > 0 &&
	    api.hasState(unit, AuraState(rec)) != 0) {
		api.scheduleAuraTick(game, unit, right, level, api.tickArg(game), 1);
	}
}

// Starts the aura the vanilla way and moves it onto its own per-skill timer.
auto StartManaged(Game* game, Unit* unit, int32_t skill, int32_t level, int32_t state) -> bool {
	aura::startRightAuraOriginal(game, unit, skill);
	api.removeEvents(game, unit, aura::EventAura, -1);
	void* list = api.stateList(unit, state);
	if (list == nullptr) {
		return false;
	}
	// the per-skill tick runs only while the list names this skill (0x437460); auras the vanilla start runs through
	// their do-function may build the list without it [A]
	if (api.listStat(Df(game), list, aura::StatAuraSkill, 0) != skill) {
		api.setListStat(Df(game), list, aura::StatAuraSkill, skill, 0);
		api.setListStat(Df(game), list, aura::StatAuraSkill + 1, level, 0);
	}
	api.scheduleAuraTick(game, unit, skill, level, api.tickArg(game), 0);
	RestoreRightAura(game, unit);
	return true;
}

// Removes the aura (timer, list, state); the reservation goes with the list.
void Stop(Game* game, Unit* unit, const Active& a) {
	api.removeEvents(game, unit, aura::EventAura, a.managed ? a.skill : -1);
	void* list = api.stateList(unit, a.state);
	if (list != nullptr) {
		api.detachList(unit, list);
		api.freeList(Df(game), list);
	}
	api.setState(unit, a.state, 0);
}

void Erase(const Active* a) {
	g_active.erase(g_active.begin() + (a - g_active.data()));
}

void StopAndErase(Game* game, Unit* unit, Active* a, const char* why) {
	plog::Info("aura %d off (%s)", a->skill, why);
	Stop(game, unit, *a);
	Erase(a);
}

// Reservation for a new entry: the amount, or -1 when the pool can't cover it (with every other reservation).
auto PlanReservation(Unit* unit, const config::SkillConfig& cfg, int32_t& maxStat) -> int32_t {
	maxStat = 0;
	if (!g_on[Reserve] || cfg.reserveResource == config::Resource::None || cfg.reserveValue <= 0) {
		return 0;
	}
	maxStat = cfg.reserveResource == config::Resource::Mana ? aura::StatMaxMana : aura::StatMaxLife;
	const int64_t base = BaseMax(unit, maxStat);
	const int32_t amount = ReserveAmount(cfg, base);
	// at least one point of the pool stays free
	return base - ReservedTotal(unit, maxStat, nullptr) - amount >= 256 ? amount : -1;
}

auto ToggleOn(Game* game, Unit* unit, int32_t skill, const config::SkillConfig& cfg) -> bool {
	const uint8_t* rec = Record(game, skill);
	if (!IsAuraSkill(rec)) {
		static std::atomic<uint32_t> s_notAura {0};
		if (plog::Sample(s_notAura)) plog::Warn("skill %d (%s) is listed but uses no aura function: left vanilla", skill, cfg.key);
		return false;
	}
	const int32_t level = Level(unit, skill);
	const int32_t state = AuraState(rec);
	for (const Active& a : g_active) {
		if (a.unit == unit && a.state == state) {
			plog::Info("aura %d refused: aura %d already uses state %d", skill, a.skill, state);
			++g_refused;
			return false;
		}
	}
	int32_t maxStat = 0;
	const int32_t reserve = PlanReservation(unit, cfg, maxStat);
	if (level <= 0 || reserve < 0) {
		plog::Info("aura %d refused: %s", skill, level <= 0 ? "no skill level" : "not enough to reserve");
		++g_refused;
		return false;
	}
	if (!StartManaged(game, unit, skill, level, state)) {
		plog::Warn("aura %d: the vanilla start made no state list; not toggled", skill);
		++g_refused;
		return false;
	}
	g_active.push_back({game, unit, skill, level, state, true, maxStat, reserve, Frame(game), 0.0, ++g_order});
	ApplyReservation(game, unit, g_active.back());
	plog::Info("aura %d on (level %d, reserve %d %s)", skill, level, reserve >> 8, maxStat == aura::StatMaxMana ? "mana" : maxStat ? "life" : "-");
	++g_toggledOn;
	return true;
}

void ToggleSkill(Game* game, Unit* unit, int32_t skill, const config::SkillConfig& cfg) {
	if (Active* a = FindEntry(game, unit, skill); a != nullptr && a->managed) {
		StopAndErase(game, unit, a, "toggled off");
		++g_toggledOff;
	} else {
		ToggleOn(game, unit, skill, cfg);
	}
}

// Charges the drain since the last tick. False when the pool can't pay: the aura stops.
auto ChargeDrain(Game* game, Unit* unit, Active& a, const config::SkillConfig& cfg, const uint8_t* rec) -> bool {
	const int32_t now = Frame(game);
	const int32_t elapsed = std::max(0, now - a.lastFrame);
	a.lastFrame = now;
	if (!g_on[Drain] || cfg.drainResource == config::Resource::None || cfg.drainPerSecond <= 0 || TownBlocked(rec, unit)) {
		return true;
	}
	a.drainCarry += cfg.drainPerSecond * 256.0 * elapsed / aura::FramesPerSecond;
	const int32_t cost = static_cast<int32_t>(a.drainCarry);
	if (cost <= 0) {
		return true;
	}
	const int32_t stat = cfg.drainResource == config::Resource::Mana ? aura::StatMana : aura::StatLife;
	const int32_t have = api.stat(unit, stat, 0);
	// life drain never kills: one point stays
	if (have - cost < (stat == aura::StatLife ? 256 : 0)) {
		return false;
	}
	api.addStat(unit, stat, -cost, 0);
	a.drainCarry -= cost;
	return true;
}

// Percent reservations follow the pool (level-up, gear). When the pool can't cover them all, the newest go first.
void RefreshReservations(Game* game, Unit* unit) {
	for (int32_t maxStat : {aura::StatMaxMana, aura::StatMaxLife}) {
		for (;;) {
			const int64_t base = BaseMax(unit, maxStat);
			for (Active& a : g_active) {
				if (a.unit == unit && a.reserveStat == maxStat) {
					if (const config::SkillConfig* cfg = config::Find(a.skill); cfg != nullptr && cfg->reserveMode == config::ReserveMode::Percent) {
						a.reserved = ReserveAmount(*cfg, base);
					}
				}
			}
			if (base - ReservedTotal(unit, maxStat, nullptr) >= 256) {
				break;
			}
			Active* newest = nullptr;
			for (Active& a : g_active) {
				if (a.unit == unit && a.reserveStat == maxStat && (newest == nullptr || a.order > newest->order)) newest = &a;
			}
			if (newest == nullptr) break;
			StopAndErase(game, unit, newest, "pool too small for its reservation");
		}
	}
}

// --- Hooks --------------------------------------------------------------------------------------------------------

// Event 8: the upkeep tick of one aura.
void AuraTick(Game* game, Unit* unit, int32_t key, int32_t a4, int32_t a5) noexcept {
	if (game == nullptr || !IsPlayer(unit)) {
		g_tickOriginal(game, unit, key, a4, a5);
		return;
	}
	int32_t skill = -1;
	{
		Lock lock;
		skill = key > 0 ? key : key == -1 ? RightSkillId(unit) : -1;
		Active* a = skill > 0 ? FindEntry(game, unit, skill) : nullptr;
		if (a != nullptr && a->managed != (key > 0)) {
			a = nullptr;  // e.g. the right-skill tick of a skill that is also toggled
		}
		const config::SkillConfig* cfg = a != nullptr ? config::Find(skill) : nullptr;
		const uint8_t* rec = a != nullptr ? Record(game, skill) : nullptr;
		if (a != nullptr && (cfg == nullptr || rec == nullptr || api.hasState(unit, a->state) == 0)) {
			Erase(a);  // gone without us (the engine's own cleanup runs in the original)
			a = nullptr;
		}
		if (a != nullptr) {
			if (!ChargeDrain(game, unit, *a, *cfg, rec)) {
				++g_drainStops;
				StopAndErase(game, unit, a, "drain could not be paid");
				return;
			}
			RefreshReservations(game, unit);
		}
	}
	g_tickOriginal(game, unit, key, a4, a5);
	if (skill > 0) {
		Lock lock;
		if (const Active* a = FindEntry(game, unit, skill); a != nullptr && a->managed == (key > 0)) {
			ApplyReservation(game, unit, *a);  // the do-function may rebuild the list
		}
	}
}

// Selecting an aura on the right button starts it (vanilla). Toggled skills don't start this way.
void StartRightAura(Game* game, Unit* unit, int32_t skill) noexcept {
	const config::SkillConfig* cfg = IsPlayer(unit) ? config::Find(skill) : nullptr;
	if (cfg != nullptr && cfg->toggle && g_on[Toggle]) {
		return;
	}
	aura::startRightAuraOriginal(game, unit, skill);
	if (cfg == nullptr || game == nullptr) {
		return;
	}
	// a listed skill on vanilla select: drain and reservation while it runs
	Lock lock;
	const uint8_t* rec = Record(game, skill);
	if (!IsAuraSkill(rec) || api.hasState(unit, AuraState(rec)) == 0) {
		return;
	}
	if (Active* old = FindEntry(game, unit, skill); old != nullptr) {
		Erase(old);
	}
	int32_t maxStat = 0;
	const int32_t reserve = PlanReservation(unit, *cfg, maxStat);
	Active a {game, unit, skill, Level(unit, skill), AuraState(rec), false, maxStat, std::max(reserve, 0), Frame(game), 0.0, ++g_order};
	if (reserve < 0) {
		plog::Info("aura %d refused: not enough to reserve", skill);
		++g_refused;
		Stop(game, unit, a);
		return;
	}
	if (maxStat != 0 || (g_on[Drain] && cfg->drainResource != config::Resource::None)) {
		g_active.push_back(a);
		ApplyReservation(game, unit, g_active.back());
	}
}

// Server skill select (hooked past its first branch, see aura_api.h).
void SelectSkill(Unit* unit, int32_t isLeft, int32_t skill, int32_t a4) noexcept {
	if (isLeft != 0 || !IsPlayer(unit)) {
		g_selectOriginal(unit, isLeft, skill, a4);
		return;
	}
	Game* game = api.unitGame(unit);
	int32_t old = -1;
	bool keep = false;
	if (game != nullptr) {
		Lock lock;
		old = RightSkillId(unit);
		if (Active* a = old > 0 ? FindEntry(game, unit, old) : nullptr; a != nullptr) {
			keep = a->managed && g_on[Decouple];
			if (!keep) {
				if (a->managed) api.removeEvents(game, unit, aura::EventAura, a->skill);
				Erase(a);  // the vanilla switch removes it now
			}
		}
	}
	g_selectOriginal(unit, isLeft, skill, a4);  // may remove the old right-skill aura and start a new one
	if (game == nullptr) {
		return;
	}
	Lock lock;
	if (keep) {
		if (Active* a = FindEntry(game, unit, old); a != nullptr && api.hasState(unit, a->state) == 0) {
			// the switch removed its list: build it again (its per-skill timer survived the switch)
			if (StartManaged(game, unit, a->skill, a->level, a->state)) {
				ApplyReservation(game, unit, *a);
				++g_rearms;
				plog::Info("aura %d kept through the skill switch", a->skill);
			} else {
				StopAndErase(game, unit, a, "could not be kept through the skill switch");
			}
		}
	}
	const config::SkillConfig* cfg = config::Find(skill);
	if (cfg != nullptr && cfg->toggle && cfg->trigger == config::Trigger::Select && g_on[Toggle]) {
		ToggleSkill(game, unit, skill, *cfg);
	}
}

// Using a skill: toggles listed skills whose trigger is "cast" [A: whether using an aura reaches the server].
auto StartSkill(Game* game, Unit* unit, int32_t skill, int32_t level, int32_t force) noexcept -> int32_t {
	if (g_on[Toggle] && game != nullptr && IsPlayer(unit)) {
		const config::SkillConfig* cfg = config::Find(skill);
		if (cfg != nullptr && cfg->toggle && cfg->trigger == config::Trigger::Cast) {
			Lock lock;
			ToggleSkill(game, unit, skill, *cfg);
			return 1;
		}
	}
	return g_startSkillOriginal(game, unit, skill, level, force);
}

// Death: every aura of the player goes off, newest first.
void UnitDeath(Game* game, Unit* victim, Unit* killer, int32_t flag) noexcept {
	if (game != nullptr && IsPlayer(victim) && api.unitDead(victim) == 0) {
		Lock lock;
		for (;;) {
			FindEntry(game, victim, -1);  // drop other games' entries
			Active* newest = nullptr;
			for (Active& a : g_active) {
				if (a.unit == victim && (newest == nullptr || a.order > newest->order)) newest = &a;
			}
			if (newest == nullptr) break;
			++g_deaths;
			StopAndErase(game, victim, newest, "death");
		}
	}
	g_deathOriginal(game, victim, killer, flag);
}

auto AlreadyHooked(uint64_t exeBase, uint64_t rva) -> bool {
	const auto* entry = reinterpret_cast<const uint8_t*>(exeBase + rva);
	return entry[0] == 0xff && entry[1] == 0x25;
}

auto InstallHooks(const D2RL::PluginContext* context) -> bool {
	if (g_hooksTried) {
		return g_hooksOk;
	}
	g_hooksTried = true;
	aura::Bind(context->exeBase);
	struct H {
		const char* name;
		uint64_t rva;
		bool ok;
	} hooks[] = {{"tick", aura::RvaAuraTick, false}, {"start", aura::RvaStartRightAura, false},
	             {"select", aura::RvaSelectSkillBody, false}, {"cast", aura::RvaStartSkill, false},
	             {"death", aura::RvaUnitDeath, false}};
	for (H& h : hooks) {
		if (AlreadyHooked(context->exeBase, h.rva)) {
			std::snprintf(g_hookReport, sizeof(g_hookReport), "%s 0x%llx already hooked by someone else", h.name, static_cast<unsigned long long>(h.rva));
			return false;
		}
	}
	hooks[0].ok = context->InstallInlineHook(aura::RvaAuraTick, aura::AuraTickEntry, sizeof(aura::AuraTickEntry), AuraTick, &g_tickOriginal);
	hooks[1].ok = context->InstallInlineHook(aura::RvaStartRightAura, aura::StartRightAuraEntry, sizeof(aura::StartRightAuraEntry), StartRightAura, &aura::startRightAuraOriginal);
	hooks[2].ok = context->InstallInlineHook(aura::RvaSelectSkillBody, aura::SelectSkillBodyEntry, sizeof(aura::SelectSkillBodyEntry), SelectSkill, &g_selectOriginal);
	hooks[3].ok = context->InstallInlineHook(aura::RvaStartSkill, aura::StartSkillEntry, sizeof(aura::StartSkillEntry), StartSkill, &g_startSkillOriginal);
	hooks[4].ok = context->InstallInlineHook(aura::RvaUnitDeath, aura::UnitDeathEntry, sizeof(aura::UnitDeathEntry), UnitDeath, &g_deathOriginal);
	g_hooksOk = true;
	int n = 0;
	n += std::snprintf(g_hookReport, sizeof(g_hookReport), "hooks");
	for (const H& h : hooks) {
		g_hooksOk = g_hooksOk && h.ok;
		n += std::snprintf(g_hookReport + n, sizeof(g_hookReport) - static_cast<size_t>(n), " %s%s", h.name, h.ok ? "" : "(REFUSED)");
	}
	// A refused hook leaves the others installed but every feature off: the hooks then pass straight through.
	return g_hooksOk;
}

}  // namespace

auto FeatureWord(Feature f) noexcept -> const char* {
	constexpr const char* words[] = {"toggle", "drain", "reserve", "decouple"};
	return words[f];
}

auto Enable(const D2RL::PluginContext* context, Feature f, char* report, size_t reportSize) noexcept -> bool {
	if (!InstallHooks(context)) {
		std::snprintf(report, reportSize, "off: %s", g_hookReport);
		return false;
	}
	g_on[f] = true;
	constexpr const char* what[] = {"cast/select toggles listed auras", "per-second mana/life drain",
	                                "mana/life reservation", "toggled auras survive a skill switch"};
	std::snprintf(report, reportSize, "on: %s (%s)", what[f], g_hookReport);
	return true;
}

void Describe(void (*line)(const char* text, void* user), void* user) noexcept {
	Lock lock;
	for (const Active& a : g_active) {
		char text[200];
		const config::SkillConfig* cfg = config::Find(a.skill);
		std::snprintf(text, sizeof(text), "  aura %d %s level %d%s, reserves %d %s, drain %s %.2f/s", a.skill,
		              cfg != nullptr ? cfg->key : "?", a.level, a.managed ? " toggled" : " (vanilla select)", a.reserved >> 8,
		              a.reserveStat == aura::StatMaxMana ? "mana" : a.reserveStat ? "life" : "-",
		              cfg != nullptr ? config::ResourceName(cfg->drainResource) : "?", cfg != nullptr ? cfg->drainPerSecond : 0.0);
		line(text, user);
	}
}

void Counters(char* out, size_t size) noexcept {
	std::snprintf(out, size, "on %u, off %u, refused %u, drain stops %u, kept through switch %u, death releases %u",
	              g_toggledOn.load(), g_toggledOff.load(), g_refused.load(), g_drainStops.load(), g_rearms.load(), g_deaths.load());
}

}  // namespace auras
