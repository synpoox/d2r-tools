#include "engine/event_apply.h"

#include "core/part_signatures.h"
#include "engine/maze_hooks.h"
#include "engine/scan.h"
#include "log.h"

#include <windows.h>

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <mutex>
#include <optional>
#include <string>

namespace carto {

namespace {

namespace ep = cartographer::eventplan;
namespace ev = cartographer::events;

// --- Engine facts. Build 93847; each is recorded with its evidence in docs/reverse-engineering/cartographer.md section 8.
constexpr int UnitTypeOffset = 0x00;        // int32: 0 player
constexpr int UnitIdOffset = 0x08;          // uint32
constexpr int ServerDrlgRoomOffset = 0x18;  // a server room's DRLG room
constexpr int ServerRectOffset = 0x80;      // a server room's x, y, w, h (int32 each); units unknown, logged next to the DRLG rect
constexpr int DrlgRectOffset = 0x60;        // a DRLG room's x, y, w, h (int32 each), in tiles
constexpr int GameSuperBitmapOffset = 0x1d8;  // the game's once-per-game superunique bitmap
constexpr int SubtilesPerTile = 5;
constexpr int32_t SpawnMode = 1;            // what the superunique code passes to the monster spawn
constexpr int32_t SpawnParam = 0xa;         // the same caller's seventh argument
constexpr uint16_t SpawnFlags = 0;          // 0: the game's normal path, so the map's stats reach event monsters too
constexpr int MaxPopulateProbes = 12;       // rooms of the map level logged in full

struct Game;
struct Unit;

using PopulateFn = uint64_t (*)(Game*, void* room) noexcept;
using SpawnMonsterFn = void* (*)(Game*, void* room, int32_t x, int32_t y, int32_t monsterId, int32_t mode, int32_t param, uint16_t flags) noexcept;
using SpawnSuperFn = void* (*)(Game*, void* room, int32_t x, int32_t y, int32_t row, uint16_t flags) noexcept;
using UnitRoomFn = void* (*)(Unit*) noexcept;
using RoomLevelFn = int32_t (*)(void* room) noexcept;

enum PartId { PopulateRoom, SpawnMonster, SpawnSuperUnique, EvUnitRoom, EvRoomLevelId, PartCount };

Part g_parts[PartCount] = {
	{"PopulateRoom", sig::PopulateRoom, 12, false, nullptr, {}, {}},
	{"SpawnMonster", sig::SpawnMonster, 0, false, nullptr, {}, {}},
	{"SpawnSuperUnique", sig::SpawnSuperUnique, 0, false, nullptr, {}, {}},
	{"EventUnitRoom", sig::UnitRoom, 0, false, nullptr, {}, {}},
	{"EventRoomLevelId", sig::RoomLevelId, 0, false, nullptr, {}, {}},
};

const D2RL::PluginContext* g_context = nullptr;
PopulateFn g_original = nullptr;
std::atomic<bool> g_spawnOn{false};   // the populator hook is in and the spawn parts are found
std::atomic<bool> g_tickOn{false};    // the entry message can be shown (the portal's tick part and the room parts)
std::atomic<bool> g_busy{false};      // a map is armed
std::atomic<bool> g_bannerOverlay{false};

template <typename Fn>
auto Call(PartId id) noexcept -> Fn {
	return reinterpret_cast<Fn>(const_cast<uint8_t*>(g_parts[id].address));
}

auto On(PartId id) noexcept -> bool {
	return g_parts[id].address != nullptr && g_parts[id].status.on;
}

// The armed map. Guarded by g_lock; replaced when a map opens, cleared when the game is left.
struct State {
	std::string mapName;
	int level = -1;
	bool maze = false;
	std::optional<ev::Event> event;  // only an event that can be spawned: its Name and Message are what the player is shown
	ep::Resolved spawn;
	uint64_t seed = 0;
	std::optional<ep::EventRoomTracker> tracker;  // made at the first room of the level, when the plan's event room is known
	ep::EntryTracker entry;
	bool spawned = false;
	int probes = 0;
};
std::mutex g_lock;
std::shared_ptr<State> g_state;
uint64_t g_generation = 0;

// --- Banner ---------------------------------------------------------------------------------------------------------------

struct Banner {
	std::vector<std::string> lines;
	uint64_t startMs = 0;
	bool active = false;
};
std::mutex g_bannerLock;
Banner g_banner;
const D2RL::OverlayService* g_overlay = nullptr;
D2RL::Overlay::CallbackHandle g_callback = D2RL::Overlay::InvalidCallbackHandle;
std::atomic<bool> g_drawFailedLogged{false};

void __cdecl OnFrame(const D2RL::PluginContext* context, const D2RL::Overlay::Frame* frame, void*) noexcept {
	if (g_overlay == nullptr || frame == nullptr) {
		return;
	}
	std::vector<std::string> lines;
	int alpha = 0;
	{
		std::lock_guard<std::mutex> lock(g_bannerLock);
		if (!g_banner.active) {
			return;
		}
		alpha = ep::bannerAlpha(static_cast<int>(GetTickCount64() - g_banner.startMs));
		if (alpha == 0 && GetTickCount64() - g_banner.startMs >= static_cast<uint64_t>(ep::kBannerTotalMs)) {
			g_banner.active = false;
			return;
		}
		lines = g_banner.lines;
	}
	if (alpha <= 0 || lines.empty()) {
		return;
	}
	const float a = static_cast<float>(alpha) / 255.0f;
	float y = frame->screenHeight * 0.16f;
	for (size_t i = 0; i < lines.size(); ++i) {
		const float size = frame->defaultTextSize * (i == 0 ? 1.7f : 1.25f);
		D2RL::Overlay::MeasureTextRequest measure{};
		measure.structSize = D2RL::Overlay::MeasureTextRequestSize;
		measure.canvas = frame->canvas;
		measure.textSize = size;
		measure.text = lines[i].c_str();
		measure.textLength = static_cast<uint32_t>(lines[i].size());
		D2RL::Overlay::TextMetrics metrics{};
		metrics.structSize = D2RL::Overlay::TextMetricsSize;
		if (g_overlay->measureText(context, &measure, &metrics) != D2RL::Overlay::Result::Success) {
			metrics.width = 0;
			metrics.height = size;
		}
		const D2RL::Overlay::Color colours[3] = {{0.86f, 0.76f, 0.46f, a}, {0.92f, 0.32f, 0.26f, a}, {0.85f, 0.85f, 0.85f, a}};
		const D2RL::Overlay::Color shadow{0.0f, 0.0f, 0.0f, a * 0.85f};
		const float x = (frame->screenWidth - metrics.width) * 0.5f;
		for (int pass = 0; pass < 2; ++pass) {
			D2RL::Overlay::TextRequest request{};
			request.structSize = D2RL::Overlay::TextRequestSize;
			request.canvas = frame->canvas;
			request.position = {x + (pass == 0 ? 2.0f : 0.0f), y + (pass == 0 ? 2.0f : 0.0f)};
			request.color = pass == 0 ? shadow : colours[std::min<size_t>(i, 2)];
			request.textSize = size;
			request.text = lines[i].c_str();
			request.textLength = static_cast<uint32_t>(lines[i].size());
			if (g_overlay->drawText(context, &request) != D2RL::Overlay::Result::Success && !g_drawFailedLogged.exchange(true)) {
				plog::Warn("the entry banner could not draw its text through the overlay; the console shows the message instead from now on");
				g_bannerOverlay = false;
			}
		}
		y += (metrics.height > 0 ? metrics.height : size) * 1.2f;
	}
}

void ShowEntryMessage(const std::vector<std::string>& lines) noexcept {
	if (lines.empty()) {
		return;
	}
	if (g_bannerOverlay.load()) {
		std::lock_guard<std::mutex> lock(g_bannerLock);
		g_banner.lines = lines;
		g_banner.startMs = GetTickCount64();
		g_banner.active = true;
		plog::Info("eventprobe: entry message shown on the overlay (%zu line(s))", lines.size());
		return;
	}
	for (const std::string& line : lines) {
		if (g_context != nullptr) {
			g_context->WriteConsoleMessage(line.c_str());
		}
	}
	plog::Info("eventprobe: entry message written to the console (%zu line(s)); no overlay", lines.size());
}

// --- Rooms and spawns -----------------------------------------------------------------------------------------------------

struct RoomRects {
	bool ok = false;
	ep::Rect drlg;    // tiles
	ep::Rect server;  // units unknown
};

auto ReadRects(void* serverRoom) noexcept -> RoomRects {
	RoomRects out;
	void* drlg = At<void*>(serverRoom, ServerDrlgRoomOffset);
	if (drlg == nullptr) {
		return out;
	}
	out.drlg = {At<int32_t>(drlg, DrlgRectOffset), At<int32_t>(drlg, DrlgRectOffset + 4), At<int32_t>(drlg, DrlgRectOffset + 8), At<int32_t>(drlg, DrlgRectOffset + 12)};
	out.server = {At<int32_t>(serverRoom, ServerRectOffset), At<int32_t>(serverRoom, ServerRectOffset + 4), At<int32_t>(serverRoom, ServerRectOffset + 8), At<int32_t>(serverRoom, ServerRectOffset + 12)};
	out.ok = out.drlg.w > 0 && out.drlg.h > 0;
	return out;
}

auto Describe(const ep::Rect& r) -> std::string {
	char text[64];
	std::snprintf(text, sizeof(text), "(%d,%d) %d x %d", r.x, r.y, r.w, r.h);
	return text;
}

// One monster at a world position (subtiles). Returns whether the game gave a unit.
auto SpawnOne(Game* game, void* room, const ep::Spawn& s, bool* asSuper) noexcept -> bool {
	*asSuper = false;
	const int32_t x = s.spot.x * SubtilesPerTile + SubtilesPerTile / 2, y = s.spot.y * SubtilesPerTile + SubtilesPerTile / 2;
	if (s.superUnique && On(SpawnSuperUnique)) {
		const uint8_t* bitmap = reinterpret_cast<const uint8_t*>(game) + GameSuperBitmapOffset;
		if (!ep::superSeen(bitmap, s.row)) {
			void* unit = Call<SpawnSuperFn>(SpawnSuperUnique)(game, room, x, y, s.row, 0);
			const bool setNow = ep::superSeen(bitmap, s.row);
			plog::Info("eventprobe: superunique row %d at (%d,%d): returned %p, once-per-game bit now %s", s.row, x, y, unit, setNow ? "set" : "NOT set");
			if (unit != nullptr) {
				*asSuper = true;
				return true;
			}
		} else {
			plog::Info("eventprobe: superunique row %d was already spawned this game; its class (monstats row %d) is spawned instead", s.row, s.classRow);
		}
	}
	const int row = s.superUnique ? s.classRow : s.row;
	if (row < 0) {
		return false;
	}
	void* unit = Call<SpawnMonsterFn>(SpawnMonster)(game, room, x, y, row, SpawnMode, SpawnParam, SpawnFlags);
	return unit != nullptr;
}

void SpawnEvent(Game* game, void* room, const State& state, const RoomRects& rects, uint64_t seed) noexcept {
	const ep::SpawnPlan plan = ep::planEvent(*state.event, state.spawn, rects.drlg, seed);
	if (!plan.problem.empty()) {
		plog::Warn("event '%s' not spawned: %s", state.event->name.c_str(), plan.problem.c_str());
		return;
	}
	int made = 0, failed = 0, supers = 0;
	for (const ep::Spawn& s : plan.spawns) {
		bool asSuper = false;
		if (SpawnOne(game, room, s, &asSuper)) {
			++made;
			supers += asSuper ? 1 : 0;
		} else {
			++failed;
		}
	}
	plog::Info("event '%s' (%s): spawned %d of %zu in the event room %s (%d as a superunique, %d refused by the game)", state.event->name.c_str(),
	           ev::kindName(state.event->kind), made, plan.spawns.size(), Describe(rects.drlg).c_str(), supers, failed);
}

void OnPopulated(Game* game, void* room) noexcept {
	std::shared_ptr<State> state;
	uint64_t generation = 0;
	{
		std::lock_guard<std::mutex> lock(g_lock);
		state = g_state;
		generation = g_generation;
	}
	if (state == nullptr || state->level <= 0 || room == nullptr || !On(EvRoomLevelId)) {
		return;
	}
	if (Call<RoomLevelFn>(EvRoomLevelId)(room) != state->level) {
		return;
	}
	const RoomRects rects = ReadRects(room);
	bool fire = false;
	bool planned = false;
	int number = 0;
	{
		std::lock_guard<std::mutex> lock(g_lock);
		if (generation != g_generation || g_state != state) {
			return;  // the game was left, or another map opened, in between
		}
		number = ++state->probes;
		if (!rects.ok) {
			if (number <= MaxPopulateProbes) {
				plog::Info("eventprobe: room #%d of level %d populated, but its DRLG room is not readable; ignored", number, state->level);
			}
			return;
		}
		if (!state->tracker) {
			std::optional<ep::Rect> plan;
			if (state->maze) {
				const MazeEventRect m = MazeEventRoom();
				if (m.known && m.levelId == state->level) {
					plan = ep::Rect{m.x, m.y, m.w, m.h};
				} else {
					plog::Warn("the maze plan's event room is not known; the event room is chosen like a preset map's (the first room at least %d tiles from the first populated one)", ep::kMinEventDistance);
				}
			}
			state->tracker.emplace(plan);
		}
		planned = state->tracker->planned();
		fire = !state->spawned && state->tracker->onPopulated(rects.drlg);
		if (fire && state->event) {
			state->spawned = true;
		} else {
			fire = false;
		}
	}
	if (number <= MaxPopulateProbes) {
		plog::Info("eventprobe: room #%d of level %d populated: DRLG %s tiles, server %s (server w %s the DRLG w, so the server rect is %s), %s%s", number, state->level,
		           Describe(rects.drlg).c_str(), Describe(rects.server).c_str(), rects.server.w == rects.drlg.w ? "equals" : rects.server.w == rects.drlg.w * SubtilesPerTile ? "is 5x" : "differs from",
		           rects.server.w == rects.drlg.w ? "in tiles" : rects.server.w == rects.drlg.w * SubtilesPerTile ? "in subtiles" : "in unknown units",
		           planned ? "event room from the plan" : "event room by distance", fire ? ": THIS IS THE EVENT ROOM" : "");
	}
	if (fire) {
		SpawnEvent(game, room, *state, rects, ep::eventSeed(state->seed));
	}
}

auto PopulateHook(Game* game, void* room) noexcept -> uint64_t {
	if (g_original == nullptr) {
		return 0;  // reachable only if the loader gave no original (the install logs it and marks the part off)
	}
	const uint64_t result = g_original(game, room);
	if (g_busy.load(std::memory_order_relaxed) && game != nullptr) {
		try {
			OnPopulated(game, room);
		} catch (...) {
			plog::Error("map events: handling a populated room failed");
		}
	}
	return result;
}

auto SummaryNow() -> std::string {
	const std::string spawn = g_spawnOn.load() ? "Boss and Horde events on" : "Boss and Horde events off";
	const std::string banner = !g_tickOn.load() ? "entry message off (a part was not found)" : g_bannerOverlay.load() ? "entry message on the overlay" : "entry message on the console";
	return spawn + ", " + banner + " (not yet verified in game)";
}

}  // namespace

auto InstallEventApply(const D2RL::PluginContext* context, const std::vector<SignatureOverride>& overrides, std::vector<PartStatus>* parts) -> bool {
	g_context = context;
	CodeRange ranges[8];
	const int count = CodeRanges(context->exeBase, ranges, 8);
	for (Part& part : g_parts) {
		ResolvePart(context, overrides, ranges, count, part);
	}
	// The populator hook goes in at load, only when everything it needs was found. Never installed half-on.
	if (On(PopulateRoom) && On(SpawnMonster) && On(EvRoomLevelId)) {
		Part& part = g_parts[PopulateRoom];
		const uint64_t rva = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(part.address) - context->exeBase);
		const bool hooked = context->InstallInlineHook(rva, part.guard, part.hookSize, reinterpret_cast<void*>(&PopulateHook), reinterpret_cast<void**>(&g_original));
		const bool ok = hooked && g_original != nullptr;
		part.status.on = ok;
		part.status.detail = ok ? "found by signature, hooked" : !hooked ? "off: D2RLoader refused the hook" : "off: hooked, but D2RLoader gave no way back to the game's own function, so every room's own population is skipped until the game is restarted";
		if (hooked && !ok) {
			plog::Error("part %s: D2RLoader installed the hook but gave no original; the game's own room population is skipped for EVERY room until the game is restarted; restart it after removing what conflicts", part.name);
		}
		g_spawnOn = ok;
	} else if (On(PopulateRoom)) {
		g_parts[PopulateRoom].status.on = false;
		g_parts[PopulateRoom].status.detail = "off: not hooked, because a part it needs was not found";
	}
	// The entry message follows the portal's tick part and needs the room parts.
	bool tickOn = false;
	for (const PartStatus& s : PartStatuses()) {
		tickOn = tickOn || (s.name == "PlayerTick" && s.on);
	}
	g_tickOn = tickOn && On(EvUnitRoom) && On(EvRoomLevelId);
	if (!g_tickOn) {
		plog::Warn("the entry message is off: %s", !tickOn ? "the PlayerTick part is off" : "a room part was not found");
	}
	if (!g_spawnOn) {
		plog::Warn("Boss and Horde events are off (see the parts above)");
	}
	// The overlay, best effort; without it the entry message goes to the console.
	if (context->QueryService(&g_overlay) == D2RL::ServiceQueryResult::Success && D2RL::HasOverlayServiceField(g_overlay, D2RL::OverlayServiceRequiredSize)) {
		D2RL::Overlay::CallbackRegistration registration{};
		registration.structSize = D2RL::Overlay::CallbackRegistrationSize;
		registration.phase = D2RL::Overlay::Phase::AfterGameUi;
		registration.callback = OnFrame;
		const auto result = g_overlay->registerFrameCallback(context, &registration, &g_callback);
		g_bannerOverlay = result == D2RL::Overlay::Result::Success;
		if (!g_bannerOverlay) {
			plog::Warn("the overlay refused the banner callback (result %u); the entry message goes to the console", static_cast<uint32_t>(result));
		}
	} else {
		g_overlay = nullptr;
		plog::Warn("no overlay service; the entry message goes to the console");
	}
	plog::Info("map events: %s", SummaryNow().c_str());
	if (parts != nullptr) {
		*parts = EventPartStatuses();
	}
	for (const Part& part : g_parts) {
		plog::Info("part %s: %s", part.name, part.status.detail.c_str());
	}
	return g_spawnOn.load();
}

