// D2R game image interface for Better Automap: RVAs, entry bytes, offsets and function types.
//
// EVERYTHING IN THIS FILE IS FOR ONE GAME IMAGE: D2R 3.3.0 build 93847 running under D2RLoader 1.3.1 (PE header
// TimeDateStamp 0x6ab3782c, checked by build_guard.h before any of it is used). Found in the dumped in-memory image
// (D2RLoader_6ab3782c_dump.exe); the evidence for every row is in docs/hook-sites.md. A different D2R patch or
// D2RLoader release moves them: re-find each one before changing build_guard::ExpectedImage.
//
// Calling convention: plain Windows x64 (rcx, rdx, r8, r9).
#pragma once

#include <cstdint>

namespace d2r {

// --- Functions (RVAs), signature in the comment -------------------------------------------------------------------

// bool MinimapOnLeft(void): returns the byte at MinimapLeftFlag. Only caller: the mini-map layout 0x14e48e0, which
// anchors the panel at miniOffsetLeft when this is true (or a right-side panel is open), else at miniOffsetRight.
// movzx eax, byte ptr [rip+0x295791e] (0f b6 05 1e 79 95 02) / ret (c3): the whole function, 8 bytes.
inline constexpr uint64_t RvaMinimapOnLeft = 0xd34b0;
inline constexpr uint8_t MinimapOnLeftBytes[] = {0x0f, 0xb6, 0x05, 0x1e, 0x79, 0x95, 0x02, 0xc3};

// void AutoMapPanel::Draw(AutoMapPanel* this) const. Full screen: scale = uiScale * this+0x80 * tileScale (+0x15a8);
// mini map: miniTileScale (+0x15ac). tileScale is read nowhere else.
// mov r11,rsp (4c 8b dc) / push rbp (55) / push rbx (53) / push rsi (56) / push rdi (57) / push r12 (41 54) /
// lea rbp,[r11-0x5f] (49 8d 6b a1) / sub rsp,0xe0 (48 81 ec e0 00 00 00): 20 bytes, 8 whole instructions.
inline constexpr uint64_t RvaAutoMapDraw = 0x14e3480;
inline constexpr uint8_t AutoMapDrawEntry[] = {0x4c, 0x8b, 0xdc, 0x55, 0x53, 0x56, 0x57, 0x41, 0x54, 0x49,
                                               0x8d, 0x6b, 0xa1, 0x48, 0x81, 0xec, 0xe0, 0x00, 0x00, 0x00};

// uint64 AutoMapPanel::OnMessage(AutoMapPanel* this, Message* msg): handles InputMessage/KeyDown (arrow keys pan the
// map, then marks the input handled), SystemMessage/ResolutionChanged, then tail-calls Panel::OnMessage and returns
// its value.
// mov [rsp+0x18],rbx (48 89 5c 24 18) / mov [rsp+0x20],rsi (48 89 74 24 20) / push rbp (55) / push rdi (57) /
// push r14 (41 56) / lea rbp,[rsp-0x10] (48 8d 6c 24 f0): 19 bytes, 6 whole instructions.
inline constexpr uint64_t RvaAutoMapOnMessage = 0x14e4470;
inline constexpr uint8_t AutoMapOnMessageEntry[] = {0x48, 0x89, 0x5c, 0x24, 0x18, 0x48, 0x89, 0x74, 0x24, 0x20,
                                                    0x55, 0x57, 0x41, 0x56, 0x48, 0x8d, 0x6c, 0x24, 0xf0};

// void ApplyAutoMapMode(int32 mode): the change callback of the AutoMapMode setting (registered by its accessor
// 0xe06a0). Sets MinimapLeftFlag = (mode == 1), AutoMapMiniFlag = (mode != 0), re-derives the tileset scales
// (0xd6920) and resets the arrow-key pan. Changes the session only; the saved setting is untouched.
inline constexpr uint64_t RvaApplyAutoMapMode = 0xd3d80;

// bool InputHandled(void): the current input event was already consumed by a UI panel.
// void MarkInputHandled(void): consume it (what the panel's arrow-key handler does).
inline constexpr uint64_t RvaInputHandled = 0x8460d0;
inline constexpr uint64_t RvaMarkInputHandled = 0x843fc0;

// --- Tables -------------------------------------------------------------------------------------------------------

// Key command table: 0x18-byte entries {void (*down)(), void (*up)(), u8 flags...}, indexed by the classic keyconfig
// id (0 CfgCharacter ... 7 CfgAutoMap, 8 CfgAutoMapCenter, ... 11 CfgAutoMapNames). The key-down invoker 0x11ff30 calls
// entry->down only when no UI panel handled the key first. Entry 7's down handler (the automap key, Tab) is a
// D2RLoader thunk in .maho at exeBase + 0x3e2b16c: D2RCore already replaces it, so it is wrapped, never patched.
inline constexpr uint64_t KeyCommandTable = 0x22a7930;
inline constexpr int KeyCommandStride = 0x18;
inline constexpr int KeyAutoMap = 7;
inline constexpr uint64_t RvaAutoMapKeySlot = KeyCommandTable + KeyAutoMap * KeyCommandStride;  // 0x22a79d8
inline constexpr uint64_t RvaAutoMapKeyThunk = 0x3e2b16c;

// --- Globals ------------------------------------------------------------------------------------------------------

inline constexpr uint64_t PanelOpen = 0x2a2ada0;       // u8[0x30]: panel id -> open (read by 0xce500)
inline constexpr int PanelAutoMap = 10;                // opened by the CfgAutoMapCenter handler 0x119da0
inline constexpr int PanelChat = 5;                    // toggled by the CfgChat handler 0x119c90
inline constexpr uint64_t AutoMapMiniFlag = 0x2a2add0; // u32: 1 = mini map (setting mode 1 or 2), 0 = full screen
inline constexpr uint64_t MinimapLeftFlag = 0x2a2add5; // u8: 1 = mini map on the left (setting mode 1)
inline constexpr uint64_t ScreenShift = 0x2a26bb0;     // u32: 0 = no side panel, 1/2 = one side, 3 = both (0xb9c20)

// --- Offsets ------------------------------------------------------------------------------------------------------

inline constexpr int AutoMapTileScale = 0x15a8;  // float, AutoMapPanel layout field "tileScale" (default 1.0)

// Message (UI): +0 FNV-1a hash of the message type, +0x88 hash of the subtype, +0x110 payload pointer.
inline constexpr int MsgType = 0x00;
inline constexpr int MsgSubtype = 0x88;
inline constexpr int MsgPayload = 0x110;
// MouseScroll payload: +4 cursor x, +8 cursor y, +0x10 wheel delta (int, 120 per notch, positive = wheel up).
inline constexpr int ScrollDelta = 0x10;
inline constexpr int WheelDelta = 120;

// FNV-1a 64 of the type names (D2R hashes names with 0x80d70, plain FNV-1a; verified against its seeds).
constexpr auto Fnv1a(const char* s) noexcept -> uint64_t {
	uint64_t h = 0xcbf29ce484222325ULL;
	for (; *s != '\0'; ++s) {
		h = (h ^ static_cast<uint8_t>(*s)) * 0x100000001b3ULL;
	}
	return h;
}
inline constexpr uint64_t HashInputMessage = Fnv1a("InputMessage");
inline constexpr uint64_t HashMouseScroll = Fnv1a("MouseScroll");
static_assert(HashInputMessage == 0x4ae067c6248b6042ULL);
static_assert(HashMouseScroll == 0x87a68230a588578dULL);

// --- Function types -----------------------------------------------------------------------------------------------
using DrawFn = void (*)(void* panel) noexcept;
using OnMessageFn = uint64_t (*)(void* panel, void* msg) noexcept;
using KeyHandlerFn = void (*)(uint64_t a, uint64_t b, uint64_t c, uint64_t d) noexcept;
using ApplyAutoMapModeFn = void (*)(int32_t mode) noexcept;
using InputHandledFn = bool (*)() noexcept;
using MarkInputHandledFn = void (*)() noexcept;

// --- Access -------------------------------------------------------------------------------------------------------
void Bind(uintptr_t exeBase) noexcept;
auto Base() noexcept -> uintptr_t;

template <typename T>
inline auto Global(uint64_t rva) noexcept -> T* {
	return reinterpret_cast<T*>(Base() + rva);
}

template <typename Fn>
inline auto Function(uint64_t rva) noexcept -> Fn {
	return reinterpret_cast<Fn>(Base() + rva);
}

// Read a field of an opaque game structure: At<uint64_t>(msg, MsgType).
template <typename T>
inline auto At(const void* p, int offset) noexcept -> T {
	return *reinterpret_cast<const T*>(static_cast<const uint8_t*>(p) + offset);
}

inline auto PanelIsOpen(int id) noexcept -> bool {
	return Global<const uint8_t>(PanelOpen)[id] != 0;
}

inline auto MiniMapMode() noexcept -> bool {
	return *Global<const uint32_t>(AutoMapMiniFlag) == 1;
}

}  // namespace d2r
