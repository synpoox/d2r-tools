// Host tests for the Cartographer map sync core: the message, the client's check, the peer list and the connect schedule.
// No game, no SDK.
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <cstring>

#include "mapsync.h"

using namespace cartographer::mapsync;

namespace {

MapIdentity sample() {
	MapIdentity id;
	id.mapIndex = 3;
	id.nameHash = nameHash("Colossal Summit");
	id.slotLevel = 140;
	id.opening = 2;
	id.seed = 0x0123456789abcdefULL;
	id.maze = true;
	return id;
}

}  // namespace

TEST_CASE("the name hash ignores case and tells names apart") {
	CHECK(nameHash("Colossal Summit") == nameHash("colossal SUMMIT"));
	CHECK(nameHash("Colossal Summit") != nameHash("Colossal Summit "));
	CHECK(nameHash("") == 2166136261u);
	CHECK(nameHash("a") == 0xe40c292cu);  // FNV-1a of "a"
}

TEST_CASE("a map identity survives encode and decode") {
	uint8_t buf[kMaxMessageSize];
	const MapIdentity id = sample();
	REQUIRE(encodeMapOpen(id, buf, sizeof(buf)) == kMapOpenSize);
	MapIdentity back;
	CHECK(decodeMapOpen(buf, kMapOpenSize, &back) == DecodeError::None);
	CHECK(back == id);

	MapIdentity preset = id;
	preset.maze = false;
	preset.seed = 0;
	REQUIRE(encodeMapOpen(preset, buf, sizeof(buf)) == kMapOpenSize);
	CHECK(decodeMapOpen(buf, kMapOpenSize, &back) == DecodeError::None);
	CHECK(back == preset);
}

TEST_CASE("the wire bytes are fixed (little endian)") {
	uint8_t buf[kMapOpenSize];
	MapIdentity id = sample();
	id.nameHash = 0x11223344u;
	REQUIRE(encodeMapOpen(id, buf, sizeof(buf)) == kMapOpenSize);
	const uint8_t want[kMapOpenSize] = {kWireVersion, kFlagMaze, 3, 0, 0x44, 0x33, 0x22, 0x11, 140, 0, 2, 0, 0xef, 0xcd, 0xab, 0x89, 0x67, 0x45, 0x23, 0x01};
	CHECK(std::memcmp(buf, want, sizeof(want)) == 0);
}

TEST_CASE("encode refuses what decode would refuse") {
	uint8_t buf[kMaxMessageSize];
	MapIdentity id = sample();
	CHECK(encodeMapOpen(id, buf, kMapOpenSize - 1) == 0);
	CHECK(encodeMapOpen(id, nullptr, sizeof(buf)) == 0);
	id.slotLevel = 0;
	CHECK(encodeMapOpen(id, buf, sizeof(buf)) == 0);
	id = sample();
	id.opening = 0;
	CHECK(encodeMapOpen(id, buf, sizeof(buf)) == 0);
	id = sample();
	id.maze = false;  // a preset map with a seed
	CHECK(encodeMapOpen(id, buf, sizeof(buf)) == 0);
}

