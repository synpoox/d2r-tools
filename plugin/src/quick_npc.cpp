// Quick NPC engine part (quick_npc.h). D2R 3.3.0 build 93847 under D2RLoader 1.3.1 (image 0x6ab3782c) only.
//
// D2R's own path when the player left-clicks an NPC (client side, docs/reverse-engineering.md):
//   0xfe3b0 player action (left press: flags 1|4) -> 0x102490 (D2Client Unit/Player.cpp) -> 0x101910 interact(cmd, 1, id):
//       in range: CCMD_GET_QUEST_DIALOG (0x2f); out of range: walk, and send it on arrival (player path +0x1c8..+0x1d4)
//   server answers SCMD_INTERACTION_ENABLE (0x28) -> 0x12b030 -> 0x10e6e0 -> 0x1141d0 opens the NPC menu
//       (also reached from 0x10da20 after an NPC speech)
//   the player clicks "Trade" -> the row's handler 0x112f30: closes the menu, sends CCMD_SUBSCRIBE_TO_VENDOR (0x38) mode 1
//
// This part changes none of that. It watches two points and, for one interaction the player asked for, presses
// "Trade" for them:
//   1. hook 0x101910 (pass-through): a left press on a monster-type unit while the hotkey is held, and the unit's class
//      offers Trade in D2R's live NPC menu table -> remember that unit. Any other interaction click forgets it.
//   2. hook 0x1141d0 (pass-through, after the original): the remembered NPC's menu is open -> queue a UI-thread
//      callback (ThreadService), which checks the menu again and calls the Trade handler, as a click on "Trade" does.
// Movement, range, quest and server-side vendor checks stay D2R's: nothing here sends a packet or opens a panel.
// The decisions live in quick_npc_logic.cpp (host-tested); this file reads game memory and talks to D2RLoader.
#include "quick_npc.h"

#include <D2RLPlugin/input.h>
#include <D2RLPlugin/mutations.h>
#include <D2RLPlugin/threads.h>

#include "log.h"
#include "quick_npc_logic.h"

#define NOMINMAX
#include <windows.h>

#include <atomic>
#include <cstdint>
#include <cstdio>
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

// The menu's "Trade" option: () -> bool. Uses the menu globals below, closes the menu, sends CCMD 0x38 mode 1.
constexpr uint64_t RvaTradeHandler = 0x112f30;

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
using TradeFn = bool (*)() noexcept;

// --- State ----------------------------------------------------------------------------------------------------------
const D2RL::PluginContext* g_context = nullptr;
const D2RL::InputService* g_input = nullptr;
const D2RL::ThreadService* g_threads = nullptr;
const D2RL::MutationService* g_mutations = nullptr;
D2RL::Input::ActionHandle g_action = D2RL::Input::InvalidHandle;
uint64_t g_base = 0;

// Trampolines to D2R's original functions, published after the hook transaction commits. Both hooks are only reached
// in a game (a click on a unit, an NPC dialog), never during plugin load, so they are set by then. If one ever were
// null the hook skips the call: neither function's result is used by its callers.
std::atomic<InteractFn> g_interactOriginal{nullptr};
std::atomic<OpenNpcMenuFn> g_openMenuOriginal{nullptr};

std::atomic<bool> g_installed{false};
std::atomic<bool> g_enabled{false};  // mirror of the controller's switch, read without the lock on every hook call
std::atomic<bool> g_keyDown{false};

SRWLOCK g_lock = SRWLOCK_INIT;
quick_npc::Controller g_controller;  // guarded by g_lock

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

auto ReadMenu(bool withTrade) noexcept -> quick_npc::MenuState {
	quick_npc::MenuState menu;
	menu.npcId = At<uint32_t>(g_base + RvaMenuNpcId);
	menu.shown = At<uint32_t>(g_base + RvaMenuHasNpc) != 0 && At<uint32_t>(g_base + RvaNpcUiState) == UiStateMenu &&
	             At<uint64_t>(g_base + RvaMenuWidget) != 0;
	if (withTrade && menu.shown) {
		size_t count = 0;
		const quick_npc::MenuRow* rows = MenuRows(&count);
		menu.offersTrade = quick_npc::IsVendorClass(rows, count, At<uint16_t>(g_base + RvaMenuNpcClass), TradeHandler());
	}
	return menu;
}

auto KeyPhysicallyDown(D2RL::Input::Key key) noexcept -> bool {
	return key != D2RL::Input::Key::None && (GetAsyncKeyState(static_cast<int>(key)) & 0x8000) != 0;
}

auto HotkeyHeld() noexcept -> bool {
	if (!g_keyDown.load()) {
		return false;
	}
	bool known = false;
	bool down = false;
	if (g_input != nullptr && g_action != D2RL::Input::InvalidHandle) {
		for (const auto slot : {D2RL::Input::BindingSlot::Primary, D2RL::Input::BindingSlot::Secondary}) {
			D2RL::Input::Binding binding {};
			if (g_input->getBinding(g_context, g_action, slot, &binding) == D2RL::Input::Result::Success &&
			    binding.key != D2RL::Input::Key::None) {
				known = true;
				down = down || KeyPhysicallyDown(binding.key);
			}
		}
	}
	return quick_npc::HotkeyHeld(true, known, down);
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
		g_controller.OnInteract(click, hotkey, table, rows, TradeHandler(), GetTickCount64());
	}
	const InteractFn original = g_interactOriginal.load(std::memory_order_acquire);
	return original != nullptr ? original(command, unitType, unitId) : 0;
}

