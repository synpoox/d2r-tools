// Host tests for plugin/src/quick_npc_logic.cpp: no game, no Windows, no SDK.
//   cmake -S tests -B build-tests && cmake --build build-tests && ctest --test-dir build-tests --output-on-failure
//
// The menu rows below are synthetic: they use D2R's row layout (0x48 bytes, handlers at +0x18) with made-up class ids
// and handler addresses, not data copied from the game.
#include "quick_npc_config.h"
#include "quick_npc_logic.h"

#include <cstdio>
#include <cstring>
#include <vector>

using namespace quick_npc;

namespace {

int g_failures = 0;
int g_checks = 0;

void Check(bool ok, const char* what, int line) {
	++g_checks;
	if (!ok) {
		++g_failures;
		std::printf("FAIL line %d: %s\n", line, what);
	}
}
#define CHECK(x) Check((x), #x, __LINE__)

constexpr uint64_t Trade = 0x140112f30;
constexpr uint64_t Talk = 0x140112eb0;
constexpr uint64_t Gamble = 0x140112c80;
constexpr uint64_t Hire = 0x140112620;
constexpr uint64_t Identify = 0x140114000;

constexpr uint32_t VendorClass = 148;      // Talk, Trade, Cancel
constexpr uint32_t TalkOnlyClass = 146;    // Talk, Cancel (a quest/story NPC)
constexpr uint32_t GambleOnlyClass = 514;  // Talk, Gamble, Cancel
constexpr uint32_t HireClass = 150;        // Talk, Hire, Cancel
constexpr uint32_t TradeLastClass = 199;   // Talk, Trade, Gamble, Cancel
constexpr uint32_t DupClass = 257;         // first row: Trade, Cancel; second row: Talk, Cancel
constexpr uint32_t HiddenTradeClass = 300; // Trade sits in a slot past count-1: not shown, so not a vendor
constexpr uint32_t WideClass = 0x10000 + 148;  // compared as 16 bits, like D2R's lookup
constexpr uint32_t UnknownClass = 999;     // no row: ordinary monster
constexpr uint32_t CainClass = 244;        // Talk, Identify, Cancel (Deckard Cain in town)

auto Row(uint32_t classId, std::initializer_list<uint64_t> options) -> MenuRow {
	MenuRow row {};
	row.classId = classId;
	row.count = static_cast<uint32_t>(options.size()) + 1;  // plus Cancel
	int i = 0;
	for (const uint64_t h : options) {
		row.handlers[i++] = h;
	}
	return row;
}

auto Table() -> std::vector<MenuRow> {
	std::vector<MenuRow> rows;
	rows.push_back(Row(VendorClass, {Talk, Trade}));
	rows.push_back(Row(TalkOnlyClass, {Talk}));
	rows.push_back(Row(GambleOnlyClass, {Talk, Gamble}));
	rows.push_back(Row(HireClass, {Talk, Hire}));
	rows.push_back(Row(TradeLastClass, {Talk, Gamble, Trade}));
	rows.push_back(Row(DupClass, {Trade}));
	rows.push_back(Row(DupClass, {Talk}));
	MenuRow hidden = Row(HiddenTradeClass, {Talk});
	hidden.handlers[1] = Trade;  // present in memory, but count says only Talk + Cancel are shown
	rows.push_back(hidden);
	rows.push_back(Row(CainClass, {Talk, Identify}));
	return rows;
}

auto Press(uint32_t id, uint32_t classId, uint32_t flags = cmd::Left | cmd::Press) -> ClickInfo {
	ClickInfo c;
	c.unitType = UnitTypeMonster;
	c.unitId = id;
	c.flags = flags;
	c.hasTarget = true;
	c.targetType = UnitTypeMonster;
	c.targetId = id;
	c.targetClass = classId;
	return c;
}

auto Shown(uint32_t npc, bool trade = true) -> MenuState {
	MenuState m;
	m.shown = true;
	m.npcId = npc;
	m.offersTrade = trade;
	return m;
}

// --- Vendor classification ------------------------------------------------------------------------------------------
void TestVendorClassification() {
	const auto rows = Table();
	const MenuRow* t = rows.data();
	const size_t n = rows.size();
	CHECK(IsVendorClass(t, n, VendorClass, Trade));
	CHECK(IsVendorClass(t, n, TradeLastClass, Trade));
	CHECK(!IsVendorClass(t, n, TalkOnlyClass, Trade));    // quest/story NPC
	CHECK(!IsVendorClass(t, n, GambleOnlyClass, Trade));  // gamble is not the vendor screen
	CHECK(!IsVendorClass(t, n, HireClass, Trade));        // mercenary NPC
	CHECK(!IsVendorClass(t, n, UnknownClass, Trade));     // monster / no menu row
	CHECK(!IsVendorClass(t, n, HiddenTradeClass, Trade)); // only shown options count
	CHECK(IsVendorClass(t, n, DupClass, Trade));          // first matching row wins, as in D2R
	CHECK(FindMenuRow(t, n, DupClass) == 5);
	CHECK(IsVendorClass(t, n, WideClass, Trade));         // 16-bit compare
	CHECK(!IsVendorClass(t, n, VendorClass, 0));          // no handler address: never a vendor
	CHECK(!IsVendorClass(nullptr, 0, VendorClass, Trade));
	CHECK(FindMenuRow(t, n, UnknownClass) == -1);

	MenuRow broken = Row(VendorClass, {Talk, Trade});
	broken.count = 0;  // corrupt / unread row: nothing shown
	CHECK(!RowOffersTrade(broken, Trade));
	broken.count = 200;  // larger than the slots: only the 5 slots are read
	CHECK(RowOffersTrade(broken, Trade));
}

// --- Click and hotkey -----------------------------------------------------------------------------------------------
void TestClickAndHotkey() {
	CHECK(IsLeftPress(cmd::Left | cmd::Press));
	CHECK(IsLeftPress(cmd::Left | cmd::Press | 0x80));  // extra D2R bits (0x80 skill-usable) don't matter
	CHECK(!IsLeftPress(cmd::Left | cmd::Held));       // held repeat
	CHECK(!IsLeftPress(cmd::Left | cmd::Release));    // release
	CHECK(!IsLeftPress(cmd::Left | cmd::Press | cmd::Held));     // never a press if D2R also marks it held
	CHECK(!IsLeftPress(cmd::Left | cmd::Press | cmd::Release));
	CHECK(!IsLeftPress(cmd::Right | cmd::Press));     // right click
	CHECK(!IsLeftPress(cmd::Left));                   // no press bit

	const auto rows = Table();
	const MenuRow* t = rows.data();
	const size_t n = rows.size();
	CHECK(ClassifyClick(Press(7, VendorClass), true, t, n, Trade) == Click::Trade);
	CHECK(ClassifyClick(Press(7, VendorClass), false, t, n, Trade) == Click::Vanilla);  // hotkey not held
	CHECK(ClassifyClick(Press(7, TalkOnlyClass), true, t, n, Trade) == Click::NotVendor);
	CHECK(ClassifyClick(Press(7, UnknownClass), true, t, n, Trade) == Click::NotVendor);  // a monster
	CHECK(ClassifyClick(Press(7, VendorClass, cmd::Left | cmd::Held), true, t, n, Trade) == Click::Vanilla);
	CHECK(ClassifyClick(Press(7, VendorClass, cmd::Right | cmd::Press), true, t, n, Trade) == Click::Vanilla);

	ClickInfo object = Press(7, VendorClass);
	object.unitType = UnitTypeObject;
	object.targetType = UnitTypeObject;
	CHECK(ClassifyClick(object, true, t, n, Trade) == Click::Vanilla);  // world object

	ClickInfo player = Press(7, VendorClass);
	player.unitType = UnitTypePlayer;
	player.targetType = UnitTypePlayer;
	CHECK(ClassifyClick(player, true, t, n, Trade) == Click::Vanilla);  // another player

	ClickInfo noTarget = Press(7, VendorClass);
	noTarget.hasTarget = false;
	CHECK(ClassifyClick(noTarget, true, t, n, Trade) == Click::Vanilla);

	ClickInfo mismatch = Press(7, VendorClass);
	mismatch.targetId = 8;  // the command's target is not the unit being interacted with
	CHECK(ClassifyClick(mismatch, true, t, n, Trade) == Click::Vanilla);
}

// --- One interaction, repeats and timeouts --------------------------------------------------------------------------
void TestInteraction() {
	const auto rows = Table();
	const MenuRow* t = rows.data();
	const size_t n = rows.size();

	Controller c;
	c.SetEnabled(true);

	// Normal: hotkey click, walk, dialog opens, Trade once.
	c.OnInteract(Press(7, VendorClass), true, t, n, Trade, 1000);
	CHECK(c.interaction().state() == Interaction::State::Armed);
	CHECK(c.OnMenuOpened(Shown(7), 4000));
	CHECK(!c.OnMenuOpened(Shown(7), 4001));  // the menu opens again (e.g. after a speech): no second queue
	CHECK(c.OnQueuedCallback(Shown(7)) == Action::Trade);
	CHECK(c.OnQueuedCallback(Shown(7)) == Action::None);    // a second callback does nothing
	CHECK(c.counters().traded == 1);
	CHECK(c.counters().queued == 1);

	// After the trade, the next dialog of the same NPC is vanilla.
	CHECK(!c.OnMenuOpened(Shown(7), 5000));

	// Clicking the vendor without the hotkey: vanilla, and it cancels an earlier hotkey click.
	c.OnInteract(Press(7, VendorClass), true, t, n, Trade, 6000);
	c.OnInteract(Press(7, VendorClass), false, t, n, Trade, 6100);
	CHECK(c.interaction().state() == Interaction::State::Idle);
	CHECK(!c.OnMenuOpened(Shown(7), 6200));

	// Hotkey click on a non-vendor: nothing armed.
	c.OnInteract(Press(9, TalkOnlyClass), true, t, n, Trade, 7000);
	CHECK(c.interaction().state() == Interaction::State::Idle);
	CHECK(!c.OnMenuOpened(Shown(9), 7100));
	CHECK(c.counters().notVendor == 1);

	// Another NPC's dialog opens first: the interaction ends, no trade with anyone.
	c.OnInteract(Press(7, VendorClass), true, t, n, Trade, 8000);
	CHECK(!c.OnMenuOpened(Shown(11), 8100));
	CHECK(!c.OnMenuOpened(Shown(7), 8200));

	// A clicked vendor that is reached too late: expired.
	c.OnInteract(Press(7, VendorClass), true, t, n, Trade, 10000);
	CHECK(!c.OnMenuOpened(Shown(7), 10000 + Interaction::DefaultTimeoutMs + 1));
	c.OnInteract(Press(7, VendorClass), true, t, n, Trade, 20000);
	CHECK(c.OnMenuOpened(Shown(7), 20000 + Interaction::DefaultTimeoutMs));  // exactly at the limit is still fine

	// The callback finds the menu changed: drop it, call nothing.
	CHECK(c.OnQueuedCallback(Shown(7, false)) == Action::None);  // its row has no Trade any more
	CHECK(c.counters().dropped == 1);
	c.OnInteract(Press(7, VendorClass), true, t, n, Trade, 30000);
	CHECK(c.OnMenuOpened(Shown(7), 30100));
	MenuState closed;
	closed.npcId = 7;
	CHECK(c.OnQueuedCallback(closed) == Action::None);  // the player closed the menu meanwhile
	c.OnInteract(Press(7, VendorClass), true, t, n, Trade, 31000);
	CHECK(c.OnMenuOpened(Shown(7), 31100));
	CHECK(c.OnQueuedCallback(Shown(12)) == Action::None);  // another NPC's menu is open now
	CHECK(c.counters().traded == 1);

	// A menu that is not shown does not consume the interaction.
	c.OnInteract(Press(7, VendorClass), true, t, n, Trade, 40000);
	MenuState hidden;
	hidden.npcId = 7;
	CHECK(!c.OnMenuOpened(hidden, 40100));
	CHECK(c.interaction().state() == Interaction::State::Armed);

	// Queuing failed: forget the interaction.
	CHECK(c.OnMenuOpened(Shown(7), 40200));
	c.OnQueueFailed();
	CHECK(c.interaction().state() == Interaction::State::Idle);
	CHECK(c.OnQueuedCallback(Shown(7)) == Action::None);
	CHECK(c.counters().queueFailed == 1);

	// The clock going backwards (should not happen with GetTickCount64) never arms a stale click.
	c.OnInteract(Press(7, VendorClass), true, t, n, Trade, 50000);
	CHECK(!c.OnMenuOpened(Shown(7), 49000));
}

void TestRepeatedInput() {
	const auto rows = Table();
	Controller c;
	c.SetEnabled(true);
	// The button held over the vendor: D2R reports held repeats (flags 1|8), never a new press. They are vanilla and
	// must not re-arm after the trade.
	c.OnInteract(Press(7, VendorClass), true, rows.data(), rows.size(), Trade, 1000);
	CHECK(c.OnMenuOpened(Shown(7), 1500));
	CHECK(c.OnQueuedCallback(Shown(7)) == Action::Trade);
	for (int i = 0; i < 100; ++i) {
		c.OnInteract(Press(7, VendorClass, cmd::Left | cmd::Held), true, rows.data(), rows.size(), Trade, 1600 + i);
		CHECK(!c.OnMenuOpened(Shown(7), 1600 + i));
	}
	CHECK(c.counters().traded == 1);
	CHECK(c.counters().armed == 1);
	// Two quick presses before arrival: still one trade.
	c.OnInteract(Press(7, VendorClass), true, rows.data(), rows.size(), Trade, 5000);
	c.OnInteract(Press(7, VendorClass), true, rows.data(), rows.size(), Trade, 5100);
	CHECK(c.OnMenuOpened(Shown(7), 6000));
	CHECK(!c.OnMenuOpened(Shown(7), 6001));
	CHECK(c.OnQueuedCallback(Shown(7)) == Action::Trade);
	CHECK(c.OnQueuedCallback(Shown(7)) == Action::None);
	CHECK(c.counters().traded == 2);
}

void TestConfiguredTimeout() {
	const auto rows = Table();
	Controller c;
	c.SetEnabled(true);
	c.SetTimeout(5000);  // timeout_seconds = 5
	c.OnInteract(Press(7, VendorClass), true, rows.data(), rows.size(), Trade, 1000);
	CHECK(!c.OnMenuOpened(Shown(7), 6001));
	c.OnInteract(Press(7, VendorClass), true, rows.data(), rows.size(), Trade, 10000);
	CHECK(c.OnMenuOpened(Shown(7), 15000));
	c.SetTimeout(600000);  // reload with a longer timeout keeps working
	c.OnInteract(Press(7, VendorClass), true, rows.data(), rows.size(), Trade, 20000);
	CHECK(c.OnMenuOpened(Shown(7), 20000 + 599000));
}

// --- Deckard Cain: Identify ---------------------------------------------------------------------------------------
auto ShownCain(uint32_t npc, bool identify) -> MenuState {
	MenuState m;
	m.shown = true;
	m.npcId = npc;
	m.offersIdentify = identify;
	return m;
}

void TestCainIdentify() {
	auto rows = Table();
	const MenuRow* t = rows.data();
	const size_t n = rows.size();
	const Handlers both(Trade, Identify);
	const Handlers tradeOnly(Trade);  // cain_identify = false

	CHECK(ServiceForClass(t, n, CainClass, both) == Action::Identify);
	CHECK(ServiceForClass(t, n, CainClass, tradeOnly) == Action::None);
	CHECK(ServiceForClass(t, n, VendorClass, both) == Action::Trade);
	CHECK(ServiceForClass(t, n, TalkOnlyClass, both) == Action::None);  // caged Cain / quest NPCs: Talk only
	CHECK(ServiceForClass(t, n, UnknownClass, both) == Action::None);
	CHECK(!IsVendorClass(t, n, CainClass, Trade));  // Cain is not a vendor

	// D2R hides Identify while nothing needs identifying (row count 2): the click still arms Identify...
	rows.back().count = 2;
	CHECK(!RowOffersTrade(rows.back(), Identify));
	CHECK(RowHasHandler(rows.back(), Identify));
	CHECK(ServiceForClass(t, n, CainClass, both) == Action::Identify);
	CHECK(!RowHasHandler(rows.back(), 0));
	CHECK(ClassifyClick(Press(5, CainClass), true, t, n, both) == Click::Identify);
	CHECK(ClassifyClick(Press(5, CainClass), false, t, n, both) == Click::Vanilla);
	CHECK(ClassifyClick(Press(5, CainClass), true, t, n, tradeOnly) == Click::NotVendor);

	Controller c;
	c.SetEnabled(true);
	// Normal: hotkey click on Cain, his menu shows Identify, the callback presses Identify once.
	c.OnInteract(Press(5, CainClass), true, t, n, both, 1000);
	CHECK(c.interaction().action() == Action::Identify);
	CHECK(c.OnMenuOpened(ShownCain(5, true), 2000));
	CHECK(c.OnQueuedCallback(ShownCain(5, true)) == Action::Identify);
	CHECK(c.OnQueuedCallback(ShownCain(5, true)) == Action::None);
	CHECK(c.counters().identified == 1 && c.counters().traded == 0 && c.counters().armedCain == 1);

	// ...but with nothing to identify his menu has no Identify: nothing is pressed, the normal menu stays.
	c.OnInteract(Press(5, CainClass), true, t, n, both, 3000);
	CHECK(c.OnMenuOpened(ShownCain(5, false), 3100));
	CHECK(c.OnQueuedCallback(ShownCain(5, false)) == Action::None);
	CHECK(c.counters().identified == 1 && c.counters().dropped == 1);

	// An Identify interaction never presses Trade, even if the menu shows it.
	c.OnInteract(Press(5, CainClass), true, t, n, both, 4000);
	CHECK(c.OnMenuOpened(ShownCain(5, false), 4100));
	MenuState tradeShown = ShownCain(5, false);
	tradeShown.offersTrade = true;
	CHECK(c.OnQueuedCallback(tradeShown) == Action::None);

	// A Trade interaction never presses Identify.
	c.OnInteract(Press(7, VendorClass), true, t, n, both, 5000);
	CHECK(c.OnMenuOpened(Shown(7, false), 5100));
	MenuState identifyShown = Shown(7, false);
	identifyShown.offersIdentify = true;
	CHECK(c.OnQueuedCallback(identifyShown) == Action::None);

	// cain_identify = false: Cain clicks are vanilla.
	c.OnInteract(Press(5, CainClass), true, t, n, tradeOnly, 6000);
	CHECK(c.interaction().state() == Interaction::State::Idle);
	CHECK(!c.OnMenuOpened(ShownCain(5, true), 6100));
	CHECK(c.counters().identified == 1);

	// Cain without the hotkey: vanilla.
	c.OnInteract(Press(5, CainClass), false, t, n, both, 7000);
	CHECK(!c.OnMenuOpened(ShownCain(5, true), 7100));
}

// --- Feature off ----------------------------------------------------------------------------------------------------
void TestDisabled() {
	const auto rows = Table();
	Controller c;  // starts disabled (nothing installed yet)
	c.OnInteract(Press(7, VendorClass), true, rows.data(), rows.size(), Trade, 1000);
	CHECK(c.interaction().state() == Interaction::State::Idle);
	CHECK(!c.OnMenuOpened(Shown(7), 1100));
	CHECK(c.counters().npcClicks == 0);

	c.SetEnabled(true);
	c.OnInteract(Press(7, VendorClass), true, rows.data(), rows.size(), Trade, 2000);
	c.SetEnabled(false);  // "quick-npc off" while walking to the vendor
	CHECK(c.interaction().state() == Interaction::State::Idle);
	CHECK(!c.OnMenuOpened(Shown(7), 2100));

	c.SetEnabled(true);
	c.OnInteract(Press(7, VendorClass), true, rows.data(), rows.size(), Trade, 3000);
	CHECK(c.OnMenuOpened(Shown(7), 3100));
	c.SetEnabled(false);  // switched off after the callback was queued
	CHECK(c.OnQueuedCallback(Shown(7)) == Action::None);
	CHECK(c.counters().traded == 0);
}

// --- Install: unsupported build, failed hooks -----------------------------------------------------------------------
struct FakeEngine {
	bool services = true;
	bool hooked[2] = {false, false};  // [0] interact, [1] open-menu: FF 25 at the entry
	bool hooksOk = true;              // the transaction commits (entry bytes match)
	int hookCalls = 0;
	bool hooksInstalled = false;
};

auto Steps(FakeEngine& e) -> InstallSteps {
	InstallSteps s;
	s.user = &e;
	s.servicesReady = [](void* u, char* why, size_t n) {
		const bool ok = static_cast<FakeEngine*>(u)->services;
		if (!ok) {
			std::snprintf(why, n, "D2RLoader has no thread service");
		}
		return ok;
	};
	s.alreadyHooked = [](void* u, uint64_t rva) { return static_cast<FakeEngine*>(u)->hooked[rva == RvaInteract ? 0 : 1]; };
	s.installHooks = [](void* u, char* why, size_t n) {
		auto* e = static_cast<FakeEngine*>(u);
		++e->hookCalls;
		if (!e->hooksOk) {
			std::snprintf(why, n, "commit refused (result 4, reason 4)");
			return false;
		}
		e->hooksInstalled = true;
		return true;
	};
	return s;
}

void TestInstall() {
	// Unsupported build: the engine part is never installed; the environment guard still is.
	CHECK(DecidePart(false, true, false) == PartDecision::OffImage);
	CHECK(DecidePart(false, false, false) == PartDecision::Install);
	CHECK(DecidePart(false, true, true) == PartDecision::Install);
	CHECK(DecidePart(true, true, true) == PartDecision::OffByFile);  // "quicknpc" in quick-npc-off.txt
	CHECK(DecidePart(true, false, false) == PartDecision::OffByFile);

	char report[300];
	{
		FakeEngine e;
		CHECK(RunInstall(Steps(e), report, sizeof(report)) == InstallResult::On);
		CHECK(e.hooksInstalled);
		CHECK(std::strncmp(report, "on:", 3) == 0);
	}
	{
		FakeEngine e;
		e.services = false;
		CHECK(RunInstall(Steps(e), report, sizeof(report)) == InstallResult::NoServices);
		CHECK(e.hookCalls == 0 && !e.hooksInstalled);
		CHECK(std::strstr(report, "thread service") != nullptr);
	}
	for (int which = 0; which < 2; ++which) {
		FakeEngine e;
		e.hooked[which] = true;  // D2RLoader or another plugin already patched the entry
		CHECK(RunInstall(Steps(e), report, sizeof(report)) == InstallResult::AlreadyHooked);
		CHECK(e.hookCalls == 0 && !e.hooksInstalled);
		CHECK(std::strstr(report, which == 0 ? "0x101910" : "0x1141d0") != nullptr);
	}
	{
		FakeEngine e;
		e.hooksOk = false;  // entry bytes differ (another build, someone else's patch): the transaction patches nothing
		CHECK(RunInstall(Steps(e), report, sizeof(report)) == InstallResult::HooksRefused);
		CHECK(!e.hooksInstalled);
		CHECK(std::strstr(report, "nothing patched") != nullptr);
		CHECK(std::strstr(report, "commit refused") != nullptr);
	}
	{
		InstallSteps missing;  // no callbacks at all: refuse, don't crash
		CHECK(RunInstall(missing, report, sizeof(report)) == InstallResult::NoServices);
	}
	{
		// A controller that was never enabled (any failed install) acts on nothing.
		const auto rows = Table();
		Controller c;
		c.OnInteract(Press(7, VendorClass), true, rows.data(), rows.size(), Trade, 1);
		CHECK(!c.OnMenuOpened(Shown(7), 2));
		CHECK(c.OnQueuedCallback(Shown(7)) == Action::None);
	}
}

// --- Settings (quick-npc.toml) --------------------------------------------------------------------------------------
auto Key(const char* name) -> uint32_t {
	uint32_t vk = 0xFFFF;
	return KeyFromName(name, &vk) ? vk : 0xFFFF;
}

void TestKeyNames() {
	CHECK(Key("LeftCtrl") == 0xA2);
	CHECK(Key("left ctrl") == 0xA2);
	CHECK(Key("LEFT_CONTROL") == 0xA2);
	CHECK(Key("LCtrl") == 0xA2);
	CHECK(Key("RightCtrl") == 0xA3);
	CHECK(Key("Ctrl") == 0x11);
	CHECK(Key("LeftAlt") == 0xA4);
	CHECK(Key("Alt") == 0x12);
	CHECK(Key("LeftShift") == 0xA0);
	CHECK(Key("g") == 'G');
	CHECK(Key("7") == '7');
	CHECK(Key("F1") == 0x70);
	CHECK(Key("F24") == 0x87);
	CHECK(Key("Numpad5") == 0x65);
	CHECK(Key("Mouse4") == 0x05);
	CHECK(Key("XButton2") == 0x06);
	CHECK(Key("Mouse3") == 0x04);
	CHECK(Key("Backquote") == 0xC0);
	CHECK(Key("0xA2") == 0xA2);
	CHECK(Key("0x01") == 0xFFFF);  // the left mouse button is the click itself
	CHECK(Key("0x02") == 0xFFFF);  // right mouse button
	CHECK(Key("0x100") == 0xFFFF);
	CHECK(Key("F0") == 0xFFFF);
	CHECK(Key("F25") == 0xFFFF);
	CHECK(Key("Hyper") == 0xFFFF);
	CHECK(Key("") == 0xFFFF);
	CHECK(Key("AB") == 0xFFFF);
	uint32_t vk = 0;
	CHECK(!KeyFromName(nullptr, &vk));

	CHECK(HotkeyBlocksInteraction(0xA0) && HotkeyBlocksInteraction(0xA1) && HotkeyBlocksInteraction(0x10));
	CHECK(!HotkeyBlocksInteraction(0xA2));
	CHECK(HotkeyNote(0xA2) != nullptr && std::strstr(HotkeyNote(0xA2), "Run") != nullptr);
	CHECK(HotkeyNote(0x10) != nullptr && std::strstr(HotkeyNote(0x10), "Stand Still") != nullptr);
	CHECK(HotkeyNote('G') == nullptr);
}

auto Parse(const char* text, Config* config, int* count, ConfigIssue* issues = nullptr) -> int {
	static ConfigIssue local[8];
	*count = ParseConfig(text, std::strlen(text), config, issues != nullptr ? issues : local, 8);
	return *count;
}

void TestConfig() {
	int n = 0;
	{
		Config d;  // built-in defaults: what the user asked for
		CHECK(d.enabled && d.hotkeyVk == 0xA2 && std::strcmp(d.hotkeyName, "LeftCtrl") == 0 && d.timeoutMs == 60000);
	}
	{
		// The shipped default file (plugin/quick-npc.toml) parses with no issues and gives the defaults.
		const char* shipped =
			"# Quick NPC settings.\n"
			"\n"
			"[quick-npc]\n"
			"# comment\n"
			"enabled = true\n"
			"hotkey = \"LeftCtrl\"\n"
			"timeout_seconds = 60\n";
		Config c;
		CHECK(Parse(shipped, &c, &n) == 0);
		CHECK(c.enabled && c.hotkeyVk == 0xA2 && c.timeoutMs == 60000);
	}
	{
		// The real default file embedded in the DLL.
		std::FILE* f = std::fopen(QUICK_NPC_DEFAULT_TOML, "rb");
		CHECK(f != nullptr);
		if (f != nullptr) {
			static char text[16384];
			const size_t len = std::fread(text, 1, sizeof(text), f);
			std::fclose(f);
			Config c;
			ConfigIssue issues[4];
			CHECK(ParseConfig(text, len, &c, issues, 4) == 0);
			CHECK(c.enabled && c.hotkeyVk == 0xA2 && std::strcmp(c.hotkeyName, "LeftCtrl") == 0 && c.timeoutMs == 60000);
			CHECK(c.cainIdentify);
		}
	}
	{
		Config c;
		CHECK(Parse("[quick-npc]\ncain_identify = false\n", &c, &n) == 0);
		CHECK(!c.cainIdentify);
		CHECK(Parse("[quick-npc]\ncain_identify = \"no\"\n", &c, &n) == 1);  // not a boolean: kept
		CHECK(!c.cainIdentify);
		Config d;
		CHECK(Parse("[quick-npc]\nhotkey = \"LeftCtrl\"\n", &d, &n) == 0);  // a 0.2.0 file without the setting: on
		CHECK(d.cainIdentify);
	}
	{
		Config c;
		CHECK(Parse("[quick-npc]\r\nenabled = false  # off\r\nhotkey = 'F5' # single quotes\r\ntimeout_seconds = 1_20\r\n", &c, &n) == 0);
		CHECK(!c.enabled && c.hotkeyVk == 0x74 && std::strcmp(c.hotkeyName, "F5") == 0 && c.timeoutMs == 120000);
	}
	{
		Config c;
		CHECK(Parse("\xEF\xBB\xBF[ \"quick-npc\" ]\nhotkey = \"Right Ctrl\"\n", &c, &n) == 0);  // BOM, quoted table
		CHECK(c.hotkeyVk == 0xA3);
	}
	{
		// Other tables (D2RLoader's [d2rl]) and their keys are ignored without complaint.
		Config c;
		CHECK(Parse("[quick-npc]\nhotkey = \"Mouse4\"\n[d2rl]\nmatch = [\"x\"]\nhotkey = \"G\"\n", &c, &n) == 0);
		CHECK(c.hotkeyVk == 0x05);
	}
	{
		ConfigIssue issues[8];
		Config c;
		Parse("[quick-npc]\nhotkey = \"Hyper\"\nenabled = 1\ntimeout_seconds = 4\ntimeout_seconds = 601\ncolour = 3\n"
		      "hotkey = LeftCtrl\nnot a setting\nhotkey = \"unterminated\n",
		      &c, &n, issues);
		CHECK(n == 8);
		CHECK(c.enabled && c.hotkeyVk == 0xA2 && c.timeoutMs == 60000);  // every bad value keeps the default
		CHECK(issues[0].line == 2 && std::strstr(issues[0].text, "Hyper") != nullptr);
		CHECK(issues[1].line == 3 && std::strstr(issues[1].text, "enabled") != nullptr);
		CHECK(issues[2].line == 4 && issues[3].line == 5);
		CHECK(std::strstr(issues[4].text, "unknown setting colour") != nullptr);
		CHECK(issues[5].line == 7);  // bare word: not a string
		CHECK(issues[6].line == 8 && std::strstr(issues[6].text, "key = value") != nullptr);
		CHECK(issues[7].line == 9);  // unterminated string
	}
	{
		Config c;
		ConfigIssue issues[8];
		Parse("hotkey = \"G\"\n", &c, &n, issues);  // no table: the key is outside [quick-npc]
		CHECK(c.hotkeyVk == 0xA2);
		CHECK(n == 2 && std::strstr(issues[0].text, "outside [quick-npc]") != nullptr);
		CHECK(std::strstr(issues[1].text, "no [quick-npc] table") != nullptr);
	}
	{
		Config c;
		CHECK(ParseConfig(nullptr, 0, &c, nullptr, 0) == 1);  // empty file: defaults, one issue, no crash
		CHECK(c.hotkeyVk == 0xA2);
		CHECK(Parse("[quick-npc]\nhotkey = \"G\" trailing\n", &c, &n) == 1);  // junk after a value
		CHECK(c.hotkeyVk == 0xA2);
		CHECK(Parse("[quick-npc]\nhotkey = \"0123456789012345678901234567890123456789012345678901234567890123456789\"\n", &c, &n) == 1);
		const char noNul[] = {'[', 'q', 'u', 'i', 'c', 'k', '-', 'n', 'p', 'c', ']', '\n', 'e', 'n', 'a', 'b', 'l', 'e', 'd', '=',
		                      'f', 'a', 'l', 's', 'e'};
		CHECK(ParseConfig(noNul, sizeof(noNul), &c, nullptr, 0) == 0);  // not null-terminated, no final newline
		CHECK(!c.enabled);
	}
}

}  // namespace

int main() {
	TestVendorClassification();
	TestClickAndHotkey();
	TestInteraction();
	TestRepeatedInput();
	TestConfiguredTimeout();
	TestCainIdentify();
	TestDisabled();
	TestKeyNames();
	TestConfig();
	TestInstall();
	std::printf("%d checks, %d failed\n", g_checks, g_failures);
	return g_failures == 0 ? 0 : 1;
}