TEST_CASE("decode refuses a bad message") {
	uint8_t buf[kMaxMessageSize];
	REQUIRE(encodeMapOpen(sample(), buf, sizeof(buf)) == kMapOpenSize);
	MapIdentity out;
	CHECK(decodeMapOpen(buf, kMapOpenSize - 1, &out) == DecodeError::BadSize);
	CHECK(decodeMapOpen(buf, kMapOpenSize + 1, &out) == DecodeError::BadSize);
	CHECK(decodeMapOpen(nullptr, kMapOpenSize, &out) == DecodeError::BadSize);
	CHECK(decodeMapOpen(buf, 0, &out) == DecodeError::BadSize);

	uint8_t bad[kMapOpenSize];
	std::memcpy(bad, buf, sizeof(bad));
	bad[0] = kWireVersion + 1;
	CHECK(decodeMapOpen(bad, sizeof(bad), &out) == DecodeError::BadVersion);
	std::memcpy(bad, buf, sizeof(bad));
	bad[0] = 1;  // a literal v1 message (before the outdoor flag) is refused by version before any field is read
	CHECK(decodeMapOpen(bad, sizeof(bad), &out) == DecodeError::BadVersion);
	std::memcpy(bad, buf, sizeof(bad));
	bad[1] = 0x04;
	CHECK(decodeMapOpen(bad, sizeof(bad), &out) == DecodeError::BadFlags);
	std::memcpy(bad, buf, sizeof(bad));
	bad[8] = bad[9] = 0;
	CHECK(decodeMapOpen(bad, sizeof(bad), &out) == DecodeError::BadSlot);
	std::memcpy(bad, buf, sizeof(bad));
	bad[10] = bad[11] = 0;
	CHECK(decodeMapOpen(bad, sizeof(bad), &out) == DecodeError::BadOpening);
	std::memcpy(bad, buf, sizeof(bad));
	bad[1] = 0;  // no maze flag, but a seed
	CHECK(decodeMapOpen(bad, sizeof(bad), &out) == DecodeError::BadSeed);
	for (const DecodeError e : {DecodeError::None, DecodeError::BadSize, DecodeError::BadVersion, DecodeError::BadFlags, DecodeError::BadSlot, DecodeError::BadOpening, DecodeError::BadSeed, DecodeError::BadKind}) {
		CHECK(std::strlen(describe(e)) > 0);
	}
}

TEST_CASE("every truncation and every single-byte change is handled") {
	uint8_t buf[kMapOpenSize];
	REQUIRE(encodeMapOpen(sample(), buf, sizeof(buf)) == kMapOpenSize);
	MapIdentity out;
	for (size_t n = 0; n <= kMapOpenSize + 2; ++n) {
		const DecodeError e = decodeMapOpen(buf, n, &out);
		CHECK((n == kMapOpenSize) == (e == DecodeError::None));
	}
	for (size_t i = 0; i < kMapOpenSize; ++i) {
		for (int v = 0; v < 256; v += 51) {
			uint8_t copy[kMapOpenSize];
			std::memcpy(copy, buf, sizeof(copy));
			copy[i] = static_cast<uint8_t>(v);
			(void)decodeMapOpen(copy, sizeof(copy), &out);
		}
	}
}

TEST_CASE("hello") {
	uint8_t buf[kHelloSize];
	CHECK(encodeHello(buf, 1) == 0);
	REQUIRE(encodeHello(buf, sizeof(buf)) == kHelloSize);
	CHECK(decodeHello(buf, sizeof(buf)) == DecodeError::None);
	CHECK(decodeHello(buf, 1) == DecodeError::BadSize);
	CHECK(decodeHello(nullptr, kHelloSize) == DecodeError::BadSize);
	buf[0] = 9;
	CHECK(decodeHello(buf, sizeof(buf)) == DecodeError::BadVersion);
	buf[0] = kWireVersion;
	buf[1] = 1;
	CHECK(decodeHello(buf, sizeof(buf)) == DecodeError::BadFlags);
}

TEST_CASE("the channel token carries the wire version") {
	CHECK((kChannelToken & 0xff) == kWireVersion);
	CHECK(kChannelToken != 0);
	CHECK(kChannelId != 0);
}

