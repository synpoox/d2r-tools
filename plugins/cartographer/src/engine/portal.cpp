#include "portal.h"

#include "core/part_signatures.h"
#include "core/request.h"
#include "core/sig.h"
#include "core/text.h"
#include "log.h"

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

template <typename T>
auto At(const void* base, int offset) noexcept -> T {
	T value{};
	std::memcpy(&value, static_cast<const uint8_t*>(base) + offset, sizeof(T));
	return value;
}

// One part: a function (or a hooked entry) found by signature. Signatures are anchored at the function's first byte.
// The signatures are in core/part_signatures.h (build 93847); the part table in docs/reverse-engineering/cartographer.md
// records each one, and signatures.txt can replace them without a rebuild.
struct Part {
	const char* name;
	const char* signature;  // nullptr: none; only a signatures.txt line can then turn the part on
	uint32_t hookSize;      // entry bytes copied for an inline hook; 0: only called
	bool required;
	const uint8_t* address;
	PartStatus status;
	uint8_t guard[32];  // the signature's own first hookSize bytes: what D2RLoader re-checks at the entry
};

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
	{"PlayerTick",
	 "48 89 5C 24 10 48 89 6C 24 18 56 57 41 57 48 83 EC 20 48 8B FA 4C 8B F9 48 85 D2 0F 84 ?? ?? ?? ?? 45 33 C0 48 8B CF 41 8D 50 08 E8 ?? ?? ?? ?? "
	 "48 8B CF 8B F0 E8 ?? ?? ?? ?? 33 DB 48 8B CF 8B E8 8D 53 55 E8 ?? ?? ?? ?? 85 C0 0F 85 ?? ?? ?? ??",
	 14, false, nullptr, {}, {}},
};

std::mutex g_lock;
std::shared_ptr<const Tables> g_tables;
std::string g_armed;
const MapDef* g_pending = nullptr;  // chosen at the check, used at the use; kept alive by g_pendingTables
std::shared_ptr<const Tables> g_pendingTables;
const void* g_pendingGame = nullptr;    // the game, player and item the check passed for: the use must be the same
const void* g_pendingPlayer = nullptr;
const void* g_pendingItem = nullptr;
const void* g_rollGame = nullptr;       // the game the map roll was seeded for
std::atomic<bool> g_live{false};
std::atomic<uint32_t> g_roll{0x9e3779b9u};
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
std::atomic<bool> g_firstLogged{false};         // the first player the tick saw this game has been logged

auto NowMs() noexcept -> uint64_t {
	return GetTickCount64();
}

template <typename Fn>
auto Call(PartId id) noexcept -> Fn {
	return reinterpret_cast<Fn>(const_cast<uint8_t*>(g_parts[id].address));
}

struct CodeRange {
	const uint8_t* start;
	size_t size;
};

auto CodeRanges(uintptr_t exeBase, CodeRange* out, int max) noexcept -> int {
	const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(exeBase);
	if (exeBase == 0 || dos->e_magic != IMAGE_DOS_SIGNATURE) {
		return 0;
	}
	const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(exeBase + static_cast<uintptr_t>(dos->e_lfanew));
	if (nt->Signature != IMAGE_NT_SIGNATURE) {
		return 0;
	}
	const IMAGE_SECTION_HEADER* section = IMAGE_FIRST_SECTION(nt);
	int n = 0;
	for (WORD i = 0; i < nt->FileHeader.NumberOfSections && n < max; ++i, ++section) {
		if ((section->Characteristics & IMAGE_SCN_MEM_EXECUTE) != 0 && section->Misc.VirtualSize > 0) {
			out[n++] = {reinterpret_cast<const uint8_t*>(exeBase + section->VirtualAddress), section->Misc.VirtualSize};
		}
	}
	return n;
}

// The one place `sig` matches in the game's code. Fails (with the reason) when it matches nowhere or more than once.
auto FindUnique(const CodeRange* ranges, int count, const char* sig, Pattern& pattern, std::string* why) noexcept -> const uint8_t* {
	if (!ParsePattern(sig, &pattern)) {
		*why = "the signature is not valid (hex bytes and ?? separated by spaces, not starting with ??)";
		return nullptr;
	}
	const uint8_t* found = nullptr;
	int matches = 0;
	for (int i = 0; i < count && matches < 2; ++i) {
		size_t at = 0;
		const int n = FindPattern(ranges[i].start, ranges[i].size, pattern, &at);
		if (n > 0 && matches == 0) {
			found = ranges[i].start + at;
		}
		matches += n;
	}
	if (matches != 1) {
		*why = matches == 0 ? "the signature matches nowhere in this game's code" : "the signature matches more than once in this game's code";
		return nullptr;
	}
	return found;
}

