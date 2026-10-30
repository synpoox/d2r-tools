// Quick NPC engine part (quick_npc.h). D2R 3.3.0 build 93847 under D2RLoader 1.3.1 (image 0x6ab3782c) only.
//
// Evidence for every address below: docs/reverse-engineering/quick-npc.md (repository root).
// D2R's own path when the player left-clicks an NPC (client side, checked by tools/verify_dump.py):
//   0xfe3b0 player action (left press: flags 1|4) -> 0x102490 (D2Client Unit/Player.cpp) -> 0x101910 interact(cmd, 1, id):
//       in range: CCMD_GET_QUEST_DIALOG (0x2f); out of range: walk, and send it on arrival (player path +0x1c8..+0x1d4)
//   server answers SCMD_INTERACTION_ENABLE (0x28) -> 0x12b030 -> 0x10e6e0 -> 0x1141d0 opens the NPC menu
//       (also reached from 0x10da20 after an NPC speech)
//   the player clicks an option -> the menu (0x2bb5e1) calls the row's handler(dataFlag), e.g. Trade 0x112f30: closes the
//       menu, sends CCMD_SUBSCRIBE_TO_VENDOR (0x38) mode 1
//
// This part changes none of that. It watches two points and, for one interaction the player asked for, presses one
// menu option for them:
//   1. hook 0x101910 (pass-through): a left press on a monster-type unit with an NPC menu row while the hotkey
//      (quick-npc.toml, default Left Ctrl) is held -> remember that unit. Any other interaction click forgets it.
//   2. hook 0x1141d0 (pass-through, after the original): the remembered NPC's menu is open -> queue a UI-thread
//      callback (ThreadService), which reads the options the menu shows and calls the first enabled one, in the order
//      Trade, Identify (Deckard Cain), Resurrect (dead mercenary), Travel, the way the menu calls it on a click.
// Movement, range, quest and server-side vendor checks stay D2R's: nothing here sends a packet or opens a panel.
// The decisions live in quick_npc_logic.cpp (host-tested); this file reads game memory and talks to D2RLoader.
#include "quick_npc.h"

#include <D2RLPlugin/mutations.h>
#include <D2RLPlugin/threads.h>

#include "log.h"
#include "quick_npc_config.h"
#include "quick_npc_logic.h"

#define NOMINMAX
#include <windows.h>

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <initializer_list>

