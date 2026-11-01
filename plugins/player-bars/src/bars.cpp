// player-bars: reads and native bars. Evidence for every signature: docs/reverse-engineering/player-bars.md.
//
// The overlay frame callback is the clock: it queues one UI-thread task at a time and calls no game function. The task
// fetches the player on every run and never keeps the pointer: units die with the game. Each bar is a rectangle widget in
// a plugin panel whose width is set to fill it; three panels sit where the view can put the character.
#include "bars.h"

#include "bars_logic.h"
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

constexpr const char* PluginId = "player-bars";

// --- Engine: found by signature (values in comments are build 93847's) ----------------------------------------------

// ExpBarWidget update 0x14cc8c0. Its calls give the local slot 0x8b2d0, the player 0x9a480 and total stat 0x2f5020.
// Calling through the thunk D2RCore put on the stat function is intended: call it, never hook it.
constexpr const char* ExpBarSig =
	"40 53 48 83 EC 50 80 B9 68 01 00 00 00 48 8B D9 0F 85 ?? ?? ?? ?? 48 89 7C 24 70 E8 ?? ?? ?? ?? 8B C8 E8 ?? ?? ?? ?? "
	"48 8B F8 48 85 C0 0F 84 ?? ?? ?? ?? 48 89 6C 24 60 48 8D 15 ?? ?? ?? ?? 48 89 74 24 68 41 B8 23 00 00 00 4C 89 64 24 "
	"48 48 8B C8 4C 89 6C 24 40 4C 89 74 24 38 4C 89 7C 24 30 0F 29 74 24 20 E8 ?? ?? ?? ?? 45 33 C0 48 8B CF 44 8B F8 41 "
	"8D 50 0C E8 ?? ?? ?? ?? 45 33 C0 48 8B CF 44 8B E8 41 8D 50 0D E8 ?? ?? ?? ??";
constexpr size_t ExpBarSlotCall = 0x1b;
constexpr size_t ExpBarPlayerCall = 0x22;
constexpr size_t ExpBarStatCall = 0x8a;

// Screen shift setter 0xb9c20; its `mov [rip+x], ecx` at +0xf gives the global 0x2a26bb0.
constexpr const char* ScreenShiftSig =
	"48 83 EC 38 44 8B 0D ?? ?? ?? ?? 41 8B C1 99 89 0D ?? ?? ?? ?? 83 E2 03 44 8D 14 02 41 C1 FA 02";
constexpr size_t ScreenShiftWrite = 0xf;
// For the numbers and the HUD art: FindPanel 0x846170, FindChild 0x856220, TextBoxWidget::SetText 0x86dd50 and the
// widget's screen position 0x8562f0.
constexpr const char* FindPanelSig = "48 8B D1 48 8B 0D ?? ?? ?? ?? 48 85 C9 0F 85 ?? ?? ?? ?? 33 C0 C3";
constexpr const char* FindChildSig = "48 89 5C 24 10 48 89 74 24 18 57 48 83 EC 20 48 8B 59 58 48 8B F2 48 8B";
constexpr const char* SetTextSig = "40 53 48 83 EC 20 4C 8B 42 08 48 8B D9 48 8B 12 48 81 C1 88 00 00 00 E8";
constexpr const char* WidgetScreenPosSig = "48 8B C4 48 89 70 20 57 48 81 EC A0 00 00 00 0F 29 70 E8 48 8B F2 0F 29 78 D8 0F 57 FF 48 8B F9";

// The HUD image front_panel holds the globes, the statues and the frame behind the buttons. Image widgets draw through
// a clip rectangle (x, y, width, height in screen pixels), so the widget stays in place, showing only the frame.
// AbstractImageWidget::DrawFrame 0x858c50 loads that rectangle here, which gives its offset (+0xac).
constexpr const char* ImageClipSig = "0F 10 83 ?? ?? ?? ?? 0F 29 45 ?? 84 C0 74 ?? 0F 10 8B ?? ?? ?? ??";
constexpr size_t ImageClipDisplacement = 3;
constexpr int WidgetRect = 0x70;  // x, y, width, height in layout units
// BGClickCatcher in hudpanelhd.json; the controller layout inherits it.
constexpr int32_t CatcherStockX = 70, CatcherStockWidth = 2774;