// True when `address` is the first byte of a function in the exe's .pdata. *known: whether that could be checked.
auto IsFunctionEntry(uintptr_t exeBase, const uint8_t* address, bool* known) noexcept -> bool {
	*known = false;
	const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(exeBase);
	if (exeBase == 0 || dos->e_magic != IMAGE_DOS_SIGNATURE) {
		return false;
	}
	const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(exeBase + static_cast<uintptr_t>(dos->e_lfanew));
	const IMAGE_DATA_DIRECTORY& dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXCEPTION];
	if (dir.VirtualAddress == 0 || dir.Size < sizeof(RUNTIME_FUNCTION)) {
		return false;
	}
	*known = true;
	const auto* table = reinterpret_cast<const RUNTIME_FUNCTION*>(exeBase + dir.VirtualAddress);
	const size_t n = dir.Size / sizeof(RUNTIME_FUNCTION);
	const auto rva = static_cast<DWORD>(reinterpret_cast<uintptr_t>(address) - exeBase);
	size_t lo = 0, hi = n;
	while (lo < hi) {
		const size_t mid = lo + (hi - lo) / 2;
		if (table[mid].BeginAddress == rva) {
			return true;
		}
		if (table[mid].BeginAddress < rva) {
			lo = mid + 1;
		} else {
			hi = mid;
		}
	}
	return false;
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

