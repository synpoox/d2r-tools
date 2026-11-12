#include "map_names.h"

#include "scan.h"

#include "core/part_signatures.h"
#include "game_memory.h"
#include "log.h"
#include "mapname.h"

#include <windows.h>

#include <atomic>
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

std::mutex g_lock;     // state below; the game thread takes it in MapNamesBeforeOpen, so no SDK call is made while holding it
std::mutex g_regLock;  // one registration change at a time; the game thread only try-locks it (ApplyPlan), so it never waits behind an SDK call
std::atomic<bool> g_again{false};  // a change was asked for while another thread held g_regLock: the holder runs it
const D2RL::PluginContext* g_context = nullptr;
const D2RL::ResourceService* g_resources = nullptr;
const D2RL::LocalizationService* g_localization = nullptr;
bool g_installed = false;
mn::RegistrationPlan g_plan;  // what the tables want against what the loader holds
D2RL::Resources::RegistrationHandle g_handle = D2RL::Resources::InvalidHandle;

// A row this plugin wrote into (the keys it wrote are in g_saver; the restore only touches a field that still holds them).
struct Target {
	int level;
	const uint8_t* row;
};
mn::RowSaver g_saver;
std::vector<Target> g_targets;

// The last write of MapNamesBeforeOpen, so a failed open can undo just that.
struct LastWrite {
	bool valid = false;
	int level = 0;
	bool first = false;  // that write was the level's first, so undoing it restores the saved original
	uint8_t prevName[mn::FieldSize] = {};
	uint8_t prevEntry[mn::FieldSize] = {};
};
LastWrite g_last;

// Rows hold keys of ours (or a restore failed, for the rest of the process): the registration must stay as it is. g_lock must be held.
auto RowsHeldLocked() -> bool {
	return g_saver.HoldsKeys();
}

