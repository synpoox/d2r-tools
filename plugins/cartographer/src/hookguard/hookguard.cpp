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

const char* hookDetail(HookState state)
{
	switch (state) {
	case HookState::Live: return "found by signature, hooked";
	case HookState::Refused: return "off: D2RLoader refused the hook";
	case HookState::NoOriginal: return "off: D2RLoader gave no way back to the game's own function";
	}
	return "off";
}

}  // namespace cartographer::hookguard
