// The spec panel's layout (portable, unit-tested). Every sprite is the game's own, named by path: the player's install
// supplies the art, so nothing from the game is in this repository, and the wing matches the stock panels.
//   wing     PANEL\Modals\Modal_No_Title_BG, 1245 x 453. It sits below the skill tree (priority -5), so only its left
//            edge shows: a short wing in the tree's own stone and frame (spike 8)
//   sockets  PANEL\DropdownButton_Large_Left, 116 x 111: blank stone with a gold-rimmed frame. It is the left cap of a
#pragma once

#include <cstddef>
#include <cstdint>

namespace ms::panel {

constexpr int32_t WingTotalWidth = 1245;
constexpr int32_t WingHeight = 453;
constexpr int32_t VisibleWidth = 160;  // the part left of the skill tree
constexpr int32_t WingBorder = 30;     // the wing's frame
constexpr int32_t SocketWidth = 116;
constexpr int32_t SocketHeight = 111;
constexpr int32_t SocketPitch = 114;
constexpr int32_t HeadingHeight = 48;
constexpr int32_t MaxSlots = 3;  // what the wing holds; more specs stay on the console command

// Skill tree placement ($RightPanelAnchor): flush with its left edge, top at its tab row.
constexpr int32_t TreeLeft = -1394;
constexpr int32_t TreeTop = -651;
constexpr int32_t TabRow = 145;

constexpr int32_t SocketX = 48;  // its open right side ends under the tree

// Slots shown for a spec count.
constexpr auto VisibleSlots(int32_t specCount) noexcept -> int32_t {
	return specCount < 1 ? 1 : (specCount > MaxSlots ? MaxSlots : specCount);
}

// Socket top: the column is centred below the heading.
constexpr auto SlotY(int32_t slots, int32_t index) noexcept -> int32_t {
	const int32_t column = slots * SocketPitch - (SocketPitch - SocketHeight);
	const int32_t top = WingBorder + HeadingHeight;
	return top + (WingHeight - WingBorder - top - column) / 2 + index * SocketPitch;
}

// Writes the panel's JSON to out. Returns its length, 0 when it does not fit.
auto BuildLayout(int32_t slots, char* out, size_t size) noexcept -> size_t;

// The spec number in a slot's click message ("multispec/Swap<n>"), 0 for any other text.
auto ParseSwap(const char* text) noexcept -> int32_t;

}  // namespace ms::panel