TEST_CASE("the client checks the identity against its own maps.txt") {
	const TableHashes table = {nameHash("Alpha"), nameHash("Beta"), nameHash("Gamma"), nameHash("Colossal Summit")};
	ClientMap map;
	const MapIdentity id = sample();
	CHECK(!map.current());
	CHECK(map.offer(id, {}) == Verdict::NoTables);
	CHECK(map.offer(id, table) == Verdict::Accepted);
	REQUIRE(map.current());
	CHECK(*map.current() == id);
	CHECK(map.offer(id, table) == Verdict::Unchanged);

	MapIdentity older = id;
	older.opening = 1;
	older.seed = 7;
	CHECK(map.offer(older, table) == Verdict::Stale);
	CHECK(*map.current() == id);  // the refusal changed nothing

	MapIdentity newer = id;
	newer.opening = 3;
	newer.seed = 8;
	CHECK(map.offer(newer, table) == Verdict::Accepted);
	CHECK(map.current()->seed == 8);

	MapIdentity same = newer;  // the same opening with another seed is not stale: it replaces
	same.seed = 9;
	CHECK(map.offer(same, table) == Verdict::Accepted);

	MapIdentity pastEnd = id;
	pastEnd.mapIndex = 4;
	CHECK(map.offer(pastEnd, table) == Verdict::NoSuchRow);
	MapIdentity wrongName = id;
	wrongName.mapIndex = 1;  // Beta, not Colossal Summit
	CHECK(map.offer(wrongName, table) == Verdict::NameMismatch);
	CHECK(map.current()->seed == 9);  // both refusals changed nothing

	map.clear();
	CHECK(!map.current());
	CHECK(map.offer(older, table) == Verdict::Accepted);  // nothing kept, so nothing is stale
	for (const Verdict v : {Verdict::Accepted, Verdict::Unchanged, Verdict::Stale, Verdict::NoTables, Verdict::NoSuchRow, Verdict::NameMismatch}) {
		CHECK(std::strlen(describe(v)) > 0);
	}
}

TEST_CASE("the peer list is bounded and de-duplicated") {
	PeerBook book;
	CHECK(book.add(10) == PeerBook::Add::Added);
	CHECK(book.add(10) == PeerBook::Add::Known);
	CHECK(book.has(10));
	CHECK(!book.has(11));
	CHECK(book.peers().size() == 1);
	CHECK(book.remove(10));
	CHECK(!book.remove(10));
	for (uint64_t p = 1; p <= kMaxPeers; ++p) {
		CHECK(book.add(p) == PeerBook::Add::Added);
	}
	CHECK(book.add(1) == PeerBook::Add::Known);  // a known peer is still known when the book is full
	CHECK(book.add(1000) == PeerBook::Add::AddedDroppedOldest);  // the oldest (1) makes room
	CHECK(book.peers().size() == kMaxPeers);
	CHECK(!book.has(1));
	CHECK(book.has(2));
	CHECK(book.peers().back() == 1000);
	book.clear();
	CHECK(book.peers().empty());
	CHECK(book.add(1000) == PeerBook::Add::Added);
}

TEST_CASE("the connect schedule waits, retries and gives up") {
	ConnectSchedule s;
	CHECK(step(s, 0) == Step::GiveUp);  // never begun
	begin(s, 1000);
	CHECK(step(s, 1000) == Step::Wait);
	CHECK(step(s, 1000 + kFirstDelayMs - 1) == Step::Wait);
	CHECK(step(s, 1000 + kFirstDelayMs) == Step::Try);
	CHECK(s.attempts == 1);
	CHECK(step(s, 1000 + kFirstDelayMs + kRetryMs - 1) == Step::Wait);
	uint64_t now = 1000 + kFirstDelayMs + kRetryMs;
	CHECK(step(s, now) == Step::Try);
	CHECK(s.attempts == 2);
	while (s.attempts < kMaxAttempts) {
		now += kRetryMs;
		CHECK(step(s, now) == Step::Try);
	}
	now += kRetryMs;
	CHECK(step(s, now) == Step::GiveUp);
	CHECK(s.done);
	CHECK(step(s, now + 100000) == Step::GiveUp);
}

TEST_CASE("a rejection ends the schedule unless the host is unavailable") {
	ConnectSchedule s;
	begin(s, 0);
	CHECK(step(s, kFirstDelayMs) == Step::Try);
	CHECK(onRejected(s, true));  // HostUnavailable: retry
	CHECK(step(s, kFirstDelayMs + kRetryMs) == Step::Try);
	CHECK(!onRejected(s, false));
	CHECK(step(s, kFirstDelayMs + 2 * kRetryMs) == Step::GiveUp);
}

