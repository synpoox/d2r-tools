#include "hookguard.h"

namespace cartographer::hookguard {

HookState classifyHook(bool installed, bool haveOriginal)
{
	if (!installed) return HookState::Refused;
	return haveOriginal ? HookState::Live : HookState::NoOriginal;
}

bool hookUsable(HookState state)
{
	return state == HookState::Live;
}

bool canRestoreHook(HookState state, uint32_t hookSize, uint32_t guardCapacity, uint32_t exactBytes)
{
	return state == HookState::NoOriginal && hookSize > 0 && hookSize <= guardCapacity && exactBytes >= hookSize;
}

const char* hookDetail(HookState state, bool restored)
{
	switch (state) {
	case HookState::Live: return "found by signature, hooked";
	case HookState::Refused: return "off: D2RLoader refused the hook";
	case HookState::NoOriginal:
		return restored ? "off: D2RLoader gave no way back to the game's own function; the hook was taken out again"
		                : "off: D2RLoader gave no way back to the game's own function, and the hook could not be taken out; the game's function is skipped until the game is restarted";
	}
	return "off";
}

}  // namespace cartographer::hookguard