// The stock HUD images, told apart by Overlay's x. Any other layout is left alone.
struct HudArt {
	int32_t x;           // Overlay's x in its layout
	float leftOfCentre;  // layout units from the screen centre to the image's left edge
	int32_t frameLeft, frameRight, height;  // in the image, whose pixels are layout units
};
constexpr HudArt HudArts[] = {
	{34, 1420.0f, 608, 2238, 415},    // PANEL/HUD_02/front_panel
	{-121, 1594.0f, 612, 2574, 413},  // Controller/HUD/HUDBase
};

// (unit, state) -> nonzero 0x3351b0
constexpr const char* HasStateSig =
	"48 89 5C 24 08 48 89 74 24 10 57 48 83 EC 20 8B DA 48 8B F1 E8 ?? ?? ?? ?? 85 C0 74 0E 83 E8 01";

constexpr int32_t StatLife = 6, StatMaxLife = 7, StatMana = 8, StatMaxMana = 9;
// The potion's state and rate stat, as AttributeBallWidget reads them (table 0x1fd89f0).
constexpr int32_t StateLifePotion = 100, StatLifePotion = 74, StateManaPotion = 106, StatManaPotion = 26;
constexpr float PotionPreviewAlpha = 0.35f;  // potionPreviewTransparency in hudpanelhd.json
constexpr int StatShift = 8;  // these four are stored as value << 8

using LocalSlotFn = uint32_t (*)() noexcept;
using PlayerForSlotFn = void* (*)(uint32_t slot) noexcept;
using TotalStatFn = int32_t (*)(void* unit, int32_t stat, int32_t layer) noexcept;
using HasStateFn = int32_t (*)(void* unit, int32_t state) noexcept;
struct StringView {
	const char* text;
	uint64_t size;
};
using NativeFindPanelFn = void* (*)(const char* name) noexcept;
using NativeFindChildFn = void* (*)(void* parent, const char* name) noexcept;
using NativeSetTextFn = void (*)(void* textBox, const StringView* text) noexcept;
using WidgetScreenPosFn = void* (*)(void* widget, int32_t* out) noexcept;

LocalSlotFn g_localSlot = nullptr;
PlayerForSlotFn g_playerForSlot = nullptr;
TotalStatFn g_totalStat = nullptr;
HasStateFn g_hasState = nullptr;
const int32_t* g_screenShift = nullptr;
NativeFindPanelFn g_nativeFindPanel = nullptr;
NativeFindChildFn g_nativeFindChild = nullptr;
NativeSetTextFn g_nativeSetText = nullptr;
WidgetScreenPosFn g_widgetScreenPos = nullptr;
int32_t g_imageClipRect = 0;

const D2RL::PluginContext* g_context = nullptr;
const D2RL::OverlayService* g_overlay = nullptr;
const D2RL::ThreadService* g_threads = nullptr;
const D2RL::WidgetService* g_widgets = nullptr;
const D2RL::PanelService* g_panelService = nullptr;
D2RL::Overlay::CallbackHandle g_callback = D2RL::Overlay::InvalidCallbackHandle;
pb::Settings g_settings;