auto RowOf(uint32_t dataFlag, int level) noexcept -> uint8_t* {
	if (g_part.address == nullptr || !g_part.status.on || dataFlag >= DataFlagLimit || level <= 0) {
		return nullptr;
	}
	return reinterpret_cast<LevelsRowFn>(const_cast<uint8_t*>(g_part.address))(dataFlag, level);
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

// Makes the loader's registration match what the tables want, unless a row holds one of our keys (then the change stays
// pending until MapNamesRestore / MapNamesOpenFailed calls this again). The SDK calls are made without g_lock, so the game
// thread is never held up behind them; g_regLock keeps two changes apart, and the plan says "nothing registered" while one
// is under way, which makes MapNamesBeforeOpen skip.
void ApplyOnce() noexcept {
	try {
		D2RL::Resources::RegistrationHandle oldHandle = D2RL::Resources::InvalidHandle;
		std::string json;
		bool registerNew = false;
		std::vector<std::string> registeredNames;
		{
			std::lock_guard<std::mutex> lock(g_lock);
			if (!g_installed || g_resources == nullptr || g_localization == nullptr) {
				return;
			}
			const mn::RegistrationPlan::Step step = g_plan.Next(RowsHeldLocked());
			if (step == mn::RegistrationPlan::Step::Nothing) {
				return;
			}
			if (step == mn::RegistrationPlan::Step::Deferred) {
				plog::Info("map names: the change to the map names waits until the open map's text is back");
				return;
			}
			oldHandle = g_handle;
			g_handle = D2RL::Resources::InvalidHandle;
			g_plan.Dropped();
			registerNew = g_plan.WantsRegistration();
			if (registerNew) {
				registeredNames = g_plan.Wanted();
				json = mn::StringsJson(registeredNames);
			}
		}
		if (oldHandle != D2RL::Resources::InvalidHandle) {
			(void)g_resources->unregisterResource(g_context, oldHandle);
		}
		if (!registerNew) {
			return;
		}
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
		std::lock_guard<std::mutex> lock(g_lock);
		g_handle = handle;
		g_plan.Applied(registeredNames);
		plog::Info("map names: registered %zu map name(s) as strings", g_plan.Held().size());
	} catch (...) {
		plog::Warn("map names: building the strings file failed; maps keep the slot level's name");
	}
}

// Runs ApplyOnce under g_regLock. The game thread (gameThread) only try-locks: if another thread is in the middle of a
// change it asks that thread to run it again and returns, so the game thread never waits behind an SDK call.
void ApplyPlan(bool gameThread = false) noexcept {
	g_again = true;
	for (;;) {
		std::unique_lock<std::mutex> change(g_regLock, std::defer_lock);
		if (gameThread) {
			if (!change.try_lock()) {
				return;  // the holder sees g_again and runs it
			}
		} else {
			change.lock();
		}
		while (g_again.exchange(false)) {
			ApplyOnce();
		}
		change.unlock();
		if (!g_again.load()) {
			return;
		}
	}
}

void SetNamesLocked(const std::shared_ptr<const Tables>& tables) {
	std::vector<std::string> names;
	if (tables != nullptr) {
		for (const MapDef& map : tables->maps) {
			if (names.size() >= mn::MaxMaps) {
				break;
			}
			names.push_back(mn::DisplayName(map.name));
		}
	}
	g_plan.Want(tables != nullptr && tables->settings.mapNames != 0, std::move(names));
}

auto FieldHolds(const uint8_t* field, const uint8_t (&bytes)[mn::FieldSize]) noexcept -> bool {
	return std::memcmp(field, bytes, mn::FieldSize) == 0;
}

// Puts one level's saved original back, through the row pointer kept at write time (the game's row getter is not called: at
// process exit its tables may be gone). Memory is checked first, and a field is touched only if it still holds the key this
// plugin wrote. g_lock must be held.
void RestoreLevelLocked(int level) noexcept {
	mn::SavedRow saved;
	if (!g_saver.Take(level, &saved)) {
		return;
	}
	const Target* target = nullptr;
	for (const Target& t : g_targets) {
		if (t.level == level) {
			target = &t;
		}
	}
	if (target == nullptr) {
		plog::Warn("map names: level %d has no recorded row; its text was not put back", level);
		return;
	}
	uint8_t* row = const_cast<uint8_t*>(target->row);
	uint8_t* nameField = row + mn::RowNameOffset;
	uint8_t* entryField = row + mn::RowEntryOffset;
	if (!Readable(nameField, mn::FieldSize) || !Readable(entryField, mn::FieldSize)) {
		plog::Warn("map names: level %d row is no longer readable; its text was not put back", level);
	} else {
		// Each field on its own: after a half-finished write only one of them holds our key.
		bool ok = true;
		bool any = false;
		if (FieldHolds(nameField, saved.keyName)) {
			any = true;
			ok = WriteGame(nameField, saved.name, mn::FieldSize) && ok;
		}
		if (FieldHolds(entryField, saved.keyEntry)) {
			any = true;
			ok = WriteGame(entryField, saved.entry, mn::FieldSize) && ok;
		}
		if (!any) {
			plog::Warn("map names: level %d row no longer holds the plugin's keys (moved or changed); left as it is", level);
		} else if (ok) {
			plog::Info("map names: level %d text put back", level);
		} else {
			// The saved original is dropped (Take) so the plugin's own key is never remembered as the original next game.
			g_saver.MarkStuck();
			plog::Error("map names: level %d text could not be put back; the registered strings stay", level);
		}
	}
	for (auto it = g_targets.begin(); it != g_targets.end(); ++it) {
		if (it->level == level) {
			g_targets.erase(it);
			break;
		}
	}
}

void RestoreAllLocked() noexcept {
	try {
		std::vector<int> levels;
		for (const Target& t : g_targets) {
			levels.push_back(t.level);
		}
		for (const int level : levels) {
			RestoreLevelLocked(level);
		}
		g_saver.TakeAll();  // anything left has no target and cannot be put back
		g_targets.clear();
	} catch (...) {
		plog::Error("map names: restoring the level text failed");
	}
}

// Undoes the last BeforeOpen write: the level's original if that write was the first one, else the previous key. g_lock must be held.
void UndoLastLocked() noexcept {
	const LastWrite last = g_last;
	g_last = LastWrite();
	if (!last.valid) {
		return;
	}
	if (last.first) {
		RestoreLevelLocked(last.level);
		return;
	}
	for (const Target& t : g_targets) {
		if (t.level == last.level) {
			uint8_t* row = const_cast<uint8_t*>(t.row);
			g_saver.SetKeys(last.level, last.prevName, last.prevEntry);  // the row holds the previous map's keys again; the restore must match those
			if (!WriteGame(row + mn::RowNameOffset, last.prevName, mn::FieldSize) || !WriteGame(row + mn::RowEntryOffset, last.prevEntry, mn::FieldSize)) {
				g_saver.MarkStuck();
				plog::Error("map names: level %d previous text could not be put back", last.level);
			}
			return;
		}
	}
}

}  // namespace

auto InstallMapNames(const D2RL::PluginContext* context, const std::vector<SignatureOverride>& overrides, std::vector<PartStatus>* parts) -> bool {
	std::unique_lock<std::mutex> lock(g_lock);
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
	const bool wanted = g_plan.WantsRegistration();
	const bool services = g_resources != nullptr && g_localization != nullptr;
	const bool ok = g_part.status.on && services;
	lock.unlock();
	if (wanted) {
		if (!services) {
			plog::Warn("map names: D2RLoader has no Resource or Localization service of the expected version; maps keep the slot level's name");
		} else {
			ApplyPlan();
		}
	}
	return ok;
}

