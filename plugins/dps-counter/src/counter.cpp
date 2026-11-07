// dps-counter: the damage feed and the text. Evidence for every signature: docs/reverse-engineering/dps-counter.md.
//
// The hook runs on the server thread and only adds to an atomic. The overlay frame callback is the clock: it queues
// one UI-thread task at a time, which moves that damage into the window and sets the text.
#include "counter.h"

#include "dps_logic.h"
#include "log.h"

#define NOMINMAX
#include <windows.h>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

namespace {

constexpr const char* PluginId = "dps-counter";

// --- Engine: found by signature (values in comments are build 93847's) ----------------------------------------------

// Native hook, as the PluginSDK has no damage event: D2Game SUnitDmg.cpp's apply 0x44a9b0 (game, attacker, victim,
// &damage), which melee, missile, area and aura hits all reach (35 call sites). The hook copies its first 25 bytes.
constexpr const char* DealDamageSig =
	"40 53 55 57 41 56 48 81 EC 88 00 00 00 49 8B D9 49 8B F8 4C 8B F2 48 8B E9 E8 ?? ?? ?? ?? 85 C0 75 0C F7 03 00 10 00 00 0F 84";
constexpr uint32_t DealDamageEntrySize = 25;  // PUSH RBX/RBP/RDI/R14, SUB RSP,0x88, three MOVs: no relative operand
// (game, missile) -> owner 0x490300
constexpr const char* MissileOwnerSig =
	"48 89 74 24 18 57 48 83 EC 20 48 8B FA 48 8B F1 48 85 D2 75 20 48 8D 4C 24 38 88 54 24 38 E8 ?? ?? ?? ?? 84 C0 74 1F CC";
// ExpBarWidget update 0x14cc8c0; its call at +0x8a is the total stat function 0x2f5020 (as player-bars finds it).
constexpr const char* ExpBarSig =
	"40 53 48 83 EC 50 80 B9 68 01 00 00 00 48 8B D9 0F 85 ?? ?? ?? ?? 48 89 7C 24 70 E8 ?? ?? ?? ?? 8B C8 E8 ?? ?? ?? ?? "
	"48 8B F8 48 85 C0 0F 84 ?? ?? ?? ?? 48 89 6C 24 60 48 8D 15 ?? ?? ?? ?? 48 89 74 24 68 41 B8 23 00 00 00 4C 89 64 24 "
	"48 48 8B C8 4C 89 6C 24 40 4C 89 74 24 38 4C 89 7C 24 30 0F 29 74 24 20 E8 ?? ?? ?? ?? 45 33 C0 48 8B CF 44 8B F8 41 "
	"8D 50 0C E8 ?? ?? ?? ?? 45 33 C0 48 8B CF 44 8B E8 41 8D 50 0D E8 ?? ?? ?? ??";
constexpr size_t ExpBarStatCall = 0x8a;
// For the text: FindPanel 0x846170, FindChild 0x856220, TextBoxWidget::SetText 0x86dd50 (as player-bars finds them).
constexpr const char* FindPanelSig = "48 8B D1 48 8B 0D ?? ?? ?? ?? 48 85 C9 0F 85 ?? ?? ?? ?? 33 C0 C3";
constexpr const char* FindChildSig = "48 89 5C 24 10 48 89 74 24 18 57 48 83 EC 20 48 8B 59 58 48 8B F2 48 8B";
constexpr const char* SetTextSig = "40 53 48 83 EC 20 4C 8B 42 08 48 8B D9 48 8B 12 48 81 C1 88 00 00 00 E8";

constexpr uint32_t UnitPlayer = 0, UnitMonster = 1, UnitMissile = 3;  // u32 unit type at +0
constexpr int32_t StatLife = 6;  // stored as value << 8

using DealDamageFn = void (*)(void* game, void* attacker, void* victim, void* damage) noexcept;
using MissileOwnerFn = void* (*)(void* game, void* missile) noexcept;
using TotalStatFn = int32_t (*)(void* unit, int32_t stat, int32_t layer) noexcept;
struct StringView {
	const char* text;
	uint64_t size;
};
using NativeFindPanelFn = void* (*)(const char* name) noexcept;
using NativeFindChildFn = void* (*)(void* parent, const char* name) noexcept;
using NativeSetTextFn = void (*)(void* textBox, const StringView* text) noexcept;

DealDamageFn g_dealDamage = nullptr;
MissileOwnerFn g_missileOwner = nullptr;
TotalStatFn g_totalStat = nullptr;
NativeFindPanelFn g_nativeFindPanel = nullptr;
NativeFindChildFn g_nativeFindChild = nullptr;
NativeSetTextFn g_nativeSetText = nullptr;

const D2RL::PluginContext* g_context = nullptr;
const D2RL::OverlayService* g_overlay = nullptr;
const D2RL::ThreadService* g_threads = nullptr;
const D2RL::PanelService* g_panelService = nullptr;
D2RL::Overlay::CallbackHandle g_callback = D2RL::Overlay::InvalidCallbackHandle;
D2RL::Panels::RegistrationHandle g_panel = D2RL::Panels::InvalidHandle;
dps::Settings g_settings;

// Server thread
std::atomic<int64_t> g_pending{0};  // life taken, in the game's 1/256 units, not yet in the window
std::atomic<uint64_t> g_hits{0};
std::atomic<uint32_t> g_faults{0};
std::atomic<bool> g_feedOn{false};

// UI thread
dps::Window* g_window = nullptr;
bool g_panelOpen = false;
uint32_t g_tick = 0;
char g_shown[40] = "?";
std::atomic<bool> g_taskQueued{false};
std::atomic<uint64_t> g_frames{0};
std::atomic<int32_t> g_panelResult{-1};
std::atomic<int64_t> g_lastDps{0};

auto Type(void* unit) noexcept -> uint32_t {
	return *static_cast<const uint32_t*>(unit);
}

// shortcut: counts every player's hits, so in a hosted game with others it is the party's damage; pets and hirelings
// are left out. Upgrade (unit id against the client's player, pet owners) if multiplayer or summoners need it.
auto CountedRaw(void* game, void* attacker, void* victim) noexcept -> bool {
	if (attacker == nullptr || victim == nullptr || Type(victim) != UnitMonster) {
		return false;
	}
	if (Type(attacker) == UnitMissile) {
		attacker = g_missileOwner(game, attacker);
	}
	return attacker != nullptr && Type(attacker) == UnitPlayer;
}

auto GuardedCounted(void* game, void* attacker, void* victim, int32_t* life) noexcept -> bool {
	__try {
		if (!CountedRaw(game, attacker, victim)) {
			return false;
		}
		*life = g_totalStat(victim, StatLife, 0);
		return true;
	} __except (EXCEPTION_EXECUTE_HANDLER) {
		return false;
	}
}

auto GuardedLife(void* victim, int32_t* life) noexcept -> bool {
	__try {
		*life = g_totalStat(victim, StatLife, 0);
		return true;
	} __except (EXCEPTION_EXECUTE_HANDLER) {
		return false;
	}
}

// The damage is the life the call took: after resistances, absorbs and the killing blow's overkill.
void DealDamage(void* game, void* attacker, void* victim, void* damage) noexcept {
	int32_t before = 0;
	const bool counted = g_feedOn && GuardedCounted(game, attacker, victim, &before);
	g_dealDamage(game, attacker, victim, damage);
	if (!counted) {
		return;
	}
	int32_t after = 0;
	if (!GuardedLife(victim, &after)) {
		if (plog::Sample(g_faults)) {
			plog::Warn("memory fault reading a monster's life after a hit");
		}
		return;
	}
	if (before > after) {
		g_pending += before - after;
	}
	++g_hits;
}

void AppendLayout(std::string* json, const dps::Settings& s) {
	const int unit = 2;  // layout units are twice the 1080p pixels the settings use
	const int height = static_cast<int>(s.textSize) * unit, width = 600 * unit;
	char buf[700];
	std::snprintf(buf, sizeof(buf),
	              "{\n\"type\": \"Panel\", \"name\": \"%s/DpsPanel\",\n\"fields\": { \"rect\": { \"x\": %d, \"y\": %d, \"width\": %d, "
	              "\"height\": %d }, \"anchor\": { \"x\": 0.5, \"y\": 0.0 } },\n\"children\": [\n"
	              "{ \"type\": \"TextBoxWidget\", \"name\": \"DpsText\", \"fields\": { \"text\": \"\", \"rect\": { \"x\": 0, \"y\": 0, "
	              "\"width\": %d, \"height\": %d }, \"style\": { \"fontColor\": { \"r\": 255, \"g\": 255, \"b\": 255, \"a\": 255 }, "
	              "\"pointSize\": %d, \"alignment\": { \"h\": \"center\", \"v\": \"center\" } } } }\n]\n}\n",
	              PluginId, static_cast<int>(s.offsetX) * unit - width / 2, static_cast<int>(s.offsetY) * unit, width, height,
	              width, height, height);
	*json += buf;
}

auto SetTextRaw(const char* text) noexcept -> bool {
	char full[64];
	std::snprintf(full, sizeof(full), "%s/DpsPanel", PluginId);
	void* const panel = g_nativeFindPanel(full);
	void* const box = panel != nullptr ? g_nativeFindChild(panel, "DpsText") : nullptr;
	if (box == nullptr) {
		return false;
	}
	const StringView view {text, std::strlen(text)};
	g_nativeSetText(box, &view);
	return true;
}

auto GuardedSetText(const char* text, bool* faulted) noexcept -> bool {
	__try {
		return SetTextRaw(text);
	} __except (EXCEPTION_EXECUTE_HANDLER) {
		*faulted = true;
		return false;
	}
}

// Something other than this plugin may close the panel (a game ends); it is opened again now and then.
void KeepPanelOpen() noexcept {
	if (g_panelOpen) {
		D2RL::Panels::PanelInfo info {};
		info.structSize = D2RL::Panels::PanelInfoSize;
		if (g_panelService->getPanelInfo(g_context, g_panel, &info) == D2RL::Panels::Result::Success &&
		    info.presentationState != D2RL::Panels::PresentationState::Open) {
			g_panelOpen = false;
		}
	}
	if (!g_panelOpen && g_tick % 30 == 1) {
		const auto result = g_panelService->openPanel(g_context, g_panel);
		g_panelResult = static_cast<int32_t>(result);
		g_panelOpen = result == D2RL::Panels::Result::Success;
		g_shown[0] = '?';  // set the text again in a panel that has just opened
		g_shown[1] = '\0';
	}
}

void __cdecl UiTask(const D2RL::PluginContext*, void*) noexcept {
	g_taskQueued = false;
	++g_tick;
	const double now = std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
	g_window->Add(now, g_pending.exchange(0) / 256);
	const double perSecond = g_window->PerSecond(now);
	g_lastDps = static_cast<int64_t>(perSecond);
	KeepPanelOpen();
	if (!g_panelOpen || g_nativeSetText == nullptr) {
		return;
	}
	char text[sizeof(g_shown)];
	dps::FormatDps(perSecond, text, sizeof(text));
	if (std::strcmp(text, g_shown) == 0) {
		return;
	}
	bool faulted = false;
	if (GuardedSetText(text, &faulted)) {
		std::memcpy(g_shown, text, sizeof(g_shown));
	} else if (faulted) {
		g_nativeSetText = nullptr;
		plog::Error("memory fault while setting the text; the counter is now hidden");
	}
}

void __cdecl OnFrame(const D2RL::PluginContext* context, const D2RL::Overlay::Frame*, void*) noexcept {
	++g_frames;
	if (!g_taskQueued.exchange(true) && g_threads->runOnUiThread(context, UiTask, nullptr) != D2RL::Threads::Result::Success) {
		g_taskQueued = false;
	}
}

auto FindSig(uintptr_t exeBase, const char* sig, const char* name, char* report, size_t size) noexcept -> const uint8_t* {
	dps::Pattern pattern {};
	if (!dps::ParsePattern(sig, &pattern)) {
		std::snprintf(report, size, "bad signature for %s", name);
		return nullptr;
	}
	const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(exeBase);
	const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(exeBase + static_cast<uintptr_t>(dos->e_lfanew));
	const IMAGE_SECTION_HEADER* section = IMAGE_FIRST_SECTION(nt);
	int found = 0;
	const uint8_t* match = nullptr;
	for (unsigned i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++section) {
		if ((section->Characteristics & IMAGE_SCN_MEM_EXECUTE) == 0 || section->Misc.VirtualSize == 0) {
			continue;
		}
		const auto* start = reinterpret_cast<const uint8_t*>(exeBase + section->VirtualAddress);
		size_t at = 0;
		const int n = dps::FindPattern(start, section->Misc.VirtualSize, pattern, &at);
		if (n > 0 && found == 0) {
			match = start + at;
		}
		found += n;
	}
	if (found != 1) {
		std::snprintf(report, size, "off: signature for %s found %d times", name, found);
		return nullptr;
	}
	return match;
}

auto Rel32Target(const uint8_t* instruction, size_t operandOffset) noexcept -> const uint8_t* {
	int32_t rel = 0;
	std::memcpy(&rel, instruction + operandOffset, sizeof(rel));
	return instruction + operandOffset + 4 + rel;
}

auto RegisterPanel(const D2RL::ResourceService* resources, char* report, size_t size) noexcept -> bool {
	std::string layout;
	AppendLayout(&layout, g_settings);
	char path[96];
	std::snprintf(path, sizeof(path), "data/global/ui/layouts/%s/DpsPanelhd.json", PluginId);
	D2RL::Resources::ResourceRegistration resource {};
	resource.structSize = D2RL::Resources::ResourceRegistrationSize;
	resource.path = path;
	resource.bytes = layout.data();
	resource.byteCount = layout.size();
	D2RL::Resources::RegistrationHandle resourceHandle = D2RL::Resources::InvalidHandle;
	const auto resourceResult = resources->registerResource(g_context, &resource, &resourceHandle);
	D2RL::Panels::PanelRegistration registration {};
	registration.structSize = D2RL::Panels::PanelRegistrationSize;
	registration.flags = D2RL::Panels::PanelFlags::None;
	registration.localId = "DpsPanel";
	const auto panelResult = g_panelService->registerPanel(g_context, &registration, &g_panel);
	if (resourceResult != D2RL::Resources::Result::Success || panelResult != D2RL::Panels::Result::Success) {
		std::snprintf(report, size, "off: panel refused (%u, %u)", static_cast<uint32_t>(resourceResult),
		              static_cast<uint32_t>(panelResult));
		return false;
	}
	return true;
}

}  // namespace