// The map this use opens: the armed one, else the one an item of this code opens.
auto PickMap(const Tables& tables, const std::string& code, const std::string& armed, const void* game) noexcept -> const MapDef* {
	if (!armed.empty()) {
		return FindMap(tables.maps, armed);
	}
	uint32_t roll = g_roll.load();
	if (g_rollGame != game) {  // a new game: a new sequence of maps
		LARGE_INTEGER counter{};
		QueryPerformanceCounter(&counter);
		roll = static_cast<uint32_t>(counter.QuadPart) ^ static_cast<uint32_t>(GetTickCount64()) ^ static_cast<uint32_t>(reinterpret_cast<uintptr_t>(game) >> 4);
		roll = roll == 0 ? 0x9e3779b9u : roll;
		g_rollGame = game;
	}
	roll ^= roll << 13;
	roll ^= roll >> 17;
	roll ^= roll << 5;
	g_roll.store(roll);
	return ChooseMap(tables.maps, code, roll);
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
	std::string armed;
	{
		std::lock_guard<std::mutex> lock(g_lock);
		tables = g_tables;
		armed = g_armed;
	}
	{
		// Any earlier pass is stale from here on: only a check that passes below sets it again.
		std::lock_guard<std::mutex> lock(g_lock);
		g_pending = nullptr;
		g_pendingTables.reset();
	}
	const MapDef* map = tables != nullptr ? PickMap(*tables, code, armed, game) : nullptr;
	const int level = tables != nullptr ? SlotLevel(tables->slots, PresetSlot) : -1;
	const auto refuse = [&](const std::string& text) -> uint64_t {
		Tell(text);
		Call<ClientMessageFn>(ClientMessage)(player, CantDoThat, player);
		return 0;
	};
	if (map == nullptr) {
		return refuse("Cartographer: no map is set up for this item. Run 'cartographer check'.");
	}
	if (level <= 0) {
		return refuse("Cartographer: the map slot is not installed. Run tools\\install.ps1 and restart the game.");
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
	if (game != nullptr && player != nullptr && item != nullptr) {
		(void)CodeOf(game, item, &record);
	}
	if (record == nullptr || At<int32_t>(record, RecordSpellOffset) != MapSpell) {
		g_useOriginal(game, player, item, target);
		return;
	}
	std::shared_ptr<const Tables> tables;
	const MapDef* map = nullptr;
	{
		std::lock_guard<std::mutex> lock(g_lock);
		tables = g_pendingTables;
		if (g_pendingGame == game && g_pendingPlayer == player && g_pendingItem == item) {
			map = g_pending;
		}
		g_pending = nullptr;
		g_pendingTables.reset();
		g_pendingGame = g_pendingPlayer = g_pendingItem = nullptr;
		g_armed.clear();
	}
	if (map == nullptr || tables == nullptr) {
		return;  // the check refused (or did not run, or ran for another game, player or item): nothing opens
	}
	const int level = SlotLevel(tables->slots, PresetSlot);
	plog::Info("opening '%s' (%s) through level %d", map->name.c_str(), map->tileset.c_str(), level);
	const int32_t opened = Call<OpenRedPortalFn>(OpenRedPortal)(game, player, OpenKey, level, 1);
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
	const int level = SlotLevel(tables->slots, PresetSlot);
	if (level <= 0) {
		Tell("Cartographer: the map slot is not installed. Run tools\\install.ps1 and restart the game.");
		return;
	}
	const GateState state = GateStateOf(game, unit);
	const Gate gate = CheckGates(*map, state);
	if (gate != Gate::Open) {
		Tell(RefusalText(gate, *map, state));
		return;
	}
	plog::Info("opening '%s' (%s) through level %d from the console", map->name.c_str(), map->tileset.c_str(), level);
	const int32_t opened = Call<OpenRedPortalFn>(OpenRedPortal)(game, unit, OpenKey, level, 1);
	Tell(opened != 0 ? "Cartographer: " + map->name + " is open." : "Cartographer: the game would not open a portal for " + map->name + ".");
}

auto PlayerTickHook(Game* game, Unit* unit) noexcept -> uint64_t {
	const uint64_t result = g_tickOriginal(game, unit);
	if (!g_tickSeen.load(std::memory_order_relaxed)) {
		g_tickSeen = true;
	}
	if (!g_firstLogged.load(std::memory_order_relaxed) && unit != nullptr && At<int32_t>(unit, UnitTypeOffset) == 0) {
		g_firstLogged = true;  // once per game, to settle whether LocalPlayerReady.playerId is the unit id
		plog::Info("first player unit seen by the tick: id %u (LocalPlayerReady playerId %u)", At<uint32_t>(unit, UnitIdOffset), g_localPlayer.load());
	}
	if (g_requested.load(std::memory_order_relaxed) && g_live.load() && game != nullptr && unit != nullptr) {
		ServeRequest(game, unit);
	}
	return result;
}

void __cdecl OnGameplayEvent(const D2RL::PluginContext*, const D2RL::Lifecycle::GameplayEvent* event, void*) noexcept {
	if (!D2RL::Lifecycle::HasGameplayEventField(event, D2RL::Lifecycle::GameplayEventRequiredSize)) {
		return;
	}
	if (event->kind == D2RL::Lifecycle::GameplayEventKind::LocalPlayerReady) {
		plog::Info("local player ready (unit id %u)", event->playerId);
		g_localPlayer = event->playerId;
	} else if (event->kind == D2RL::Lifecycle::GameplayEventKind::GameLeft) {
		g_localPlayer = NoPlayer;
		g_tickSeen = false;
		g_firstLogged = false;
		std::lock_guard<std::mutex> lock(g_lock);
		g_request.Clear();
		g_requested = false;
	}
}

// Listens for the local player and, when the tick part is on, hooks it. Best effort: failure leaves 'cartographer open'
// arming the map for the next item instead.
void InstallConsolePath(const D2RL::PluginContext* context) noexcept {
	Part& part = g_parts[PlayerTick];
	const D2RL::LifecycleService* lifecycle = nullptr;
	if (part.address == nullptr) {
		return;
	}
	if (context->QueryService(&lifecycle) != D2RL::ServiceQueryResult::Success || lifecycle == nullptr ||
	    !D2RL::HasLifecycleServiceField(lifecycle, D2RL::LifecycleServiceRequiredSize)) {
		part.status.on = false;
		part.status.detail = "off: D2RLoader has no Lifecycle service of the expected version";
		return;
	}
	D2RL::Lifecycle::ListenerHandle handles[2] = {D2RL::Lifecycle::InvalidHandle, D2RL::Lifecycle::InvalidHandle};
	int registered = 0;
	const auto dropListeners = [&]() {
		for (int i = 0; i < registered; ++i) {
			if (lifecycle->unregisterGameplayEventListener != nullptr) {
				(void)lifecycle->unregisterGameplayEventListener(context, handles[i]);
			}
		}
		registered = 0;
	};
	for (const D2RL::Lifecycle::GameplayEventKind kind : {D2RL::Lifecycle::GameplayEventKind::LocalPlayerReady, D2RL::Lifecycle::GameplayEventKind::GameLeft}) {
		D2RL::Lifecycle::GameplayEventListener listener{};
		listener.structSize = D2RL::Lifecycle::GameplayEventListenerSize;
		listener.kind = kind;
		listener.callback = &OnGameplayEvent;
		D2RL::Lifecycle::ListenerHandle handle = D2RL::Lifecycle::InvalidHandle;
		if (lifecycle->registerGameplayEventListener(context, &listener, &handle) != D2RL::Lifecycle::Result::Success) {
			part.status.on = false;
			part.status.detail = "off: D2RLoader refused a gameplay event listener";
			dropListeners();
			return;
		}
		handles[registered++] = handle;
	}
	const uint64_t rva = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(part.address) - context->exeBase);
	const bool ok = context->InstallInlineHook(rva, part.guard, part.hookSize, reinterpret_cast<void*>(&PlayerTickHook), reinterpret_cast<void**>(&g_tickOriginal)) &&
	                g_tickOriginal != nullptr;
	part.status.on = ok;
	part.status.detail = ok ? "found by signature, hooked" : "off: D2RLoader refused the hook";
	g_consoleLive = ok;
	if (!ok) {
		dropListeners();
	}
}

