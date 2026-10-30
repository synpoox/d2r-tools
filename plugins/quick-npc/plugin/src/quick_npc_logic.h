// Quick NPC decision logic: no Windows, no SDK, no game memory. The engine part (quick_npc.cpp) reads the game's state
// and asks these functions what to do; the host tests (tests/logic_tests.cpp) run them on any platform.
//
// Layouts and constants are for D2R 3.3.0 build 93847 under D2RLoader 1.3.1 (image 0x6ab3782c); tools/verify_dump.py
// checks each of them against a dump of the game.
#pragma once

#include <cstddef>
#include <cstdint>

namespace quick_npc {

// --- D2R's NPC menu table (D2Client UI/npcmenu.cpp) -----------------------------------------------------------------
// One row per NPC class: the dialog's options and the function each option runs. The client builds 48 rows at start-up
// (static initializer 0x360d0), adds rows for customnpcs.txt (0x10b6e0), and changes rows while a dialog opens: quest
// options (Warriv's and Meshif's travel), the mercenary's Resurrect (0x116480), Cain's Identify only
// when something needs identifying. The game looks a class up with the FIRST row whose 16-bit class matches (0x113990).
inline constexpr int MenuMaxHandlers = 5;

struct MenuRow {
	uint32_t classId;                     // +0x00 monstats row (compared as 16 bits)
	uint32_t count;                       // +0x04 options shown, including the final Cancel
	uint16_t strings[8];                  // +0x08 string id per option
	uint64_t handlers[MenuMaxHandlers];   // +0x18 bool(u8 dataFlag) per option, absolute addresses
	uint8_t flag;                         // +0x40
	uint8_t pad[7];
};
static_assert(sizeof(MenuRow) == 0x48, "D2R menu rows are 0x48 bytes (copy loop in 0x10b6e0)");
static_assert(offsetof(MenuRow, handlers) == 0x18, "handler slots start at +0x18");

// Index of the row D2R uses for `classId`, or -1.
auto FindMenuRow(const MenuRow* rows, size_t rowCount, uint32_t classId) noexcept -> int;

// True when `handler` is one of the row's options on screen: options 0 .. count-2 (0x1147a0); the last is Cancel.
auto RowShows(const MenuRow& row, uint64_t handler) noexcept -> bool;

// --- The menu options the plugin presses ----------------------------------------------------------------------------
// In priority order: the first one the open menu shows is pressed. Hire is never pressed: hireling NPCs only get
// Resurrect (shown while the mercenary is dead).
enum class Action { None, Trade, Identify, Resurrect, Travel };
inline constexpr int ActionCount = 5;

// Absolute handler addresses in the running game; 0 = switched off in quick-npc.toml.
struct Handlers {
	uint64_t of[ActionCount] = {};  // indexed by Action
	auto operator[](Action a) const noexcept -> uint64_t { return of[static_cast<int>(a)]; }
	auto operator[](Action a) noexcept -> uint64_t& { return of[static_cast<int>(a)]; }
};

// What the open menu shows, by Action, as read from the live row after D2R built the menu.
struct MenuState {
	bool shown = false;               // NPC set, UI state "menu", menu object present
	uint32_t npcId = 0;               // its unit id
	bool offers[ActionCount] = {};    // only read in OnQueuedCallback
};

// Which of `handlers` the row shows, into `state.offers`.
void ReadOffers(const MenuRow& row, const Handlers& handlers, MenuState* state) noexcept;

// The option to press: the first enabled one (in Action order) the menu shows, or None.
auto ChooseAction(const MenuState& menu, const Handlers& handlers) noexcept -> Action;

// --- Clicks ---------------------------------------------------------------------------------------------------------
// Flags of the client's player command (0xfe3b0 builds them, 0x102490 reads them). Left/right press = 5/6,
// held = 9/0xa, release = 0x11/0x12; D2R only starts an NPC interaction for a press (bit 4, 0x102919).
namespace cmd {
inline constexpr uint32_t Left = 0x1;
inline constexpr uint32_t Right = 0x2;
inline constexpr uint32_t Press = 0x4;
inline constexpr uint32_t Held = 0x8;
inline constexpr uint32_t Release = 0x10;
}  // namespace cmd

inline constexpr int32_t UnitTypePlayer = 0;
inline constexpr int32_t UnitTypeMonster = 1;
inline constexpr int32_t UnitTypeObject = 2;

// A deliberate left click: the left button's press, not a held repeat or the release.
auto IsLeftPress(uint32_t flags) noexcept -> bool;

// What 0x101910 (client "interact with unit") was called with, plus the command's target unit.
struct ClickInfo {
	int32_t unitType = -1;     // argument 2
	uint32_t unitId = 0;       // argument 3
	uint32_t flags = 0;        // command +0x00
	bool hasTarget = false;    // command +0x10 is set
	int32_t targetType = -1;   // target +0x00
	uint32_t targetId = 0;     // target +0x08
	uint32_t targetClass = 0;  // target +0x04
};

enum class Click {
	Vanilla,  // not a hotkey left press on an NPC: leave it to D2R
	NoMenu,   // hotkey left press on a monster with no NPC menu: leave it to D2R
	Npc,      // hotkey left press on an NPC with a menu: press an option when its dialog opens
};

// Options are decided when the menu is open, not here: several (quest travel, Resurrect, Cain's Identify) are
// only added to the row while the dialog opens.
auto ClassifyClick(const ClickInfo& click, bool hotkeyHeld, const MenuRow* rows, size_t rowCount) noexcept -> Click;

// --- One interaction ------------------------------------------------------------------------------------------------
// Tracks the one NPC the player hotkey-clicked:
//   hotkey + left click on NPC X         -> Arm(X)
//   any other interaction click          -> Clear()   (vanilla interaction, nothing pressed)
//   X's dialog opens within the timeout  -> MenuOpened(X) true: queue the callback once
//   queued callback runs                 -> TakeQueued(X) true: choose and press an option once
class Interaction {
public:
	static constexpr uint64_t DefaultTimeoutMs = 60000;  // walking across town plus an NPC greeting; stale clicks expire

