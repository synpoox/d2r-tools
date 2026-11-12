// What came back from InstallInlineHook, as one decision. Pure C++ (no engine, no SDK).
#pragma once

namespace cartographer::hookguard {

// Live: hooked and the loader returned the game's own function. Refused: the loader did not install the hook (the game is
// untouched). NoOriginal: installed but no way back to the game's function; the SDK has no call to remove a hook, so the
// hook cannot be taken out and cannot call through, and the part must be treated as broken: its feature stays off, no
// dependent part is hooked, and the log says so.
enum class HookState { Live, Refused, NoOriginal };

HookState classifyHook(bool installed, bool haveOriginal);

// Only a Live hook may be used by a feature or have other hooks built on it.
bool hookUsable(HookState state);

// The part status detail for the state.
const char* hookDetail(HookState state);

}  // namespace cartographer::hookguard
