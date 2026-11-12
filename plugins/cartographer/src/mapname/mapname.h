// A map's own Name on the red portal tooltip, the automap title and the entry banner (issue #90). Pure C++, no engine
// headers. The game reads three text keys from the slot level's row in its levels table (LevelName at +0xfd and
// LevelEntry at +0x14d, each a char[40]); the plugin registers one text per map under its own string namespace and, when a
// map opens, writes the map's two keys into those fields, then puts the originals back. Notes:
// docs/reverse-engineering/cartographer/10-portal-tooltip.md.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace cartographer::mapname {

inline constexpr int RowNameOffset = 0xfd;    // levels.txt LevelName in a levels row (build 93847)
inline constexpr int RowEntryOffset = 0x14d;  // levels.txt LevelEntry
inline constexpr size_t FieldSize = 40;       // char[40]: a key of at most 39 characters and the NUL
inline constexpr size_t MaxNameBytes = 60;    // a longer Name is cut (at a whole character) before it becomes text
inline constexpr size_t MaxMaps = 9999;       // keeps every key far inside the field

// The languages the game ships. A string that lacks a language does not fall back to English, so every record names all of them.
inline constexpr const char* Languages[] = {"enUS", "zhTW", "deDE", "esES", "frFR", "itIT", "koKR", "plPL", "esMX", "jaJP", "ptBR", "ruRU", "zhCN"};

// The text shown for a map: control characters become spaces, invalid UTF-8 becomes U+FFFD (one per bad byte), the ends are
// trimmed, and at most MaxNameBytes bytes are kept without splitting a UTF-8 character. An empty result becomes "Map".
auto DisplayName(const std::string& name) -> std::string;

// "Entering <name>": the entry banner text.
auto EntryText(const std::string& displayName) -> std::string;

// Keys in the plugin's own namespace. LocalKey is what the plugin's string file defines; RowKey (the namespace, a colon,
// then the local key) is what is written into the levels row. Index is the map's place in the registered list.
auto NameLocalKey(size_t index) -> std::string;
auto EntryLocalKey(size_t index) -> std::string;
auto RowKey(const std::string& pluginId, const std::string& localKey) -> std::string;

// True when `key` can go into a char[40] field: 1 to 39 characters, printable ASCII without spaces, quotes or backslashes.
auto KeyFits(const std::string& key) -> bool;

// The plugin's string file: a JSON array with a name record and an entry record for every map, in every language.
// `names` are display names (see DisplayName); more than MaxMaps are ignored.
auto StringsJson(const std::vector<std::string>& names) -> std::string;

// A dedicated map Boss's own name (issue #174). tools\install.ps1 writes the superuniques.txt Name cell as BossRowKey(name) and the
// plugin registers a text for BossLocalKey(name) in its string namespace, so the game shows the bosses.txt Name. Both sides build the
// key from the entry's Name as written (trimmed), so the key is stable and a removed entry takes its key with it. The key is ASCII only:
// "boss_" then each byte of the Name, kept when it is A-Z a-z 0-9 and otherwise written as '_' and two uppercase hex digits
// ("Shoreline Warden" -> "boss_Shoreline_20Warden"). install.ps1 (Get-BossLocalKey, built on Get-BossEscaped) does the same.
auto BossLocalKey(const std::string& name) -> std::string;
auto BossRowKey(const std::string& pluginId, const std::string& name) -> std::string;

// The plugin's boss string file: one record per name (a repeated name once), the display text in every language (see DisplayName).
auto BossStringsJson(const std::vector<std::string>& names) -> std::string;

// Writes `key` into a 40-byte field (the rest zero). False, and the field untouched, when the key does not fit.
auto WriteKey(uint8_t* field, const std::string& key) -> bool;

// True when a 40-byte field looks like a text key as the game stores it: a NUL inside, and printable ASCII before it.
// An empty key is accepted (a level may have none). The check before the plugin overwrites anything.
auto LooksLikeKey(const uint8_t* field) -> bool;

// True when the field starts with "<pluginId>:", that is, it still holds a key this plugin wrote.
auto IsOwnKey(const uint8_t* field, const std::string& pluginId) -> bool;

// The original LevelName and LevelEntry bytes of the levels the plugin changed, so they can be put back.
struct SavedRow {
	int level = 0;
	uint8_t name[FieldSize] = {};
	uint8_t entry[FieldSize] = {};
	// The keys this plugin last wrote into the row (set with SetKeys); the restore touches a field only while it holds them.
	uint8_t keyName[FieldSize] = {};
	uint8_t keyEntry[FieldSize] = {};
};

class RowSaver {
public:
	// Remembers the fields of `level`. False, and nothing stored, when that level is already remembered: the first copy is
	// the original, and a later one would be the plugin's own key.
	auto Remember(int level, const uint8_t* name, const uint8_t* entry) -> bool;
	auto Has(int level) const -> bool;
	auto Count() const -> size_t { return rows_.size(); }
	// Records the keys now written into `level`'s row. A failed open that puts the previous map's keys back records those
	// again, so the later restore still matches the row. False when the level is not remembered.
	auto SetKeys(int level, const uint8_t* name, const uint8_t* entry) -> bool;
	// A restore failed: a row may still hold one of the plugin's keys. Stays set for the rest of the process (TakeAll does not clear it).
	void MarkStuck() { stuck_ = true; }
	auto Stuck() const -> bool { return stuck_; }
	// True while the registration must stay as it is: a level is remembered, or a restore failed.
	auto HoldsKeys() const -> bool { return !rows_.empty() || stuck_; }
	// Hands back everything remembered and forgets it.
	auto TakeAll() -> std::vector<SavedRow>;
	// Hands back the one level's copy and forgets only that. False when the level is not remembered.
	auto Take(int level, SavedRow* out) -> bool;

private:
	std::vector<SavedRow> rows_;
	bool stuck_ = false;
};

// What the plugin's string registration should be (from the tables) against what the loader holds. A change is applied only
// while no levels row holds one of the plugin's keys, because a key whose text is gone reads "Missing String" and a changed
// list would put another map's name on the open portal; until then it stays pending.
class RegistrationPlan {
public:
	enum class Step { Nothing, Deferred, Apply };

	// What the tables now ask for. With wanted false, or no names, the ask is "nothing registered".
	void Want(bool wanted, std::vector<std::string> names);
	// Nothing when the loader already holds what is wanted, Deferred when it differs but `rowsHeld`, else Apply.
	auto Next(bool rowsHeld) const -> Step;
	// The loader's registration was removed (or is being replaced): it holds nothing now.
	void Dropped();
	// A registration of `names` was made: the loader holds that list now (not whatever is wanted at this moment, which a
	// reload may have changed while the SDK call was under way; then the next Next() says Apply).
	void Applied(std::vector<std::string> names);

	auto Registered() const -> bool { return heldRegistered_; }
	auto Held() const -> const std::vector<std::string>& { return held_; }
	auto WantsRegistration() const -> bool { return wantRegistered_; }
	auto Wanted() const -> const std::vector<std::string>& { return want_; }

private:
	bool wantRegistered_ = false;
	std::vector<std::string> want_;
	bool heldRegistered_ = false;
	std::vector<std::string> held_;
};

}  // namespace cartographer::mapname
