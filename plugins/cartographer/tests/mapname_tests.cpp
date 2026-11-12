// Host tests for the map-name pure parts: display text, keys, the plugin's string file, the 40-byte field writes and the
// restore bookkeeping. No game, no SDK.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <cstring>
#include <string>
#include <vector>

#include "mapname.h"

using namespace cartographer::mapname;

TEST_CASE("display name: trimmed, control characters blanked, empty becomes Map") {
	CHECK(DisplayName("Shorelines") == "Shorelines");
	CHECK(DisplayName("  Shore\tlines \n") == "Shore lines");
	CHECK(DisplayName("") == "Map");
	CHECK(DisplayName(" \t ") == "Map");
}

TEST_CASE("display name: long names are cut at a whole character") {
	const std::string longAscii(200, 'a');
	CHECK(DisplayName(longAscii).size() == MaxNameBytes);
	// 59 ASCII bytes then a two-byte character straddling the limit: the character is dropped whole.
	const std::string straddle = std::string(MaxNameBytes - 1, 'b') + "\xC3\xA9" + "tail";
	const std::string cut = DisplayName(straddle);
	CHECK(cut == std::string(MaxNameBytes - 1, 'b'));
	// A character that ends exactly at the limit stays.
	const std::string fits = std::string(MaxNameBytes - 2, 'c') + "\xC3\xA9" + "tail";
	CHECK(DisplayName(fits) == std::string(MaxNameBytes - 2, 'c') + "\xC3\xA9");
	// The cut never leaves trailing spaces.
	CHECK(DisplayName(std::string(MaxNameBytes - 1, 'd') + " zzz") == std::string(MaxNameBytes - 1, 'd'));
}

TEST_CASE("entry text and keys") {
	CHECK(EntryText("Shorelines") == "Entering Shorelines");
	CHECK(NameLocalKey(0) == "N0");
	CHECK(EntryLocalKey(12) == "E12");
	CHECK(RowKey("cartographer", NameLocalKey(7)) == "cartographer:N7");
	CHECK(KeyFits(RowKey("cartographer", EntryLocalKey(MaxMaps))));
}

TEST_CASE("key fit: 1 to 39 printable characters") {
	CHECK(KeyFits("a"));
	CHECK(KeyFits(std::string(39, 'k')));
	CHECK_FALSE(KeyFits(std::string(40, 'k')));
	CHECK_FALSE(KeyFits(""));
	CHECK_FALSE(KeyFits("has space"));
	CHECK_FALSE(KeyFits("quo\"te"));
	CHECK_FALSE(KeyFits("back\\slash"));
	CHECK_FALSE(KeyFits(std::string("nul\0x", 5)));
	CHECK_FALSE(KeyFits("\xC3\xA9"));
}

TEST_CASE("a plugin id too long for the field is refused") {
	CHECK_FALSE(KeyFits(RowKey(std::string(38, 'p'), NameLocalKey(0))));
}

TEST_CASE("strings file: both records for every map in every language") {
	const std::string json = StringsJson({"Shorelines", "Quote \"Q\" \\ back"});
	CHECK(json.front() == '[');
	CHECK(json.back() == ']');
	CHECK(json.find("\"Key\":\"N0\"") != std::string::npos);
	CHECK(json.find("\"Key\":\"E0\"") != std::string::npos);
	CHECK(json.find("\"Key\":\"N1\"") != std::string::npos);
	CHECK(json.find("\"Key\":\"E1\"") != std::string::npos);
	CHECK(json.find("\"enUS\":\"Entering Shorelines\"") != std::string::npos);
	CHECK(json.find("\"zhCN\":\"Shorelines\"") != std::string::npos);
	CHECK(json.find("Quote \\\"Q\\\" \\\\ back") != std::string::npos);
	size_t records = 0;
	for (size_t at = json.find("\"Key\""); at != std::string::npos; at = json.find("\"Key\"", at + 1)) {
		++records;
	}
	CHECK(records == 4);
	size_t enUS = 0;
	for (size_t at = json.find("\"enUS\""); at != std::string::npos; at = json.find("\"enUS\"", at + 1)) {
		++enUS;
	}
	CHECK(enUS == 4);
}

