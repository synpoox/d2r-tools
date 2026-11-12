#include "portal.h"

#include "maze_hooks.h"
#include "scan.h"

#include "core/part_signatures.h"
#include "core/request.h"
#include "core/sig.h"
#include "core/text.h"
#include "log.h"
#include "steer.h"

#include <windows.h>

#include <atomic>
#include <cstdio>
#include <mutex>

namespace carto {

namespace {

// --- Engine facts. Offsets are into D2R's own structures (build 93847); each one is recorded with its evidence in
// docs/reverse-engineering/cartographer.md. The functions are found by signature, never by address.
constexpr int GameDifficultyOffset = 0x104;  // uint8: 0 Normal, 1 Nightmare, 2 Hell
constexpr int GameDataFlagOffset = 0x106;    // uint8: the first argument of the item record lookup
constexpr int GameCountersOffset = 0x1aac8;  // per-game counters (key -> int)
constexpr int RecordCodeOffset = 0x80;       // uint32: the item code, four letters
constexpr int RecordSpellOffset = 0x94;      // int32: misc.txt pSpell
constexpr int32_t MapSpell = 16;             // free in vanilla D2R
constexpr uint32_t OpenKey = 0x4d415053;     // 'MAPS': the game counter that allows one opening per game
constexpr int32_t StatLevel = 12;
constexpr int32_t HarrogathId = 109;
constexpr int32_t CantDoThat = 0x15;
constexpr int UnitTypeOffset = 0x00;        // int32: 0 for a player
constexpr int UnitIdOffset = 0x08;          // uint32: the unit id (quick-npc.md section 6)
constexpr uint32_t NoPlayer = 0xffffffffu;  // no local player known
constexpr uint64_t RequestLifeMs = 10000;   // a console request not served within this long is dropped

struct Game;
struct Unit;

using CheckFn = uint64_t (*)(Game*, Unit*, Unit*, Unit*) noexcept;
using UseFn = void (*)(Game*, Unit*, Unit*, Unit*) noexcept;
using ItemRecordFn = const uint8_t* (*)(uint32_t dataFlag, int32_t row) noexcept;
using UnitClassFn = int32_t (*)(Unit*, const char* file, int32_t line) noexcept;
using UnitRoomFn = void* (*)(Unit*) noexcept;
using RoomLevelFn = int32_t (*)(void* room) noexcept;
using ClientMessageFn = void (*)(Unit* player, int32_t message, Unit* from) noexcept;
using OpenRedPortalFn = int32_t (*)(Game*, Unit* player, uint32_t key, int32_t level, int32_t max) noexcept;
using GameCounterFn = int32_t* (*)(uint8_t* counters, uint32_t* key) noexcept;
using UnitStatFn = int32_t (*)(Unit*, int32_t stat, int32_t index) noexcept;
using UnitCoordsFn = void (*)(Unit*, int32_t* xy) noexcept;
using FindFreeSpotFn = void (*)(Game*, void* room, int32_t* xy, uint32_t, uint32_t, void** found, uint64_t, int32_t) noexcept;
using PlayerTickFn = uint64_t (*)(Game*, Unit*) noexcept;

enum PartId { ItemSpellCheck, ItemSpellUse, ItemRecord, UnitClassOf, UnitRoom, RoomLevelId, ClientMessage, OpenRedPortal, GameCounter, UnitStat, UnitCoords, FindFreeSpot, PlayerTick, PartCount };

Part g_parts[PartCount] = {
	{"ItemSpellCheck", sig::ItemSpellCheck, 15, true, nullptr, {}, {}},
	{"ItemSpellUse", sig::ItemSpellUse, 14, true, nullptr, {}, {}},
	{"ItemRecord", sig::ItemRecord, 0, true, nullptr, {}, {}},
	{"UnitClassOf", sig::UnitClassOf, 0, true, nullptr, {}, {}},
	{"UnitRoom", sig::UnitRoom, 0, true, nullptr, {}, {}},
	{"RoomLevelId", sig::RoomLevelId, 0, true, nullptr, {}, {}},
	{"ClientMessage", sig::ClientMessage, 0, true, nullptr, {}, {}},
	{"OpenRedPortal", sig::OpenRedPortal, 0, true, nullptr, {}, {}},
	{"GameCounter", sig::GameCounter, 0, true, nullptr, {}, {}},
	{"UnitStat", sig::UnitStat, 0, true, nullptr, {}, {}},
	{"UnitCoords", sig::UnitCoords, 0, false, nullptr, {}, {}},
	{"FindFreeSpot", sig::FindFreeSpot, 0, false, nullptr, {}, {}},
	// Not needed by the map item: only the console command waits on it. Build 93847: the player mana step
	// (game, player), run once per frame for every live player (docs/reverse-engineering/cartographer.md section 3).
	{"PlayerTick", sig::PlayerTick, 14, false, nullptr, {}, {}},
};

std::mutex g_lock;
std::shared_ptr<const Tables> g_tables;
const MapDef* g_pending = nullptr;  // chosen at the check, used at the use; kept alive by g_pendingTables
std::shared_ptr<const Tables> g_pendingTables;
enum class Verdict { None, Map, Plain };  // what the check decided for g_pendingItem: opens a map, or goes to the game
Verdict g_verdict = Verdict::None;
const void* g_pendingGame = nullptr;    // the game, player and item the check decided for: the use must be the same
const void* g_pendingPlayer = nullptr;
const void* g_pendingItem = nullptr;
std::atomic<bool> g_live{false};
const D2RL::PluginContext* g_context = nullptr;
std::vector<SignatureOverride> g_overrides;  // kept for RescanPortal
bool g_hooksIn = false;                      // both item hooks were installed at load; they pass through until g_live
CheckFn g_checkOriginal = nullptr;
UseFn g_useOriginal = nullptr;
PlayerTickFn g_tickOriginal = nullptr;
OpenRequest g_request;                     // the console's request; guarded by g_lock
std::atomic<bool> g_requested{false};      // g_request has something to serve: the per-frame hook reads only this
std::atomic<bool> g_consoleLive{false};    // the tick hook and the game events are in
std::atomic<uint32_t> g_localPlayer{NoPlayer};  // unit id of the local player (LocalPlayerReady), else NoPlayer
std::atomic<bool> g_tickSeen{false};            // the tick hook has run in this game: this process has a server
bool g_installed = false;                       // InstallPortal ran to the end
std::atomic<bool> g_firstLogged{false};         // the first player the tick saw this game has been logged

// Forgets the check-to-use state. g_lock must be held.
void ClearPending() noexcept {
	g_pending = nullptr;
	g_pendingTables.reset();
	g_verdict = Verdict::None;
	g_pendingGame = g_pendingPlayer = g_pendingItem = nullptr;
}

auto NowMs() noexcept -> uint64_t {
	return GetTickCount64();
}

template <typename Fn>
auto Call(PartId id) noexcept -> Fn {
	return reinterpret_cast<Fn>(const_cast<uint8_t*>(g_parts[id].address));
}

// --- The game's own helpers ----------------------------------------------------------------------------------------

auto CodeOf(Game* game, Unit* item, const uint8_t** record) noexcept -> std::string {
	const int32_t row = Call<UnitClassFn>(UnitClassOf)(item, __FILE__, __LINE__);
	*record = Call<ItemRecordFn>(ItemRecord)(At<uint8_t>(game, GameDataFlagOffset), row);
	if (*record == nullptr) {
		return {};
	}
	char code[5] = {};
	std::memcpy(code, *record + RecordCodeOffset, 4);
	return Trim(code);
}

auto PlayerLevelId(Unit* player) noexcept -> int32_t {
	void* room = Call<UnitRoomFn>(UnitRoom)(player);
	return room != nullptr ? Call<RoomLevelFn>(RoomLevelId)(room) : -1;
}

auto Openings(Game* game) noexcept -> int32_t {
	uint32_t key = OpenKey;
	const int32_t* counter = Call<GameCounterFn>(GameCounter)(reinterpret_cast<uint8_t*>(game) + GameCountersOffset, &key);
	return counter != nullptr ? *counter : 0;
}

// OpenRedPortal's own spot search, so a refusal does not use the map up. Without the two helpers, assume a spot.
auto SpotFree(Game* game, Unit* player) noexcept -> bool {
	if (g_parts[UnitCoords].address == nullptr || g_parts[FindFreeSpot].address == nullptr) {
		return true;
	}
	void* room = Call<UnitRoomFn>(UnitRoom)(player);
	if (room == nullptr) {
		return false;
	}
	int32_t xy[2] = {};
	Call<UnitCoordsFn>(UnitCoords)(player, xy);
	void* found = nullptr;
	Call<FindFreeSpotFn>(FindFreeSpot)(game, room, xy, 3, 0x400, &found, 5, 100);
	return found != nullptr;
}

// What the gates look at, read from the game for this player. Game thread only.
auto GateStateOf(Game* game, Unit* player) noexcept -> GateState {
	GateState state;
	state.difficulty = At<uint8_t>(game, GameDifficultyOffset);
	state.charLevel = Call<UnitStatFn>(UnitStat)(player, StatLevel, 0);
	state.areaId = PlayerLevelId(player);
	state.townId = HarrogathId;
	state.openings = Openings(game);
	state.spotFree = SpotFree(game, player);
	return state;
}

void Tell(const std::string& text) noexcept {
	plog::Info("%s", text.c_str());
	if (g_context != nullptr) {
		g_context->WriteConsoleMessage(text.c_str());
	}
}

// A Cave or Crypt map opens through the Maze slot and is built by the maze steering hooks; every other map through the
// Preset slot.
auto IsMazeMap(const MapDef& map) noexcept -> bool {
	cartographer::maze::Tileset ts;
	return cartographer::maze::builtinTileset(map.tileset, ts);
}

auto SlotNameFor(const MapDef& map) noexcept -> const char* {
	return IsMazeMap(map) ? MazeSlot : PresetSlot;
}

// What stops a map from opening before the gates, or empty. `level` is the map's slot level (-1: not installed).
auto SlotProblem(const MapDef& map, const Tables& tables, int level) -> std::string {
	if (level <= 0) {
		return IsMazeMap(map) ? "Cartographer: the Maze slot is not installed. Add it to slots.txt and levels.txt (see the Cartographer README), then restart the game."
		                      : "Cartographer: the map slot is not installed. Run tools\\install.ps1 and restart the game.";
	}
	if (IsMazeMap(map) && !MazeSteeringLive()) {
		return "Cartographer: the maze steering is off, so " + map.tileset + " maps cannot be built. Run 'cartographer check' and see the plugin log.";
	}
	if (IsMazeMap(map)) {
		if (const std::string problem = MazeTilesetProblem(map, tables); !problem.empty()) {
			return problem;
		}
		if (MazeSlotBuilt()) {
			return "Cartographer: the Maze level was already built in this game and is not rebuilt, so " + map.name + " cannot be opened now. Leave the game and start a new one.";
		}
	}
	return {};
}

// Opens `map` through its slot. A maze map hands its plan to the maze hooks first; if the game then opens no portal, the plan is dropped.
auto OpenMapPortal(Game* game, Unit* player, const MapDef& map, const Tables& tables, int level) noexcept -> int32_t {
	const bool maze = IsMazeMap(map);
	if (maze) {
		MazeMapOpened(map, tables, NowMs() ^ static_cast<uint64_t>(reinterpret_cast<uintptr_t>(game)));
	}
	const int32_t opened = Call<OpenRedPortalFn>(OpenRedPortal)(game, player, OpenKey, level, 1);
	if (maze && opened == 0) {
		MazeMapCancelled();
	}
	return opened;
}

// --- The hooks ---------------------------------------------------------------------------------------------------------

auto SpellCheckHook(Game* game, Unit* player, Unit* item, Unit* target) noexcept -> uint64_t {
	if (!g_live.load()) {
		return g_checkOriginal != nullptr ? g_checkOriginal(game, player, item, target) : 0;  // the portal is off: the game's own check
	}
	const uint8_t* record = nullptr;
	const std::string code = game != nullptr && player != nullptr && item != nullptr ? CodeOf(game, item, &record) : std::string();
	if (record == nullptr || At<int32_t>(record, RecordSpellOffset) != MapSpell) {
		return g_checkOriginal(game, player, item, target);
	}
	std::shared_ptr<const Tables> tables;
	{
		std::lock_guard<std::mutex> lock(g_lock);
		tables = g_tables;
	}
	{
		// Any earlier verdict is stale from here on: only a check that decides below sets it again.
		std::lock_guard<std::mutex> lock(g_lock);
		ClearPending();
	}
	const MapDef* map = tables != nullptr ? FindMapByItem(tables->maps, code) : nullptr;
	const int level = tables != nullptr && map != nullptr ? SlotLevel(tables->slots, SlotNameFor(*map)) : -1;
	const auto refuse = [&](const std::string& text) -> uint64_t {
		Tell(text);
		Call<ClientMessageFn>(ClientMessage)(player, CantDoThat, player);
		return 0;
	};
	if (map == nullptr) {
		{
			// Recorded, so the use goes to the game as well even if a map for this code is added in between.
			std::lock_guard<std::mutex> lock(g_lock);
			g_verdict = Verdict::Plain;
			g_pendingGame = game;
			g_pendingPlayer = player;
			g_pendingItem = item;
		}
		return g_checkOriginal(game, player, item, target);  // no map names this item: it is not a map item, so the game's own check
	}
	if (const std::string problem = SlotProblem(*map, *tables, level); !problem.empty()) {
		return refuse(problem);
	}
	const GateState state = GateStateOf(game, player);
	const Gate gate = CheckGates(*map, state);
	if (gate != Gate::Open) {
		return refuse(RefusalText(gate, *map, state));
	}
	{
		std::lock_guard<std::mutex> lock(g_lock);
		g_pendingTables = tables;
		g_pending = map;
		g_verdict = Verdict::Map;
		g_pendingGame = game;
		g_pendingPlayer = player;
		g_pendingItem = item;
	}
	return 1;
}

void SpellUseHook(Game* game, Unit* player, Unit* item, Unit* target) noexcept {
	if (!g_live.load()) {
		if (g_useOriginal != nullptr) {
			g_useOriginal(game, player, item, target);  // the portal is off: the game's own use
		}
		return;
	}
	const uint8_t* record = nullptr;
	const std::string code = game != nullptr && player != nullptr && item != nullptr ? CodeOf(game, item, &record) : std::string();
	if (record == nullptr || At<int32_t>(record, RecordSpellOffset) != MapSpell) {
		g_useOriginal(game, player, item, target);
		return;
	}
	std::shared_ptr<const Tables> tables;
	const MapDef* map = nullptr;
	bool mapItem = false;
	{
		std::lock_guard<std::mutex> lock(g_lock);
		tables = g_pendingTables;
		Verdict verdict = Verdict::None;
		if (g_pendingGame == game && g_pendingPlayer == player && g_pendingItem == item) {
			verdict = g_verdict;
			map = g_pending;
		}
		// The check's verdict decides, both ways, so a reload between check and use cannot turn a map item into a plain
		// one or a plain one into a map item. With no verdict for this use, the tables as they are now decide.
		mapItem = verdict == Verdict::Map     ? map != nullptr
		          : verdict == Verdict::Plain ? false
		                                      : g_tables != nullptr && FindMapByItem(g_tables->maps, code) != nullptr;
		ClearPending();
	}
	if (!mapItem) {
		g_useOriginal(game, player, item, target);  // no map names this item: it is not a map item, so the game's own use
		return;
	}
	if (map == nullptr || tables == nullptr) {
		return;  // the check refused (or did not run, or ran for another game, player or item): nothing opens
	}
	const int level = SlotLevel(tables->slots, SlotNameFor(*map));
	if (const std::string problem = SlotProblem(*map, *tables, level); !problem.empty()) {
		Tell(problem);  // the check passed, so a slot or the steering went away in between
		return;
	}
	plog::Info("opening '%s' (%s) through level %d", map->name.c_str(), map->tileset.c_str(), level);
	const int32_t opened = OpenMapPortal(game, player, *map, *tables, level);
	Tell(opened != 0 ? "Cartographer: " + map->name + " is open." : "Cartographer: the game would not open a portal for " + map->name + ".");
}

// --- The console path ------------------------------------------------------------------------------------------------
// 'cartographer open <Name>' runs on the UI thread and has no game or player. It posts a request; the player tick
// (game thread, once per frame per player, with the game and the player) serves it for the local player, through the
// same gates as the item. No game or player pointer is kept between frames.

void ServeRequest(Game* game, Unit* unit) noexcept {
	if (At<int32_t>(unit, UnitTypeOffset) != 0) {
		return;
	}
	const uint32_t local = g_localPlayer.load();
	const uint32_t unitId = At<uint32_t>(unit, UnitIdOffset);
	std::shared_ptr<const Tables> tables;
	OpenRequest::Taken taken;
	bool stale = false;
	{
		std::lock_guard<std::mutex> lock(g_lock);
		tables = g_tables;
		if (unitId == local) {
			taken = g_request.Take(NowMs(), RequestLifeMs);
			g_requested = false;
		} else if (g_request.Stale(NowMs(), RequestLifeMs)) {
			g_request.Clear();
			g_requested = false;
			stale = true;
		}
	}
	if (stale) {
		plog::Warn("open request dropped: no player with unit id %u was seen (this tick: %u)", local, unitId);
		Tell("Cartographer: could not find your character in the game. Try again, and see the plugin log.");
		return;
	}
	if (taken.state == OpenRequest::State::None) {
		return;
	}
	if (taken.state == OpenRequest::State::Expired) {
		Tell("Cartographer: the request waited too long. Run the command again.");
		return;
	}
	const MapDef* map = tables != nullptr ? FindMap(tables->maps, taken.map) : nullptr;
	if (map == nullptr) {
		Tell("Cartographer: there is no map called '" + taken.map + "'. Run 'cartographer check'.");
		return;
	}
	const int level = SlotLevel(tables->slots, SlotNameFor(*map));
	if (const std::string problem = SlotProblem(*map, *tables, level); !problem.empty()) {
		Tell(problem);
		return;
	}
	const GateState state = GateStateOf(game, unit);
	const Gate gate = CheckGates(*map, state);
	if (gate != Gate::Open) {
		Tell(RefusalText(gate, *map, state));
		return;
	}
	plog::Info("opening '%s' (%s) through level %d from the console", map->name.c_str(), map->tileset.c_str(), level);
	const int32_t opened = OpenMapPortal(game, unit, *map, *tables, level);
	Tell(opened != 0 ? "Cartographer: " + map->name + " is open." : "Cartographer: the game would not open a portal for " + map->name + ".");
}

// MazeProbe: once per game, where the local player stands when it is in the Maze slot level (compared with the entrance room).
void MazeArrivalCheck(Unit* unit) noexcept {
	if (!MazeArrivalPending()) {
		return;  // the common case: MazeProbe is off, or the arrival was logged
	}
	const int slot = MazeSlotLevelId();
	if (slot <= 0 || g_parts[UnitRoom].address == nullptr || g_parts[RoomLevelId].address == nullptr) {
		return;
	}
	void* room = Call<UnitRoomFn>(UnitRoom)(unit);
	if (room != nullptr && Call<RoomLevelFn>(RoomLevelId)(room) == slot) {
		MazeProbeArrival(room, slot);
	}
}

auto PlayerTickHook(Game* game, Unit* unit) noexcept -> uint64_t {
	if (g_tickOriginal == nullptr) {
		// Reachable: the loader can patch the tick and still return no original (the install then logs an error and marks
		// the part off). The hook cannot be taken out and there is nothing to call, so the game's mana step is skipped for
		// every player until the game is restarted.
		return 0;
	}
	const uint64_t result = g_tickOriginal(game, unit);
	if (!g_tickSeen.load(std::memory_order_relaxed)) {
		g_tickSeen = true;
	}
	const uint32_t known = g_localPlayer.load();
	if (known != NoPlayer && !g_firstLogged.load(std::memory_order_relaxed) && unit != nullptr && At<int32_t>(unit, UnitTypeOffset) == 0) {
		g_firstLogged = true;  // once per game, and only once the player id is known: settles whether LocalPlayerReady.playerId is the unit id
		plog::Info("first player unit seen by the tick: id %u (LocalPlayerReady playerId %u)", At<uint32_t>(unit, UnitIdOffset), known);
	}
	if (known != NoPlayer && MazeSteeringLive() && unit != nullptr && At<int32_t>(unit, UnitTypeOffset) == 0 && At<uint32_t>(unit, UnitIdOffset) == known) {
		MazeArrivalCheck(unit);
	}
	if (g_requested.load(std::memory_order_relaxed) && g_live.load() && game != nullptr && unit != nullptr) {
		ServeRequest(game, unit);
	}
	return result;
}

void (*g_firstGameStart)() noexcept = nullptr;
std::atomic<bool> g_firstGameStartDone{false};

void __cdecl OnGameplayEvent(const D2RL::PluginContext*, const D2RL::Lifecycle::GameplayEvent* event, void*) noexcept {
	if (!D2RL::Lifecycle::HasGameplayEventField(event, D2RL::Lifecycle::GameplayEventRequiredSize)) {
		return;
	}
	if (event->kind == D2RL::Lifecycle::GameplayEventKind::LocalPlayerReady) {
		plog::Info("local player ready (unit id %u)", event->playerId);
		g_localPlayer = event->playerId;
		if (g_firstGameStart != nullptr && !g_firstGameStartDone.exchange(true)) {
			g_firstGameStart();
		}
		g_tickSeen = false;  // an old server may tick once more after GameLeft: this game must show its own tick
		g_firstLogged = false;
	} else if (event->kind == D2RL::Lifecycle::GameplayEventKind::GameLeft) {
		g_localPlayer = NoPlayer;
		g_tickSeen = false;
		g_firstLogged = false;
		std::lock_guard<std::mutex> lock(g_lock);
		g_request.Clear();
		g_requested = false;
		ClearPending();
	}
	if (event->kind == D2RL::Lifecycle::GameplayEventKind::GameLeft) {
		MazeGameLeft();
	}
}

// Registers the game events and, when the tick part is on, hooks it. Best effort: failure leaves 'cartographer open'
// refusing (the map items still work). GameLeft is registered whether or not the tick part was found or hooked: the
// check-to-use reset on leaving a game depends on it.
void InstallConsolePath(const D2RL::PluginContext* context) noexcept {
	Part& part = g_parts[PlayerTick];
	const D2RL::LifecycleService* lifecycle = nullptr;
	if (context->QueryService(&lifecycle) != D2RL::ServiceQueryResult::Success || lifecycle == nullptr ||
	    !D2RL::HasLifecycleServiceField(lifecycle, D2RL::LifecycleServiceRequiredSize)) {
		plog::Warn("D2RLoader has no Lifecycle service of the expected version: the check-to-use state is not reset on leaving a game");
		if (part.address != nullptr) {
			part.status.on = false;
			part.status.detail = "off: D2RLoader has no Lifecycle service of the expected version";
		}
		return;
	}
	const auto listen = [&](D2RL::Lifecycle::GameplayEventKind kind, D2RL::Lifecycle::ListenerHandle* handle) {
		D2RL::Lifecycle::GameplayEventListener listener{};
		listener.structSize = D2RL::Lifecycle::GameplayEventListenerSize;
		listener.kind = kind;
		listener.callback = &OnGameplayEvent;
		*handle = D2RL::Lifecycle::InvalidHandle;
		return lifecycle->registerGameplayEventListener(context, &listener, handle) == D2RL::Lifecycle::Result::Success;
	};
	D2RL::Lifecycle::ListenerHandle leftHandle = D2RL::Lifecycle::InvalidHandle;
	if (!listen(D2RL::Lifecycle::GameplayEventKind::GameLeft, &leftHandle)) {
		plog::Warn("D2RLoader refused the GameLeft listener: the check-to-use state is not reset on leaving a game");
	}
	if (part.address == nullptr) {
		return;
	}
	D2RL::Lifecycle::ListenerHandle readyHandle = D2RL::Lifecycle::InvalidHandle;
	if (!listen(D2RL::Lifecycle::GameplayEventKind::LocalPlayerReady, &readyHandle)) {
		part.status.on = false;
		part.status.detail = "off: D2RLoader refused a gameplay event listener";
		return;
	}
	const uint64_t rva = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(part.address) - context->exeBase);
	const bool hooked = context->InstallInlineHook(rva, part.guard, part.hookSize, reinterpret_cast<void*>(&PlayerTickHook), reinterpret_cast<void**>(&g_tickOriginal));
	const bool ok = hooked && g_tickOriginal != nullptr;
	part.status.on = ok;
	part.status.detail = ok ? "found by signature, hooked" : !hooked ? "off: D2RLoader refused the hook" : "off: D2RLoader gave no way back to the game's own function";
	g_consoleLive = ok;
	if (hooked && !ok) {
		// The hook cannot be taken out again and the plugin cannot call the game's mana step without an original.
		plog::Error("part %s: D2RLoader installed the hook but gave no original; the game's player mana step is skipped for every player until the game is restarted; restart it after removing what conflicts", part.name);
	}
	if (!ok) {
		if (lifecycle->unregisterGameplayEventListener != nullptr) {
			(void)lifecycle->unregisterGameplayEventListener(context, readyHandle);  // GameLeft stays
		}
	}
}

auto RequiredPartsOn() noexcept -> bool {
	for (const Part& part : g_parts) {
		if (part.required && !part.status.on) {
			return false;
		}
	}
	return true;
}

}  // namespace

