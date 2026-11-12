// Host tests for the hook outcome decision. No game, no SDK: the code is pure C++.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <cstring>

#include "hookguard.h"

using namespace cartographer::hookguard;

TEST_CASE("a hook is usable only when the loader installed it and returned the original")
{
	CHECK(classifyHook(true, true) == HookState::Live);
	CHECK(classifyHook(false, false) == HookState::Refused);
	CHECK(classifyHook(false, true) == HookState::Refused);  // a stale original from a refused install is not used
	CHECK(classifyHook(true, false) == HookState::NoOriginal);
	CHECK(hookUsable(HookState::Live));
	CHECK_FALSE(hookUsable(HookState::Refused));
	CHECK_FALSE(hookUsable(HookState::NoOriginal));
}

TEST_CASE("only a live hook reports itself as hooked")
{
	CHECK(std::strstr(hookDetail(HookState::Live, false), "hooked") != nullptr);
	CHECK(std::strstr(hookDetail(HookState::Refused, false), "off") == hookDetail(HookState::Refused, false));
	CHECK(std::strstr(hookDetail(HookState::NoOriginal, false), "off") == hookDetail(HookState::NoOriginal, false));
	CHECK(std::strstr(hookDetail(HookState::NoOriginal, true), "taken out again") != nullptr);
	CHECK(std::strstr(hookDetail(HookState::NoOriginal, false), "could not be taken out") != nullptr);
}

TEST_CASE("the game's bytes are written back only for a no-original hook whose guard is fully exact")
{
	CHECK(canRestoreHook(HookState::NoOriginal, 14, 32, 14));
	CHECK(canRestoreHook(HookState::NoOriginal, 14, 32, 20));
	CHECK(canRestoreHook(HookState::NoOriginal, 32, 32, 32));
	CHECK_FALSE(canRestoreHook(HookState::NoOriginal, 14, 32, 13));  // a masked byte inside the hook range
	CHECK_FALSE(canRestoreHook(HookState::NoOriginal, 14, 32, 0));
	CHECK_FALSE(canRestoreHook(HookState::NoOriginal, 0, 32, 0));    // a called-only part has no hook
	CHECK_FALSE(canRestoreHook(HookState::NoOriginal, 33, 32, 33));  // does not fit the guard
	CHECK_FALSE(canRestoreHook(HookState::Live, 14, 32, 14));        // a working hook stays
	CHECK_FALSE(canRestoreHook(HookState::Refused, 14, 32, 14));     // nothing was written
}
