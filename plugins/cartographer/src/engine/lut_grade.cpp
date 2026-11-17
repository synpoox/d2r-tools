#include "engine/lut_grade.h"

#include "core/part_signatures.h"
#include "engine/scan.h"
#include "log.h"

#include <atomic>
#include <cstdint>
#include <cstring>
#include <intrin.h>
#include <map>
#include <windows.h>

namespace carto {

namespace {

// Build 93847; evidence in docs/reverse-engineering/cartographer/50-colour-grading.md.
namespace lg = cartographer::lutgrade;

// The texture upload inside CreateDefaultLut (its target is the call LutUploadCall): its entry bytes are checked before it is hooked, because the
// call site alone does not prove what it calls.
constexpr uint32_t UploadHookSize = 25;
constexpr uint32_t ReleaseCallOffset = 0x5c;  // LutSubmit + 0x5c is its call to the reference release (0xe2924c in build 93847)
const uint8_t UploadGuard[UploadHookSize] = {0x40, 0x55, 0x53, 0x56, 0x57, 0x41, 0x54, 0x41, 0x55, 0x41, 0x56, 0x41, 0x57,
                                            0x48, 0x8D, 0x6C, 0x24, 0xE8, 0x48, 0x81, 0xEC, 0x18, 0x01, 0x00, 0x00};

// The LUT resource (note 50): +0x00 reference count (int32), +0x04 state byte (2 = ready), +0x08 texture, +0xf8 shader view.
constexpr size_t ResourceSize = 0x1000;  // the real object is smaller; the combine pass reads only the fields above
constexpr size_t ResourceTexture = 0x08, ResourceView = 0xf8;

using SubmitFn = void (*)(void* renderer, void** handle, float weight) noexcept;
using UploadFn = uint64_t (*)(void*, void*, void*, void*, uint16_t*, void*, void*, void*, void*) noexcept;
using CreateFn = void (*)(void* renderer, void* resource) noexcept;
using ReleaseFn = void (*)(void* resource) noexcept;

Part g_submitPart = {"LutSubmit", sig::LutSubmit, 15, false, nullptr, {}, {}};
Part g_fallbackPart = {"LutFallbackCall", sig::LutFallbackCall, 0, false, nullptr, {}, {}};
Part g_uploadCallPart = {"LutUploadCall", sig::LutUploadCall, 0, false, nullptr, {}, {}};
Part g_uploadPart = {"LutUpload", nullptr, UploadHookSize, false, nullptr, {}, {}};

SubmitFn g_submit = nullptr;
UploadFn g_upload = nullptr;
CreateFn g_create = nullptr;
ReleaseFn g_release = nullptr;
std::atomic<bool> g_live{false};
std::atomic<uint32_t> g_wanted{0};  // lg::pack of the grade to apply, 0 for none

// Render thread only (the hooks run there): one resource per grade, kept for the whole game.
std::map<uint32_t, uint8_t*> g_luts;  // nullptr: creating it failed, not tried again
uint32_t g_lastLogged = 0;
thread_local const lg::Grade* t_building = nullptr;  // set while CreateDefaultLut runs for a plugin LUT

auto UploadHook(void* a, void* b, void* c, void* d, uint16_t* data, void* e, void* f, void* g, void* h) noexcept -> uint64_t {
	if (t_building != nullptr && data != nullptr) {
		try {
			lg::transformLut(data, lg::LutTexels, *t_building);
		} catch (...) {
		}
	}
	return g_upload(a, b, c, d, data, e, f, g, h);
}

// The plugin's resource for `grade`, made on first use with the game's own default-LUT function while the upload hook transforms its data.
auto LutFor(void* renderer, uint32_t word) -> uint8_t* {
	const auto found = g_luts.find(word);
	if (found != g_luts.end()) {
		return found->second;
	}
	auto* resource = static_cast<uint8_t*>(VirtualAlloc(nullptr, ResourceSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
	if (resource == nullptr) {
		g_luts[word] = nullptr;
		return nullptr;
	}
	// The plugin keeps this reference for good: if the count ever hit zero the game would free the block with its own allocator, corrupting the heap (it is a VirtualAlloc block).
	*reinterpret_cast<int32_t*>(resource) = 1;
	resource[4] = 2;
	const lg::Grade grade = lg::unpack(word);
	t_building = &grade;
	g_create(renderer, resource);
	t_building = nullptr;
	void* const texture = *reinterpret_cast<void**>(resource + ResourceTexture);
	void* const view = *reinterpret_cast<void**>(resource + ResourceView);
	if (texture == nullptr || view == nullptr) {
		plog::Warn("lut grade: the game made no table for hue %d, saturation %d%%: hue and saturation stay off for this map", grade.hue, grade.saturation);
		g_luts[word] = nullptr;  // the block is left allocated and unreferenced by the game
		return nullptr;
	}
	plog::Info("lut grade: made the table for hue %d, saturation %d%% (texture %p, view %p)", grade.hue, grade.saturation, texture, view);
	g_luts[word] = resource;
	return resource;
}

void SubmitHook(void* renderer, void** handle, float weight) noexcept {
	const uint32_t word = g_wanted.load(std::memory_order_relaxed);
	if (word != 0 && renderer != nullptr && handle != nullptr && *handle != nullptr) {
		try {
			uint8_t* ours = LutFor(renderer, word);
			if (ours != nullptr) {
				if (word != g_lastLogged) {
					g_lastLogged = word;
					plog::Info("lut grade: replacing the level's grading table with the plugin's (weight kept: %.3f)", weight);
				}
				void* const game = *handle;
				_InterlockedIncrement(reinterpret_cast<volatile long*>(ours));  // the reference the submit hands on to the list and drops
				void* mine = ours;
				g_submit(renderer, &mine, weight);
				g_release(game);  // the submit no longer drops the caller's reference on the game's own table
				return;
			}
		} catch (...) {
		}
	}
	g_submit(renderer, handle, weight);
}

auto InCode(const CodeRange* ranges, int count, const uint8_t* p) -> bool {
	for (int i = 0; i < count; i++) {
		if (p >= ranges[i].start && p < ranges[i].start + ranges[i].size) {
			return true;
		}
	}
	return false;
}

// The target of the call whose return address is `returnRva`.
auto CallTarget(const D2RL::PluginContext* context, uint64_t returnRva, int afterOffset) -> const uint8_t* {
	int32_t rel = 0;
	const uint8_t* next = reinterpret_cast<const uint8_t*>(context->exeBase + returnRva + afterOffset);
	std::memcpy(&rel, next - 4, sizeof(rel));
	return next + rel;
}

}  // namespace

auto InstallLutGrade(const D2RL::PluginContext* context, const std::vector<SignatureOverride>& overrides, std::vector<PartStatus>* parts) -> bool {
	CodeRange ranges[8];
	const int count = CodeRanges(context->exeBase, ranges, 8);
	ResolvePart(context, overrides, ranges, count, g_submitPart);
	uint64_t fallbackRva = 0, uploadCallRva = 0;
	ResolveCallSite(context, overrides, ranges, count, g_fallbackPart, &fallbackRva);
	ResolveCallSite(context, overrides, ranges, count, g_uploadCallPart, &uploadCallRva);

	const char* missing = nullptr;
	const uint8_t* uploadTarget = nullptr;
	if (g_submitPart.address == nullptr || !g_submitPart.status.on) {
		missing = "LutSubmit";
	} else if (fallbackRva == 0) {
		missing = "LutFallbackCall";
	} else if (uploadCallRva == 0) {
		missing = "LutUploadCall";
	} else {
		// CreateDefaultLut is the second call of the fallback signature (its return address is 15 bytes on from the first call's).
		const uint8_t* create = CallTarget(context, fallbackRva, 15);
		// The reference release is the call LutSubmit makes at +0x5c (opcode E8), so it follows LutSubmit: no signature of its own.
		const uint8_t* const releaseCall = g_submitPart.address + ReleaseCallOffset;
		const uint8_t* release = *releaseCall == 0xE8 ? CallTarget(context, static_cast<uint64_t>(releaseCall + 5 - reinterpret_cast<const uint8_t*>(context->exeBase)), 0) : nullptr;
		uploadTarget = CallTarget(context, uploadCallRva, 0);
		if (!InCode(ranges, count, create) || !InCode(ranges, count, release) || !InCode(ranges, count, uploadTarget)) {
			missing = "a call target outside the code";
		} else if (std::memcmp(uploadTarget, UploadGuard, UploadHookSize) != 0) {
			missing = "LutUpload (its entry bytes are not the recorded ones)";
		} else {
			g_create = reinterpret_cast<CreateFn>(const_cast<uint8_t*>(create));
			g_release = reinterpret_cast<ReleaseFn>(const_cast<uint8_t*>(release));
		}
	}
	if (missing == nullptr) {
		g_uploadPart.address = uploadTarget;
		std::memcpy(g_uploadPart.guard, UploadGuard, UploadHookSize);
		g_uploadPart.guardExact = UploadHookSize;
		g_uploadPart.status = {"LutUpload", true, ""};
		const uint64_t uploadRva = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(uploadTarget) - context->exeBase);
		const bool hooked = context->InstallInlineHook(uploadRva, g_uploadPart.guard, UploadHookSize, reinterpret_cast<void*>(&UploadHook), reinterpret_cast<void**>(&g_upload));
		if (HookOutcome(g_uploadPart, hooked, g_upload != nullptr)) {
			const uint64_t rva = static_cast<uint64_t>(reinterpret_cast<uintptr_t>(g_submitPart.address) - context->exeBase);
			const bool submitHooked = context->InstallInlineHook(rva, g_submitPart.guard, g_submitPart.hookSize, reinterpret_cast<void*>(&SubmitHook), reinterpret_cast<void**>(&g_submit));
			g_live = HookOutcome(g_submitPart, submitHooked, g_submit != nullptr);
			if (!g_live) {
				missing = "LutSubmit (the hook was not installed)";
			}
		} else {
			missing = "LutUpload (the hook was not installed)";
		}
		plog::Info("part %s: %s", g_uploadPart.name, g_uploadPart.status.detail.c_str());
	}
	plog::Info("part %s: %s", g_submitPart.name, g_submitPart.status.detail.c_str());
	plog::Info("part %s: %s", g_fallbackPart.name, g_fallbackPart.status.detail.c_str());
	plog::Info("part %s: %s", g_uploadCallPart.name, g_uploadCallPart.status.detail.c_str());
	if (!g_live.load()) {
		plog::Warn("map Hue and Saturation are off: part %s is not available (the other map looks still work)", missing != nullptr ? missing : "?");
	}
	if (parts != nullptr) {
		parts->push_back(g_submitPart.status);
		parts->push_back(g_fallbackPart.status);
		parts->push_back(g_uploadCallPart.status);
		parts->push_back(g_uploadPart.status);
	}
	return g_live.load();
}

void LutGradeSet(const lg::Grade* grade) noexcept {
	g_wanted.store(grade != nullptr && grade->any() ? lg::pack(*grade) : 0, std::memory_order_relaxed);
}

auto LutGradeLive() noexcept -> bool {
	return g_live.load(std::memory_order_relaxed);
}

}  // namespace carto