auto RequestOpen(const std::string& name, std::string* mapName) -> OpenStart {
	bool replaced = false;
	{
		std::lock_guard<std::mutex> lock(g_lock);
		const MapDef* map = g_tables != nullptr ? FindMap(g_tables->maps, name) : nullptr;
		if (map == nullptr) {
			return OpenStart::NoSuchMap;
		}
		if (!g_consoleLive.load() || !g_live.load()) {
			return OpenStart::Unavailable;
		}
		if (g_localPlayer.load() == NoPlayer) {
			return OpenStart::NotInGame;
		}
		if (!g_tickSeen.load()) {
			return OpenStart::NoServer;  // a remote client has no server: the tick would never run, and nothing would answer
		}
		if (g_request.Stale(NowMs(), RequestLifeMs)) {  // an old request the hook never reached
			plog::Warn("an earlier open request was never served and is dropped");
			g_request.Clear();
			replaced = true;
		}
		g_request.Post(map->name, NowMs());
		g_requested = true;
		if (mapName != nullptr) {
			*mapName = map->name;
		}
	}
	if (replaced) {
		Tell("Cartographer: your earlier open request was never served and was dropped.");
	}
	return OpenStart::Posted;
}

void SetFirstGameStartNotify(void (*notify)() noexcept) {
	g_firstGameStart = notify;
}

