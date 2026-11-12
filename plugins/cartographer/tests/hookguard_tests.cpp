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
	CHECK(std::strstr(hookDetail(HookState::Live), "hooked") != nullptr);
	CHECK(std::strstr(hookDetail(HookState::Refused), "off") == hookDetail(HookState::Refused));
	CHECK(std::strstr(hookDetail(HookState::NoOriginal), "off") == hookDetail(HookState::NoOriginal));
}