namespace {

// --- Build 93847 addresses (RVAs) -----------------------------------------------------------------------------------
// Client "interact with unit" (D2Client Unit/Player.cpp): (PlayerCommand* cmd, int unitType, uint32 unitId); the result
// is not used by its only caller (0x102490). PUSH RBX (40 53) / PUSH RBP (55) / PUSH RSI (56) / PUSH RDI (57) /
// PUSH R14 (41 56) / SUB RSP,0x30 (48 83 ec 30) / MOV ESI,R8D (41 8b f0): 14 bytes, 7 whole instructions.
constexpr uint8_t InteractEntry[] = {0x40, 0x53, 0x55, 0x56, 0x57, 0x41, 0x56, 0x48, 0x83, 0xec, 0x30, 0x41, 0x8b, 0xf0};

// Open the NPC menu (D2Client UI/npcmenu.cpp): (Unit* npc, int flag); callers 0x10e6e0 and 0x10da20 ignore the result.
// PUSH RBP (40 55) / PUSH RDI (57) / PUSH R13 (41 55) / PUSH R15 (41 57) / MOV RBP,RSP (48 8b ec) /
// SUB RSP,0x48 (48 83 ec 48): 14 bytes, 6 whole instructions.
constexpr uint8_t OpenNpcMenuEntry[] = {0x40, 0x55, 0x57, 0x41, 0x55, 0x41, 0x57, 0x48, 0x8b, 0xec, 0x48, 0x83, 0xec, 0x48};

static_assert(sizeof(InteractEntry) >= 14 && sizeof(OpenNpcMenuEntry) >= 14, "inline hooks need 14 bytes of whole instructions");

// Menu options: bool(u8 dataFlag), called by the menu at 0x2bb5e1 for the NPC in the menu globals below.
constexpr uint64_t RvaTradeHandler = 0x112f30;      // "Trade" / "Trade/Repair": CCMD_SUBSCRIBE_TO_VENDOR (0x38) mode 1
constexpr uint64_t RvaIdentifyHandler = 0x114000;   // Cain's "Identify": CCMD_IDENTIFY_TRANSACTION (0x34), his price
constexpr uint64_t RvaResurrectHandler = 0x113130;  // "Resurrect" (dead mercenary): CCMD_REZZ_MERCENARY_TRANSACTION (0x52)
constexpr uint64_t RvaTravelHandler = 0x1162f0;     // "Go East/West", "Sail East/West": CCMD_TRAVEL (0x37)

// The data-table selector the menu passes (0x2bb058..0x2bb088): of the local player, or of the client game when none.
constexpr uint64_t RvaLocalPlayerId = 0x8b2d0;   // () -> u32
constexpr uint64_t RvaClientUnit = 0x9a480;      // (u32 player id) -> unit or null
constexpr uint64_t RvaUnitDataFlag = 0x34a0e0;   // (unit) -> u8
constexpr uint64_t RvaClientGame = 0x76010;      // () -> client game; u8 dataFlag at +0x9cf
constexpr int ClientGameDataFlag = 0x9cf;

// NPC menu globals (.data).
constexpr uint64_t RvaMenuRows = 0x2a48830;      // MenuRow* (vector data); u64 row count at +8
constexpr uint64_t RvaMenuNpcId = 0x2a4875c;     // u32 unit id of the NPC whose dialog is open
constexpr uint64_t RvaMenuHasNpc = 0x2a48760;    // u32 non-zero while that NPC is set
constexpr uint64_t RvaMenuNpcClass = 0x2a48764;  // u16 its monstats row
constexpr uint64_t RvaNpcUiState = 0x2a486a4;    // u32 0 idle, 1 menu shown, 2 trade/gamble requested
constexpr uint64_t RvaMenuWidget = 0x2a487b0;    // the open menu object, null when none

constexpr uint32_t UiStateMenu = 1;
constexpr uint64_t MaxMenuRows = 1024;  // sanity bound: 48 vanilla rows plus customnpcs.txt rows

// Client player command (built by 0xfe3b0): +0 u32 flags, +8 player, +0x10 target unit.
constexpr int CmdFlags = 0x00;
constexpr int CmdTarget = 0x10;
// Unit: +0 i32 type (0x34b9d0), +4 u32 class (0x349860), +8 u32 unit id (0x34a330).
constexpr int UnitType = 0x00;
constexpr int UnitClass = 0x04;
constexpr int UnitId = 0x08;

using InteractFn = uint64_t (*)(void* command, int32_t unitType, uint32_t unitId) noexcept;
using OpenNpcMenuFn = uint64_t (*)(void* npc, int32_t flag) noexcept;
using MenuOptionFn = bool (*)(uint8_t dataFlag) noexcept;
using LocalPlayerIdFn = uint32_t (*)() noexcept;
using ClientUnitFn = void* (*)(uint32_t id) noexcept;
using UnitDataFlagFn = uint8_t (*)(void* unit) noexcept;
using ClientGameFn = uint8_t* (*)() noexcept;

// --- State ----------------------------------------------------------------------------------------------------------
const D2RL::PluginContext* g_context = nullptr;
const D2RL::ThreadService* g_threads = nullptr;
const D2RL::MutationService* g_mutations = nullptr;
uint64_t g_base = 0;

// Trampolines to D2R's original functions, published after the hook transaction commits. Both hooks are only reached
// in a game (a click on a unit, an NPC dialog), never during plugin load, so they are set by then. If one ever were
// null the hook skips the call: neither function's result is used by its callers.
std::atomic<InteractFn> g_interactOriginal{nullptr};
std::atomic<OpenNpcMenuFn> g_openMenuOriginal{nullptr};

std::atomic<bool> g_installed{false};
std::atomic<bool> g_enabled{false};  // mirror of the controller's switch, read without the lock on every hook call
std::atomic<uint32_t> g_hotkeyVk{quick_npc::Config{}.hotkeyVk};

SRWLOCK g_lock = SRWLOCK_INIT;
quick_npc::Controller g_controller;  // guarded by g_lock
quick_npc::Config g_config;          // guarded by g_lock

class Lock {
public:
	Lock() noexcept { AcquireSRWLockExclusive(&g_lock); }
	~Lock() { ReleaseSRWLockExclusive(&g_lock); }
	Lock(const Lock&) = delete;
	auto operator=(const Lock&) -> Lock& = delete;
};

template <typename T>
auto At(uint64_t address) noexcept -> T {
	return *reinterpret_cast<const T*>(address);
}

auto TradeHandler() noexcept -> uint64_t {
	return g_base + RvaTradeHandler;
}

// The options this plugin may press, as switched in quick-npc.toml. Caller holds g_lock.
auto HandlersLocked() noexcept -> quick_npc::Handlers {
	using quick_npc::Action;
	quick_npc::Handlers h;
	h[Action::Trade] = g_config.trade ? TradeHandler() : 0;
	h[Action::Identify] = g_config.cainIdentify ? g_base + RvaIdentifyHandler : 0;
	h[Action::Resurrect] = g_config.mercenary ? g_base + RvaResurrectHandler : 0;
	h[Action::Travel] = g_config.travel ? g_base + RvaTravelHandler : 0;
	return h;
}

auto HandlerRva(quick_npc::Action a) noexcept -> uint64_t {
	switch (a) {
	case quick_npc::Action::Trade: return RvaTradeHandler;
	case quick_npc::Action::Identify: return RvaIdentifyHandler;
	case quick_npc::Action::Resurrect: return RvaResurrectHandler;
	case quick_npc::Action::Travel: return RvaTravelHandler;
	case quick_npc::Action::None: break;
	}
	return 0;
}

auto ActionName(quick_npc::Action a) noexcept -> const char* {
	static const char* const names[quick_npc::ActionCount] = {"none", "trade", "identify", "resurrect", "travel"};
	return names[static_cast<int>(a)];
}

// The u8 the menu passes to every option (0x2bb058..0x2bb088).
auto DataFlag() noexcept -> uint8_t {
	const uint32_t id = reinterpret_cast<LocalPlayerIdFn>(g_base + RvaLocalPlayerId)();
	if (void* player = reinterpret_cast<ClientUnitFn>(g_base + RvaClientUnit)(id)) {
		return reinterpret_cast<UnitDataFlagFn>(g_base + RvaUnitDataFlag)(player);
	}
	const uint8_t* game = reinterpret_cast<ClientGameFn>(g_base + RvaClientGame)();
	return game != nullptr ? game[ClientGameDataFlag] : 0;
}

auto MenuRows(size_t* count) noexcept -> const quick_npc::MenuRow* {
	const auto* rows = At<const quick_npc::MenuRow*>(g_base + RvaMenuRows);
	const uint64_t n = At<uint64_t>(g_base + RvaMenuRows + 8);
	if (rows == nullptr || n == 0 || n > MaxMenuRows) {
		*count = 0;
		return nullptr;
	}
	*count = static_cast<size_t>(n);
	return rows;
}

auto ReadMenu(bool withOffers, const quick_npc::Handlers& handlers) noexcept -> quick_npc::MenuState {
	quick_npc::MenuState menu;
	menu.npcId = At<uint32_t>(g_base + RvaMenuNpcId);
	menu.shown = At<uint32_t>(g_base + RvaMenuHasNpc) != 0 && At<uint32_t>(g_base + RvaNpcUiState) == UiStateMenu &&
	             At<uint64_t>(g_base + RvaMenuWidget) != 0;
	if (withOffers && menu.shown) {
		size_t count = 0;
		const quick_npc::MenuRow* rows = MenuRows(&count);
		const int row = quick_npc::FindMenuRow(rows, count, At<uint16_t>(g_base + RvaMenuNpcClass));
		if (row >= 0) {
			quick_npc::ReadOffers(rows[row], handlers, &menu);  // the live row after the menu was built
		}
	}
	return menu;
}

// The configured key (quick-npc.toml "hotkey", default Left Ctrl) is down right now. GetAsyncKeyState tells left and
// right Ctrl apart; the click itself arrives through D2R's own command flags (left press), not from here.
auto HotkeyHeld() noexcept -> bool {
	const uint32_t vk = g_hotkeyVk.load();
	return vk != 0 && (GetAsyncKeyState(static_cast<int>(vk)) & 0x8000) != 0;
}

// --- 1. The click -----------------------------------------------------------------------------------------------------
auto InteractHook(void* command, int32_t unitType, uint32_t unitId) noexcept -> uint64_t {
	if (g_enabled.load()) {
		quick_npc::ClickInfo click;
		click.unitType = unitType;
		click.unitId = unitId;
		const auto cmd = reinterpret_cast<uint64_t>(command);
		if (cmd != 0) {
			click.flags = At<uint32_t>(cmd + CmdFlags);
			const uint64_t target = At<uint64_t>(cmd + CmdTarget);  // 0x102490 has just used it
			click.hasTarget = target != 0;
			if (target != 0) {
				click.targetType = At<int32_t>(target + UnitType);
				click.targetClass = At<uint32_t>(target + UnitClass);
				click.targetId = At<uint32_t>(target + UnitId);
			}
		}
		// Only read the key state for the clicks that can need it.
		const bool hotkey = unitType == quick_npc::UnitTypeMonster && quick_npc::IsLeftPress(click.flags) && HotkeyHeld();
		size_t rows = 0;
		const quick_npc::MenuRow* table = MenuRows(&rows);
		Lock lock;
		g_controller.OnInteract(click, hotkey, table, rows, GetTickCount64());
	}
	const InteractFn original = g_interactOriginal.load(std::memory_order_acquire);
	return original != nullptr ? original(command, unitType, unitId) : 0;
}

// --- 2. The dialog ----------------------------------------------------------------------------------------------------
void __cdecl PressTrade(const D2RL::PluginContext* context, void* userData) noexcept {
	(void)context;
	(void)userData;
	quick_npc::Handlers handlers;
	{
		Lock lock;
		handlers = HandlersLocked();
	}
	const quick_npc::MenuState menu = ReadMenu(true, handlers);
	quick_npc::Action action = quick_npc::Action::None;
	uint32_t count = 0;
	{
		Lock lock;
		action = g_controller.OnQueuedCallback(menu, handlers);
		count = g_controller.counters().pressed[static_cast<int>(action)];
	}
	if (action == quick_npc::Action::None) {
		return;
	}
	const auto option = reinterpret_cast<MenuOptionFn>(g_base + HandlerRva(action));
	option(DataFlag());  // what D2R's menu runs when the player clicks that option
	if (count == 1) {
		plog::Info("quicknpc: first %s (npc %u)", ActionName(action), menu.npcId);
	}
}

auto OpenNpcMenuHook(void* npc, int32_t flag) noexcept -> uint64_t {
	const OpenNpcMenuFn original = g_openMenuOriginal.load(std::memory_order_acquire);
	if (original == nullptr) {
		return 0;
	}
	const uint64_t result = original(npc, flag);
	if (g_enabled.load()) {
		const quick_npc::MenuState menu = ReadMenu(false, quick_npc::Handlers{});
		bool queue = false;
		{
			Lock lock;
			queue = g_controller.OnMenuOpened(menu, GetTickCount64());
		}
		// Not from inside this call: the packet handler that opened the menu still writes the menu globals after it
		// returns (0x10efac). A UI-thread callback runs where a player's click on "Trade" would.
		if (queue && g_threads->runOnUiThread(g_context, PressTrade, nullptr) != D2RL::Threads::Result::Success) {
			uint32_t failed = 0;
			{
				Lock lock;
				g_controller.OnQueueFailed();
				failed = g_controller.counters().queueFailed;
			}
			if (failed == 1 || failed == 100 || failed == 10000) {
				plog::Warn("quicknpc: could not queue the trade callback (%u so far)", failed);
			}
		}
	}
	return result;
}

// --- Install steps (quick_npc::RunInstall) ----------------------------------------------------------------------------
auto ServicesReady(void* user, char* why, size_t whySize) -> bool {
	const auto* context = static_cast<const D2RL::PluginContext*>(user);
	if (context->QueryService(&g_threads) != D2RL::ServiceQueryResult::Success ||
	    !D2RL::HasThreadServiceField(g_threads, D2RL::ThreadServiceRequiredSize) || g_threads->runOnUiThread == nullptr) {
		g_threads = nullptr;
		std::snprintf(why, whySize, "D2RLoader has no thread service");
		return false;
	}
	if (context->QueryService(&g_mutations) != D2RL::ServiceQueryResult::Success || g_mutations == nullptr ||
	    g_mutations->serviceVersion != D2RL::MutationService::AbiVersion ||
	    g_mutations->serviceSize < D2RL::MutationServiceRequiredSize) {
		g_mutations = nullptr;
		std::snprintf(why, whySize, "D2RLoader has no mutation service");
		return false;
	}
	return true;
}

auto AlreadyHooked(void* user, uint64_t rva) -> bool {
	(void)user;
	const auto* entry = reinterpret_cast<const uint8_t*>(g_base + rva);
	return entry[0] == 0xff && entry[1] == 0x25;  // jmp [rip+x]: D2RLoader or another plugin is there
}

// Both hooks in one transaction: commit checks both entries' bytes, overlaps and call-through pointers, then applies
// both or neither. D2RLoader removes them when the plugin unloads (PluginSDK README, "Atomic Patch Transactions").
auto InstallHooks(void* user, char* why, size_t whySize) -> bool {
	using namespace D2RL::Mutations;
	const auto* context = static_cast<const D2RL::PluginContext*>(user);
	TransactionHandle transaction = InvalidTransactionHandle;
	if (g_mutations->beginTransaction(context, &transaction) != Result::Success) {
		std::snprintf(why, whySize, "no transaction");
		return false;
	}
	const InlineHookRequest requests[2] = {
		{.structSize = InlineHookRequestSize, .flags = 0, .rva = quick_npc::RvaInteract, .expected = InteractEntry,
		 .expectedSize = sizeof(InteractEntry), .reserved = 0, .target = reinterpret_cast<void*>(&InteractHook), .reserved2 = 0},
		{.structSize = InlineHookRequestSize, .flags = 0, .rva = quick_npc::RvaOpenNpcMenu, .expected = OpenNpcMenuEntry,
		 .expectedSize = sizeof(OpenNpcMenuEntry), .reserved = 0, .target = reinterpret_cast<void*>(&OpenNpcMenuHook), .reserved2 = 0},
	};
	OperationHandle operations[2] = {InvalidOperationHandle, InvalidOperationHandle};
	for (int i = 0; i < 2; ++i) {
		const Result staged = g_mutations->stageInlineHook(context, transaction, &requests[i], &operations[i]);
		if (staged != Result::Success) {
			g_mutations->cancelTransaction(context, transaction);
			std::snprintf(why, whySize, "staging 0x%llx failed (result %u)", static_cast<unsigned long long>(requests[i].rva),
			              static_cast<unsigned>(staged));
			return false;
		}
	}
	CommitResult commit {.structSize = CommitResultSize, .flags = CommitFlags::None, .result = Result::Success,
	                     .failureReason = FailureReason::None, .operation = InvalidOperationHandle};
	if (g_mutations->commit(context, transaction, &commit) != Result::Success) {
		g_mutations->cancelTransaction(context, transaction);  // also retries a failed rollback
		std::snprintf(why, whySize, "commit refused (result %u, reason %u)", static_cast<unsigned>(commit.result),
		              static_cast<unsigned>(commit.failureReason));
		return false;
	}
	void* originals[2] = {nullptr, nullptr};
	for (int i = 0; i < 2; ++i) {
		if (g_mutations->getInlineHookOriginal(context, transaction, operations[i], &originals[i]) != Result::Success) {
			originals[i] = nullptr;
		}
	}
	g_interactOriginal.store(reinterpret_cast<InteractFn>(originals[0]), std::memory_order_release);
	g_openMenuOriginal.store(reinterpret_cast<OpenNpcMenuFn>(originals[1]), std::memory_order_release);
	if (originals[0] == nullptr || originals[1] == nullptr) {
		std::snprintf(why, whySize, "no call-through pointer after commit");
		plog::Error("quicknpc: hooks committed but D2RLoader gave no original pointer; the hooks skip the call until restart");
		return false;
	}
	return true;
}

// Reads quick-npc.toml (D2RLoader created it from the embedded default if it was missing) and logs what it found.
auto ReadConfigFile(quick_npc::Config* config) noexcept -> bool {
	static char text[64 * 1024];
	std::memset(text, 0, sizeof(text));  // reused on reload: no tail of a longer earlier file
	uint32_t required = 0;
	if (g_context == nullptr || !g_context->ReadConfig(text, sizeof(text) - 1, &required)) {
		plog::Warn("config: could not read %ls (needs %u bytes): defaults used",
		           g_context != nullptr && g_context->pluginConfigPath != nullptr ? g_context->pluginConfigPath : L"?", required);
		return false;
	}
	size_t length = 0;
	const size_t limit = required != 0 && required < sizeof(text) - 1 ? required : sizeof(text) - 1;
	while (length < limit && text[length] != '\0') {
		++length;
	}
	quick_npc::ConfigIssue issues[8];
	const int n = quick_npc::ParseConfig(text, length, config, issues, 8);
	for (int i = 0; i < n && i < 8; ++i) {
		plog::Warn("config line %d: %s", issues[i].line, issues[i].text);
	}
	if (n > 8) {
		plog::Warn("config: %d more problems", n - 8);
	}
	return true;
}

void ApplyConfig(const quick_npc::Config& config, bool setEnabled) noexcept {
	{
		Lock lock;
		g_config = config;
		g_controller.SetTimeout(config.timeoutMs);
		if (setEnabled) {
			g_controller.SetEnabled(config.enabled);
		}
	}
	g_hotkeyVk = config.hotkeyVk;
	if (setEnabled) {
		g_enabled = config.enabled;
	}
	plog::Info("config: hotkey %s (vk 0x%02x), timeout %u s, trade %d, cain_identify %d, mercenary %d, travel %d, %s",
	           config.hotkeyName, config.hotkeyVk, config.timeoutMs / 1000, config.trade, config.cainIdentify, config.mercenary,
	           config.travel, config.enabled ? "enabled" : "disabled");
	if (const char* note = quick_npc::HotkeyNote(config.hotkeyVk)) {
		if (quick_npc::HotkeyBlocksInteraction(config.hotkeyVk)) {
			plog::Warn("config: %s", note);
		} else {
			plog::Info("config: %s", note);
		}
	}
}

}  // namespace