void SetTables(std::shared_ptr<const Tables> tables) {
	SetMazeTables(tables);
	std::lock_guard<std::mutex> lock(g_lock);
	g_tables = std::move(tables);
}

auto RescanPortal() -> bool {
	// The hooks are written at load only, never here. This looks again for the called parts (a part that was missing at
	// load, such as D2RLoader's UnitStat thunk, may be there now) and turns the portal on when every required part is.
	if (g_context == nullptr || g_live.load()) {
		return g_live.load();
	}
	if (!g_hooksIn) {
		plog::Warn("the item hooks were not installed at load: restart the game after fixing the parts that are off");
		return false;
	}
	CodeRange ranges[8];
	const int count = CodeRanges(g_context->exeBase, ranges, 8);
	for (Part& part : g_parts) {
		if (part.hookSize == 0) {  // the called parts only; a hooked part is settled at load
			ResolvePart(g_context, g_overrides, ranges, count, part);
		}
	}
	g_live = RequiredPartsOn();
	for (const Part& part : g_parts) {
		plog::Info("part %s: %s", part.name, part.status.detail.c_str());
	}
	return g_live.load();
}

auto PortalLive() -> bool {
	return g_live.load();
}

auto NeedsRestart() -> bool {
	return g_installed && !g_hooksIn;  // not when loading threw before the parts were looked at
}

