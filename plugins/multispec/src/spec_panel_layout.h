// The spec panel's layout (portable, unit-tested): one slot showing the active spec, with an arrow above and below, so any
// number of specs fits. Every sprite is the game's own, named by path: the player's install supplies the art, so nothing
// from the game is in this repository, and the panel matches the stock ones.
//   wing     PANEL\Modals\Modal_No_Title_BG, 1245 x 453. It sits below the skill tree (priority -5), so only its left
//            edge shows: a short wing in the tree's own stone and frame (spike 8)
//   slot     PANEL\HUD_02\belt_popped_row, 493 x 113: the belt's four empty slots. Only the first shows; the tree covers
//            the rest
//   arrows   FrontEnd\HD\Final\FrontEnd_UpButton and _DownButton, 70 x 70
// The widget API cannot set text, so the layout holds one number per spec and the panel shows the active one.
#pragma once

#include <cstddef>
#include <cstdint>

namespace ms::panel {

constexpr int32_t WingTotalWidth = 1245;
constexpr int32_t WingHeight = 453;
constexpr int32_t VisibleWidth = 160;  // the part left of the skill tree
constexpr int32_t WingBorder = 30;     // the wing's frame
constexpr int32_t HeadingTop = 44;     // below the wing's corner ornament
constexpr int32_t HeadingHeight = 48;
constexpr int32_t SlotWidth = 123;     // one belt slot
constexpr int32_t SlotHeight = 113;
constexpr int32_t ArrowSize = 70;

// Skill tree placement ($RightPanelAnchor): flush with its left edge, top at its tab row.
constexpr int32_t TreeLeft = -1394;
constexpr int32_t TreeTop = -651;
constexpr int32_t TabRow = 145;

constexpr int32_t SlotX = VisibleWidth - SlotWidth;  // ends where the tree starts
constexpr int32_t ArrowX = SlotX + (SlotWidth - ArrowSize) / 2;

// The column of arrow, slot and arrow, touching, centred below the heading.
constexpr int32_t ColumnHeight = ArrowSize + SlotHeight + ArrowSize;
constexpr int32_t ColumnTop = HeadingTop + HeadingHeight;
constexpr int32_t UpY = ColumnTop + (WingHeight - WingBorder - ColumnTop - ColumnHeight) / 2;
constexpr int32_t SlotY = UpY + ArrowSize;
constexpr int32_t DownY = SlotY + SlotHeight;

// Writes the panel's JSON to out. Returns its length, 0 when it does not fit.
auto BuildLayout(int32_t specCount, char* out, size_t size) noexcept -> size_t;

// The step in an arrow's click message: +1 "multispec/SpecUp", -1 "multispec/SpecDown", 0 for any other text.
auto ParseStep(const char* text) noexcept -> int32_t;

// The spec one step away, wrapping at both ends. 0 when there is nowhere to go (one spec, or the active spec is unknown).
auto NextSpec(int32_t active, int32_t specCount, int32_t step) noexcept -> int32_t;

}  // namespace ms::panel
