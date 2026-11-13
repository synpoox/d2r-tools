// Host tests for the levels-row and region-record pure parts: the value mapping (clamps, percent scaling, group building), the sanity
// and match checks, the open / undo / revert / restore choreography on plain buffers, and the slot guard. No game, no SDK.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <array>
#include <cstring>
#include <vector>

#include "levelrow.h"

using namespace cartographer::levelrow;

namespace {

using Row = std::array<uint8_t, RowSpan>;
using Record = std::array<uint8_t, RecSpan>;

auto FindField(const std::vector<Field>& fields, int offset) -> const Field* {
	for (const Field& f : fields) {
		if (f.offset == offset) {
			return &f;
		}
	}
	return nullptr;
}

void Put(Row& row, int offset, int size, uint32_t value) {
	Store(row.data(), Field{static_cast<uint16_t>(offset), static_cast<uint8_t>(size), value});
}

void PutRec(Record& rec, int offset, int size, uint32_t value) {
	Store(rec.data(), Field{static_cast<uint16_t>(offset), static_cast<uint8_t>(size), value});
}

// A row like the template's: levels 27 to 87, density 100 and 200, packs 1 to 3.
auto SampleRow() -> Row {
	Row row{};
	Put(row, RowIdOffset, 2, 138);
	Put(row, RowByte0dOffset, 1, 7);
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
	return row;
}

// The record the game builds from SampleRow for difficulty 2 (Hell): the Hell values, two groups.
auto SampleRecord() -> Record {
	Record rec{};
	PutRec(rec, RecByte0Offset, 1, 7);
	PutRec(rec, RecDensityOffset, 4, 0);
	PutRec(rec, RecUMinOffset, 1, 2);
	PutRec(rec, RecUMaxOffset, 1, 15);
	PutRec(rec, RecGroupCountOffset, 1, 2);
	PutRec(rec, RecGroupWeightOffset, 1, 9);
	PutRec(rec, RecGroupCount2Offset, 1, 2);
	PutRec(rec, RecGroupsOffset, 2, 40);
	PutRec(rec, RecGroupsOffset + RecGroupWeightDelta, 1, 4);
	PutRec(rec, RecGroupsOffset + RecGroupMembersDelta, 1, 3);
	PutRec(rec, RecGroupsOffset + RecGroupStride, 2, 41);
	PutRec(rec, RecGroupsOffset + RecGroupStride + RecGroupWeightDelta, 1, 5);
	return rec;
}

Parts RecordOnly(bool monsters, bool density, bool uniques) {
	Parts p;
	p.level = false;
	p.monsters = monsters;
	p.density = density;
	p.uniques = uniques;
	return p;
}

// A buffer that writes go to, and that can be told to fail.
struct Memory {
	std::vector<uint8_t> bytes;
	int failAfter = -1;  // writes allowed before every write fails; -1: never
	int writes = 0;
	explicit Memory(const uint8_t* data, size_t size) : bytes(data, data + size) {}
	auto Writer() -> WriteFn {
		return [this](const Field& f) {
			if (failAfter >= 0 && writes >= failAfter) {
				return false;
			}
			++writes;
			Store(bytes.data(), f);
			return true;
		};
	}
	auto Get(int offset, int size) const -> uint32_t { return Load(bytes.data(), offset, size); }
};

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

TEST_CASE("the slot guard refuses vanilla levels and any level that is not the map's slot") {
	CHECK(SlotAllowed(138, 138));
	CHECK(SlotAllowed(139, 139));
	CHECK(!SlotAllowed(137, 137));  // Colossal Summit, the last vanilla level
	CHECK(!SlotAllowed(1, 1));
	CHECK(!SlotAllowed(0, 0));
	CHECK(!SlotAllowed(138, 139));  // not the slot the map opens through
	CHECK(!SlotAllowed(138, -1));   // no slot installed
	CHECK(!SlotAllowed(-5, -5));
}

TEST_CASE("snapshots read the documented offsets") {
	const Row row = SampleRow();
	CHECK(RowId(row.data()) == 138);
	const Snapshot s = SnapshotOf(row.data());
	CHECK(s.byte0d == 7);
	CHECK(s.monLvl[0] == 27);
	CHECK(s.monLvl[5] == 87);
	CHECK(s.monDen[1] == 200);
	CHECK(s.uMin[0] == 1);
	CHECK(s.uMax[2] == 15);
	const Record rec = SampleRecord();
	const RecordSnapshot r = RecordSnapshotOf(rec.data());
	CHECK(r.firstByte == 7);
	CHECK(r.uMin == 2);
	CHECK(r.uMax == 15);
	CHECK(r.groupCount == 2);
	CHECK(r.groupWeight == 9);
	CHECK(r.groupCount2 == 2);
}

TEST_CASE("level fills all six monster level fields in the row, skipping those that already hold it") {
	const Row row = SampleRow();
	const Snapshot s = SnapshotOf(row.data());
	Request r;
	r.level = 90;
	const std::vector<Field> plan = PlanRow(r, s, true);
	CHECK(plan.size() == 6);
	for (const int offset : MonLvlOffsets) {
		const Field* f = FindField(plan, offset);
		REQUIRE(f != nullptr);
		CHECK(f->size == 2);
		CHECK(f->value == 90);
	}
	r.level = 87;
	CHECK(PlanRow(r, s, true).size() == 5);
	r.level = 1000;  // clamped
	CHECK(FindField(PlanRow(r, s, true), MonLvlOffsets[0])->value == 255);
	CHECK(PlanRow(r, s, false).empty());
}

TEST_CASE("the row plan touches nothing but the monster level fields") {
	const Row row = SampleRow();
	Request r;
	r.level = 90;
	r.density = 50;
	r.uniques = 50;
	r.monsterRows = {5};
	r.monsterWeights = {3};
	for (const Field& f : PlanRow(r, SnapshotOf(row.data()), true)) {
		bool isLevel = false;
		for (const int offset : MonLvlOffsets) {
			isLevel = isLevel || f.offset == offset;
		}
		CHECK(isLevel);
	}
}

TEST_CASE("monsters fill the group count, weight sum and entries of the record, and only for a valid list") {
	const Record rec = SampleRecord();
	const RecordSnapshot s = RecordSnapshotOf(rec.data());
	Request r;
	r.monsterRows = {5, 6};
	r.monsterWeights = {3, 4};
	const std::vector<Field> plan = PlanRecord(r, s, RecordOnly(true, false, false));
	CHECK(plan.size() == 3 + 3 * 2 + (RecMaxGroups - 2));
	CHECK(FindField(plan, RecGroupCountOffset)->value == 2);
	CHECK(FindField(plan, RecGroupCountOffset)->size == 1);
	CHECK(FindField(plan, RecGroupWeightOffset)->value == 7);
	CHECK(FindField(plan, RecGroupCount2Offset)->value == 2);
	CHECK(FindField(plan, RecGroupsOffset)->value == 5);
	CHECK(FindField(plan, RecGroupsOffset)->size == 2);
	CHECK(FindField(plan, RecGroupsOffset + RecGroupWeightDelta)->value == 3);
	CHECK(FindField(plan, RecGroupsOffset + RecGroupMembersDelta)->value == 0);  // the old group's members are not kept
	CHECK(FindField(plan, RecGroupsOffset + RecGroupStride)->value == 6);
	CHECK(FindField(plan, RecGroupsOffset + RecGroupStride + RecGroupWeightDelta)->value == 4);
	// only the members byte of the slots past the list is written (zero), so the game's appended group starts clean (#263)
	CHECK(FindField(plan, RecGroupsOffset + 2 * RecGroupStride) == nullptr);
	for (int i = 2; i < RecMaxGroups; ++i) {
		const Field* members = FindField(plan, RecGroupsOffset + i * RecGroupStride + RecGroupMembersDelta);
		CHECK(members != nullptr);
		CHECK(members->value == 0);
		CHECK(members->size == 1);
	}
	r.monsterRows = {};
	r.monsterWeights = {};
	CHECK(PlanRecord(r, s, RecordOnly(true, false, false)).empty());
	r.monsterRows = std::vector<int>(MaxMonsters + 1, 3);
	r.monsterWeights = std::vector<int>(MaxMonsters + 1, 1);
	CHECK(PlanRecord(r, s, RecordOnly(true, false, false)).empty());
	r.monsterRows = std::vector<int>(MaxMonsters, 3);
	r.monsterWeights = std::vector<int>(MaxMonsters, 1);
	CHECK(PlanRecord(r, s, RecordOnly(true, false, false)).size() == 3 + 3 * MaxMonsters + (RecMaxGroups - MaxMonsters));
	r.monsterRows = {5, -1};
	r.monsterWeights = {1, 1};
	CHECK(PlanRecord(r, s, RecordOnly(true, false, false)).empty());
	r.monsterRows = {MaxRow + 1};
	r.monsterWeights = {1};
	CHECK(PlanRecord(r, s, RecordOnly(true, false, false)).empty());
	r.monsterRows = {5, 6};
	r.monsterWeights = {1};  // a weight is missing
	CHECK(PlanRecord(r, s, RecordOnly(true, false, false)).empty());
	r.monsterWeights = {1, 256};
	CHECK(PlanRecord(r, s, RecordOnly(true, false, false)).empty());
	r.monsterRows = {MaxRow};
	r.monsterWeights = {1};
	CHECK(FindField(PlanRecord(r, s, RecordOnly(true, false, false)), RecGroupsOffset)->value == static_cast<uint32_t>(MaxRow));
}

TEST_CASE("the group weight sum is a byte: 255 is written, more is refused with a reason") {
	const Record rec = SampleRecord();
	const RecordSnapshot snap = RecordSnapshotOf(rec.data());
	Request r;
	r.monsterRows = {3, 4, 5};
	r.monsterWeights = {100, 100, 55};  // 255
	CHECK(MonstersRefusal(r).empty());
	std::vector<Field> plan = PlanRecord(r, snap, RecordOnly(true, false, false));
	CHECK(FindField(plan, RecGroupWeightOffset)->value == 255);
	CHECK(FindField(plan, RecGroupWeightOffset)->size == 1);
	r.monsterWeights = {100, 100, 56};  // 256 would wrap to 0
	CHECK(!MonstersRefusal(r).empty());
	CHECK(PlanRecord(r, snap, RecordOnly(true, false, false)).empty());
	r.monsterRows = std::vector<int>(MaxMonsters, 3);
	r.monsterWeights = std::vector<int>(MaxMonsters, 100);  // 1000
	CHECK(!MonstersRefusal(r).empty());
	CHECK(PlanRecord(r, snap, RecordOnly(true, false, false)).empty());
}

TEST_CASE("a Monsters list whose weights are all 0 is refused, one nonzero weight is enough") {
	const Record rec = SampleRecord();
	const RecordSnapshot snap = RecordSnapshotOf(rec.data());
	Request r;
	r.monsterRows = {3, 4};
	r.monsterWeights = {0, 0};
	CHECK(!MonstersRefusal(r).empty());
	CHECK(PlanRecord(r, snap, RecordOnly(true, false, false)).empty());
	r.monsterWeights = {0, 1};
	CHECK(MonstersRefusal(r).empty());
	CHECK(FindField(PlanRecord(r, snap, RecordOnly(true, false, false)), RecGroupWeightOffset)->value == 1);
	r.monsterRows = {};
	r.monsterWeights = {};
	CHECK(MonstersRefusal(r).empty());  // no list is not a refusal
}

TEST_CASE("density scales the record's MonDen and leaves a zero alone") {
	Record rec = SampleRecord();
	PutRec(rec, RecDensityOffset, 4, 100);
	Request r;
	r.density = 50;
	const std::vector<Field> plan = PlanRecord(r, RecordSnapshotOf(rec.data()), RecordOnly(false, true, false));
	REQUIRE(plan.size() == 1);
	CHECK(plan[0].offset == RecDensityOffset);
	CHECK(plan[0].size == 4);
	CHECK(plan[0].value == 150);
	r.density = 0;
	CHECK(PlanRecord(r, RecordSnapshotOf(rec.data()), RecordOnly(false, true, false)).empty());
	PutRec(rec, RecDensityOffset, 4, 0);
	r.density = 50;
	CHECK(PlanRecord(r, RecordSnapshotOf(rec.data()), RecordOnly(false, true, false)).empty());  // a base of 0 stays 0
}

TEST_CASE("uniques alone scale MonUMax and leave MonUMin, champions are not a request field") {
	const Record rec = SampleRecord();  // MonUMin 2, MonUMax 15
	Request r;
	r.uniques = 100;  // +100%
	const std::vector<Field> plan = PlanRecord(r, RecordSnapshotOf(rec.data()), RecordOnly(false, false, true));
	REQUIRE(plan.size() == 1);
	CHECK(FindField(plan, RecUMinOffset) == nullptr);  // a raised minimum is spent at the first rooms (issue #317)
	CHECK(FindField(plan, RecUMaxOffset)->value == 30);
	CHECK(FindField(plan, RecUMaxOffset)->size == 1);
	r.uniques = 50;
	const std::vector<Field> half = PlanRecord(r, RecordSnapshotOf(rec.data()), RecordOnly(false, false, true));
	CHECK(FindField(half, RecUMinOffset) == nullptr);
	CHECK(FindField(half, RecUMaxOffset)->value == 23);  // 15 + 7.5 -> 23
	r.uniques = 0;
	CHECK(PlanRecord(r, RecordSnapshotOf(rec.data()), RecordOnly(false, false, true)).empty());
}

TEST_CASE("pack counts stop at a byte") {
	Record rec{};
	PutRec(rec, RecUMaxOffset, 1, 90);
	Request r;
	r.uniques = 1000;
	CHECK(FindField(PlanRecord(r, RecordSnapshotOf(rec.data()), RecordOnly(false, false, true)), RecUMaxOffset)->value == 255);
}

TEST_CASE("record parts can be switched off one by one") {
	Record rec = SampleRecord();
	PutRec(rec, RecDensityOffset, 4, 100);
	Request r;
	r.density = 10;
	r.uniques = 10;
	r.monsterRows = {1};
	r.monsterWeights = {1};
	CHECK(!PlanRecord(r, RecordSnapshotOf(rec.data()), RecordOnly(true, true, true)).empty());
	CHECK(PlanRecord(r, RecordSnapshotOf(rec.data()), RecordOnly(false, false, false)).empty());
}

TEST_CASE("the row check switches Level off when a monster level field looks wrong") {
	Row row = SampleRow();
	CHECK(CheckRow(SnapshotOf(row.data())).level);
	Store(row.data(), Field{static_cast<uint16_t>(MonLvlOffsets[2]), 2, 0x7000});
	const RowVerdict v = CheckRow(SnapshotOf(row.data()));
	CHECK(!v.level);
	CHECK(v.why.size() == 1);
}

TEST_CASE("the record must match the row for the game's difficulty") {
	const Row row = SampleRow();
	const Snapshot rs = SnapshotOf(row.data());
	Record rec = SampleRecord();
	RecordVerdict v = CheckRecord(RecordSnapshotOf(rec.data()), rs, 2);
	CHECK(v.why.empty());
	CHECK(v.on.density);
	CHECK(v.on.uniques);
	CHECK(v.on.monsters);
	// the same record against Normal (difficulty 0): MonUMin 1, MonUMax 3 in the row, so it is not this row's Normal record
	v = CheckRecord(RecordSnapshotOf(rec.data()), rs, 0);
	CHECK(!v.on.density);
	CHECK(!v.on.uniques);
	CHECK(!v.on.monsters);
	CHECK(v.why.size() == 1);
	// a wrong first byte, density, difficulty
	Record other = SampleRecord();
	PutRec(other, RecByte0Offset, 1, 8);
	CHECK(!CheckRecord(RecordSnapshotOf(other.data()), rs, 2).on.monsters);
	other = SampleRecord();
	PutRec(other, RecDensityOffset, 4, 5);
	CHECK(!CheckRecord(RecordSnapshotOf(other.data()), rs, 2).on.density);
	CHECK(!CheckRecord(RecordSnapshotOf(rec.data()), rs, 3).on.monsters);
	CHECK(!CheckRecord(RecordSnapshotOf(rec.data()), rs, -1).on.monsters);
}

TEST_CASE("the record's own sanity switches only the part whose fields look wrong") {
	Row row = SampleRow();
	Put(row, MonUMaxOffsets[2], 1, 200);
	Put(row, MonDenOffsets[2], 4, 0x7fffffff);
	const Snapshot rs = SnapshotOf(row.data());
	Record rec = SampleRecord();
	PutRec(rec, RecUMaxOffset, 1, 200);
	PutRec(rec, RecDensityOffset, 4, 0x7fffffff);
	RecordVerdict v = CheckRecord(RecordSnapshotOf(rec.data()), rs, 2);
	CHECK(!v.on.uniques);
	CHECK(!v.on.density);
	CHECK(v.on.monsters);
	// group counts that disagree or are too many switch Monsters off
	row = SampleRow();
	rec = SampleRecord();
	PutRec(rec, RecGroupCount2Offset, 1, 3);
	v = CheckRecord(RecordSnapshotOf(rec.data()), SnapshotOf(row.data()), 2);
	CHECK(!v.on.monsters);
	CHECK(v.on.density);
	rec = SampleRecord();
	PutRec(rec, RecGroupCountOffset, 1, 14);
	PutRec(rec, RecGroupCount2Offset, 1, 14);
	CHECK(!CheckRecord(RecordSnapshotOf(rec.data()), SnapshotOf(row.data()), 2).on.monsters);
}

TEST_CASE("store and load round trip little-endian") {
	std::array<uint8_t, RowSpan> row{};
	Store(row.data(), Field{0x24, 4, 0x01020304});
	CHECK(row[0x24] == 0x04);
	CHECK(row[0x27] == 0x01);
	CHECK(Load(row.data(), 0x24, 4) == 0x01020304);
	Store(row.data(), Field{0x30, 1, 0x1ff});  // only the low byte
	CHECK(row[0x30] == 0xff);
	CHECK(row[0x31] == 0);
}

TEST_CASE("ledger keeps the first original and the last written value") {
	Ledger ledger;
	CHECK(!ledger.Has(138));
	CHECK(ledger.Record(138, Field{0x18, 2, 90}, 27));
	CHECK(!ledger.Record(138, Field{0x18, 2, 95}, 90));  // the buffer held our own 90: not the original
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
	ledger.ForgetLevel(139);
	CHECK(!ledger.Has(139));
	CHECK(ledger.Count() == 1);
	ledger.Clear();
	CHECK(!ledger.Has(138));
}

TEST_CASE("AllHold tells a buffer that still holds the written values from a fresh one") {
	Row row = SampleRow();
	Ledger ledger;
	Put(row, MonLvlOffsets[0], 2, 90);
	ledger.Record(138, Field{static_cast<uint16_t>(MonLvlOffsets[0]), 2, 90}, 27);
	CHECK(ledger.AllHold(138, row.data()));
	Put(row, MonLvlOffsets[0], 2, 27);
	CHECK(!ledger.AllHold(138, row.data()));
	CHECK(ledger.AllHold(139, row.data()));  // nothing recorded
}

TEST_CASE("the original image takes the plugin's writes out") {
	Row row = SampleRow();
	Ledger ledger;
	ledger.Record(138, Field{static_cast<uint16_t>(MonLvlOffsets[0]), 2, 90}, 27);
	Put(row, MonLvlOffsets[0], 2, 90);
	uint8_t image[RowSpan];
	OriginalImage(ledger, 138, row.data(), sizeof(image), image);
	CHECK(Load(image, MonLvlOffsets[0], 2) == 27);
	CHECK(Load(image, MonLvlOffsets[1], 2) == 30);
	OriginalImage(ledger, 139, row.data(), sizeof(image), image);  // another level: nothing taken out
	CHECK(Load(image, MonLvlOffsets[0], 2) == 90);
}

TEST_CASE("prepare drops a new field that already holds its value, and keeps one the ledger knows") {
	const Row row = SampleRow();
	Ledger ledger;
	const std::vector<Field> plan = {Field{static_cast<uint16_t>(MonLvlOffsets[0]), 2, 27}, Field{static_cast<uint16_t>(MonLvlOffsets[1]), 2, 99}};
	Prepared p = Prepare(ledger, 138, row.data(), plan);
	REQUIRE(p.ops.size() == 1);
	CHECK(p.ops[0].field.offset == MonLvlOffsets[1]);
	CHECK(p.ops[0].previous == 30);
	CHECK(p.ops[0].original == 30);
	CHECK(!p.ops[0].inLedger);
	CHECK(p.reverts.empty());
	ledger.Record(138, Field{static_cast<uint16_t>(MonLvlOffsets[0]), 2, 27}, 20);  // the ledger holds an original of 20
	p = Prepare(ledger, 138, row.data(), plan);
	REQUIRE(p.ops.size() == 2);
	CHECK(p.ops[0].inLedger);
	CHECK(p.ops[0].original == 20);
}

TEST_CASE("a second map keeps the first original and reverts a field it does not set") {
	Row row = SampleRow();
	Memory memory(row.data(), row.size());
	Ledger ledger;
	const auto a = static_cast<uint16_t>(MonLvlOffsets[0]), b = static_cast<uint16_t>(MonLvlOffsets[1]);
	// first map sets a and b
	std::vector<Op> done1;
	Prepared p1 = Prepare(ledger, 138, memory.bytes.data(), {Field{a, 2, 90}, Field{b, 2, 91}});
	CHECK(Apply(ledger, 138, p1, memory.Writer(), &done1));
	CHECK(memory.Get(a, 2) == 90);
	CHECK(memory.Get(b, 2) == 91);
	// second map sets only a: b goes back to its original, a keeps its first original
	std::vector<Op> done2;
	Prepared p2 = Prepare(ledger, 138, memory.bytes.data(), {Field{a, 2, 70}});
	REQUIRE(p2.ops.size() == 1);
	REQUIRE(p2.reverts.size() == 1);
	CHECK(p2.reverts[0].field.offset == b);
	CHECK(p2.reverts[0].field.value == 30);
	CHECK(Apply(ledger, 138, p2, memory.Writer(), &done2));
	CHECK(memory.Get(a, 2) == 70);
	CHECK(memory.Get(b, 2) == 30);
	uint32_t original = 0;
	REQUIRE(ledger.Original(138, a, &original));
	CHECK(original == 27);
	CHECK(!ledger.Original(138, b, &original));
	// restore puts a back
	const RestoreResult result = RestoreLevel(ledger, 138, memory.bytes.data(), memory.Writer());
	CHECK(result.back == 1);
	CHECK(result.moved == 0);
	CHECK(result.failed == 0);
	CHECK(memory.Get(a, 2) == 27);
	CHECK(!ledger.Has(138));
}

TEST_CASE("undoing a failed open puts the memory and the ledger back, reverts included") {
	Row row = SampleRow();
	Memory memory(row.data(), row.size());
	Ledger ledger;
	const auto a = static_cast<uint16_t>(MonLvlOffsets[0]), b = static_cast<uint16_t>(MonLvlOffsets[1]);
	std::vector<Op> done1;
	Prepared p1 = Prepare(ledger, 138, memory.bytes.data(), {Field{a, 2, 90}, Field{b, 2, 91}});
	REQUIRE(Apply(ledger, 138, p1, memory.Writer(), &done1));
	// the second open sets a and reverts b, then is undone
	std::vector<Op> done2;
	Prepared p2 = Prepare(ledger, 138, memory.bytes.data(), {Field{a, 2, 70}});
	REQUIRE(Apply(ledger, 138, p2, memory.Writer(), &done2));
	CHECK(Undo(ledger, 138, done2, memory.Writer()) == 0);
	CHECK(memory.Get(a, 2) == 90);
	CHECK(memory.Get(b, 2) == 91);
	const auto entries = ledger.EntriesOf(138);
	REQUIRE(entries.size() == 2);
	for (const Ledger::Entry& e : entries) {
		CHECK(e.original == (e.offset == a ? 27u : 30u));
		CHECK(e.written == (e.offset == a ? 90u : 91u));
	}
	// and the first open undone leaves nothing behind
	CHECK(Undo(ledger, 138, done1, memory.Writer()) == 0);
	CHECK(memory.Get(a, 2) == 27);
	CHECK(memory.Get(b, 2) == 30);
	CHECK(!ledger.Has(138));
}

TEST_CASE("a write that fails stops the open, and the undo restores what was written") {
	Row row = SampleRow();
	Memory memory(row.data(), row.size());
	Ledger ledger;
	const auto a = static_cast<uint16_t>(MonLvlOffsets[0]), b = static_cast<uint16_t>(MonLvlOffsets[1]);
	memory.failAfter = 1;  // the first write works, the second fails
	std::vector<Op> done;
	Prepared p = Prepare(ledger, 138, memory.bytes.data(), {Field{a, 2, 90}, Field{b, 2, 91}});
	CHECK(!Apply(ledger, 138, p, memory.Writer(), &done));
	REQUIRE(done.size() == 1);
	CHECK(memory.Get(a, 2) == 90);
	CHECK(memory.Get(b, 2) == 30);
	memory.failAfter = -1;
	CHECK(Undo(ledger, 138, done, memory.Writer()) == 0);
	CHECK(memory.Get(a, 2) == 27);
	CHECK(!ledger.Has(138));
}

TEST_CASE("an undo that cannot write counts the failures") {
	Row row = SampleRow();
	Memory memory(row.data(), row.size());
	Ledger ledger;
	std::vector<Op> done;
	Prepared p = Prepare(ledger, 138, memory.bytes.data(), {Field{static_cast<uint16_t>(MonLvlOffsets[0]), 2, 90}});
	REQUIRE(Apply(ledger, 138, p, memory.Writer(), &done));
	memory.failAfter = 0;
	CHECK(Undo(ledger, 138, done, memory.Writer()) == 1);
}

TEST_CASE("restore leaves a field someone else changed, and counts a failed write") {
	Row row = SampleRow();
	Memory memory(row.data(), row.size());
	Ledger ledger;
	const auto a = static_cast<uint16_t>(MonLvlOffsets[0]), b = static_cast<uint16_t>(MonLvlOffsets[1]), c = static_cast<uint16_t>(MonLvlOffsets[2]);
	std::vector<Op> done;
	Prepared p = Prepare(ledger, 138, memory.bytes.data(), {Field{a, 2, 90}, Field{b, 2, 91}, Field{c, 2, 92}});
	REQUIRE(Apply(ledger, 138, p, memory.Writer(), &done));
	Store(memory.bytes.data(), Field{b, 2, 55});  // someone else
	memory.failAfter = memory.writes;             // the writes from here fail
	RestoreResult result = RestoreLevel(ledger, 138, memory.bytes.data(), memory.Writer());
	CHECK(result.moved == 1);
	CHECK(result.failed == 2);
	CHECK(result.back == 0);
	CHECK(memory.Get(b, 2) == 55);
	CHECK(!ledger.Has(138));
}

TEST_CASE("monster groups round trip through the record") {
	Record rec = SampleRecord();
	Memory memory(rec.data(), rec.size());
	Ledger ledger;
	Request r;
	r.monsterRows = {5, 6, 7};
	r.monsterWeights = {3, 4, 5};
	const std::vector<Field> plan = PlanRecord(r, RecordSnapshotOf(memory.bytes.data()), RecordOnly(true, false, false));
	std::vector<Op> done;
	Prepared p = Prepare(ledger, 139, memory.bytes.data(), plan);
	REQUIRE(Apply(ledger, 139, p, memory.Writer(), &done));
	CHECK(memory.Get(RecGroupCountOffset, 1) == 3);
	CHECK(memory.Get(RecGroupCount2Offset, 1) == 3);
	CHECK(memory.Get(RecGroupWeightOffset, 1) == 12);
	CHECK(memory.Get(RecGroupsOffset + 2 * RecGroupStride, 2) == 7);
	CHECK(memory.Get(RecGroupsOffset + RecGroupMembersDelta, 1) == 0);
	// the check still accepts the record the game built, from its original image
	uint8_t image[RecSpan];
	OriginalImage(ledger, 139, memory.bytes.data(), sizeof(image), image);
	CHECK(std::memcmp(image, rec.data(), sizeof(image)) == 0);
	CHECK(Undo(ledger, 139, done, memory.Writer()) == 0);
	CHECK(std::memcmp(memory.bytes.data(), rec.data(), rec.size()) == 0);
}

TEST_CASE("Density and Uniques have no 1000 percent cap; only the game's field sizes limit them") {
	Record rec = SampleRecord();  // MonUMin 2, MonUMax 15
	PutRec(rec, RecDensityOffset, 4, 100);
	Request r;
	r.density = 5000;
	r.uniques = 5000;
	const RecordSnapshot s = RecordSnapshotOf(rec.data());
	CHECK(FindField(PlanRecord(r, s, RecordOnly(false, true, false)), RecDensityOffset)->value == 5100);  // 100 + 5000%
	const std::vector<Field> packs = PlanRecord(r, s, RecordOnly(false, false, true));
	CHECK(FindField(packs, RecUMinOffset) == nullptr);      // MonUMin is left alone (issue #317)
	CHECK(FindField(packs, RecUMaxOffset)->value == 255);   // 15 + 750 stops at a byte
	r.density = 2147483647;
	CHECK(FindField(PlanRecord(r, s, RecordOnly(false, true, false)), RecDensityOffset)->value == 0x7fffffffu);  // MonDen is a signed 32-bit value
	CHECK(ScalePercent(0x7fffffff, 2147483647, 0x7fffffff) == 0x7fffffff);
}

TEST_CASE("the unique total is MonUMax scaled by Uniques, and 0 when the part is off or the level has none") {
	RecordSnapshot rec;
	rec.uMax = 8;
	Request r;
	CHECK(UniqueTotal(r, rec, true) == 8);  // Uniques 0: the level's own count, still spread
	r.uniques = 100;
	CHECK(UniqueTotal(r, rec, true) == 16);
	r.uniques = 5000;
	CHECK(UniqueTotal(r, rec, true) == 255);
	CHECK(UniqueTotal(r, rec, false) == 0);
	rec.uMax = 0;
	CHECK(UniqueTotal(r, rec, true) == 0);
}

TEST_CASE("the pace gives the unique packs out with the rooms done, from the first room to the last") {
	CHECK(PaceBudget(0, 10, 100) == 0);
	CHECK(PaceBudget(16, 1, 0) == 1);  // the room count is not known before the first room is populated
	CHECK(PaceBudget(16, 0, 558) == 1);
	CHECK(PaceBudget(16, 1, 558) == 1);
	CHECK(PaceBudget(16, 35, 558) == 2);  // 16 * 35 / 558 = 1.003, rounded up
	CHECK(PaceBudget(16, 279, 558) == 8);
	CHECK(PaceBudget(16, 558, 558) == 16);
	CHECK(PaceBudget(16, 600, 558) == 16);
	int last = 0;
	for (int seen = 1; seen <= 558; ++seen) {
		const int b = PaceBudget(16, seen, 558);
		CHECK(b >= last);
		CHECK(b <= 16);
		last = b;
	}
	CHECK(PaceBudget(255, 400, 401) == 255);
}