TEST_CASE("a finished schedule makes no more attempts") {
	ConnectSchedule s;
	begin(s, 0);
	finish(s);
	CHECK(step(s, 1000000) == Step::GiveUp);
	CHECK(s.attempts == 0);
	begin(s, 0);  // a new game starts afresh
	CHECK(!s.done);
	CHECK(step(s, kFirstDelayMs) == Step::Try);
}

namespace {

ApplyFacts goodFacts() {
	ApplyFacts f;
	f.slotLevelMatches = true;
	f.clientMazeMap = true;
	f.namesSetting = true;
	f.mazeSteeringLive = true;
	f.tilesetFits = true;
	f.slotBuilt = false;
	return f;
}

}  // namespace

TEST_CASE("a good maze identity applies both halves") {
	const ApplyPlan p = decideApply(sample(), goodFacts());
	CHECK(p.names);
	CHECK(p.maze);
	CHECK(p.namesWhy == Skip::None);
	CHECK(p.mazeWhy == Skip::None);
}

TEST_CASE("a preset identity applies the name only") {
	MapIdentity id = sample();
	id.maze = false;
	id.seed = 0;
	ApplyFacts f = goodFacts();
	f.clientMazeMap = false;
	const ApplyPlan p = decideApply(id, f);
	CHECK(p.names);
	CHECK(!p.maze);
	CHECK(p.mazeWhy == Skip::NotMaze);
}

TEST_CASE("a slot level or map kind that differs skips both halves") {
	ApplyFacts f = goodFacts();
	f.slotLevelMatches = false;
	ApplyPlan p = decideApply(sample(), f);
	CHECK(!p.names);
	CHECK(!p.maze);
	CHECK(p.namesWhy == Skip::SlotMismatch);
	CHECK(p.mazeWhy == Skip::SlotMismatch);

	f = goodFacts();
	f.clientMazeMap = false;  // the host says maze, this client's row is a preset map
	p = decideApply(sample(), f);
	CHECK(!p.names);
	CHECK(!p.maze);
	CHECK(p.namesWhy == Skip::KindMismatch);

	MapIdentity preset = sample();
	preset.maze = false;
	preset.seed = 0;
	p = decideApply(preset, goodFacts());  // the host says preset, this client's row is a maze map
	CHECK(!p.names);
	CHECK(p.mazeWhy == Skip::KindMismatch);
}

TEST_CASE("the maze half has its own refusals and the name half does not depend on them") {
	ApplyFacts f = goodFacts();
	f.mazeSteeringLive = false;
	ApplyPlan p = decideApply(sample(), f);
	CHECK(p.names);
	CHECK(!p.maze);
	CHECK(p.mazeWhy == Skip::SteeringOff);

	f = goodFacts();
	f.tilesetFits = false;
	p = decideApply(sample(), f);
	CHECK(p.names);
	CHECK(p.mazeWhy == Skip::TilesetMismatch);

	f = goodFacts();
	f.slotBuilt = true;  // the identity arrived after the slot level was built: logged, not rebuilt, the name still applies
	p = decideApply(sample(), f);
	CHECK(p.names);
	CHECK(!p.maze);
	CHECK(p.mazeWhy == Skip::AlreadyBuilt);

	MapIdentity noSeed = sample();
	noSeed.seed = 0;
	p = decideApply(noSeed, goodFacts());
	CHECK(p.names);
	CHECK(p.mazeWhy == Skip::NoSeed);
}

TEST_CASE("MapNames off on the client skips the name only") {
	ApplyFacts f = goodFacts();
	f.namesSetting = false;
	const ApplyPlan p = decideApply(sample(), f);
	CHECK(!p.names);
	CHECK(p.namesWhy == Skip::SettingOff);
	CHECK(p.maze);
}

TEST_CASE("every skip reason has a sentence") {
	for (const Skip s : {Skip::None, Skip::SlotMismatch, Skip::KindMismatch, Skip::SettingOff, Skip::NotMaze, Skip::NoSeed, Skip::SteeringOff, Skip::TilesetMismatch, Skip::AlreadyBuilt}) {
		CHECK(std::strlen(describe(s)) > 3);
		CHECK(std::strcmp(describe(s), "?") != 0);
	}
}

