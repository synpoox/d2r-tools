// What came back from InstallInlineHook, as one decision. Pure C++ (no engine, no SDK).
#pragma once

#include <cstdint>

namespace cartographer::hookguard {

// Live: hooked and the loader returned the game's own function. Refused: the loader did not install the hook (the game is
// untouched). NoOriginal: installed but no way back to the game's function; the SDK has no call to remove a hook, so the
// plugin writes the guard's original bytes back where it can (canRestoreHook); the part is then off and the game's own
// function runs. Where it cannot, the hook keeps returning 0 until the game restarts. Either way the feature stays off, no
// dependent part is hooked, and the log says so.
enum class HookState { Live, Refused, NoOriginal };

HookState classifyHook(bool installed, bool haveOriginal);

// Only a Live hook may be used by a feature or have other hooks built on it.
bool hookUsable(HookState state);

// Whether the plugin may write the game's own entry bytes back over a hook that came back without an original. Only for
// NoOriginal, and only when the guard holds the exact original bytes for the whole hookSize: hookSize is not zero, fits the
// guard (guardCapacity) and every one of those bytes is concrete (exactBytes counts the leading bytes the signature pinned
// with no wildcard). Otherwise the bytes are not known and the hook cannot be undone.
bool canRestoreHook(HookState state, uint32_t hookSize, uint32_t guardCapacity, uint32_t exactBytes);

// The part status detail for the state. restored: the hook was taken out again, so the game runs its own function.
const char* hookDetail(HookState state, bool restored);

}  // namespace cartographer::hookguard