auto PartStatuses() -> std::vector<PartStatus> {
	std::vector<PartStatus> out;
	for (const Part& part : g_parts) {
		out.push_back(part.status);
	}
	for (PartStatus& status : MazePartStatuses()) {
		out.push_back(std::move(status));
	}
	return out;
}

auto InstallPortal(const D2RL::PluginContext* context, const std::vector<SignatureOverride>& overrides, std::vector<PartStatus>* parts) -> bool {
	g_context = context;
	g_overrides = overrides;
	CodeRange ranges[8];
	const int count = CodeRanges(context->exeBase, ranges, 8);
	for (Part& part : g_parts) {
		ResolvePart(context, overrides, ranges, count, part);
	}
	// Both item hooks go in now, once, whenever the two hooked parts were found: code is never written later. They pass
	// straight through to the game until g_live, which needs every required part. The use hook goes in first.
	const auto install = [&](PartId id, void* target, void** original) {
		Part& part = g_parts[id];
		if (!part.status.on) {
			return false;
		}
		const uint64_t rva = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(part.address) - context->exeBase);
		const bool hooked = context->InstallInlineHook(rva, part.guard, part.hookSize, target, original);
		const bool ok = hooked && *original != nullptr;
		part.status.on = ok;
		part.status.detail = ok ? "found by signature, hooked" : !hooked ? "off: D2RLoader refused the hook" : "off: D2RLoader gave no way back to the game's own function";
		if (hooked && !ok) {
			// The hook cannot be taken out again, so the portal can never go live in this session and every item-spell use (or
			// check) the hook sees is dropped, not only the map items; restart the game.
			plog::Error("part %s: D2RLoader installed the hook but gave no original; the portal stays off for this session and every item-spell use is dropped (not only map items) until the game is restarted; restart it after removing what conflicts", part.name);
		}
		return ok;
	};
	g_hooksIn = install(ItemSpellUse, reinterpret_cast<void*>(&SpellUseHook), reinterpret_cast<void**>(&g_useOriginal));
	if (g_hooksIn) {
		g_hooksIn = install(ItemSpellCheck, reinterpret_cast<void*>(&SpellCheckHook), reinterpret_cast<void**>(&g_checkOriginal));
	} else {
		Part& check = g_parts[ItemSpellCheck];
		if (check.status.on) {
			check.status.on = false;  // found, but never tried
			check.status.detail = "off: not hooked, because the ItemSpellUse hook did not go in";
		}
	}
	g_live = g_hooksIn && RequiredPartsOn();
	InstallConsolePath(context);
	InstallMazeHooks(context, overrides, ranges, count);  // after the portal's: a maze part that is missing turns off only Cave and Crypt
	if (!g_live) {
		plog::Warn("the portal is off: not every part it needs was found (see 'cartographer check')");
	}
	if (parts != nullptr) {
		*parts = PartStatuses();
	}
	for (const Part& part : g_parts) {
		plog::Info("part %s: %s", part.name, part.status.detail.c_str());
	}
	for (const PartStatus& status : MazePartStatuses()) {
		plog::Info("part %s: %s", status.name.c_str(), status.detail.c_str());
	}
	g_installed = true;
	return g_live;
}

}  // namespace carto