auto InstallCounter(const D2RL::PluginContext* context, char* report, size_t size) noexcept -> bool {
	g_context = context;
	static char toml[4096];
	if (context->ReadConfig(toml, sizeof(toml) - 1)) {
		g_settings = dps::ParseSettings(toml);
	} else {
		plog::Warn("config not readable; using defaults");
	}
	static dps::Window window(g_settings.windowSeconds);
	g_window = &window;
	if (context->QueryService(&g_overlay) != D2RL::ServiceQueryResult::Success ||
	    !D2RL::HasOverlayServiceField(g_overlay, D2RL::OverlayServiceRequiredSize)) {
		g_overlay = nullptr;
		std::snprintf(report, size, "off: no Overlay service");
		return false;
	}
	if (context->QueryService(&g_threads) != D2RL::ServiceQueryResult::Success || g_threads == nullptr) {
		g_threads = nullptr;
		std::snprintf(report, size, "off: no Thread service");
		return false;
	}
	if (context->QueryService(&g_panelService) != D2RL::ServiceQueryResult::Success ||
	    !D2RL::HasPanelServiceField(g_panelService, D2RL::PanelServiceRequiredSize)) {
		g_panelService = nullptr;
		std::snprintf(report, size, "off: no Panel service");
		return false;
	}
	const D2RL::ResourceService* resources = nullptr;
	if (context->QueryService(&resources) != D2RL::ServiceQueryResult::Success || resources == nullptr ||
	    resources->serviceSize < D2RL::ResourceServiceRequiredSize) {
		std::snprintf(report, size, "off: no Resource service");
		return false;
	}
	const uintptr_t base = context->exeBase;
	const uint8_t* const dealDamage = FindSig(base, DealDamageSig, "damage apply", report, size);
	const uint8_t* const missileOwner = FindSig(base, MissileOwnerSig, "missile owner", report, size);
	const uint8_t* const expBar = FindSig(base, ExpBarSig, "ExpBarWidget update", report, size);
	const uint8_t* const findPanel = FindSig(base, FindPanelSig, "FindPanel", report, size);
	const uint8_t* const findChild = FindSig(base, FindChildSig, "FindChild", report, size);
	const uint8_t* const setText = FindSig(base, SetTextSig, "SetText", report, size);
	if (dealDamage == nullptr || missileOwner == nullptr || expBar == nullptr || findPanel == nullptr ||
	    findChild == nullptr || setText == nullptr) {
		return false;
	}
	g_missileOwner = reinterpret_cast<MissileOwnerFn>(missileOwner);
	g_totalStat = reinterpret_cast<TotalStatFn>(Rel32Target(expBar + ExpBarStatCall, 1));
	g_nativeFindPanel = reinterpret_cast<NativeFindPanelFn>(findPanel);
	g_nativeFindChild = reinterpret_cast<NativeFindChildFn>(findChild);
	g_nativeSetText = reinterpret_cast<NativeSetTextFn>(setText);
	if (!RegisterPanel(resources, report, size)) {
		return false;
	}
	const uint64_t rva = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(dealDamage) - base);
	// The signature has just matched these bytes; the loader checks them again as it hooks.
	if (!context->InstallInlineHook(rva, dealDamage, DealDamageEntrySize, reinterpret_cast<void*>(&DealDamage),
	                                reinterpret_cast<void**>(&g_dealDamage))) {
		std::snprintf(report, size, "off: hook on the damage apply 0x%llx refused", static_cast<unsigned long long>(rva));
		return false;
	}
	g_feedOn = true;
	D2RL::Overlay::CallbackRegistration clock {};
	clock.structSize = D2RL::Overlay::CallbackRegistrationSize;
	clock.phase = D2RL::Overlay::Phase::AfterGameUi;
	clock.callback = OnFrame;
	const auto result = g_overlay->registerFrameCallback(context, &clock, &g_callback);
	if (result != D2RL::Overlay::Result::Success) {
		std::snprintf(report, size, "off: overlay callback refused (result %u)", static_cast<uint32_t>(result));
		return false;
	}
	std::snprintf(report, size, "on: damage apply rva 0x%llx, stat rva 0x%llx, window %.0f s",
	              static_cast<unsigned long long>(rva),
	              static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(g_totalStat) - base), g_settings.windowSeconds);
	return true;
}

void UninstallCounter() noexcept {
	g_feedOn = false;
	if (g_overlay != nullptr && g_callback != D2RL::Overlay::InvalidCallbackHandle) {
		(void)g_overlay->unregisterFrameCallback(g_context, g_callback);
		g_callback = D2RL::Overlay::InvalidCallbackHandle;
	}
}

void CounterStatus(char* out, size_t size) noexcept {
	std::snprintf(out, size, "feed %s, hits %llu, faults %llu, dps %lld, frames %llu, panel %s (open result %d), shown \"%s\"",
	              g_feedOn ? "on" : "off", static_cast<unsigned long long>(g_hits.load()),
	              static_cast<unsigned long long>(g_faults.load()), static_cast<long long>(g_lastDps.load()),
	              static_cast<unsigned long long>(g_frames.load()), g_panelOpen ? "open" : "closed", g_panelResult.load(), g_shown);
}