// UI thread, except the counters.
struct BarsPanel {
	const char* localId;
	int shiftX;  // layout units from the screen centre
	D2RL::Panels::RegistrationHandle handle = D2RL::Panels::InvalidHandle;
	int32_t shownText[4] {-1, -1, -1, -1};  // life, max life, mana, max mana
};
BarsPanel g_panels[pb::PanelCount] = {{"BarsCenter", 0}, {"BarsLeft", -pb::ViewShiftUnits}, {"BarsRight", pb::ViewShiftUnits}};
int g_openPanel = -1;
int g_barFailures = 0;
constexpr int MaxBarFailures = 120;  // UI ticks, about two seconds
uint32_t g_tick = 0;
bool g_show = false;
int32_t g_life = 0, g_maxLife = 0, g_mana = 0, g_maxMana = 0, g_shift = 0;
int32_t g_lifeRate = 0, g_manaRate = 0;
pb::Smoother g_shownLife, g_shownMana;
pb::PotionPreview g_lifePotion, g_manaPotion;
int g_barWidth = 0, g_healthHeight = 0, g_manaHeight = 0;  // layout units
std::chrono::steady_clock::time_point g_lastUpdate;

std::atomic<bool> g_taskQueued{false};
std::atomic<bool> g_readsOn{false};
std::atomic<uint64_t> g_frames{0}, g_reads{0};
std::atomic<uint32_t> g_orbCalls{0};
std::atomic<int32_t> g_orbResult{-1};
std::atomic<int32_t> g_panelResult{-1};
std::atomic<float> g_screenWidth{0.0f};

void ReadRaw() noexcept {
	void* const player = g_playerForSlot(g_localSlot());
	if (player != nullptr) {
		g_life = g_totalStat(player, StatLife, 0) >> StatShift;
		g_maxLife = g_totalStat(player, StatMaxLife, 0) >> StatShift;
		g_mana = g_totalStat(player, StatMana, 0) >> StatShift;
		g_maxMana = g_totalStat(player, StatMaxMana, 0) >> StatShift;
		g_lifeRate = g_hasState(player, StateLifePotion) != 0 ? g_totalStat(player, StatLifePotion, 0) : 0;
		g_manaRate = g_hasState(player, StateManaPotion) != 0 ? g_totalStat(player, StatManaPotion, 0) : 0;
	}
	g_shift = g_screenShift != nullptr ? *g_screenShift : 0;
	g_show = player != nullptr;
	++g_reads;
}

auto GuardedRead() noexcept -> bool {
	__try {
		ReadRaw();
		return true;
	} __except (EXCEPTION_EXECUTE_HANDLER) {
		return false;
	}
}

// The HUD panel is rebuilt with the game, so this runs now and then.
void SetOrbsVisible(bool visible) noexcept {
	D2RL::Widgets::WidgetHandle hud = D2RL::Widgets::InvalidHandle;
	auto result = g_widgets->findPanel(g_context, "HUDPanel", &hud);
	if (result == D2RL::Widgets::Result::Success) {
		for (const char* name : {"HealthBall", "ManaBall"}) {
			D2RL::Widgets::WidgetHandle ball = D2RL::Widgets::InvalidHandle;
			result = g_widgets->findWidget(g_context, hud, name, &ball);
			if (result == D2RL::Widgets::Result::Success) {
				result = g_widgets->setWidgetVisible(g_context, ball, visible);
			}
			if (result != D2RL::Widgets::Result::Success) {
				break;
			}
		}
	}
	const auto last = static_cast<int32_t>(result);
	if (g_orbResult.exchange(last) != last) {
		plog::Info("orbs: result %d", last);
	}
	++g_orbCalls;
}