	enum class State { Idle, Armed, Queued };

	void SetTimeout(uint64_t ms) noexcept { m_timeoutMs = ms; }
	auto timeout() const noexcept -> uint64_t { return m_timeoutMs; }

	void Arm(uint32_t unitId, uint64_t nowMs) noexcept;
	void Clear() noexcept;

	// The dialog of `unitId` opened. True exactly once per armed interaction: the caller queues the callback.
	auto MenuOpened(uint32_t unitId, uint64_t nowMs) noexcept -> bool;

	// The queued callback runs; `currentNpc` is the NPC whose dialog is open now. True: press an option (once).
	auto TakeQueued(uint32_t currentNpc) noexcept -> bool;

	auto state() const noexcept -> State { return m_state; }
	auto unitId() const noexcept -> uint32_t { return m_unit; }

private:
	State m_state = State::Idle;
	uint32_t m_unit = 0;
	uint64_t m_armedAt = 0;
	uint64_t m_timeoutMs = DefaultTimeoutMs;
};

// --- The feature's state machine ------------------------------------------------------------------------------------
// Everything the hooks decide, in one place. Not thread safe: the engine part holds a lock around every call.
struct Counters {
	uint32_t npcClicks = 0;            // hotkey left presses on NPCs with a menu
	uint32_t noMenu = 0;               // hotkey left presses on monsters without one
	uint32_t queued = 0;               // their dialog opened: callback queued
	uint32_t pressed[ActionCount] = {};// options pressed, by Action
	uint32_t dropped = 0;              // menu showed none of the enabled options (or changed): left as it was
	uint32_t queueFailed = 0;          // the callback could not be queued
};

class Controller {
public:
	void SetEnabled(bool enabled) noexcept;
	auto enabled() const noexcept -> bool { return m_enabled; }
	void SetTimeout(uint64_t ms) noexcept { m_interaction.SetTimeout(ms); }

	// Hook 1, before D2R's interact function runs (which always runs).
	void OnInteract(const ClickInfo& click, bool hotkeyHeld, const MenuRow* rows, size_t rowCount, uint64_t nowMs) noexcept;

	// Hook 2, after D2R opened an NPC menu. True: queue the callback.
	auto OnMenuOpened(const MenuState& menu, uint64_t nowMs) noexcept -> bool;
	void OnQueueFailed() noexcept;

	// The queued callback: the option to press now (at most once per interaction), or None to leave the menu open.
	auto OnQueuedCallback(const MenuState& menu, const Handlers& handlers) noexcept -> Action;

	auto counters() const noexcept -> const Counters& { return m_counters; }
	auto interaction() const noexcept -> const Interaction& { return m_interaction; }

private:
	bool m_enabled = false;
	Interaction m_interaction;
	Counters m_counters;
};

// --- Install ----------------------------------------------------------------------------------------------------------
// Template parts: left out by the off-file, or installed.
enum class PartDecision { Install, OffByFile };
auto DecidePart(bool offByFile) noexcept -> PartDecision;

// The feature's install steps, in order, with the engine work behind callbacks (the tests pass fakes).
// Fails closed: the first failing step stops the install and nothing is enabled.
struct InstallSteps {
	void* user = nullptr;
	bool (*servicesReady)(void* user, char* why, size_t whySize) = nullptr;
	bool (*alreadyHooked)(void* user, uint64_t rva) = nullptr;
	// Both hooks in one MutationService transaction: both are installed, or neither (D2RLoader rolls back).
	bool (*installHooks)(void* user, char* why, size_t whySize) = nullptr;
};

inline constexpr uint64_t RvaInteract = 0x101910;
inline constexpr uint64_t RvaOpenNpcMenu = 0x1141d0;

enum class InstallResult {
	On,
	NoServices,
	AlreadyHooked,
	HooksRefused,  // nothing was patched
};

auto RunInstall(const InstallSteps& steps, char* report, size_t reportSize) noexcept -> InstallResult;

}  // namespace quick_npc
