// Quick NPC settings: <game>\d2rloader\config\quick-npc.toml (D2RLoader writes it from the copy embedded in the DLL,
// plugin/quick-npc.toml, when it is missing, and never overwrites it). No Windows, no SDK: host-tested.
//
//   [quick-npc]
//   enabled = true             # false: start switched off ("quick-npc on" in the console switches it on)
//   hotkey = "LeftCtrl"        # hold this, then left-click a vendor
//   timeout_seconds = 60       # how long a click waits for the vendor's dialog (walking, NPC speech)
//
// The parser reads the subset of TOML these settings need: [table] headers, key = value, booleans, integers, "basic"
// and 'literal' strings, # comments. Anything it can't use is reported and leaves the default in place.
#pragma once

#include <cstddef>
#include <cstdint>

namespace quick_npc {

inline constexpr const char* ConfigTable = "quick-npc";

struct Config {
	bool enabled = true;
	uint32_t hotkeyVk = 0xA2;  // VK_LCONTROL
	char hotkeyName[64] = "LeftCtrl";
	uint32_t timeoutMs = 60000;
};

inline constexpr uint32_t MinTimeoutSeconds = 5;
inline constexpr uint32_t MaxTimeoutSeconds = 600;

struct ConfigIssue {
	int line = 0;  // 1-based, 0 for the whole file
	char text[160] = "";
};

// Parses `toml` (not necessarily null-terminated) into `out`, which keeps its defaults for anything missing or
// invalid. Fills up to `maxIssues` issues and returns how many there were (it can exceed maxIssues).
auto ParseConfig(const char* toml, size_t length, Config* out, ConfigIssue* issues, int maxIssues) noexcept -> int;

// Key name -> Windows virtual-key code. Case, spaces, '-' and '_' don't matter: "LeftCtrl", "left_ctrl", "LCtrl".
// Accepts LeftCtrl/RightCtrl/Ctrl, LeftAlt/RightAlt/Alt, LeftShift/RightShift/Shift, A-Z, 0-9, F1-F24, Numpad0-9,
// Space, Tab, CapsLock, Backquote, Mouse3/Mouse4/Mouse5 (middle and side buttons), or a code such as "0xA2".
// Refuses the left and right mouse buttons.
auto KeyFromName(const char* name, uint32_t* vk) noexcept -> bool;

// A note on how D2R's default controls use this key, or null. Shift: Stand Still stops NPC interaction.
auto HotkeyNote(uint32_t vk) noexcept -> const char*;

// True when the hotkey cannot work with D2R's default controls (Shift = Stand Still).
auto HotkeyBlocksInteraction(uint32_t vk) noexcept -> bool;

}  // namespace quick_npc