void ClipArtRaw(float screenWidth) noexcept {
	void* const hud = g_nativeFindPanel("HUDPanel");
	void* const art = hud != nullptr ? g_nativeFindChild(hud, "Overlay") : nullptr;
	if (art == nullptr) {
		return;
	}
	const int32_t artX = *reinterpret_cast<const int32_t*>(static_cast<const uint8_t*>(art) + WidgetRect);
	const HudArt* stock = nullptr;
	for (const HudArt& candidate : HudArts) {
		if (candidate.x == artX) {
			stock = &candidate;
		}
	}
	if (stock == nullptr) {
		static bool s_logged = false;
		if (!s_logged) {
			s_logged = true;
			plog::Info("art: this HUD layout is not a stock one; its art and clicks are left alone");
		}
		return;
	}
	// Hiding the catcher leaves it taking clicks, so it is narrowed to the frame. A layout that changed it is left alone.
	if (void* const catcher = g_nativeFindChild(hud, "BGClickCatcher")) {
		auto* const rect = reinterpret_cast<int32_t*>(static_cast<uint8_t*>(catcher) + WidgetRect);
		if (rect[0] == CatcherStockX && rect[2] == CatcherStockWidth) {
			rect[0] = stock->x + stock->frameLeft;
			rect[2] = stock->frameRight - stock->frameLeft;
		}
	}
	int32_t pos[2] {};
	g_widgetScreenPos(art, pos);
	const float scale = (screenWidth / 2.0f - static_cast<float>(pos[0])) / stock->leftOfCentre;
	auto* const clip = reinterpret_cast<int32_t*>(static_cast<uint8_t*>(art) + g_imageClipRect);
	clip[0] = pos[0] + static_cast<int32_t>(static_cast<float>(stock->frameLeft) * scale);
	clip[1] = pos[1];
	clip[2] = static_cast<int32_t>(static_cast<float>(stock->frameRight - stock->frameLeft) * scale);
	clip[3] = static_cast<int32_t>(static_cast<float>(stock->height) * scale);
}

auto GuardedClipArt(float screenWidth) noexcept -> bool {
	__try {
		ClipArtRaw(screenWidth);
		return true;
	} __except (EXCEPTION_EXECUTE_HANDLER) {
		return false;
	}
}

void AppendText(std::string* json, const char* name, int x, int y, int width, int height, bool comma) {
	char buf[320];
	std::snprintf(buf, sizeof(buf),
	              "{ \"type\": \"TextBoxWidget\", \"name\": \"%s\", \"fields\": { \"text\": \"\", \"rect\": { \"x\": %d, \"y\": %d, \"width\": %d, "
	              "\"height\": %d }, \"style\": { \"fontColor\": { \"r\": 255, \"g\": 255, \"b\": 255, \"a\": 255 }, \"pointSize\": %d, "
	              "\"alignment\": { \"h\": \"center\", \"v\": \"center\" } } } }%s\n",
	              name, x, y, width, height, height, comma ? "," : "");
	*json += buf;
}

void AppendRect(std::string* json, const char* name, int x, int y, int width, int height, const pb::Rgb& c, float alpha, bool comma) {
	char buf[260];
	std::snprintf(buf, sizeof(buf),
	              "{ \"type\": \"RectangleWidget\", \"name\": \"%s\", \"fields\": { \"rect\": { \"x\": %d, \"y\": %d, \"width\": %d, "
	              "\"height\": %d }, \"color\": [ %.3f, %.3f, %.3f, %.2f ] } }%s\n",
	              name, x, y, width, height, c.r, c.g, c.b, alpha, comma ? "," : "");
	*json += buf;
}

