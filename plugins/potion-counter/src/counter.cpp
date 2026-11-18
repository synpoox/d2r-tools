// potion-counter: belt squares. Evidence for every signature: docs/reverse-engineering/potion-counter.md.
//
// The overlay frame callback is the clock: it queues one UI-thread task at a time. The task reads the local player's
// belt through the SDK, follows the live HUD layout (the Belt widget and its hotkey labels) so the squares sit above
// the belt slots in any layout, and shows one square per belt row in a plugin panel: the game's own belt slot frame
// with the item's inventory picture (ImageWidgets), or a gold mark for an item with no stock picture. The squares hide
// while the belt is popped up. It moves no item.
#include "counter.h"

#include "counter_logic.h"
#include "log.h"

#define NOMINMAX
#include <windows.h>

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

namespace {

constexpr const char* PluginId = "potion-counter";
// One panel per belt row count (Counts1-Counts4), so each has squares as big as its row fits.
constexpr const char* PanelPrefix = "potion-counter/Counts";

// --- Engine: found by signature (values in comments are build 93847's) ----------------------------------------------

// FindPanel 0x846170, FindChild 0x856220 (as player-bars finds them).
constexpr const char* FindPanelSig = "48 8B D1 48 8B 0D ?? ?? ?? ?? 48 85 C9 0F 85 ?? ?? ?? ?? 33 C0 C3";
constexpr const char* FindChildSig = "48 89 5C 24 10 48 89 74 24 18 57 48 83 EC 20 48 8B 59 58 48 8B F2 48 8B";
// Widget::SetVisible 0x8580d0 keeps the flag at the byte its first instruction reads (+0x51).
constexpr const char* SetVisibleSig = "0F B6 41 ?? 88 51 ?? 3A C2 74 ?? 48 8B 01 84 D2 74 ?? 48 FF 60 ?? 48 FF 60 ?? C3";
constexpr size_t SetVisibleFlag = 3;
// BeltWidget update 0x228db0 stores the belt's box count (0x15f140, belts.txt numboxes) at Belt+0x6a4; the field
// offset is the last 4 bytes.
constexpr const char* BeltBoxesSig = "48 8B CB 4C 8B E8 E8 ?? ?? ?? ?? B9 09 00 00 00 41 89 86 ?? ?? ?? ??";
constexpr size_t BeltBoxesField = 19;
// The same update stores whether the belt is popped up (belt UI open, or the mouse on the belt) at Belt+0x6a0; the
// field offset is the last 4 bytes.
constexpr const char* BeltPoppedSig = "B9 1A 00 00 00 E8 ?? ?? ?? ?? 84 C0 74 04 B0 01 EB 08 49 8B CE E8 ?? ?? ?? ?? 41 88 86 ?? ?? ?? ??";
constexpr size_t BeltPoppedField = 29;
constexpr int WidgetRect = 0x70;  // x, y, width, height in layout units, local to the parent (SetPosition 0x856fb0)

// hudpanelhd.json: HUDPanel anchored at the bottom centre, its Belt, and the belt's HotkeyLabel0-3 (the template at
// x 2, y 60 plus the cell offset 12, 6 and a column pitch of 98 + 27). Only the first layout uses these: the task
// copies the live values over them.
constexpr int32_t StockHudX = -1454, StockHudY = -412, StockBeltX = 1704, StockBeltY = 278;
constexpr int32_t StockLabelX = 14, StockLabelY = 66, StockPitch = 125;
// shortcut: the cell's top-left is taken as the label's position less the stock template's (2, 60); a layout that
// moves HotkeyLabelTemplate inside the cell moves the squares with it. Upgrade: read the Belt's cellOffsetInRow.
constexpr int32_t LabelInCellX = 2, LabelInCellY = 60;
constexpr int32_t CellWidth = 98;
// Squares in layout units (two per 1080p pixel): one per belt row, centred over the cell, just above it on the frame.
// Four rows must fit the 125-unit column pitch; fewer rows get bigger squares.
constexpr int32_t SquareGap = 3, MaxSquare = 40;
constexpr auto SquareOf(int rows) noexcept -> int32_t {
	return (StockPitch - 4 - (rows - 1) * SquareGap) / rows < MaxSquare ? (StockPitch - 4 - (rows - 1) * SquareGap) / rows : MaxSquare;
}
constexpr int32_t PanelWidth = 600;  // one row of squares high, so the panel covers none of the belt slots
constexpr int32_t PanelPriority = 101;  // just above HUDPanel (100), or the HUD art covers the squares
// The frame is the first cell of the game's own popped belt row (HUD_02/belt_popped_row, 493 x 113, four framed cells
// with their dark fill), cut out by a ClipViewWidget the square's size; the square is as tall as that cell's shape.
constexpr const char* FrameSprite = "PANEL/HUD_02/belt_popped_row";
constexpr float FrameCellWidth = 493 / 4.0f, FrameCellHeight = 113;
constexpr auto HeightOf(int32_t square) noexcept -> int32_t {
	return static_cast<int32_t>(square * FrameCellHeight / FrameCellWidth + 0.5f);
}
constexpr int32_t IconInset = 3;  // the inventory pictures (98 x 98) sit inside the frame's edge

using NativeFindPanelFn = void* (*)(const char* name) noexcept;
using NativeFindChildFn = void* (*)(void* parent, const char* name) noexcept;
using NativeSetVisibleFn = void (*)(void* widget, bool visible) noexcept;

NativeFindPanelFn g_nativeFindPanel = nullptr;
NativeFindChildFn g_nativeFindChild = nullptr;
NativeSetVisibleFn g_nativeSetVisible = nullptr;
int g_visibleFlag = -1;  // offset of the widget's visible byte
int32_t g_beltBoxesField = -1;  // -1: unknown, every column shows Rows squares
int32_t g_beltPoppedField = -1;  // -1: unknown, the squares stay while the belt is popped up

const D2RL::PluginContext* g_context = nullptr;
const D2RL::OverlayService* g_overlay = nullptr;
const D2RL::ThreadService* g_threads = nullptr;
const D2RL::PanelService* g_panelService = nullptr;
const D2RL::InventoryService* g_inventory = nullptr;
D2RL::Overlay::CallbackHandle g_callback = D2RL::Overlay::InvalidCallbackHandle;
D2RL::Panels::RegistrationHandle g_panels[pc::Rows] {};  // [rows - 1]

// Where the squares go, in the panel's parent (the screen, anchored like HUDPanel) and in the panel.
struct Layout {
	bool hudShown = false;
	bool popped = false;  // the belt's own rows are popped up over where the squares go
	int32_t panelX = 0, panelY = 0;
	int32_t rows = pc::Rows;
	int32_t columnX[pc::Columns] {};  // the cell's left edge in the panel
	auto operator==(const Layout&) const -> bool = default;
};

// UI thread
bool g_on = false;
int g_openRows = 0;  // the open panel's row count; 0: none open
Layout g_appliedLayout;
pc::Kind g_shown[pc::Columns][pc::Rows] {};
bool g_shownValid = false;  // false: every square and position is set again (a panel that has just opened)
int g_counts[pc::Columns] {-1, -1, -1, -1};
int g_loggedChanges = 0;
constexpr int MaxLoggedChanges = 20;
std::atomic<bool> g_taskQueued{false};
std::atomic<uint64_t> g_frames{0};
std::atomic<int32_t> g_panelResult{-1};
std::atomic<int32_t> g_playerResult{-1};
std::atomic<int32_t> g_itemsResult{-1};

auto Rect(void* widget) noexcept -> int32_t* {
	return reinterpret_cast<int32_t*>(static_cast<uint8_t*>(widget) + WidgetRect);
}

auto Shown(void* widget) noexcept -> bool {
	return static_cast<const uint8_t*>(widget)[g_visibleFlag] != 0;
}

void ReadLayoutRaw(Layout* out) noexcept {
	void* const hud = g_nativeFindPanel("HUDPanel");
	void* const belt = hud != nullptr ? g_nativeFindChild(hud, "Belt") : nullptr;
	out->hudShown = belt != nullptr && Shown(hud) && Shown(belt);
	if (!out->hudShown) {
		return;
	}
	const auto* const beltBytes = static_cast<const uint8_t*>(belt);
	const int32_t* const hudRect = Rect(hud);
	const int32_t* const beltRect = Rect(belt);
	int32_t labelY = StockLabelY;
	for (int c = 0; c < pc::Columns; ++c) {
		char name[24];
		std::snprintf(name, sizeof(name), "HotkeyLabel%d", c);
		void* const label = g_nativeFindChild(belt, name);
		const int32_t labelX = label != nullptr ? Rect(label)[0] : StockLabelX + c * StockPitch;
		labelY = label != nullptr ? Rect(label)[1] : labelY;
		out->columnX[c] = labelX - LabelInCellX;
	}
	if (g_beltBoxesField >= 0) {
		out->rows = pc::RowsOf(*reinterpret_cast<const int32_t*>(beltBytes + g_beltBoxesField));
	}
	// The game draws the popped rows only for a belt of more than one row; then the squares would sit on them.
	out->popped = g_beltPoppedField >= 0 && out->rows > 1 && beltBytes[g_beltPoppedField] != 0;
	out->panelX = hudRect[0] + beltRect[0];
	out->panelY = hudRect[1] + beltRect[1] + labelY - LabelInCellY - HeightOf(SquareOf(out->rows)) - 2;
}

auto GuardedReadLayout(Layout* out) noexcept -> bool {
	__try {
		ReadLayoutRaw(out);
		return true;
	} __except (EXCEPTION_EXECUTE_HANDLER) {
		return false;
	}
}

auto SquareX(const Layout& layout, int column, int row) noexcept -> int32_t {
	const int32_t square = SquareOf(layout.rows);
	const int32_t width = layout.rows * square + (layout.rows - 1) * SquareGap;
	return layout.columnX[column] + (CellWidth - width) / 2 + row * (square + SquareGap);
}

void Show(void* parent, const char* name, bool visible) noexcept {
	if (void* const widget = g_nativeFindChild(parent, name)) {
		g_nativeSetVisible(widget, visible);
	}
}

// Moves the panel and its squares to the live layout and shows what changed. Each square holds the frame, the Other
// mark and one picture per icon; only the current item's picture (or the mark) is visible.
void ApplyRaw(const Layout& layout, const pc::Kind squares[pc::Columns][pc::Rows]) noexcept {
	char panelName[48];
	std::snprintf(panelName, sizeof(panelName), "%s%d", PanelPrefix, layout.rows);
	void* const panel = g_nativeFindPanel(panelName);
	if (panel == nullptr) {
		return;
	}
	const bool moved = !g_shownValid || !(layout == g_appliedLayout);
	Rect(panel)[0] = layout.panelX;
	Rect(panel)[1] = layout.panelY;
	for (int c = 0; c < pc::Columns; ++c) {
		for (int r = 0; r < layout.rows; ++r) {
			const pc::Kind kind = squares[c][r];
			if (!moved && kind == g_shown[c][r]) {
				continue;
			}
			char name[16];
			std::snprintf(name, sizeof(name), "S%d%d", c, r);
			void* const square = g_nativeFindChild(panel, name);
			if (square == nullptr) {
				continue;
			}
			Rect(square)[0] = SquareX(layout, c, r);
			Show(square, "O", kind == pc::Other);
			for (int i = 0; i < pc::IconCount; ++i) {
				std::snprintf(name, sizeof(name), "I%d", i);
				Show(square, name, kind == i + 1);
			}
			g_shown[c][r] = kind;
		}
	}
	g_appliedLayout = layout;
	g_shownValid = true;
}

auto GuardedApply(const Layout& layout, const pc::Kind squares[pc::Columns][pc::Rows]) noexcept -> bool {
	__try {
		ApplyRaw(layout, squares);
		return true;
	} __except (EXCEPTION_EXECUTE_HANDLER) {
		return false;
	}
}

struct BeltItems {
	pc::BeltItem items[64];
	size_t n;
};

auto __cdecl CollectItem(const D2RL::PluginContext*, const D2RL::Items::ItemInfo* item, void* userData) noexcept
	-> D2RL::Inventory::IterationAction {
	auto* const items = static_cast<BeltItems*>(userData);
	if (item != nullptr && item->container == D2RL::Items::ItemContainer::Belt && items->n < sizeof(items->items) / sizeof(items->items[0])) {
		items->items[items->n++] = {item->x, item->code};
	}
	return D2RL::Inventory::IterationAction::Continue;
}

// False when there is no local player (menus, loading).
auto ReadBelt(BeltItems* items) noexcept -> bool {
	items->n = 0;
	D2RL::PlayerHandle player {};
	const auto playerResult = g_inventory->getLocalPlayer(g_context, &player);
	g_playerResult = static_cast<int32_t>(playerResult);
	if (playerResult != D2RL::Inventory::Result::Success) {
		return false;
	}
	D2RL::Inventory::ItemFilter filter {};
	filter.structSize = D2RL::Inventory::ItemFilterSize;
	filter.containerMask = D2RL::Items::ContainerBit(D2RL::Items::ItemContainer::Belt);
	const auto result = g_inventory->forEachInventoryItem(g_context, player, &filter, CollectItem, items);
	g_itemsResult = static_cast<int32_t>(result);
	return result == D2RL::Inventory::Result::Success;
}

void LogChange(const BeltItems& items) noexcept {
	if (g_loggedChanges >= MaxLoggedChanges) {
		return;
	}
	if (++g_loggedChanges == MaxLoggedChanges) {
		plog::Info("belt changes are no longer logged");
	}
	char list[400] = "";
	size_t used = 0;
	for (size_t i = 0; i < items.n && used + 12 < sizeof(list); ++i) {
		const uint32_t code = items.items[i].code;
		used += static_cast<size_t>(std::snprintf(list + used, sizeof(list) - used, " %d:%c%c%c", items.items[i].x,
		                                          static_cast<char>(code & 0xff), static_cast<char>((code >> 8) & 0xff),
		                                          static_cast<char>((code >> 16) & 0xff)));
	}
	plog::Info("belt: %d %d %d %d (slot:item%s)", g_counts[0], g_counts[1], g_counts[2], g_counts[3], list);
}

// rows 0 closes the open panel; otherwise the panel for that many rows is the one open.
void SetPanelOpen(int rows) noexcept {
	if (g_openRows != 0) {
		D2RL::Panels::PanelInfo info {};
		info.structSize = D2RL::Panels::PanelInfoSize;
		if (g_panelService->getPanelInfo(g_context, g_panels[g_openRows - 1], &info) == D2RL::Panels::Result::Success &&
		    info.presentationState != D2RL::Panels::PresentationState::Open) {
			g_openRows = 0;  // closed by something else (the game ended)
		}
	}
	if (rows == g_openRows) {
		return;
	}
	if (g_openRows != 0) {
		g_panelResult = static_cast<int32_t>(g_panelService->closePanel(g_context, g_panels[g_openRows - 1]));
		g_openRows = 0;
	}
	if (rows != 0) {
		const auto result = g_panelService->openPanel(g_context, g_panels[rows - 1]);
		g_panelResult = static_cast<int32_t>(result);
		g_openRows = result == D2RL::Panels::Result::Success ? rows : 0;
	}
	g_shownValid = false;  // a panel that has just opened gets every square again
}

void __cdecl UiTask(const D2RL::PluginContext*, void*) noexcept {
	g_taskQueued = false;
	if (!g_on) {
		return;
	}
	Layout layout;
	if (!GuardedReadLayout(&layout)) {
		g_on = false;
		SetPanelOpen(0);
		plog::Error("memory fault while reading the HUD layout; the counter is now off");
		return;
	}
	BeltItems items;
	const bool inGame = ReadBelt(&items);
	pc::Kind squares[pc::Columns][pc::Rows];
	pc::FillSquares(items.items, items.n, squares);
	int counts[pc::Columns];
	for (int c = 0; c < pc::Columns; ++c) {
		counts[c] = pc::CountColumn(squares[c]);
	}
	if (inGame && std::memcmp(counts, g_counts, sizeof(counts)) != 0) {
		std::memcpy(g_counts, counts, sizeof(counts));
		LogChange(items);
	}
	static bool s_popped = false;
	if (inGame && layout.popped != s_popped && g_loggedChanges < MaxLoggedChanges) {
		s_popped = layout.popped;
		++g_loggedChanges;  // shares the change budget: hovering the belt toggles it
		plog::Info("belt popped up: %s", s_popped ? "yes, squares hidden" : "no");
	}
	SetPanelOpen(inGame && layout.hudShown && !layout.popped ? layout.rows : 0);
	if (g_openRows == 0) {
		return;
	}
	if (!GuardedApply(layout, squares)) {
		g_on = false;
		SetPanelOpen(0);
		plog::Error("memory fault while setting the squares; the counter is now off");
		return;
	}
	static bool s_loggedLayout = false;
	if (!s_loggedLayout) {
		s_loggedLayout = true;
		plog::Info("on the belt: panel at %d, %d; cells at x %d %d %d %d", layout.panelX, layout.panelY, layout.columnX[0],
		           layout.columnX[1], layout.columnX[2], layout.columnX[3]);
	}
	static int s_loggedRows = -1;
	if (layout.rows != s_loggedRows) {
		s_loggedRows = layout.rows;
		plog::Info("belt rows: %d", layout.rows);
	}
}

void __cdecl OnFrame(const D2RL::PluginContext* context, const D2RL::Overlay::Frame*, void*) noexcept {
	++g_frames;
	if (!g_taskQueued.exchange(true) && g_threads->runOnUiThread(context, UiTask, nullptr) != D2RL::Threads::Result::Success) {
		g_taskQueued = false;
	}
}

auto FindSig(uintptr_t exeBase, const char* sig, const char* name, char* report, size_t size) noexcept -> const uint8_t* {
	pc::Pattern pattern {};
	if (!pc::ParsePattern(sig, &pattern)) {
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
		const int n = pc::FindPattern(start, section->Misc.VirtualSize, pattern, &at);
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

void AppendWidget(std::string* json, const char* text) {
	*json += json->back() == '[' ? "\n" : ",\n";
	*json += text;
}

// The stock layout's squares for a belt of this many rows, all shown until the first apply sets them. Each square is
// a ClipViewWidget holding, in draw order, the frame, the Other mark and the pictures.
void AppendLayout(std::string* json, int rows) {
	char buf[400];
	const int32_t square = SquareOf(rows);
	const int32_t height = HeightOf(square);
	const int32_t panelY = StockHudY + StockBeltY + StockLabelY - LabelInCellY - height - 2;
	std::snprintf(buf, sizeof(buf),
	              "{\n\"type\": \"Panel\", \"name\": \"%s%d\",\n\"fields\": { \"priority\": %d, \"rect\": { \"x\": %d, \"y\": %d, \"width\": %d, "
	              "\"height\": %d }, \"anchor\": { \"x\": 0.5, \"y\": 1.0 } },\n\"children\": [",
	              PanelPrefix, rows, PanelPriority, StockHudX + StockBeltX, panelY, PanelWidth, height);
	*json += buf;
	Layout stock;
	stock.rows = rows;
	for (int c = 0; c < pc::Columns; ++c) {
		stock.columnX[c] = StockLabelX - LabelInCellX + c * StockPitch;
	}
	for (int c = 0; c < pc::Columns; ++c) {
		for (int r = 0; r < rows; ++r) {
			std::snprintf(buf, sizeof(buf),
			              "{ \"type\": \"ClipViewWidget\", \"name\": \"S%d%d\", \"fields\": { \"rect\": { \"x\": %d, \"y\": 0, \"width\": %d, "
			              "\"height\": %d } },\n\"children\": [",
			              c, r, SquareX(stock, c, r), square, height);
			*json += json->back() == '[' ? "\n" : ",\n";
			*json += buf;
			std::snprintf(buf, sizeof(buf),
			              "{ \"type\": \"ImageWidget\", \"name\": \"F\", \"fields\": { \"rect\": { \"x\": 0, \"y\": 0, \"scale\": %.4f }, "
			              "\"filename\": \"%s\" } }",
			              square / FrameCellWidth, FrameSprite);
			AppendWidget(json, buf);
			const int32_t mark = square / 3;
			std::snprintf(buf, sizeof(buf),
			              "{ \"type\": \"RectangleWidget\", \"name\": \"O\", \"fields\": { \"rect\": { \"x\": %d, \"y\": %d, \"width\": %d, "
			              "\"height\": %d }, \"color\": [ 0.85, 0.70, 0.25, 1.0 ] } }",
			              (square - mark) / 2, (height - mark) / 2, mark, mark);
			AppendWidget(json, buf);
			const int32_t icon = height - 2 * IconInset;
			for (int i = 0; i < pc::IconCount; ++i) {
				std::snprintf(buf, sizeof(buf),
				              "{ \"type\": \"ImageWidget\", \"name\": \"I%d\", \"fields\": { \"rect\": { \"x\": %d, \"y\": %d, "
				              "\"scale\": %.4f }, \"filename\": \"items/misc/%s\" } }",
				              i, (square - icon) / 2, IconInset, icon / 98.0f, pc::Icons[i].sprite);
				AppendWidget(json, buf);
			}
			*json += "\n] }";
		}
	}
	*json += "\n]\n}\n";
}

auto RegisterPanel(const D2RL::ResourceService* resources, int rows, char* report, size_t size) noexcept -> bool {
	std::string layout;
	AppendLayout(&layout, rows);
	char path[96];
	std::snprintf(path, sizeof(path), "data/global/ui/layouts/%s/Counts%dhd.json", PluginId, rows);
	char localId[16];
	std::snprintf(localId, sizeof(localId), "Counts%d", rows);
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
	registration.localId = localId;
	const auto panelResult = g_panelService->registerPanel(g_context, &registration, &g_panels[rows - 1]);
	if (resourceResult != D2RL::Resources::Result::Success || panelResult != D2RL::Panels::Result::Success) {
		std::snprintf(report, size, "off: panel %s refused (%u, %u)", localId, static_cast<uint32_t>(resourceResult),
		              static_cast<uint32_t>(panelResult));
		return false;
	}
	return true;
}

}  // namespace

auto InstallCounter(const D2RL::PluginContext* context, char* report, size_t size) noexcept -> bool {
	g_context = context;
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
	if (context->QueryService(&g_inventory) != D2RL::ServiceQueryResult::Success ||
	    !D2RL::HasInventoryServiceField(g_inventory, D2RL::InventoryServiceRequiredSize)) {
		g_inventory = nullptr;
		std::snprintf(report, size, "off: no Inventory service with belt reads");
		return false;
	}
	const D2RL::ResourceService* resources = nullptr;
	if (context->QueryService(&resources) != D2RL::ServiceQueryResult::Success || resources == nullptr ||
	    resources->serviceSize < D2RL::ResourceServiceRequiredSize) {
		std::snprintf(report, size, "off: no Resource service");
		return false;
	}
	const uintptr_t base = context->exeBase;
	const uint8_t* const findPanel = FindSig(base, FindPanelSig, "FindPanel", report, size);
	const uint8_t* const findChild = FindSig(base, FindChildSig, "FindChild", report, size);
	if (findPanel == nullptr || findChild == nullptr) {
		return false;
	}
	g_nativeFindPanel = reinterpret_cast<NativeFindPanelFn>(findPanel);
	g_nativeFindChild = reinterpret_cast<NativeFindChildFn>(findChild);
	const uint8_t* const setVisible = FindSig(base, SetVisibleSig, "SetVisible", report, size);
	if (setVisible == nullptr) {
		return false;
	}
	g_nativeSetVisible = reinterpret_cast<NativeSetVisibleFn>(setVisible);
	g_visibleFlag = setVisible[SetVisibleFlag];
	char missing[120];
	if (const uint8_t* const boxes = FindSig(base, BeltBoxesSig, "belt boxes", missing, sizeof(missing))) {
		std::memcpy(&g_beltBoxesField, boxes + BeltBoxesField, sizeof(g_beltBoxesField));
	} else {
		plog::Warn("%s: every column shows %d squares", missing, pc::Rows);
	}
	if (const uint8_t* const popped = FindSig(base, BeltPoppedSig, "belt popped", missing, sizeof(missing))) {
		std::memcpy(&g_beltPoppedField, popped + BeltPoppedField, sizeof(g_beltPoppedField));
	} else {
		plog::Warn("%s: the squares stay while the belt is popped up", missing);
	}
	for (int rows = 1; rows <= pc::Rows; ++rows) {
		if (!RegisterPanel(resources, rows, report, size)) {
			return false;
		}
	}
	g_on = true;
	D2RL::Overlay::CallbackRegistration clock {};
	clock.structSize = D2RL::Overlay::CallbackRegistrationSize;
	clock.phase = D2RL::Overlay::Phase::AfterGameUi;
	clock.callback = OnFrame;
	const auto result = g_overlay->registerFrameCallback(context, &clock, &g_callback);
	if (result != D2RL::Overlay::Result::Success) {
		g_on = false;
		std::snprintf(report, size, "off: overlay callback refused (result %u)", static_cast<uint32_t>(result));
		return false;
	}
	std::snprintf(report, size, "on: find panel rva 0x%llx, find child rva 0x%llx, visible flag +0x%x, belt boxes +0x%x, belt popped +0x%x",
	              static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(g_nativeFindPanel) - base),
	              static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(g_nativeFindChild) - base),
	              static_cast<unsigned>(g_visibleFlag), static_cast<unsigned>(g_beltBoxesField),
	              static_cast<unsigned>(g_beltPoppedField));
	return true;
}

void UninstallCounter() noexcept {
	g_on = false;
	if (g_overlay != nullptr && g_callback != D2RL::Overlay::InvalidCallbackHandle) {
		(void)g_overlay->unregisterFrameCallback(g_context, g_callback);
		g_callback = D2RL::Overlay::InvalidCallbackHandle;
	}
}

void CounterStatus(char* out, size_t size) noexcept {
	std::snprintf(out, size, "%s, counts %d %d %d %d, frames %llu, panel %s (result %d), player result %d, items result %d",
	              g_on ? "on" : "off", g_counts[0], g_counts[1], g_counts[2], g_counts[3],
	              static_cast<unsigned long long>(g_frames.load()), g_openRows != 0 ? "open" : "closed", g_panelResult.load(),
	              g_playerResult.load(), g_itemsResult.load());
}
