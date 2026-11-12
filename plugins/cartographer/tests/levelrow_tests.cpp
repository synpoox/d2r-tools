// Host tests for the levels-row pure parts: the value mapping (clamps, percent scaling, list building), the sanity check and the
// restore bookkeeping. No game, no SDK.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <array>
#include <cstring>
#include <vector>

#include "levelrow.h"

using namespace cartographer::levelrow;

namespace {

auto FindField(const std::vector<Field>& fields, int offset) -> const Field* {
	for (const Field& f : fields) {
		if (f.offset == offset) {
			return &f;
		}
	}
	return nullptr;
}

auto Put(std::array<uint8_t, RowSpan>& row, int offset, int size, uint32_t value) {
	Store(row.data(), Field{static_cast<uint16_t>(offset), static_cast<uint8_t>(size), value});
}

// A row like the template's: levels 27 to 87, density 100 and 200, packs 1 to 3, NumMon 3.
auto SampleRow() -> std::array<uint8_t, RowSpan> {
	std::array<uint8_t, RowSpan> row{};
	Put(row, RowIdOffset, 2, 138);
	const uint32_t levels[6] = {27, 30, 33, 40, 60, 87};
	for (int i = 0; i < 6; ++i) {
		Put(row, MonLvlOffsets[i], 2, levels[i]);
	}
	Put(row, MonDenOffsets[0], 4, 100);
	Put(row, MonDenOffsets[1], 4, 200);
	Put(row, MonUMinOffsets[0], 1, 1);
	Put(row, MonUMaxOffsets[0], 1, 3);
	Put(row, MonUMinOffsets[2], 1, 2);
	Put(row, MonUMaxOffsets[2], 1, 15);
	Put(row, NumMonOffset, 1, 3);
	return row;
}

}  // namespace

TEST_CASE("level is clamped to 1..255") {
	CHECK(ClampLevel(85) == 85);
	CHECK(ClampLevel(0) == 1);
	CHECK(ClampLevel(-5) == 1);
	CHECK(ClampLevel(999) == 255);
}

TEST_CASE("percent scaling rounds half up and clamps") {
	CHECK(ScalePercent(100, 50, 1000) == 150);
	CHECK(ScalePercent(3, 50, 255) == 5);   // 3 + 1.5 -> 4.5 -> 5 (half up)
	CHECK(ScalePercent(1, 25, 255) == 1);   // 1.25 rounds down
	CHECK(ScalePercent(200, 1000, 1500) == 1500);
	CHECK(ScalePercent(0, 500, 255) == 0);  // nothing to scale
	CHECK(ScalePercent(-1, 500, 255) == -1);
	CHECK(ScalePercent(100, 0, 1000) == 100);
	CHECK(ScalePercent(300, 10, 100) == 300);  // a base above the cap is never lowered
}

TEST_CASE("pack percent is the rounded-up average of Champions and Uniques") {
	CHECK(PackPercent(0, 0) == 0);
	CHECK(PackPercent(50, 50) == 50);
	CHECK(PackPercent(100, 0) == 50);
	CHECK(PackPercent(25, 0) == 13);
	CHECK(PackPercent(5000, 5000) == 1000);
	CHECK(PackPercent(-10, 0) == 0);
}

TEST_CASE("snapshot reads the documented offsets") {
	const auto row = SampleRow();
	CHECK(RowId(row.data()) == 138);
	const Snapshot s = SnapshotOf(row.data());
	CHECK(s.monLvl[0] == 27);
	CHECK(s.monLvl[5] == 87);
	CHECK(s.monDen[1] == 200);
	CHECK(s.uMin[0] == 1);
	CHECK(s.uMax[2] == 15);
	CHECK(s.numMon == 3);
}