// Layout units are twice the 1080p pixels the settings use, and D2R scales them with the screen.
auto BuildLayout(const BarsPanel& panel, const pb::Settings& s) -> std::string {
	const int unit = 2, border = unit, gap = 2 * unit;
	const int width = static_cast<int>(s.barWidth) * unit, healthHeight = static_cast<int>(s.healthHeight) * unit;
	const int manaHeight = static_cast<int>(s.manaHeight) * unit;
	g_barWidth = width;
	g_healthHeight = healthHeight;
	g_manaHeight = manaHeight;
	const int height = healthHeight + gap + manaHeight + 2 * border;
	const int total = width + 2 * border;
	const int top = s.below ? static_cast<int>(s.offsetBelow) * unit : -static_cast<int>(s.offsetY) * unit - height;
	char buf[300];
	std::snprintf(buf, sizeof(buf),
	              "{\n\"type\": \"Panel\", \"name\": \"%s/%s\",\n\"fields\": { \"rect\": { \"x\": %d, \"y\": %d, \"width\": %d, \"height\": %d }, "
	              "\"anchor\": { \"x\": 0.5, \"y\": 0.5 } },\n\"children\": [\n",
	              PluginId, panel.localId, panel.shiftX - total / 2, top, total, height);
	std::string json = buf;
	const int manaTop = border + healthHeight + gap;
	AppendRect(&json, "Border", 0, 0, total, height, s.borderColor, 1.0f, true);
	AppendRect(&json, "HealthBack", border, border, width, healthHeight, s.backColor, 1.0f, true);
	AppendRect(&json, "ManaBack", border, manaTop, width, manaHeight, s.backColor, 1.0f, true);
	AppendRect(&json, "HealthPreview", border, border, 0, healthHeight, s.healthColor, PotionPreviewAlpha, true);
	AppendRect(&json, "ManaPreview", border, manaTop, 0, manaHeight, s.manaColor, PotionPreviewAlpha, true);
	AppendRect(&json, "HealthFill", border, border, 0, healthHeight, s.healthColor, 1.0f, true);
	AppendRect(&json, "ManaFill", border, manaTop, 0, manaHeight, s.manaColor, 1.0f, s.showText);
	if (s.showText) {
		AppendText(&json, "HealthText", border, border, width, healthHeight, true);
		AppendText(&json, "ManaText", border, manaTop, width, manaHeight, false);
	}
	json += "]\n}\n";
	return json;
}

// The widget's width in layout units. The height check keeps this to the widgets BuildLayout made.
auto SetWidthRaw(void* panel, const char* name, int height, float fraction) noexcept -> bool {
	void* const widget = g_nativeFindChild(panel, name);
	if (widget == nullptr) {
		return false;
	}
	auto* const rect = reinterpret_cast<int32_t*>(static_cast<uint8_t*>(widget) + WidgetRect);
	if (rect[3] != height) {
		return false;
	}
	rect[2] = static_cast<int32_t>(static_cast<float>(g_barWidth) * fraction + 0.5f);
	return true;
}

auto SetBarsRaw(const BarsPanel& panel, float life, float lifePreview, float mana, float manaPreview) noexcept -> bool {
	char full[64];
	std::snprintf(full, sizeof(full), "%s/%s", PluginId, panel.localId);
	void* const native = g_nativeFindPanel(full);
	return native != nullptr && SetWidthRaw(native, "HealthPreview", g_healthHeight, lifePreview) &&
	       SetWidthRaw(native, "HealthFill", g_healthHeight, life) && SetWidthRaw(native, "ManaPreview", g_manaHeight, manaPreview) &&
	       SetWidthRaw(native, "ManaFill", g_manaHeight, mana);
}

auto GuardedSetBars(const BarsPanel& panel, float life, float lifePreview, float mana, float manaPreview) noexcept -> bool {
	__try {
		return SetBarsRaw(panel, life, lifePreview, mana, manaPreview);
	} __except (EXCEPTION_EXECUTE_HANDLER) {
		return false;
	}
}

// Empty bars with the orbs hidden would leave the player with no health display, so the orbs come back.
void StopBars(const char* why) noexcept {
	g_readsOn = false;
	g_show = false;
	if (g_settings.hideOrbs) {
		SetOrbsVisible(true);
	}
	plog::Error("%s; the bars are now off", why);
}

void SetTextRaw(BarsPanel& panel, const char* childName, int32_t value, int32_t max) noexcept {
	char full[64];
	std::snprintf(full, sizeof(full), "%s/%s", PluginId, panel.localId);
	void* const native = g_nativeFindPanel(full);
	void* const box = native != nullptr ? g_nativeFindChild(native, childName) : nullptr;
	if (box != nullptr) {
		char text[32];
		const int n = std::snprintf(text, sizeof(text), "%d / %d", value, max);
		const StringView view {text, static_cast<uint64_t>(n)};
		g_nativeSetText(box, &view);
	}
}