auto InstallQuickNpc(const D2RL::PluginContext* context, char* report, size_t reportSize) noexcept -> bool {
	g_context = context;
	g_base = context->exeBase;
	quick_npc::InstallSteps steps;
	steps.user = const_cast<D2RL::PluginContext*>(context);
	steps.servicesReady = [](void* u, char* w, size_t n) { return ServicesReady(u, w, n); };
	steps.alreadyHooked = [](void* u, uint64_t rva) { return AlreadyHooked(u, rva); };
	steps.installHooks = [](void* u, char* w, size_t n) { return InstallHooks(u, w, n); };
	char result[300];
	if (quick_npc::RunInstall(steps, result, sizeof(result)) != quick_npc::InstallResult::On) {
		std::snprintf(report, reportSize, "%s", result);
		return false;
	}
	g_installed = true;
	quick_npc::Config config;
	(void)ReadConfigFile(&config);
	ApplyConfig(config, true);
	size_t rows = 0;
	(void)MenuRows(&rows);
	std::snprintf(report, reportSize, "%s, %zu NPC menu rows, hotkey %s", result, rows, config.hotkeyName);
	return true;
}

void QuickNpcStatus(char* out, size_t size) noexcept {
	if (!g_installed.load()) {
		std::snprintf(out, size, "not installed");
		return;
	}
	quick_npc::Counters c;
	quick_npc::Config config;
	bool enabled = false;
	{
		Lock lock;
		c = g_controller.counters();
		config = g_config;
		enabled = g_controller.enabled();
	}
	std::snprintf(out, size,
	              "%s, hotkey %s (vk 0x%02x), timeout %u s, trade %s, cain identify %s, mercenary %s, travel %s | hotkey clicks on "
	              "NPCs %u (no menu %u) | queued %u: traded %u, identified %u, resurrected %u, travelled %u, left open %u, "
	              "queue failed %u",
	              enabled ? "enabled" : "disabled (quick-npc on)", config.hotkeyName, config.hotkeyVk, config.timeoutMs / 1000,
	              config.trade ? "on" : "off", config.cainIdentify ? "on" : "off", config.mercenary ? "on" : "off",
	              config.travel ? "on" : "off", c.npcClicks, c.noMenu, c.queued, c.pressed[1], c.pressed[2], c.pressed[3],
	              c.pressed[4], c.dropped, c.queueFailed);
}

auto ReloadQuickNpcConfig() noexcept -> bool {
	if (!g_installed.load()) {
		return false;
	}
	quick_npc::Config config;
	const bool read = ReadConfigFile(&config);
	ApplyConfig(config, true);
	return read;
}

void SetQuickNpcEnabled(bool enabled) noexcept {
	if (!g_installed.load()) {
		return;
	}
	{
		Lock lock;
		g_controller.SetEnabled(enabled);
	}
	g_enabled = enabled;
}

auto QuickNpcEnabled() noexcept -> bool {
	return g_enabled.load();
}

void ShutdownQuickNpc() noexcept {
	g_enabled = false;
	Lock lock;
	g_controller.SetEnabled(false);
}