TEST_CASE("level fills all six monster level fields, skipping those that already hold it") {
	const auto row = SampleRow();
	const Snapshot s = SnapshotOf(row.data());
	Request r;
	r.level = 90;
	const std::vector<Field> plan = Plan(r, s, Parts{});
	CHECK(plan.size() == 6);
	for (const int offset : MonLvlOffsets) {
		const Field* f = FindField(plan, offset);
		REQUIRE(f != nullptr);
		CHECK(f->size == 2);
		CHECK(f->value == 90);
	}
	r.level = 87;
	CHECK(Plan(r, s, Parts{}).size() == 5);
	r.level = 1000;  // clamped
	CHECK(FindField(Plan(r, s, Parts{}), MonLvlOffsets[0])->value == 255);
}

TEST_CASE("monsters fill NumMon and the three lists, and only for a valid list") {
	const auto row = SampleRow();
	const Snapshot s = SnapshotOf(row.data());
	Request r;
	r.level = 27;
	r.monsterRows = {5, 6};
	Parts only;
	only.level = only.density = only.packs = false;
	const std::vector<Field> plan = Plan(r, s, only);
	CHECK(plan.size() == 1 + 3 * 2);
	CHECK(FindField(plan, NumMonOffset)->value == 2);
	CHECK(FindField(plan, NumMonOffset)->size == 1);
	for (const int base : ListOffsets) {
		CHECK(FindField(plan, base)->value == 5);
		CHECK(FindField(plan, base + 2)->value == 6);
		CHECK(FindField(plan, base + 4) == nullptr);
	}
	r.monsterRows = {};
	CHECK(Plan(r, s, only).empty());
	r.monsterRows = std::vector<int>(MaxMonsters + 1, 3);
	CHECK(Plan(r, s, only).empty());
	r.monsterRows = std::vector<int>(MaxMonsters, 3);
	CHECK(Plan(r, s, only).size() == 1 + 3 * MaxMonsters);
	r.monsterRows = {5, -1};
	CHECK(Plan(r, s, only).empty());
	r.monsterRows = {MaxRow + 1};
	CHECK(Plan(r, s, only).empty());
	r.monsterRows = {MaxRow};
	CHECK(FindField(Plan(r, s, only), ListOffsets[0])->value == static_cast<uint32_t>(MaxRow));
}

TEST_CASE("density scales each MonDen and leaves a zero alone") {
	const auto row = SampleRow();
	const Snapshot s = SnapshotOf(row.data());
	Request r;
	r.level = 27;
	r.density = 50;
	Parts only;
	only.level = only.monsters = only.packs = false;
	const std::vector<Field> plan = Plan(r, s, only);
	REQUIRE(plan.size() == 2);  // the third MonDen is 0: nothing to scale
	CHECK(FindField(plan, MonDenOffsets[0])->value == 150);
	CHECK(FindField(plan, MonDenOffsets[1])->value == 300);
	CHECK(FindField(plan, MonDenOffsets[1])->size == 4);
	r.density = 0;
	CHECK(Plan(r, s, only).empty());
}

TEST_CASE("champions and uniques scale MonUMin and MonUMax by their average") {
	const auto row = SampleRow();
	const Snapshot s = SnapshotOf(row.data());
	Request r;
	r.level = 27;
	r.champions = 100;
	r.uniques = 0;  // +50%
	Parts only;
	only.level = only.monsters = only.density = false;
	const std::vector<Field> plan = Plan(r, s, only);
	CHECK(FindField(plan, MonUMinOffsets[0])->value == 2);   // 1 + 0.5 -> 2
	CHECK(FindField(plan, MonUMaxOffsets[0])->value == 5);   // 3 + 1.5 -> 5
	CHECK(FindField(plan, MonUMinOffsets[2])->value == 3);
	CHECK(FindField(plan, MonUMaxOffsets[2])->value == 23);  // 15 + 7.5 -> 23
	CHECK(FindField(plan, MonUMinOffsets[1]) == nullptr);    // 0 stays 0
	CHECK(FindField(plan, MonUMaxOffsets[0])->size == 1);
	r.champions = r.uniques = 1000;
	CHECK(FindField(Plan(r, s, only), MonUMaxOffsets[2])->value == 165);
	r.champions = r.uniques = 0;
	CHECK(Plan(r, s, only).empty());
}