TEST_CASE("an opening is applied once; a resend or an older one is not") {
	AppliedOpening a;
	CHECK(a.take(1));
	CHECK(!a.take(1));
	CHECK(a.take(2));
	CHECK(!a.take(1));
	a.clear();  // a new game
	CHECK(a.take(1));
}

namespace {

MapIdentity outdoorSample() {
	MapIdentity id = sample();
	id.maze = false;
	id.outdoor = true;
	return id;
}

ApplyFacts outdoorFacts() {
	ApplyFacts f;
	f.slotLevelMatches = true;
	f.clientOutdoorMap = true;
	f.namesSetting = true;
	f.outdoorBuildLive = true;
	return f;
}

}  // namespace

TEST_CASE("an outdoor identity carries its seed and round trips") {
	uint8_t buf[kMaxMessageSize];
	const MapIdentity id = outdoorSample();
	REQUIRE(encodeMapOpen(id, buf, sizeof(buf)) == kMapOpenSize);
	CHECK(buf[1] == kFlagOutdoor);
	MapIdentity back;
	REQUIRE(decodeMapOpen(buf, kMapOpenSize, &back) == DecodeError::None);
	CHECK(back == id);
	CHECK(back.outdoor);
	CHECK(!back.maze);
	CHECK(back.seed == 0x0123456789abcdefULL);
}

TEST_CASE("a maze and outdoor flag together is refused both ways") {
	uint8_t buf[kMaxMessageSize];
	MapIdentity both = sample();
	both.outdoor = true;
	CHECK(encodeMapOpen(both, buf, sizeof(buf)) == 0);
	REQUIRE(encodeMapOpen(sample(), buf, sizeof(buf)) == kMapOpenSize);
	buf[1] = kFlagMaze | kFlagOutdoor;
	MapIdentity out;
	CHECK(decodeMapOpen(buf, kMapOpenSize, &out) == DecodeError::BadKind);
	buf[1] = 4;
	CHECK(decodeMapOpen(buf, kMapOpenSize, &out) == DecodeError::BadFlags);
}

TEST_CASE("a good outdoor identity applies the outdoor seed and not the maze half") {
	const ApplyPlan p = decideApply(outdoorSample(), outdoorFacts());
	CHECK(p.outdoor);
	CHECK(p.outdoorWhy == Skip::None);
	CHECK(p.names);
	CHECK(!p.maze);
	CHECK(p.mazeWhy == Skip::NotMaze);
}

TEST_CASE("the outdoor half has its own refusals") {
	ApplyFacts f = outdoorFacts();
	f.outdoorBuildLive = false;
	ApplyPlan p = decideApply(outdoorSample(), f);
	CHECK(!p.outdoor);
	CHECK(p.outdoorWhy == Skip::OutdoorOff);
	CHECK(p.names);  // the name half does not depend on the layout
	f = outdoorFacts();
	f.outdoorSlotBuilt = true;
	p = decideApply(outdoorSample(), f);
	CHECK(!p.outdoor);
	CHECK(p.outdoorWhy == Skip::AlreadyBuilt);
	MapIdentity noSeed = outdoorSample();
	noSeed.seed = 0;
	p = decideApply(noSeed, outdoorFacts());
	CHECK(p.outdoorWhy == Skip::NoSeed);
	f = outdoorFacts();
	f.slotLevelMatches = false;
	p = decideApply(outdoorSample(), f);
	CHECK(p.outdoorWhy == Skip::SlotMismatch);
	CHECK(!p.names);
	f = outdoorFacts();
	f.clientOutdoorMap = false;  // the host says outdoor, this client's row is not
	p = decideApply(outdoorSample(), f);
	CHECK(p.outdoorWhy == Skip::KindMismatch);
	CHECK(!p.outdoor);
}

TEST_CASE("a maze identity never applies an outdoor seed") {
	const ApplyPlan p = decideApply(sample(), goodFacts());
	CHECK(p.maze);
	CHECK(!p.outdoor);
	CHECK(p.outdoorWhy == Skip::NotOutdoor);
}