auto GuardedSetText(BarsPanel& panel, const char* childName, int32_t value, int32_t max) noexcept -> bool {
	__try {
		SetTextRaw(panel, childName, value, max);
		return true;
	} __except (EXCEPTION_EXECUTE_HANDLER) {
		return false;
	}
}

void UpdateText(BarsPanel& panel) noexcept {
	if (g_nativeSetText == nullptr) {
		return;
	}
	bool ok = true;
	if (panel.shownText[0] != g_life || panel.shownText[1] != g_maxLife) {
		ok = GuardedSetText(panel, "HealthText", g_life, g_maxLife);
		panel.shownText[0] = g_life;
		panel.shownText[1] = g_maxLife;
	}
	if (ok && (panel.shownText[2] != g_mana || panel.shownText[3] != g_maxMana)) {
		ok = GuardedSetText(panel, "ManaText", g_mana, g_maxMana);
		panel.shownText[2] = g_mana;
		panel.shownText[3] = g_maxMana;
	}
	if (!ok) {
		g_nativeSetText = nullptr;
		plog::Error("memory fault while setting the text; the numbers are now off");
	}
}

void SetOpenPanel(int index) noexcept {
	if (index == g_openPanel) {
		return;
	}
	if (g_openPanel >= 0) {
		(void)g_panelService->closePanel(g_context, g_panels[g_openPanel].handle);
	}
	g_openPanel = -1;
	if (index >= 0) {
		const auto result = g_panelService->openPanel(g_context, g_panels[index].handle);
		g_panelResult = static_cast<int32_t>(result);
		if (result == D2RL::Panels::Result::Success) {
			g_openPanel = index;
			std::memset(g_panels[index].shownText, 0xff, sizeof(g_panels[index].shownText));
		}
	}
}

// Something other than this plugin may close the panel; the next call then opens it again.
void NoticeClosedPanel() noexcept {
	if (g_openPanel < 0) {
		return;
	}
	D2RL::Panels::PanelInfo info {};
	info.structSize = D2RL::Panels::PanelInfoSize;
	if (g_panelService->getPanelInfo(g_context, g_panels[g_openPanel].handle, &info) == D2RL::Panels::Result::Success &&
	    info.presentationState != D2RL::Panels::PresentationState::Open) {
		g_openPanel = -1;
	}
}

void UpdateBars() noexcept {
	NoticeClosedPanel();
	SetOpenPanel(g_show ? pb::PanelForShift(g_shift) : -1);
	if (g_openPanel < 0) {
		return;
	}
	BarsPanel& panel = g_panels[g_openPanel];
	const auto now = std::chrono::steady_clock::now();
	const float seconds = std::chrono::duration<float>(now - g_lastUpdate).count();
	g_lastUpdate = now;
	g_shownLife.Step(static_cast<float>(g_life), static_cast<float>(g_maxLife), g_lifeRate > 0, seconds);
	g_shownMana.Step(static_cast<float>(g_mana), static_cast<float>(g_maxMana), g_manaRate > 0, seconds);
	const float life = pb::Fraction(static_cast<int32_t>(g_shownLife.shown + 0.5f), g_maxLife);
	const float mana = pb::Fraction(static_cast<int32_t>(g_shownMana.shown + 0.5f), g_maxMana);
	const float lifePreview = pb::PreviewFraction(g_life, g_lifePotion.Remaining(g_lifeRate, g_life), g_maxLife);
	const float manaPreview = pb::PreviewFraction(g_mana, g_manaPotion.Remaining(g_manaRate, g_mana), g_maxMana);
	// A panel that has just opened may need a moment to appear, so only a lasting failure counts.
	g_barFailures = GuardedSetBars(panel, life, lifePreview, mana, manaPreview) ? 0 : g_barFailures + 1;
	if (g_barFailures >= MaxBarFailures) {
		StopBars("could not set the bars' widths");
		return;
	}
	UpdateText(panel);
}

