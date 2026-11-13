#include "scan.h"

#include "core/sig.h"
#include "core/text.h"
#include "hookguard.h"
#include "log.h"

#include <string_view>

#include <windows.h>

namespace carto {

auto CodeRanges(uintptr_t exeBase, CodeRange* out, int max) noexcept -> int {
	const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(exeBase);
	if (exeBase == 0 || dos->e_magic != IMAGE_DOS_SIGNATURE) {
		return 0;
	}
	const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(exeBase + static_cast<uintptr_t>(dos->e_lfanew));
	if (nt->Signature != IMAGE_NT_SIGNATURE) {
		return 0;
	}
	const IMAGE_SECTION_HEADER* section = IMAGE_FIRST_SECTION(nt);
	int n = 0;
	for (WORD i = 0; i < nt->FileHeader.NumberOfSections && n < max; ++i, ++section) {
		if ((section->Characteristics & IMAGE_SCN_MEM_EXECUTE) != 0 && section->Misc.VirtualSize > 0) {
			out[n++] = {reinterpret_cast<const uint8_t*>(exeBase + section->VirtualAddress), section->Misc.VirtualSize};
		}
	}
	return n;
}

// The one place `sig` matches in the game's code. Fails (with the reason) when it matches nowhere or more than once.
static auto FindUnique(const CodeRange* ranges, int count, const char* sig, Pattern& pattern, std::string* why) noexcept -> const uint8_t* {
	if (!ParsePattern(sig, &pattern)) {
		*why = "the signature is not valid (hex bytes and ?? separated by spaces, not starting with ??)";
		return nullptr;
	}
	const uint8_t* found = nullptr;
	int matches = 0;
	for (int i = 0; i < count && matches < 2; ++i) {
		size_t at = 0;
		const int n = FindPattern(ranges[i].start, ranges[i].size, pattern, &at);
		if (n > 0 && matches == 0) {
			found = ranges[i].start + at;
		}
		matches += n;
	}
	if (matches != 1) {
		*why = matches == 0 ? "the signature matches nowhere in this game's code" : "the signature matches more than once in this game's code";
		return nullptr;
	}
	return found;
}

// True when `address` is the first byte of a function in the exe's .pdata. *known: whether that could be checked.
static auto IsFunctionEntry(uintptr_t exeBase, const uint8_t* address, bool* known) noexcept -> bool {
	*known = false;
	const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(exeBase);
	if (exeBase == 0 || dos->e_magic != IMAGE_DOS_SIGNATURE) {
		return false;
	}
	const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(exeBase + static_cast<uintptr_t>(dos->e_lfanew));
	const IMAGE_DATA_DIRECTORY& dir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXCEPTION];
	if (dir.VirtualAddress == 0 || dir.Size < sizeof(RUNTIME_FUNCTION)) {
		return false;
	}
	*known = true;
	const auto* table = reinterpret_cast<const RUNTIME_FUNCTION*>(exeBase + dir.VirtualAddress);
	const size_t n = dir.Size / sizeof(RUNTIME_FUNCTION);
	const auto rva = static_cast<DWORD>(reinterpret_cast<uintptr_t>(address) - exeBase);
	size_t lo = 0, hi = n;
	while (lo < hi) {
		const size_t mid = lo + (hi - lo) / 2;
		if (table[mid].BeginAddress == rva) {
			return true;
		}
		if (table[mid].BeginAddress < rva) {
			lo = mid + 1;
		} else {
			hi = mid;
		}
	}
	return false;
}