TEST_CASE("strings file: empty list and control characters") {
	CHECK(StringsJson({}) == "[]");
	const std::string json = StringsJson({std::string("a\x01" "b")});
	CHECK(json.find("a\\u0001b") != std::string::npos);
}

TEST_CASE("strings file: more maps than the key space are ignored") {
	const std::vector<std::string> many(MaxMaps + 5, "m");
	const std::string json = StringsJson(many);
	CHECK(json.find("\"Key\":\"N9998\"") != std::string::npos);
	CHECK(json.find("\"Key\":\"N9999\"") == std::string::npos);
}

TEST_CASE("write key: fills the field, zeroes the rest, refuses a long key") {
	uint8_t field[FieldSize];
	std::memset(field, 0x41, sizeof(field));
	CHECK(WriteKey(field, "cartographer:N3"));
	CHECK(std::string(reinterpret_cast<char*>(field)) == "cartographer:N3");
	for (size_t i = 15; i < FieldSize; ++i) {
		CHECK(field[i] == 0);
	}
	uint8_t before[FieldSize];
	std::memset(field, 0x42, sizeof(field));
	std::memcpy(before, field, sizeof(field));
	CHECK_FALSE(WriteKey(field, std::string(40, 'x')));
	CHECK(std::memcmp(field, before, sizeof(field)) == 0);
	CHECK_FALSE(WriteKey(nullptr, "k"));
	// A key of exactly 39 characters keeps its NUL.
	CHECK(WriteKey(field, std::string(39, 'z')));
	CHECK(field[39] == 0);
}

TEST_CASE("looks like a key") {
	uint8_t field[FieldSize] = {};
	CHECK(LooksLikeKey(field));  // empty
	std::memcpy(field, "ColossalSummit", 14);
	CHECK(LooksLikeKey(field));
	field[3] = 0x01;
	CHECK_FALSE(LooksLikeKey(field));
	std::memset(field, 'a', sizeof(field));
	CHECK_FALSE(LooksLikeKey(field));  // no NUL
	CHECK_FALSE(LooksLikeKey(nullptr));
}

TEST_CASE("row saver: the first copy is kept, restore hands everything back once") {
	RowSaver saver;
	uint8_t name[FieldSize] = {}, entry[FieldSize] = {};
	std::memcpy(name, "ColossalSummit", 14);
	std::memcpy(entry, "EnteringColossalSummit", 22);
	CHECK(saver.Remember(138, name, entry));
	CHECK(saver.Has(138));
	CHECK_FALSE(saver.Has(139));
	// A second open before the restore would see the plugin's own key: it must not replace the original.
	uint8_t ours[FieldSize] = {};
	std::memcpy(ours, "cartographer:N0", 15);
	CHECK_FALSE(saver.Remember(138, ours, ours));
	CHECK(saver.Remember(139, ours, ours));  // the Maze slot is its own entry
	CHECK(saver.Count() == 2);
	const std::vector<SavedRow> rows = saver.TakeAll();
	REQUIRE(rows.size() == 2);
	CHECK(rows[0].level == 138);
	CHECK(std::memcmp(rows[0].name, name, FieldSize) == 0);
	CHECK(std::memcmp(rows[0].entry, entry, FieldSize) == 0);
	CHECK(rows[1].level == 139);
	CHECK(saver.Count() == 0);
	CHECK(saver.TakeAll().empty());  // nothing to restore twice
	CHECK_FALSE(saver.Has(138));
	CHECK_FALSE(saver.Remember(140, nullptr, entry));
}

TEST_CASE("display name: invalid UTF-8 becomes U+FFFD, valid characters stay") {
	const std::string bad = std::string("ab\xFF") + "cd";
	CHECK(DisplayName(bad) == "ab\xEF\xBF\xBD" "cd");
	CHECK(DisplayName("x\xC3") == "x\xEF\xBF\xBD");                  // cut-off two-byte character
	CHECK(DisplayName("x\xC0\xAF") == "x\xEF\xBF\xBD\xEF\xBF\xBD");   // overlong
	CHECK(DisplayName("x\xED\xA0\x80") == "x\xEF\xBF\xBD\xEF\xBF\xBD\xEF\xBF\xBD");  // surrogate
	CHECK(DisplayName("caf\xC3\xA9 \xE2\x82\xAC \xF0\x9F\x98\x80") == "caf\xC3\xA9 \xE2\x82\xAC \xF0\x9F\x98\x80");
	// The result is the same bytes the JSON gets, and is still valid when cut.
	const std::string json = StringsJson({DisplayName(bad)});
	CHECK(json.find('\xFF') == std::string::npos);
}