void __cdecl UiTask(const D2RL::PluginContext*, void*) noexcept {
	g_taskQueued = false;
	if (g_readsOn && !GuardedRead()) {
		StopBars("memory fault while reading the player");
	}
	UpdateBars();
	if (g_settings.hideOrbs && g_show && ++g_tick % 30 == 1) {
		SetOrbsVisible(false);
		if (g_widgetScreenPos != nullptr && !GuardedClipArt(g_screenWidth)) {
			g_widgetScreenPos = nullptr;
			plog::Error("memory fault while clipping the HUD art; it is left as it is");
		}
	}
}

// Clock only: D2RLoader's overlay runs its callbacks but nothing it draws reaches the screen
// (docs/reverse-engineering/player-bars.md), so the bars are native widgets.
void __cdecl OnFrame(const D2RL::PluginContext* context, const D2RL::Overlay::Frame* frame, void*) noexcept {
	++g_frames;
	if (frame != nullptr) {
		g_screenWidth = frame->screenWidth;
	}
	if (!g_taskQueued.exchange(true) && g_threads->runOnUiThread(context, UiTask, nullptr) != D2RL::Threads::Result::Success) {
		g_taskQueued = false;
	}
}

auto FindSig(uintptr_t exeBase, const char* sig, const char* name, char* report, size_t size) noexcept -> const uint8_t* {
	pb::Pattern pattern {};
	if (!pb::ParsePattern(sig, &pattern)) {
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
		const int n = pb::FindPattern(start, section->Misc.VirtualSize, pattern, &at);
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

auto Resolve(uintptr_t exeBase, char* report, size_t size) noexcept -> bool {
	const uint8_t* const expBar = FindSig(exeBase, ExpBarSig, "ExpBarWidget update", report, size);
	const uint8_t* const hasState = FindSig(exeBase, HasStateSig, "has state", report, size);
	const uint8_t* const findPanel = FindSig(exeBase, FindPanelSig, "FindPanel", report, size);
	const uint8_t* const findChild = FindSig(exeBase, FindChildSig, "FindChild", report, size);
	if (expBar == nullptr || hasState == nullptr || findPanel == nullptr || findChild == nullptr) {
		return false;
	}
	g_hasState = reinterpret_cast<HasStateFn>(hasState);
	g_nativeFindPanel = reinterpret_cast<NativeFindPanelFn>(findPanel);
	g_nativeFindChild = reinterpret_cast<NativeFindChildFn>(findChild);
	g_localSlot = reinterpret_cast<LocalSlotFn>(Rel32Target(expBar + ExpBarSlotCall, 1));
	g_playerForSlot = reinterpret_cast<PlayerForSlotFn>(Rel32Target(expBar + ExpBarPlayerCall, 1));
	g_totalStat = reinterpret_cast<TotalStatFn>(Rel32Target(expBar + ExpBarStatCall, 1));
	char missing[120];
	if (const uint8_t* const shift = FindSig(exeBase, ScreenShiftSig, "screen shift", missing, sizeof(missing))) {
		g_screenShift = reinterpret_cast<const int32_t*>(Rel32Target(shift + ScreenShiftWrite, 2));
	} else {
		plog::Warn("%s: bars stay centred when a side panel is open", missing);
	}
	const uint8_t* const setText = FindSig(exeBase, SetTextSig, "SetText", missing, sizeof(missing));
	const uint8_t* const screenPos = FindSig(exeBase, WidgetScreenPosSig, "widget position", missing, sizeof(missing));
	const uint8_t* const imageClip = FindSig(exeBase, ImageClipSig, "image clip", missing, sizeof(missing));
	g_nativeSetText = g_settings.showText ? reinterpret_cast<NativeSetTextFn>(setText) : nullptr;
	if (screenPos != nullptr && imageClip != nullptr) {
		g_widgetScreenPos = reinterpret_cast<WidgetScreenPosFn>(screenPos);
		std::memcpy(&g_imageClipRect, imageClip + ImageClipDisplacement, sizeof(g_imageClipRect));
	}
	if (setText == nullptr || g_widgetScreenPos == nullptr) {
		plog::Warn("%s: no numbers on the bars, or the HUD art stays whole", missing);
	}
	return true;
}

auto RegisterPanel(BarsPanel& panel, const D2RL::ResourceService* resources, char* report, size_t size) noexcept -> bool {
	const std::string layout = BuildLayout(panel, g_settings);
	char path[96];
	std::snprintf(path, sizeof(path), "data/global/ui/layouts/%s/%shd.json", PluginId, panel.localId);
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
	registration.localId = panel.localId;
	const auto panelResult = g_panelService->registerPanel(g_context, &registration, &panel.handle);
	if (resourceResult != D2RL::Resources::Result::Success || panelResult != D2RL::Panels::Result::Success) {
		std::snprintf(report, size, "off: panel %s refused (%u, %u)", panel.localId, static_cast<uint32_t>(resourceResult),
		              static_cast<uint32_t>(panelResult));
		return false;
	}
	return true;
}

}  // namespace

auto InstallBars(const D2RL::PluginContext* context, char* report, size_t size) noexcept -> bool {
	g_context = context;
	static char toml[4096];
	if (context->ReadConfig(toml, sizeof(toml) - 1)) {
		g_settings = pb::ParseSettings(toml);
	} else {
		plog::Warn("config not readable; using defaults");
	}
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
	if (context->QueryService(&g_widgets) != D2RL::ServiceQueryResult::Success ||
	    !D2RL::HasWidgetServiceField(g_widgets, D2RL::WidgetServiceRequiredSize)) {
		g_widgets = nullptr;
		std::snprintf(report, size, "off: no Widget service");
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
	if (!Resolve(context->exeBase, report, size)) {
		return false;
	}
	for (BarsPanel& panel : g_panels) {
		if (!RegisterPanel(panel, resources, report, size)) {
			return false;
		}
	}
	g_readsOn = true;
	D2RL::Overlay::CallbackRegistration clock {};
	clock.structSize = D2RL::Overlay::CallbackRegistrationSize;
	clock.phase = D2RL::Overlay::Phase::AfterGameUi;
	clock.callback = OnFrame;
	const auto result = g_overlay->registerFrameCallback(context, &clock, &g_callback);
	if (result != D2RL::Overlay::Result::Success) {
		std::snprintf(report, size, "off: overlay callback refused (result %u)", static_cast<uint32_t>(result));
		return false;
	}
	std::snprintf(report, size, "on: slot rva 0x%llx, stat rva 0x%llx, shift %s, image clip +0x%x",
	              static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(g_localSlot) - context->exeBase),
	              static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(g_totalStat) - context->exeBase),
	              g_screenShift != nullptr ? "found" : "missing", static_cast<unsigned>(g_imageClipRect));
	return true;
}

void UninstallBars() noexcept {
	if (g_overlay != nullptr && g_callback != D2RL::Overlay::InvalidCallbackHandle) {
		(void)g_overlay->unregisterFrameCallback(g_context, g_callback);
		g_callback = D2RL::Overlay::InvalidCallbackHandle;
	}
}

void BarsStatus(char* out, size_t size) noexcept {
	std::snprintf(out, size,
	              "reads %s (%llu), frames %llu, shown %d, life %d/%d, mana %d/%d, shift %d, panel %d (open result %d), shown life %.1f, potion rate %d, "
	              "orb calls %u (last result %d)",
	              g_readsOn ? "on" : "off", static_cast<unsigned long long>(g_reads.load()), static_cast<unsigned long long>(g_frames.load()),
	              g_show ? 1 : 0, g_life, g_maxLife, g_mana, g_maxMana, g_shift, g_openPanel, g_panelResult.load(),
	              g_shownLife.shown, g_lifeRate, g_orbCalls.load(), g_orbResult.load());
}