auto LevelsRowOf(uint32_t dataFlag, int level) noexcept -> uint8_t* {
	std::lock_guard<std::mutex> lock(g_lock);
	return RowOf(dataFlag, level);
}

auto MapNamesPartStatuses() -> std::vector<PartStatus> {
	std::lock_guard<std::mutex> lock(g_lock);
	return {g_part.status};
}

auto MapNamesSummary() -> std::string {
	std::lock_guard<std::mutex> lock(g_lock);
	if (!g_plan.WantsRegistration() && !g_plan.Registered()) {
		return "Map names: off (MapNames=0 in cartographer.txt, the default)";
	}
	if (!g_installed || !g_part.status.on || g_resources == nullptr || g_localization == nullptr) {
		return "Map names: off (a game part or D2RLoader service was not found)";
	}
	if (g_plan.Next(RowsHeldLocked()) == mn::RegistrationPlan::Step::Deferred) {
		if (g_saver.Stuck()) {
			return "Map names: a change from the last reload waits until the game restarts (written, not verified in game)";
		}
		return "Map names: a change from the last reload waits until you leave this game (written, not verified in game)";
	}
	return g_plan.Registered() ? "Map names: on (written, not verified in game)" : "Map names: on, but the strings are not registered";
}

void MapNamesSetTables(const std::shared_ptr<const Tables>& tables) {
	{
		std::lock_guard<std::mutex> lock(g_lock);
		SetNamesLocked(tables);
	}
	ApplyPlan();
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
		g_last = LastWrite();
		if (g_localization == nullptr || g_resources == nullptr || !g_plan.Registered()) {
			return skip("the strings are not registered");
		}
		const std::string display = mn::DisplayName(map.name);
		const std::vector<std::string>& registered = g_plan.Held();
		size_t index = registered.size();
		for (size_t i = 0; i < registered.size(); ++i) {
			if (registered[i] == display) {
				index = i;
				break;
			}
		}
		if (index >= registered.size()) {
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
		const bool had = g_saver.Has(level);
		if (!had && (mn::IsOwnKey(nameField, PluginId) || mn::IsOwnKey(entryField, PluginId))) {
			return skip("the row still holds the plugin's own key (an earlier restore failed), so it is not taken as the original");
		}
		uint8_t newName[mn::FieldSize], newEntry[mn::FieldSize];
		if (!mn::WriteKey(newName, nameKey) || !mn::WriteKey(newEntry, entryKey)) {
			return skip("a key does not fit the 40-byte field");
		}
		g_last.level = level;
		g_last.first = !had;
		std::memcpy(g_last.prevName, nameField, mn::FieldSize);
		std::memcpy(g_last.prevEntry, entryField, mn::FieldSize);
		g_last.valid = true;
		if (!had) {
			g_saver.Remember(level, nameField, entryField);
			g_targets.push_back(Target{level, row});
		}
		for (Target& t : g_targets) {
			if (t.level == level) {
				t.row = row;
			}
		}
		g_saver.SetKeys(level, newName, newEntry);
		plog::Info("map names: level %d row %p: writing '%s' and '%s'", level, static_cast<const void*>(row), nameKey.c_str(), entryKey.c_str());
		if (!WriteGame(nameField, newName, mn::FieldSize) || !WriteGame(entryField, newEntry, mn::FieldSize)) {
			plog::Warn("map names: writing the row failed; putting the previous text back");
			UndoLastLocked();
			return false;
		}
		return true;
	} catch (...) {
		plog::Error("map names: failed for '%s'; maps keep the slot level's name", map.name.c_str());
		return false;
	}
}

void MapNamesOpenFailed(int level) noexcept {
	try {
		{
			std::lock_guard<std::mutex> lock(g_lock);
			if (g_last.valid && g_last.level == level) {
				UndoLastLocked();
			}
		}
		ApplyPlan(true);
	} catch (...) {
		plog::Error("map names: undoing the level text failed");
	}
}

void MapNamesRestore() noexcept {
	MapNamesUnload();
	ApplyPlan(true);  // a table change that waited for the rows
}

void MapNamesUnload() noexcept {
	try {
		std::lock_guard<std::mutex> lock(g_lock);
		g_last = LastWrite();
		RestoreAllLocked();  // a failed restore keeps the registration (the saver stays stuck)
	} catch (...) {
		plog::Error("map names: restoring the level text failed");
	}
}

}  // namespace carto
