#include "map_names.h"

#include "scan.h"

#include "core/part_signatures.h"
#include "log.h"
#include "mapname.h"

#include <windows.h>

#include <cstring>
#include <mutex>
#include <string>

namespace carto {

namespace {

namespace mn = cartographer::mapname;

constexpr const char* PluginId = "cartographer";  // PluginInfo.id: the string namespace of the plugin's own strings
constexpr const char* StringsPath = "data/local/lng/strings/d2rloader/cartographer/maps.json";
constexpr uint32_t DataFlagLimit = 4;

using LevelsRowFn = uint8_t* (*)(uint32_t dataFlag, int32_t levelId) noexcept;

Part g_part = {"LevelsRow", sig::LevelsRow, 0, false, nullptr, {}, {}};

std::mutex g_lock;
const D2RL::PluginContext* g_context = nullptr;
const D2RL::ResourceService* g_resources = nullptr;
const D2RL::LocalizationService* g_localization = nullptr;
bool g_installed = false;
bool g_wanted = false;                          // MapNames=1 in cartographer.txt
std::vector<std::string> g_names;               // display names, in the order of the registered keys
std::vector<std::string> g_registeredNames;     // what the loader holds now
D2RL::Resources::RegistrationHandle g_handle = D2RL::Resources::InvalidHandle;
bool g_registered = false;

struct Target {
	int level;
	uint32_t dataFlag;
	const uint8_t* row;
};
mn::RowSaver g_saver;
std::vector<Target> g_targets;

auto RowOf(uint32_t dataFlag, int level) noexcept -> uint8_t* {
	if (g_part.address == nullptr || !g_part.status.on || dataFlag >= DataFlagLimit || level <= 0) {
		return nullptr;
	}
	return reinterpret_cast<LevelsRowFn>(const_cast<uint8_t*>(g_part.address))(dataFlag, level);
}

// Copies into game memory, lifting the page protection for the copy if the page is not writable. False if it could not.
auto WriteGame(void* dst, const void* src, size_t size) noexcept -> bool {
	MEMORY_BASIC_INFORMATION info{};
	if (VirtualQuery(dst, &info, sizeof(info)) == 0 || info.State != MEM_COMMIT) {
		return false;
	}
	const DWORD writable = PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
	if ((info.Protect & writable) != 0) {
		std::memcpy(dst, src, size);
		return true;
	}
	DWORD old = 0;
	if (!VirtualProtect(dst, size, PAGE_READWRITE, &old)) {
		return false;
	}
	std::memcpy(dst, src, size);
	DWORD ignored = 0;
	VirtualProtect(dst, size, old, &ignored);
	return true;
}

// The text the loader holds for one of the plugin's own keys, in the current language.
auto TextOf(const std::string& localKey, std::string* out) noexcept -> bool {
	char buffer[256] = {};
	uint32_t required = 0;
	const D2RL::Localization::Result result = g_localization->getStringByKey(g_context, localKey.c_str(), buffer, sizeof(buffer), &required);
	if (result != D2RL::Localization::Result::Success) {
		return false;
	}
	*out = buffer;
	return true;
}

// Registers (or replaces) the strings. g_lock must be held.
void RegisterLocked() noexcept {
	if (!g_installed || g_resources == nullptr) {
		return;
	}
	const bool want = g_wanted && !g_names.empty();
	if (want ? (g_registered && g_registeredNames == g_names) : !g_registered) {
		return;  // nothing to change
	}
	if (g_handle != D2RL::Resources::InvalidHandle) {
		(void)g_resources->unregisterResource(g_context, g_handle);
		g_handle = D2RL::Resources::InvalidHandle;
		g_registered = false;
		g_registeredNames.clear();
	}
	if (!want) {
		return;
	}
	try {
		const std::string json = mn::StringsJson(g_names);
		D2RL::Resources::ResourceRegistration resource{};
		resource.structSize = D2RL::Resources::ResourceRegistrationSize;
		resource.path = StringsPath;
		resource.bytes = json.data();
		resource.byteCount = json.size();
		D2RL::Resources::RegistrationHandle handle = D2RL::Resources::InvalidHandle;
		const D2RL::Resources::Result result = g_resources->registerResource(g_context, &resource, &handle);
		if (result != D2RL::Resources::Result::Success) {
			plog::Warn("map names: D2RLoader refused the strings file (result %u); maps keep the slot level's name", static_cast<uint32_t>(result));
			return;
		}
		g_handle = handle;
		g_registered = true;
		g_registeredNames = g_names;
		plog::Info("map names: registered %zu map name(s) as strings", g_names.size());
	} catch (...) {
		plog::Warn("map names: building the strings file failed; maps keep the slot level's name");
	}
}

void SetNamesLocked(const std::shared_ptr<const Tables>& tables) {
	g_wanted = tables != nullptr && tables->settings.mapNames != 0;
	g_names.clear();
	if (tables != nullptr) {
		for (const MapDef& map : tables->maps) {
			if (g_names.size() >= mn::MaxMaps) {
				break;
			}
			g_names.push_back(mn::DisplayName(map.name));
		}
	}
}

// Puts the saved fields back. g_lock must be held.
void RestoreLocked() noexcept {
	try {
		const std::vector<mn::SavedRow> rows = g_saver.TakeAll();
		const std::vector<Target> targets = std::move(g_targets);
		g_targets.clear();
		for (const mn::SavedRow& saved : rows) {
			const Target* target = nullptr;
			for (const Target& t : targets) {
				if (t.level == saved.level) {
					target = &t;
				}
			}
			uint8_t* row = target != nullptr ? RowOf(target->dataFlag, saved.level) : nullptr;
			if (row == nullptr || row != target->row) {
				plog::Warn("map names: level %d row moved or gone; its text was not put back", saved.level);
				continue;
			}
			const bool a = WriteGame(row + mn::RowNameOffset, saved.name, mn::FieldSize);
			const bool b = WriteGame(row + mn::RowEntryOffset, saved.entry, mn::FieldSize);
			plog::Info("map names: level %d text %s", saved.level, a && b ? "put back" : "could not be put back");
		}
	} catch (...) {
		plog::Error("map names: restoring the level text failed");
	}
}

}  // namespace

auto InstallMapNames(const D2RL::PluginContext* context, const std::vector<SignatureOverride>& overrides, std::vector<PartStatus>* parts) -> bool {
	std::lock_guard<std::mutex> lock(g_lock);
	g_context = context;
	CodeRange ranges[8];
	const int count = CodeRanges(context->exeBase, ranges, 8);
	ResolvePart(context, overrides, ranges, count, g_part);
	plog::Info("part %s: %s", g_part.name, g_part.status.detail.c_str());
	if (parts != nullptr) {
		parts->push_back(g_part.status);
	}
	g_resources = nullptr;
	g_localization = nullptr;
	if (context->QueryService(&g_resources) != D2RL::ServiceQueryResult::Success ||
	    !D2RL::HasResourceServiceField(g_resources, D2RL::ResourceServiceRequiredSize)) {
		g_resources = nullptr;
	}
	if (context->QueryService(&g_localization) != D2RL::ServiceQueryResult::Success ||
	    !D2RL::HasLocalizationServiceField(g_localization, D2RL::LocalizationServiceRequiredSize) || g_localization->getStringByKey == nullptr) {
		g_localization = nullptr;
	}
	g_installed = true;
	if (g_wanted) {
		if (g_resources == nullptr || g_localization == nullptr) {
			plog::Warn("map names: D2RLoader has no Resource or Localization service of the expected version; maps keep the slot level's name");
		} else {
			RegisterLocked();
		}
	}
	return g_part.status.on && g_resources != nullptr && g_localization != nullptr;
}

auto MapNamesPartStatuses() -> std::vector<PartStatus> {
	std::lock_guard<std::mutex> lock(g_lock);
	return {g_part.status};
}

auto MapNamesSummary() -> std::string {
	std::lock_guard<std::mutex> lock(g_lock);
	if (!g_wanted) {
		return "Map names: off (MapNames=0 in cartographer.txt, the default)";
	}
	if (!g_installed || !g_part.status.on || g_resources == nullptr || g_localization == nullptr) {
		return "Map names: off (a game part or D2RLoader service was not found)";
	}
	return g_registered ? "Map names: on (written, not verified in game)" : "Map names: on, but the strings are not registered";
}

void MapNamesSetTables(const std::shared_ptr<const Tables>& tables) {
	std::lock_guard<std::mutex> lock(g_lock);
	SetNamesLocked(tables);
	if (g_localization != nullptr) {
		RegisterLocked();
	}
}

auto MapNamesBeforeOpen(const MapDef& map, const Tables& tables, int level, uint32_t dataFlag) noexcept -> bool {
	if (tables.settings.mapNames == 0) {
		return false;
	}
	try {
		std::lock_guard<std::mutex> lock(g_lock);
		const auto skip = [&](const char* why) {
			plog::Warn("map names: '%s' keeps the slot level's name: %s", map.name.c_str(), why);
			return false;
		};
		if (!g_installed || !g_part.status.on) {
			return skip("the LevelsRow part is off");
		}
		if (g_localization == nullptr || g_resources == nullptr || !g_registered) {
			return skip("the strings are not registered");
		}
		const std::string display = mn::DisplayName(map.name);
		size_t index = g_registeredNames.size();
		for (size_t i = 0; i < g_registeredNames.size(); ++i) {
			if (g_registeredNames[i] == display) {
				index = i;
				break;
			}
		}
		if (index >= g_registeredNames.size()) {
			return skip("its name is not among the registered strings (tables changed since)");
		}
		const std::string nameLocal = mn::NameLocalKey(index), entryLocal = mn::EntryLocalKey(index);
		const std::string nameKey = mn::RowKey(PluginId, nameLocal), entryKey = mn::RowKey(PluginId, entryLocal);
		if (!mn::KeyFits(nameKey) || !mn::KeyFits(entryKey)) {
			return skip("a key does not fit the 40-byte field");
		}
		// The loader must hold the text before the game is pointed at it (a key it does not know reads "Missing String").
		std::string nameText, entryText;
		if (!TextOf(nameLocal, &nameText) || !TextOf(entryLocal, &entryText) || nameText != display || entryText != mn::EntryText(display)) {
			return skip("D2RLoader does not return the registered text yet");
		}
		uint8_t* row = RowOf(dataFlag, level);
		if (row == nullptr) {
			return skip("the levels row was not found");
		}
		uint8_t* nameField = row + mn::RowNameOffset;
		uint8_t* entryField = row + mn::RowEntryOffset;
		if (!mn::LooksLikeKey(nameField) || !mn::LooksLikeKey(entryField)) {
			return skip("the row's text fields do not look like keys (a different build?)");
		}
		const bool first = g_saver.Remember(level, nameField, entryField);
		if (first) {
			g_targets.push_back({level, dataFlag, row});
		}
		uint8_t newName[mn::FieldSize], newEntry[mn::FieldSize];
		if (!mn::WriteKey(newName, nameKey) || !mn::WriteKey(newEntry, entryKey)) {
			return skip("a key does not fit the 40-byte field");
		}
		plog::Info("map names: level %d row %p: writing '%s' and '%s'", level, static_cast<const void*>(row), nameKey.c_str(), entryKey.c_str());
		if (!WriteGame(nameField, newName, mn::FieldSize) || !WriteGame(entryField, newEntry, mn::FieldSize)) {
			plog::Warn("map names: writing the row failed; putting the original back");
			RestoreLocked();
			return false;
		}
		return true;
	} catch (...) {
		plog::Error("map names: failed for '%s'; maps keep the slot level's name", map.name.c_str());
		return false;
	}
}

void MapNamesRestore() noexcept {
	try {
		std::lock_guard<std::mutex> lock(g_lock);
		RestoreLocked();
	} catch (...) {
		plog::Error("map names: restoring the level text failed");
	}
}

}  // namespace carto