void EventsMapOpened(const MapDef& map, const std::shared_ptr<const Tables>& tables, uint32_t playerId, uint32_t openings, int level, bool maze) {
	if (tables == nullptr || (!g_tickOn.load() && !g_spawnOn.load())) {
		return;
	}
	auto state = std::make_shared<State>();
	state->mapName = map.name;
	state->level = level;
	state->maze = maze;
	state->seed = cartographer::statplan::mixSeed(GetTickCount64(), playerId, openings);
	const int chance = ev::effectiveEventChance(map.eventChance >= 0 ? std::optional<int>(map.eventChance) : std::nullopt, tables->settings.eventChance);
	const cartographer::layout::Point unused{0, 0};
	std::optional<ev::Event> event = ev::decide(tables->events, chance, ep::eventSeed(state->seed), &unused);
	if (!event) {
		plog::Info("event for '%s' (seed %llu, chance %d%%): none", map.name.c_str(), static_cast<unsigned long long>(state->seed), chance);
	} else {
		plog::Info("event for '%s' (seed %llu, chance %d%%): %s", map.name.c_str(), static_cast<unsigned long long>(state->seed), chance, ev::describe(*event).c_str());
		state->spawn = ep::resolveSpawn(tables->spawnNames, event->spawn);
		// Checked now with a stand-in room, so a event that cannot be spawned is neither announced nor waited for.
		const ep::SpawnPlan check = ep::planEvent(*event, state->spawn, ep::Rect{0, 0, 24, 24}, state->seed);
		if (!check.problem.empty()) {
			plog::Warn("event '%s' is decided but will not be spawned or announced: %s", event->name.c_str(), check.problem.c_str());
		} else if (!g_spawnOn.load()) {
			plog::Warn("event '%s' is decided but will not be spawned or announced: the spawn parts are off", event->name.c_str());
		} else {
			state->event = std::move(event);
		}
	}
	std::lock_guard<std::mutex> lock(g_lock);
	g_state = std::move(state);
	g_busy = true;
}