// Finds one part by its signature (or the one from signatures.txt) and checks its guards; sets its address and status.
void ResolvePart(const D2RL::PluginContext* context, const std::vector<SignatureOverride>& overrides, const CodeRange* ranges, int count, Part& part) {
	part.status = {part.name, false, ""};
	const char* signature = part.signature;
	std::string overridden;
	bool isOverride = false;
	for (const SignatureOverride& o : overrides) {
		if (EqualsNoCase(o.part, part.name)) {
			overridden = o.signature;
			signature = overridden.c_str();
			isOverride = true;
		}
	}
	if (signature == nullptr) {
		part.status.detail = "off: no signature recorded for this part yet";
	} else {
		std::string why;
		Pattern pattern{};
		part.address = nullptr;
		part.guardExact = 0;
		if (count == 0) {
			why = "the game image could not be read";
		} else {
			part.address = FindUnique(ranges, count, signature, pattern, &why);
		}
		// A hooked part's signature must pin every byte the hook cuts (they are the guard), and a signature from
		// signatures.txt is untrusted: it must also start at a function.
		if (part.address != nullptr && part.hookSize > 0) {
			bool concrete = pattern.size >= part.hookSize && part.hookSize <= sizeof(part.guard);
			for (uint32_t i = 0; concrete && i < part.hookSize; ++i) {
				concrete = !pattern.wild[i];
			}
			if (!concrete) {
				why = "the signature must give at least " + std::to_string(part.hookSize) + " exact bytes (no ??) at the entry, because the hook cuts them";
				part.address = nullptr;
			} else {
				std::memcpy(part.guard, pattern.bytes, part.hookSize);
				part.guardExact = part.hookSize;
			}
		}
		// An override of a hooked part must keep the built-in entry bytes: the hook is written for that prologue.
		if (part.address != nullptr && isOverride && part.hookSize > 0 && part.signature != nullptr) {
			Pattern builtIn{};
			if (!ParsePattern(part.signature, &builtIn) || builtIn.size < part.hookSize ||
			    std::memcmp(builtIn.bytes, pattern.bytes, part.hookSize) != 0) {
				why = "the signature from signatures.txt does not start with the built-in entry bytes this hook is written for";
				part.address = nullptr;
			}
		}
		// Only a hooked part must start at a .pdata function: a called-only part can be a leaf with no .pdata entry
		// (RoomLevelId, 0x2efc10, is one), so it needs a unique match only.
		if (part.address != nullptr && isOverride && part.hookSize > 0) {
			bool known = false;
			if (!IsFunctionEntry(context->exeBase, part.address, &known)) {
				why = known ? "the signature from signatures.txt does not match at the start of a function"
				            : "the game's exception directory is missing, so the signature from signatures.txt cannot be checked";
				part.address = nullptr;
			}
		}
		part.status.on = part.address != nullptr;
		part.status.detail = part.status.on ? (isOverride ? "found by signature (signatures.txt)" : "found by signature") : "off: " + why;
	}
}

// What a hook that cannot be undone costs the player, by part, for the log.
static const char* HookConsequence(const char* name) noexcept {
	const std::string_view n(name);
	if (n == "LevelBuild") return "EVERY level build is skipped (not only the maze), so no level will load";
	if (n == "OutdoorAct5Builder") return "the game's Act 5 outdoor builder is skipped";
	if (n == "DeathDrop") return "the game's own monster drop is skipped for EVERY monster";
	if (n == "ChestOperate") return "the game's chest use is skipped for EVERY chest";
	if (n == "PopulateRoom") return "the game's own room population is skipped for EVERY room";
	if (n == "PlayerTick") return "the game's player mana step is skipped for every player";
	if (n == "ItemSpellUse" || n == "ItemSpellCheck") return "every item-spell use is dropped (not only map items)";
	if (n == "ItemQualityRoll") return "the game's item quality roll is skipped for EVERY item";
	if (n == "MagicFallback") return "the game's magic fallback is skipped for EVERY item that reaches it";
	if (n == "MonsterSetup") return "the game's own setup is skipped for EVERY monster (not only the map's)";
	return "the game's own function is skipped (the maze code, for a maze part)";
}

auto HookOutcome(Part& part, bool installed, bool haveOriginal) noexcept -> bool {
	namespace hg = cartographer::hookguard;
	const hg::HookState state = hg::classifyHook(installed, haveOriginal);
	bool restored = false;
	if (state == hg::HookState::NoOriginal) {
		if (hg::canRestoreHook(state, part.hookSize, sizeof(part.guard), part.guardExact) && part.address != nullptr) {
			// The guard is the signature's own exact bytes and is what the loader overwrote and re-checks, so writing it back removes
			// the hook; the loader's trampoline is left unused. Nothing has run through the hook yet: this is install time.
			auto* target = const_cast<uint8_t*>(part.address);
			DWORD old = 0;
			if (VirtualProtect(target, part.hookSize, PAGE_EXECUTE_READWRITE, &old) != FALSE) {
				std::memcpy(target, part.guard, part.hookSize);
				DWORD ignored = 0;
				VirtualProtect(target, part.hookSize, old, &ignored);
				FlushInstructionCache(GetCurrentProcess(), target, part.hookSize);
				restored = true;
			}
		}
		if (restored) {
			plog::Error("part %s: D2RLoader installed the hook but gave no original; the game's own %u entry bytes were written back, so the game runs its own function; the part and its feature are off", part.name, part.hookSize);
		} else {
			plog::Error("part %s: D2RLoader installed the hook but gave no original, and it could not be taken out (%s); %s until the game is restarted; restart it after removing what conflicts", part.name,
			            part.guardExact < part.hookSize ? "the original bytes are not all known" : "the code could not be written", HookConsequence(part.name));
		}
	}
	const bool ok = hg::hookUsable(state);
	part.status.on = ok;
	part.status.detail = hg::hookDetail(state, restored);
	return ok;
}

}  // namespace carto