// Finds one part by its signature (or the one from signatures.txt) and checks its guards; sets its address and status.
void ResolvePart(const D2RL::PluginContext* context, const std::vector<SignatureOverride>& overrides, const CodeRange* ranges, int count, Part& part) {
	part.status = {part.name, false, ""};
	const char* signature = part.signature;
	std::string overridden;
	bool isOverride = false;
	for (const SignatureOverride& o : overrides) {
		if (EqualsNoCase(o.part, part.name)) {
			overridden = o.signature;
			signature = overridden.c_str();
			isOverride = true;
		}
	}
	if (signature == nullptr) {
		part.status.detail = "off: no signature recorded for this part yet";
	} else {
		std::string why;
		Pattern pattern{};
		part.address = nullptr;
		if (count == 0) {
			why = "the game image could not be read";
		} else {
			part.address = FindUnique(ranges, count, signature, pattern, &why);
		}
		// A hooked part's signature must pin every byte the hook cuts (they are the guard), and a signature from
		// signatures.txt is untrusted: it must also start at a function.
		if (part.address != nullptr && part.hookSize > 0) {
			bool concrete = pattern.size >= part.hookSize && part.hookSize <= sizeof(part.guard);
			for (uint32_t i = 0; concrete && i < part.hookSize; ++i) {
				concrete = !pattern.wild[i];
			}
			if (!concrete) {
				why = "the signature must give at least " + std::to_string(part.hookSize) + " exact bytes (no ??) at the entry, because the hook cuts them";
				part.address = nullptr;
			} else {
				std::memcpy(part.guard, pattern.bytes, part.hookSize);
			}
		}
		// An override of a hooked part must keep the built-in entry bytes: the hook is written for that prologue.
		if (part.address != nullptr && isOverride && part.hookSize > 0 && part.signature != nullptr) {
			Pattern builtIn{};
			if (!ParsePattern(part.signature, &builtIn) || builtIn.size < part.hookSize ||
			    std::memcmp(builtIn.bytes, pattern.bytes, part.hookSize) != 0) {
				why = "the signature from signatures.txt does not start with the built-in entry bytes this hook is written for";
				part.address = nullptr;
			}
		}
		// Only a hooked part must start at a .pdata function: a called-only part can be a leaf with no .pdata entry
		// (RoomLevelId, 0x2efc10, is one), so it needs a unique match only.
		if (part.address != nullptr && isOverride && part.hookSize > 0) {
			bool known = false;
			if (!IsFunctionEntry(context->exeBase, part.address, &known)) {
				why = known ? "the signature from signatures.txt does not match at the start of a function"
				            : "the game's exception directory is missing, so the signature from signatures.txt cannot be checked";
				part.address = nullptr;
			}
		}
		part.status.on = part.address != nullptr;
		part.status.detail = part.status.on ? (isOverride ? "found by signature (signatures.txt)" : "found by signature") : "off: " + why;
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

auto RequestOpen(const std::string& name) -> OpenStart {
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
	}
	g_request.Post(map->name, NowMs());
	g_requested = true;
	return OpenStart::Posted;
}

void SetTables(std::shared_ptr<const Tables> tables) {
	std::lock_guard<std::mutex> lock(g_lock);
	g_tables = std::move(tables);
}

auto ArmMap(const std::string& name) -> bool {
	std::lock_guard<std::mutex> lock(g_lock);
	const MapDef* map = g_tables != nullptr ? FindMap(g_tables->maps, name) : nullptr;
	if (map == nullptr) {
		return false;
	}
	g_armed = map->name;
	return true;
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
	return !g_hooksIn;
}

auto PartStatuses() -> std::vector<PartStatus> {
	std::vector<PartStatus> out;
	for (const Part& part : g_parts) {
		out.push_back(part.status);
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
		const bool ok = context->InstallInlineHook(rva, part.guard, part.hookSize, target, original) && *original != nullptr;
		part.status.on = ok;
		part.status.detail = ok ? "found by signature, hooked" : "off: D2RLoader refused the hook";
		return ok;
	};
	g_hooksIn = install(ItemSpellUse, reinterpret_cast<void*>(&SpellUseHook), reinterpret_cast<void**>(&g_useOriginal)) &&
	            install(ItemSpellCheck, reinterpret_cast<void*>(&SpellCheckHook), reinterpret_cast<void**>(&g_checkOriginal));
	g_live = g_hooksIn && RequiredPartsOn();
	InstallConsolePath(context);
	if (!g_live) {
		plog::Warn("the portal is off: not every part it needs was found (see 'cartographer check')");
	}
	if (parts != nullptr) {
		*parts = PartStatuses();
	}
	for (const Part& part : g_parts) {
		plog::Info("part %s: %s", part.name, part.status.detail.c_str());
	}
	return g_live;
}

}  // namespace carto