void EventsPlayerTick(void* rawGame, void* rawUnit, uint32_t localPlayer) noexcept {
	(void)rawGame;
	auto* unit = static_cast<Unit*>(rawUnit);
	if (!g_busy.load(std::memory_order_relaxed) || !g_tickOn.load() || unit == nullptr || At<int32_t>(unit, UnitTypeOffset) != 0 || At<uint32_t>(unit, UnitIdOffset) != localPlayer) {
		return;
	}
	try {
		std::shared_ptr<State> state;
		{
			std::lock_guard<std::mutex> lock(g_lock);
			state = g_state;
		}
		if (state == nullptr || state->level <= 0) {
			return;
		}
		void* room = Call<UnitRoomFn>(EvUnitRoom)(unit);
		const bool inMap = room != nullptr && Call<RoomLevelFn>(EvRoomLevelId)(room) == state->level;
		std::vector<std::string> lines;
		{
			std::lock_guard<std::mutex> lock(g_lock);
			if (g_state != state || !state->entry.update(inMap)) {
				return;
			}
			lines = ep::entryLines(state->mapName, state->event ? &*state->event : nullptr);
		}
		ShowEntryMessage(lines);
	} catch (...) {
		plog::Error("map events: the entry message failed");
	}
}

void EventsGameLeft() noexcept {
	{
		std::lock_guard<std::mutex> lock(g_lock);
		++g_generation;
		g_state.reset();
		g_busy = false;
	}
	std::lock_guard<std::mutex> lock(g_bannerLock);
	g_banner = {};
}

auto EventPartStatuses() -> std::vector<PartStatus> {
	std::vector<PartStatus> out;
	for (const Part& part : g_parts) {
		out.push_back(part.status);
	}
	return out;
}

auto EventsSummary() -> std::string {
	return "map events: " + SummaryNow();
}

}  // namespace carto
