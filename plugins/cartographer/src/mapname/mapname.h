// A map's own Name on the red portal tooltip, the automap title and the entry banner (issue #90). Pure C++, no engine
// headers. The game reads three text keys from the slot level's row in its levels table (LevelName at +0xfd and
// LevelEntry at +0x14d, each a char[40]); the plugin registers one text per map under its own string namespace and, when a
// map opens, writes the map's two keys into those fields, then puts the originals back. Notes:
// docs/reverse-engineering/cartographer.md section 10.
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

// The text shown for a map: control characters become spaces, the ends are trimmed, and at most MaxNameBytes bytes are kept
// without splitting a UTF-8 character. An empty result becomes "Map".
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

// Writes `key` into a 40-byte field (the rest zero). False, and the field untouched, when the key does not fit.
auto WriteKey(uint8_t* field, const std::string& key) -> bool;

// True when a 40-byte field looks like a text key as the game stores it: a NUL inside, and printable ASCII before it.
// An empty key is accepted (a level may have none). The check before the plugin overwrites anything.
auto LooksLikeKey(const uint8_t* field) -> bool;

// The original LevelName and LevelEntry bytes of the levels the plugin changed, so they can be put back.
struct SavedRow {
	int level = 0;
	uint8_t name[FieldSize] = {};
	uint8_t entry[FieldSize] = {};
};

class RowSaver {
public:
	// Remembers the fields of `level`. False, and nothing stored, when that level is already remembered: the first copy is
	// the original, and a later one would be the plugin's own key.
	auto Remember(int level, const uint8_t* name, const uint8_t* entry) -> bool;
	auto Has(int level) const -> bool;
	auto Count() const -> size_t { return rows_.size(); }
	// Hands back everything remembered and forgets it.
	auto TakeAll() -> std::vector<SavedRow>;

private:
	std::vector<SavedRow> rows_;
};

}  // namespace cartographer::mapname
