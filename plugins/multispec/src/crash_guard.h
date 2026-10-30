// Crash guard: if the game faults, say so and say why, in the plugin's own log. A vectored exception handler sees the
// fault before D2R or D2RLoader handle it. It only logs and never handles the exception, so the game's own crash
// handling runs as usual. It also holds the image line for the load log and status command.
#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include "log.h"

#include <windows.h>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdio>

namespace crash_guard {

inline constexpr int MaxReports = 8;
inline char g_version[32] = "?";
inline uintptr_t g_exeBase = 0;
inline uint32_t g_stamp = 0;
inline uintptr_t g_selfBase = 0;
inline uintptr_t g_selfEnd = 0;
inline void* g_handler = nullptr;
inline std::atomic<int> g_reports{0};
inline std::atomic_flag g_busy = ATOMIC_FLAG_INIT;  // a fault inside the logger must not re-enter it

inline auto Fatal(DWORD code) noexcept -> bool {
	return code == EXCEPTION_ACCESS_VIOLATION || code == EXCEPTION_ILLEGAL_INSTRUCTION || code == EXCEPTION_PRIV_INSTRUCTION ||
	       code == EXCEPTION_IN_PAGE_ERROR || code == EXCEPTION_INT_DIVIDE_BY_ZERO || code == EXCEPTION_STACK_OVERFLOW;
}

inline auto InSelf(uintptr_t a) noexcept -> bool {
	return a >= g_selfBase && a < g_selfEnd;
}

// Lowest stack slot that holds an address inside this plugin, 0 when there is none.
inline auto SelfOnStack(uintptr_t rsp) noexcept -> uintptr_t {
	const auto* tib = reinterpret_cast<const NT_TIB*>(NtCurrentTeb());
	const auto top = reinterpret_cast<uintptr_t>(tib->StackBase);
	const auto low = reinterpret_cast<uintptr_t>(tib->StackLimit);
	if (rsp < low || rsp >= top) {
		return 0;
	}
	const uintptr_t end = top - rsp > 0x4000 ? rsp + 0x4000 : top;
	for (uintptr_t p = rsp & ~uintptr_t{7}; p + 8 <= end; p += 8) {
		const uintptr_t value = *reinterpret_cast<const uintptr_t*>(p);
		if (InSelf(value)) {
			return value;
		}
	}
	return 0;
}

inline auto Why(const EXCEPTION_RECORD* e, char* out, size_t size) noexcept -> const char* {
	if (e->ExceptionCode == EXCEPTION_ACCESS_VIOLATION || e->ExceptionCode == EXCEPTION_IN_PAGE_ERROR) {
		const ULONG_PTR kind = e->NumberParameters > 0 ? e->ExceptionInformation[0] : 0;
		const ULONG_PTR at = e->NumberParameters > 1 ? e->ExceptionInformation[1] : 0;
		if (kind == 8) {
			std::snprintf(out, size, "tried to execute non-code memory at 0x%llx (a bad function address)", static_cast<unsigned long long>(at));
		} else {
			std::snprintf(out, size, "%s of bad memory at 0x%llx%s", kind == 1 ? "write" : "read", static_cast<unsigned long long>(at),
			              at < 0x10000 ? " (a null pointer plus a field offset)" : " (a wrong pointer or field offset)");
		}
	} else if (e->ExceptionCode == EXCEPTION_ILLEGAL_INSTRUCTION || e->ExceptionCode == EXCEPTION_PRIV_INSTRUCTION) {
		std::snprintf(out, size, "bad instruction (execution jumped into the middle of code or data)");
	} else if (e->ExceptionCode == EXCEPTION_STACK_OVERFLOW) {
		std::snprintf(out, size, "stack overflow (runaway recursion)");
	} else if (e->ExceptionCode == EXCEPTION_INT_DIVIDE_BY_ZERO) {
		std::snprintf(out, size, "integer division by zero");
	} else {
		std::snprintf(out, size, "memory page could not be read");
	}
	return out;
}

inline void Report(const EXCEPTION_POINTERS* info) noexcept {
	const EXCEPTION_RECORD* e = info->ExceptionRecord;
	const auto rip = static_cast<uintptr_t>(info->ContextRecord->Rip);
	const uintptr_t self = InSelf(rip) ? rip : SelfOnStack(static_cast<uintptr_t>(info->ContextRecord->Rsp));
	const char* blame = InSelf(rip)  ? "this plugin's own code faulted"
	                    : self != 0  ? "this plugin is on the stack: likely involved"
	                                 : "this plugin is not on the stack: probably not the cause";
	char why[160];
	plog::Error("CRASH %s: fault 0x%08lx at 0x%llx (image +0x%llx, plugin +0x%llx), thread %lu, may still be handled by the game | why: %s | blame: %s | "
	            "image stamp 0x%08x; fixed addresses come from the tested image, so a moved game build can cause this",
	            g_version, static_cast<unsigned long>(e->ExceptionCode), static_cast<unsigned long long>(rip),
	            static_cast<unsigned long long>(rip - g_exeBase), static_cast<unsigned long long>(self != 0 ? self - g_selfBase : 0),
	            GetCurrentThreadId(), Why(e, why, sizeof(why)), blame, g_stamp);
}

inline auto CALLBACK Handler(EXCEPTION_POINTERS* info) noexcept -> LONG {
	if (info == nullptr || info->ExceptionRecord == nullptr || info->ContextRecord == nullptr || !Fatal(info->ExceptionRecord->ExceptionCode)) {
		return EXCEPTION_CONTINUE_SEARCH;
	}
	if (g_busy.test_and_set()) {
		return EXCEPTION_CONTINUE_SEARCH;
	}
	if (g_reports.fetch_add(1) < MaxReports) {
		Report(info);
	}
	g_busy.clear();
	return EXCEPTION_CONTINUE_SEARCH;
}

// TimeDateStamp of the PE image at `exeBase` (0 when there is none).
inline auto ImageStamp(uintptr_t exeBase) noexcept -> uint32_t {
	if (exeBase == 0) {
		return 0;
	}
	const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(exeBase);
	if (dos->e_magic != IMAGE_DOS_SIGNATURE) {
		return 0;
	}
	const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(exeBase + static_cast<uintptr_t>(dos->e_lfanew));
	return nt->Signature == IMAGE_NT_SIGNATURE ? nt->FileHeader.TimeDateStamp : 0;
}

// The status / log line: "D2R 3.3.93847, image 0x6ab3782c".
inline void Describe(char* out, size_t size, const char* buildVersion) noexcept {
	std::snprintf(out, size, "D2R %s, image 0x%08x", buildVersion != nullptr ? buildVersion : "?", g_stamp);
}

inline void Install(const char* version, uintptr_t exeBase) noexcept {
	if (g_handler != nullptr) {
		return;
	}
	std::snprintf(g_version, sizeof(g_version), "%s", version);
	g_exeBase = exeBase;
	g_stamp = ImageStamp(exeBase);
	HMODULE self = nullptr;
	if (GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
	                       reinterpret_cast<LPCWSTR>(&Handler), &self) && self != nullptr) {
		const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(self);
		const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(reinterpret_cast<uintptr_t>(self) + static_cast<uintptr_t>(dos->e_lfanew));
		g_selfBase = reinterpret_cast<uintptr_t>(self);
		g_selfEnd = g_selfBase + nt->OptionalHeader.SizeOfImage;
	}
	g_handler = AddVectoredExceptionHandler(1, Handler);
}

inline void Remove() noexcept {
	if (g_handler != nullptr) {
		RemoveVectoredExceptionHandler(g_handler);
		g_handler = nullptr;
	}
}

}  // namespace crash_guard