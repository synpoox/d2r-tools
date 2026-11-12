// Cartographer map sync (phase 7, part 1): the host tells each joined client which map is open. This file is the message
// (encode, decode, validation) and the small decisions around it that need no game: the client's check against its own
// maps.txt, the host's list of clients, and the client's connect retry schedule. Pure C++ on plain structs: no engine, no
// SDK. The engine part (src/engine/map_sync.cpp) sends and receives; notes in docs/reverse-engineering/cartographer.md section 12.
#pragma once
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace cartographer::mapsync {

// --- The wire format -----------------------------------------------------------------------------------------------

// Bump when a message changes; the channel token carries it, so a host and a client on different versions never connect.
constexpr uint8_t kWireVersion = 1;
constexpr uint64_t kChannelToken = 0x4341525400000000ULL | kWireVersion;  // "CART" and the wire version
constexpr uint16_t kChannelId = 1;

// Message ids (plugin-defined, uint16).
constexpr uint16_t kMsgHello = 1;    // client to host: a client is here, send the open map if there is one
constexpr uint16_t kMsgMapOpen = 2;  // host to client: this map is open

constexpr size_t kHelloSize = 2;
constexpr size_t kMapOpenSize = 20;
constexpr size_t kMaxMessageSize = 224;  // the SDK's payload limit
static_assert(kMapOpenSize <= kMaxMessageSize);

constexpr uint8_t kFlagMaze = 1;  // the map is built by the maze steering (Cave, Crypt): `seed` is its layout seed

// The open map. `mapIndex` is the row in maps.txt (0 based, rows that failed to load are not counted), `nameHash` the hash of its
// Name, so a client whose maps.txt differs is noticed. `opening` is how many maps this game has opened (1 for the first). `seed` is
// the layout seed of a maze map; a preset map has no layout seed and sends 0.
struct MapIdentity {
	uint16_t mapIndex = 0;
	uint32_t nameHash = 0;
	uint16_t slotLevel = 0;  // levels.txt Id of the slot level the portal leads to; never 0
	uint16_t opening = 0;
	uint64_t seed = 0;
	bool maze = false;
	bool operator==(const MapIdentity &o) const {
		return mapIndex == o.mapIndex && nameHash == o.nameHash && slotLevel == o.slotLevel && opening == o.opening && seed == o.seed && maze == o.maze;
	}
};

// FNV-1a over the name's bytes, ASCII-lowercased (map names compare without case everywhere else).
uint32_t nameHash(std::string_view name);

enum class DecodeError {
	None,
	BadSize,
	BadVersion,
	BadFlags,    // an unknown flag bit, or a reserved byte that is not 0
	BadSlot,     // slot level 0
	BadOpening,  // opening 0
	BadSeed,     // a preset map with a seed
};
const char *describe(DecodeError e);

// Writes the message into `out` and returns its size. 0 when `id` is not valid to send (slot level 0, opening 0, a preset
// map with a seed) or `cap` is too small.
size_t encodeMapOpen(const MapIdentity &id, uint8_t *out, size_t cap);
DecodeError decodeMapOpen(const void *data, size_t size, MapIdentity *out);

size_t encodeHello(uint8_t *out, size_t cap);
DecodeError decodeHello(const void *data, size_t size);

// --- The client's check ------------------------------------------------------------------------------------------------

// The hash of each Name in the client's maps.txt, by row (nameHash of the name).
using TableHashes = std::vector<uint32_t>;

enum class Verdict {
	Accepted,
	Unchanged,     // the same identity again (a resend)
	Stale,         // an older opening than the one kept
	NoTables,      // the client has no maps: it cannot place the identity
	NoSuchRow,     // mapIndex is past the client's maps.txt
	NameMismatch,  // the row's name differs: the tables differ between host and client
};
const char *describe(Verdict v);

// What the client holds for this game. Not thread safe; the engine part guards it.
class ClientMap {
public:
	// Checks `id` against the client's own table and keeps it when it is good.
	Verdict offer(const MapIdentity &id, const TableHashes &table);
	const std::optional<MapIdentity> &current() const { return current_; }
	void clear() { current_.reset(); }

private:
	std::optional<MapIdentity> current_;
};

// --- The host's clients ------------------------------------------------------------------------------------------------

// The peers that said hello, at most kMaxPeers (a game holds 8 players; the cap only bounds a flood). When full, the oldest
// entry is dropped for a new peer (a dead handle may never have been removed). Not thread safe.
constexpr size_t kMaxPeers = 16;
class PeerBook {
public:
	enum class Add { Added, Known, AddedDroppedOldest };
	Add add(uint64_t peer);
	bool remove(uint64_t peer);
	bool has(uint64_t peer) const;
	const std::vector<uint64_t> &peers() const { return peers_; }
	void clear() { peers_.clear(); }

private:
	std::vector<uint64_t> peers_;
};

// --- The client's connect schedule ---------------------------------------------------------------------------------------

// A client calls connectToHost after entering a game; a TCP client is told Busy until the host's transport packet arrived, so it
// retries. A host or single player process has a server tick within moments and never connects (the engine part asks first).
constexpr uint64_t kFirstDelayMs = 3000;
constexpr uint64_t kRetryMs = 1000;
constexpr int kMaxAttempts = 30;

struct ConnectSchedule {
	uint64_t nextAtMs = 0;
	int attempts = 0;
	bool started = false;
	bool done = false;
};
enum class Step { Wait, Try, GiveUp };

void begin(ConnectSchedule &s, uint64_t nowMs);
// Wait: nothing yet. Try: call connectToHost now (counts one attempt, the next is kRetryMs later). GiveUp: no more attempts.
Step step(ConnectSchedule &s, uint64_t nowMs);
void finish(ConnectSchedule &s);  // connected, rejected or the game ended: no more steps
// The host rejected the connection. HostUnavailable (its side may not be ready yet) keeps the schedule running so the next Try
// retries, as the SDK says; any other reason ends it. Returns whether the schedule keeps going.
bool onRejected(ConnectSchedule &s, bool hostUnavailable);

}  // namespace cartographer::mapsync
