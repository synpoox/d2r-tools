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
	bad[1] = 0x02;
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
	for (const DecodeError e : {DecodeError::None, DecodeError::BadSize, DecodeError::BadVersion, DecodeError::BadFlags, DecodeError::BadSlot, DecodeError::BadOpening, DecodeError::BadSeed}) {
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
	CHECK(book.add(1000) == PeerBook::Add::Full);
	CHECK(book.add(1) == PeerBook::Add::Known);  // a known peer is still known when the book is full
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
