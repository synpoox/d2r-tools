// Spec strip art (design section 11, "Strip art"): the wing beside the skill tree and its slot sockets, drawn in code as
// HD .sprite files (version 31, uncompressed RGBA; toolkit modkit/images.py). Our own art: nothing comes from the game,
// so nothing extracted is shipped or committed. Portable: no Windows or game calls (tested in tests/).
//
// Geometry, in HD panel units:
//   wing      WingWidth wide, WingHeight(slots) tall. The rightmost WingTuck px slide under the tree (the strip draws
//             below it), so the visible wing is WingWidth - WingTuck. Stone fill, metal frame, corner rivets.
//   socket    SocketSize square, 2 frames side by side (normal, hovered): a recessed slot with a bevel; hovered has a
//             gold rim.
#pragma once

#include <cstddef>
#include <cstdint>

namespace ms::art {

constexpr int32_t WingWidth = 190;
constexpr int32_t WingTuck = 40;          // hidden under the tree's frame
constexpr int32_t WingFrame = 14;         // metal frame band
constexpr int32_t WingHeadingHeight = 70; // "Specs"
constexpr int32_t SocketSize = 96;
constexpr int32_t SlotPitch = 116;        // socket + gap
constexpr int32_t WingBottomPad = 16;
constexpr size_t SpriteHeaderSize = 40;

// Visible wing width (the strip's rect width).
constexpr int32_t VisibleWidth = WingWidth - WingTuck;

// Socket left edge inside the strip: centred in the visible part, inside the frame on the left.
constexpr int32_t SocketX = WingFrame + (VisibleWidth - WingFrame - SocketSize) / 2;

constexpr auto SlotY(int32_t index) -> int32_t {  // 0-based
	return WingFrame + WingHeadingHeight + index * SlotPitch;
}

constexpr auto WingHeight(int32_t slots) -> int32_t {
	return SlotY(slots) - (SlotPitch - SocketSize) + WingBottomPad + WingFrame;
}

// Bytes a sprite of w x h (one frame strip) needs: header + RGBA.
constexpr auto SpriteBytes(int32_t w, int32_t h) -> size_t {
	return SpriteHeaderSize + static_cast<size_t>(w) * static_cast<size_t>(h) * 4;
}

// Each writes a whole sprite (header + pixels) into out and returns its size, or 0 when out is too small. `scale` 1 is
// the HD sprite, 2 its half-size .lowend twin (same look, every dimension halved).
auto DrawWing(int32_t slots, int32_t scale, uint8_t* out, size_t size) noexcept -> size_t;
auto DrawSocket(int32_t scale, uint8_t* out, size_t size) noexcept -> size_t;

}  // namespace ms::art