TEST_CASE("own key: only a field that starts with the plugin namespace and a colon") {
	uint8_t field[FieldSize] = {};
	CHECK(WriteKey(field, "cartographer:N3"));
	CHECK(IsOwnKey(field, "cartographer"));
	CHECK_FALSE(IsOwnKey(field, "cartograph"));
	WriteKey(field, "ExpansionTown");
	CHECK_FALSE(IsOwnKey(field, "cartographer"));
	CHECK_FALSE(IsOwnKey(nullptr, "cartographer"));
}

TEST_CASE("row saver: one level can be taken without touching the others") {
	uint8_t a[FieldSize] = {}, b[FieldSize] = {};
	WriteKey(a, "A");
	WriteKey(b, "B");
	RowSaver saver;
	CHECK(saver.Remember(10, a, b));
	CHECK(saver.Remember(139, b, a));
	SavedRow row;
	CHECK(saver.Take(139, &row));
	CHECK(row.level == 139);
	CHECK(std::memcmp(row.name, b, FieldSize) == 0);
	CHECK_FALSE(saver.Take(139, &row));
	CHECK(saver.Has(10));
	CHECK(saver.Count() == 1);
	// A second Remember for a level taken out starts from the new fields.
	CHECK(saver.Remember(139, a, a));
}

TEST_CASE("registration plan: a change waits while a row holds a key, then applies") {
	using Step = RegistrationPlan::Step;
	RegistrationPlan plan;
	CHECK(plan.Next(false) == Step::Nothing);  // nothing wanted, nothing held

	plan.Want(true, {"One", "Two"});
	CHECK(plan.Next(false) == Step::Apply);
	plan.Dropped();
	plan.Applied();
	CHECK(plan.Registered());
	CHECK(plan.Held() == std::vector<std::string>({"One", "Two"}));
	CHECK(plan.Next(true) == Step::Nothing);

	// Reload while a map is open: the names change, the old registration stays until the rows are back.
	plan.Want(true, {"Zero", "One", "Two"});
	CHECK(plan.Next(true) == Step::Deferred);
	CHECK(plan.Held() == std::vector<std::string>({"One", "Two"}));
	CHECK(plan.Registered());
	CHECK(plan.Next(false) == Step::Apply);
	plan.Dropped();
	CHECK_FALSE(plan.Registered());
	plan.Applied();
	CHECK(plan.Held() == std::vector<std::string>({"Zero", "One", "Two"}));
	CHECK(plan.Next(false) == Step::Nothing);
}

TEST_CASE("registration plan: MapNames=0 mid-game is deferred, a later change replaces the pending one") {
	using Step = RegistrationPlan::Step;
	RegistrationPlan plan;
	plan.Want(true, {"One"});
	plan.Dropped();
	plan.Applied();

	plan.Want(false, {"One"});
	CHECK(plan.Next(true) == Step::Deferred);
	CHECK(plan.Registered());
	// Back to the registered list before the rows were restored: nothing left to do.
	plan.Want(true, {"One"});
	CHECK(plan.Next(true) == Step::Nothing);
	CHECK(plan.Next(false) == Step::Nothing);

	plan.Want(false, {"One"});
	CHECK(plan.Next(false) == Step::Apply);
	plan.Dropped();
	plan.Applied();  // wants nothing: holds nothing
	CHECK_FALSE(plan.Registered());
	CHECK(plan.Held().empty());
	CHECK(plan.Next(false) == Step::Nothing);

	// An empty map list is "nothing registered" even with MapNames=1.
	plan.Want(true, {});
	CHECK_FALSE(plan.WantsRegistration());
	CHECK(plan.Next(true) == Step::Nothing);
}

TEST_CASE("registration plan: a failed registration is tried again at the next chance") {
	using Step = RegistrationPlan::Step;
	RegistrationPlan plan;
	plan.Want(true, {"One"});
	CHECK(plan.Next(false) == Step::Apply);
	plan.Dropped();  // the loader refused: Applied() is not called
	CHECK_FALSE(plan.Registered());
	CHECK(plan.Next(false) == Step::Apply);
}