// --- 2. The dialog ----------------------------------------------------------------------------------------------------
void __cdecl PressTrade(const D2RL::PluginContext* context, void* userData) noexcept {
	(void)context;
	(void)userData;
	const quick_npc::MenuState menu = ReadMenu(true);
	bool go = false;
	uint32_t traded = 0;
	{
		Lock lock;
		go = g_controller.OnQueuedCallback(menu);
		traded = g_controller.counters().traded;
	}
	if (!go) {
		return;
	}
	const auto trade = reinterpret_cast<TradeFn>(TradeHandler());
	trade();  // what D2R's menu runs when the player clicks "Trade"
	if (traded == 1) {
		plog::Info("quicknpc: first trade opened (npc %u)", menu.npcId);
	}
}

auto OpenNpcMenuHook(void* npc, int32_t flag) noexcept -> uint64_t {
	const OpenNpcMenuFn original = g_openMenuOriginal.load(std::memory_order_acquire);
	if (original == nullptr) {
		return 0;
	}
	const uint64_t result = original(npc, flag);
	if (g_enabled.load()) {
		const quick_npc::MenuState menu = ReadMenu(false);
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

// --- Input action -----------------------------------------------------------------------------------------------------
auto __cdecl OnAction(const D2RL::PluginContext* context, const D2RL::Input::ActionEvent* event, void* userData) noexcept
	-> D2RL::Input::ActionResult {
	(void)context;
	(void)userData;
	if (D2RL::Input::HasActionEventField(event, D2RL::Input::ActionEventRequiredSize)) {
		g_keyDown.store(event->kind == D2RL::Input::ActionEventKind::Pressed);
	}
	return D2RL::Input::ActionResult::Ignored;  // never consume the key
}

void DescribeBinding(char* out, size_t size) noexcept {
	D2RL::Input::Binding binding {};
	if (g_input == nullptr || g_action == D2RL::Input::InvalidHandle ||
	    g_input->getBinding(g_context, g_action, D2RL::Input::BindingSlot::Primary, &binding) != D2RL::Input::Result::Success) {
		std::snprintf(out, size, "key ?");
	} else if (binding.key == D2RL::Input::Key::None) {
		std::snprintf(out, size, "no key bound: set one in Options > Controls > Quick NPC");
	} else {
		std::snprintf(out, size, "key vk 0x%02x%s", static_cast<unsigned>(binding.key),
		              binding.modifier != D2RL::Input::Modifier::None ? " + modifier" : "");
	}
}

// --- Install steps (quick_npc::RunInstall) ----------------------------------------------------------------------------
auto ServicesReady(void* user, char* why, size_t whySize) -> bool {
	const auto* context = static_cast<const D2RL::PluginContext*>(user);
	if (context->QueryService(&g_input) != D2RL::ServiceQueryResult::Success ||
	    !D2RL::HasInputServiceField(g_input, D2RL::InputServiceRequiredSize)) {
		g_input = nullptr;
		std::snprintf(why, whySize, "D2RLoader has no input service");
		return false;
	}
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

auto RegisterAction(void* user, char* why, size_t whySize) -> bool {
	const auto* context = static_cast<const D2RL::PluginContext*>(user);
	D2RL::Input::ActionRegistration registration {
		.structSize       = D2RL::Input::ActionRegistrationSize,
		.flags            = 0,
		.logicalId        = "trade-with-npc",
		.displayName      = "Quick NPC: hold, then left-click a vendor to trade",
		.category         = "Quick NPC",
		.defaultPrimary   = {.key = D2RL::Input::Key::Y, .modifier = D2RL::Input::Modifier::None},
		.defaultSecondary = {.key = D2RL::Input::Key::None, .modifier = D2RL::Input::Modifier::None},
		.callback         = OnAction,
		.userData         = nullptr,
	};
	D2RL::Input::Result result = g_input->registerAction(context, &registration, &g_action);
	if (result == D2RL::Input::Result::Conflict) {
		// Y is taken: register without a default; the player binds a key in Options > Controls.
		registration.defaultPrimary = {.key = D2RL::Input::Key::None, .modifier = D2RL::Input::Modifier::None};
		result = g_input->registerAction(context, &registration, &g_action);
	}
	if (result != D2RL::Input::Result::Success || g_action == D2RL::Input::InvalidHandle) {
		std::snprintf(why, whySize, "input action not registered (result %u)", static_cast<unsigned>(result));
		g_action = D2RL::Input::InvalidHandle;
		return false;
	}
	return true;
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
	steps.registerAction = [](void* u, char* w, size_t n) { return RegisterAction(u, w, n); };
	char result[300];
	if (quick_npc::RunInstall(steps, result, sizeof(result)) != quick_npc::InstallResult::On) {
		std::snprintf(report, reportSize, "%s", result);
		return false;
	}
	g_installed = true;
	{
		Lock lock;
		g_controller.SetEnabled(true);
	}
	g_enabled = true;
	size_t rows = 0;
	(void)MenuRows(&rows);
	char key[100];
	DescribeBinding(key, sizeof(key));
	std::snprintf(report, reportSize, "%s, %zu NPC menu rows, %s", result, rows, key);
	return true;
}

void QuickNpcStatus(char* out, size_t size) noexcept {
	if (!g_installed.load()) {
		std::snprintf(out, size, "not installed");
		return;
	}
	quick_npc::Counters c;
	bool enabled = false;
	{
		Lock lock;
		c = g_controller.counters();
		enabled = g_controller.enabled();
	}
	char key[100];
	DescribeBinding(key, sizeof(key));
	std::snprintf(out, size,
	              "%s, %s | hotkey clicks on NPCs %u (not vendors %u, vendors %u) | trade queued %u, opened %u, dropped %u, queue failed %u",
	              enabled ? "enabled" : "disabled (quick-npc on)", key, c.npcClicks, c.notVendor, c.armed, c.queued, c.traded,
	              c.dropped, c.queueFailed);
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
