// Cartographer map sync (phase 7, part 1): the host tells each joined client which map is open. This file is the message
// (encode, decode, validation) and the small decisions around it that need no game: the client's check against its own
// maps.txt, the host's list of clients, and the client's connect retry schedule. Pure C++ on plain structs: no engine, no
// SDK. The engine part (src/engine/map_sync.cpp) sends and receives; notes in docs/reverse-engineering/cartographer/12-map-sync.md.
#pragma once
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace cartographer::mapsync {

// --- The wire format -----------------------------------------------------------------------------------------------

// Bump when a message changes; the channel token carries it, so a host and a client on different versions never connect.
constexpr uint8_t kWireVersion = 2;  // 2: the outdoor flag
constexpr uint64_t kChannelToken = 0x4341525400000000ULL | kWireVersion;  // "CART" and the wire version
constexpr uint16_t kChannelId = 1;

// Message ids (plugin-defined, uint16).
constexpr uint16_t kMsgHello = 1;    // client to host: a client is here, send the open map if there is one
constexpr uint16_t kMsgMapOpen = 2;  // host to client: this map is open

constexpr size_t kHelloSize = 2;
constexpr size_t kMapOpenSize = 20;
constexpr size_t kMaxMessageSize = 224;  // the SDK's payload limit
static_assert(kMapOpenSize <= kMaxMessageSize);

constexpr uint8_t kFlagMaze = 1;     // the map is built by the maze steering (Cave, Crypt): `seed` is its layout seed
constexpr uint8_t kFlagOutdoor = 2;  // the map is built by the outdoor build (Jungle): `seed` is its layout seed. Never together with kFlagMaze

// The open map. `mapIndex` is the row in maps.txt (0 based, rows that failed to load are not counted), `nameHash` the hash of its
// Name, so a client whose maps.txt differs is noticed. `opening` is how many maps this game has opened (1 for the first). `seed` is
// the layout seed of a maze or outdoor map; a preset map has no layout seed and sends 0.
struct MapIdentity {
	uint16_t mapIndex = 0;
	uint32_t nameHash = 0;
	uint16_t slotLevel = 0;  // levels.txt Id of the slot level the portal leads to; never 0
	uint16_t opening = 0;
	uint64_t seed = 0;
	bool maze = false;
	bool outdoor = false;
	bool operator==(const MapIdentity &o) const {
		return mapIndex == o.mapIndex && nameHash == o.nameHash && slotLevel == o.slotLevel && opening == o.opening && seed == o.seed && maze == o.maze && outdoor == o.outdoor;
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
	BadKind,     // both the maze and the outdoor flag
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

// --- The client applies the identity ------------------------------------------------------------------------------------

// What the client's engine glue found out about its own side of an accepted identity. Plain facts; the decision is below.
struct ApplyFacts {
	bool slotLevelMatches = false;  // the client's own slots.txt gives this map's slot the same level id the host sent
	bool clientMazeMap = false;     // the client's own maps.txt row is a Cave or Crypt map
	bool namesSetting = false;      // MapNames=1 in the client's cartographer.txt
	bool mazeSteeringLive = false;  // every maze part is on and the hooks are in
	bool tilesetFits = false;       // the client's Maze slot level can build this map's tileset
	bool slotBuilt = false;         // the client's slot level was already built (with or without a map open) in this game
	// The outdoor half, the client's own side. For an outdoor identity `slotLevelMatches` is about the client's Outdoor slot level.
	bool clientOutdoorMap = false;  // the client's own maps.txt row is an outdoor (Jungle) map
	bool outdoorBuildLive = false;  // OutdoorBuild = 1 here, every outdoor part is on and the hooks are in
	bool outdoorSlotBuilt = false;  // the client's Outdoor slot level was already built (with or without a map open) in this game
};

enum class Skip {
	None,             // this half runs
	SlotMismatch,     // the client's slot level for this map is not the host's: the tables or the mod differ
	KindMismatch,     // the host says maze and the client's row is not (or the other way round)
	SettingOff,       // MapNames is off on this client
	NotMaze,          // a preset map has no layout to apply
	NoSeed,           // a maze identity with seed 0: the host had no plan to give
	SteeringOff,      // the client's maze steering is off
	TilesetMismatch,  // the client's Maze slot builds the other tileset
	AlreadyBuilt,     // the client built the slot level before the identity arrived; it is not rebuilt
	NotOutdoor,       // a maze or preset map has no outdoor layout
	OutdoorOff,       // the client's outdoor build is off
};
const char *describe(Skip s);

// Which halves of the identity the client applies: the map's name (MapNames), the maze layout seed and the outdoor layout seed.
struct ApplyPlan {
	bool names = false;
	Skip namesWhy = Skip::None;
	bool maze = false;
	Skip mazeWhy = Skip::None;
	bool outdoor = false;
	Skip outdoorWhy = Skip::None;
};
// A slot or kind mismatch skips both halves (the client's tables do not describe the host's map). Pure.
ApplyPlan decideApply(const MapIdentity &id, const ApplyFacts &facts);

// Applies each opening once: a resend of the same identity, or an older opening, is not applied again. Cleared when a game starts or ends.
class AppliedOpening {
public:
	// True the first time `opening` is seen, or when it is newer than the last applied one.
	bool take(uint16_t opening)
	{
		if (last_ && opening <= *last_) return false;
		last_ = opening;
		return true;
	}
	void clear() { last_.reset(); }

private:
	std::optional<uint16_t> last_;
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