TEST_CASE("pack counts stop at a byte") {
	std::array<uint8_t, RowSpan> row{};
	Store(row.data(), Field{static_cast<uint16_t>(MonUMaxOffsets[2]), 1, 90});
	const Snapshot s = SnapshotOf(row.data());
	Request r;
	r.champions = r.uniques = 1000;
	Parts only;
	only.level = only.monsters = only.density = false;
	CHECK(FindField(Plan(r, s, only), MonUMaxOffsets[2])->value == 255);
}

TEST_CASE("parts can be switched off one by one") {
	const auto row = SampleRow();
	const Snapshot s = SnapshotOf(row.data());
	Request r;
	r.level = 90;
	r.monsterRows = {1};
	r.density = 10;
	r.champions = r.uniques = 10;
	CHECK(!Plan(r, s, Parts{}).empty());
	Parts none;
	none.level = none.monsters = none.density = none.packs = false;
	CHECK(Plan(r, s, none).empty());
}

TEST_CASE("the sanity check switches off the part whose fields look wrong") {
	auto row = SampleRow();
	CHECK(Check(SnapshotOf(row.data())).why.empty());
	Store(row.data(), Field{static_cast<uint16_t>(MonLvlOffsets[2]), 2, 0x7000});
	Store(row.data(), Field{static_cast<uint16_t>(MonDenOffsets[0]), 4, 0x7fffffff});
	Store(row.data(), Field{static_cast<uint16_t>(MonUMaxOffsets[1]), 1, 200});
	Store(row.data(), Field{static_cast<uint16_t>(NumMonOffset), 1, 200});
	const Verdict v = Check(SnapshotOf(row.data()));
	CHECK(!v.on.level);
	CHECK(!v.on.density);
	CHECK(!v.on.packs);
	CHECK(!v.on.monsters);
	CHECK(v.why.size() == 4);
	auto good = SampleRow();
	Store(good.data(), Field{static_cast<uint16_t>(MonDenOffsets[0]), 4, 0xffffffffu});  // -1: wrong
	CHECK(!Check(SnapshotOf(good.data())).on.density);
}

TEST_CASE("store and load round trip little-endian") {
	std::array<uint8_t, RowSpan> row{};
	Store(row.data(), Field{0x24, 4, 0x01020304});
	CHECK(row[0x24] == 0x04);
	CHECK(row[0x27] == 0x01);
	CHECK(Load(row.data(), 0x24, 4) == 0x01020304);
	Store(row.data(), Field{0x3a, 1, 0x1ff});  // only the low byte
	CHECK(row[0x3a] == 0xff);
	CHECK(row[0x3b] == 0);
}

TEST_CASE("ledger keeps the first original and the last written value") {
	Ledger ledger;
	CHECK(!ledger.Has(138));
	CHECK(ledger.Record(138, Field{0x18, 2, 90}, 27));
	CHECK(!ledger.Record(138, Field{0x18, 2, 95}, 90));  // the row held our own 90: not the original
	CHECK(ledger.Record(138, Field{0x24, 4, 150}, 100));
	CHECK(ledger.Record(139, Field{0x18, 2, 70}, 33));
	uint32_t original = 0;
	REQUIRE(ledger.Original(138, 0x18, &original));
	CHECK(original == 27);
	const auto entries = ledger.EntriesOf(138);
	REQUIRE(entries.size() == 2);
	CHECK(entries[0].written == 95);
	CHECK(ledger.Levels() == std::vector<int>({138, 139}));
	CHECK(ledger.SetWritten(138, 0x18, 90));
	CHECK(ledger.EntriesOf(138)[0].written == 90);
	CHECK(!ledger.SetWritten(138, 0x99, 1));
	CHECK(ledger.Forget(138, 0x24));
	CHECK(!ledger.Forget(138, 0x24));
	CHECK(ledger.Count() == 2);
	ledger.Clear();
	CHECK(!ledger.Has(138));
}
