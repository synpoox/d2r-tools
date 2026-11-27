#pragma once
#define NOMINMAX
#include <windows.h>
#include <Xinput.h>
namespace recap {
// The pinned Input service exposes keyboard actions only. Observe controller
// edges while our panel is open; never intercept or consume revive controls.
class Controller {
	HMODULE module_ = nullptr;
	using ReadFn = DWORD (WINAPI*)(DWORD, XINPUT_STATE*);
	ReadFn read_ = nullptr;
	WORD previous_ = 0;
public:
	void Start() noexcept {
		module_ = LoadLibraryExW(L"XInput9_1_0.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
		if (module_) read_ = reinterpret_cast<ReadFn>(GetProcAddress(module_, "XInputGetState"));
	}
	uint32_t Poll(bool panelOpen) noexcept {
		if (!read_) return 0;
		XINPUT_STATE state {};
		bool connected = false;
		for (DWORD i = 0; i < XUSER_MAX_COUNT; ++i) if (read_(i, &state) == ERROR_SUCCESS) { connected = true; break; }
		if (!connected) { previous_ = 0; return 0; }
		const WORD pressed = state.Gamepad.wButtons & ~previous_; previous_ = state.Gamepad.wButtons;
		if (!panelOpen) return 0;
		if (pressed & XINPUT_GAMEPAD_B) return 8;
		if (pressed & XINPUT_GAMEPAD_A) return 11;
		if (pressed & XINPUT_GAMEPAD_LEFT_SHOULDER) return 4;
		if (pressed & XINPUT_GAMEPAD_RIGHT_SHOULDER) return 5;
		if (pressed & XINPUT_GAMEPAD_DPAD_LEFT) return 12;
		if (pressed & XINPUT_GAMEPAD_DPAD_RIGHT) return 13;
		if (pressed & XINPUT_GAMEPAD_DPAD_UP) return 2;
		if (pressed & XINPUT_GAMEPAD_DPAD_DOWN) return 3;
		return 0;
	}
	void Stop() noexcept { if (module_) FreeLibrary(module_); module_ = nullptr; read_ = nullptr; }
};
}
