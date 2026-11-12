#include "mapsync.h"

#include <algorithm>

namespace cartographer::mapsync {

namespace {

void put16(uint8_t *p, uint16_t v) {
	p[0] = static_cast<uint8_t>(v);
	p[1] = static_cast<uint8_t>(v >> 8);
}
void put32(uint8_t *p, uint32_t v) {
	for (int i = 0; i < 4; ++i) {
		p[i] = static_cast<uint8_t>(v >> (8 * i));
	}
}
void put64(uint8_t *p, uint64_t v) {
	for (int i = 0; i < 8; ++i) {
		p[i] = static_cast<uint8_t>(v >> (8 * i));
	}
}
uint16_t get16(const uint8_t *p) { return static_cast<uint16_t>(p[0] | (p[1] << 8)); }
uint32_t get32(const uint8_t *p) {
	uint32_t v = 0;
	for (int i = 3; i >= 0; --i) {
		v = (v << 8) | p[i];
	}
	return v;
}
uint64_t get64(const uint8_t *p) {
	uint64_t v = 0;
	for (int i = 7; i >= 0; --i) {
		v = (v << 8) | p[i];
	}
	return v;
}

bool valid(const MapIdentity &id) { return id.slotLevel != 0 && id.opening != 0 && (id.maze || id.outdoor || id.seed == 0) && !(id.maze && id.outdoor); }

}  // namespace

uint32_t nameHash(std::string_view name) {
	uint32_t h = 2166136261u;
	for (const char c : name) {
		const unsigned char u = static_cast<unsigned char>(c);
		h ^= (u >= 'A' && u <= 'Z') ? static_cast<unsigned char>(u + 32) : u;
		h *= 16777619u;
	}
	return h;
}

const char *describe(DecodeError e) {
	switch (e) {
	case DecodeError::None: return "ok";
	case DecodeError::BadSize: return "wrong size";
	case DecodeError::BadVersion: return "wire version differs";
	case DecodeError::BadFlags: return "unknown flag or reserved byte set";
	case DecodeError::BadSlot: return "slot level is 0";
	case DecodeError::BadOpening: return "opening count is 0";
	case DecodeError::BadSeed: return "a preset map with a seed";
	case DecodeError::BadKind: return "both the maze and the outdoor flag";
	}
	return "?";
}

size_t encodeMapOpen(const MapIdentity &id, uint8_t *out, size_t cap) {
	if (out == nullptr || cap < kMapOpenSize || !valid(id)) {
		return 0;
	}
	out[0] = kWireVersion;
	out[1] = static_cast<uint8_t>((id.maze ? kFlagMaze : 0) | (id.outdoor ? kFlagOutdoor : 0));
	put16(out + 2, id.mapIndex);
	put32(out + 4, id.nameHash);
	put16(out + 8, id.slotLevel);
	put16(out + 10, id.opening);
	put64(out + 12, id.seed);
	return kMapOpenSize;
}

DecodeError decodeMapOpen(const void *data, size_t size, MapIdentity *out) {
	if (data == nullptr || size != kMapOpenSize || out == nullptr) {
		return DecodeError::BadSize;
	}
	const uint8_t *p = static_cast<const uint8_t *>(data);
	if (p[0] != kWireVersion) {
		return DecodeError::BadVersion;
	}
	if ((p[1] & ~(kFlagMaze | kFlagOutdoor)) != 0) {
		return DecodeError::BadFlags;
	}
	MapIdentity id;
	id.maze = (p[1] & kFlagMaze) != 0;
	id.outdoor = (p[1] & kFlagOutdoor) != 0;
	id.mapIndex = get16(p + 2);
	id.nameHash = get32(p + 4);
	id.slotLevel = get16(p + 8);
	id.opening = get16(p + 10);
	id.seed = get64(p + 12);
	if (id.slotLevel == 0) {
		return DecodeError::BadSlot;
	}
	if (id.opening == 0) {
		return DecodeError::BadOpening;
	}
	if (id.maze && id.outdoor) {
		return DecodeError::BadKind;
	}
	if (!id.maze && !id.outdoor && id.seed != 0) {
		return DecodeError::BadSeed;
	}
	*out = id;
	return DecodeError::None;
}

size_t encodeHello(uint8_t *out, size_t cap) {
	if (out == nullptr || cap < kHelloSize) {
		return 0;
	}
	out[0] = kWireVersion;
	out[1] = 0;
	return kHelloSize;
}

DecodeError decodeHello(const void *data, size_t size) {
	if (data == nullptr || size != kHelloSize) {
		return DecodeError::BadSize;
	}
	const uint8_t *p = static_cast<const uint8_t *>(data);
	if (p[0] != kWireVersion) {
		return DecodeError::BadVersion;
	}
	return p[1] == 0 ? DecodeError::None : DecodeError::BadFlags;
}

const char *describe(Verdict v) {
	switch (v) {
	case Verdict::Accepted: return "accepted";
	case Verdict::Unchanged: return "same as the one kept";
	case Verdict::Stale: return "older than the one kept";
	case Verdict::NoTables: return "this client has no maps.txt rows";
	case Verdict::NoSuchRow: return "the host's map row is not in this client's maps.txt";
	case Verdict::NameMismatch: return "the map name at that row differs: maps.txt differs between host and client";
	}
	return "?";
}

Verdict ClientMap::offer(const MapIdentity &id, const TableHashes &table) {
	if (table.empty()) {
		return Verdict::NoTables;
	}
	if (id.mapIndex >= table.size()) {
		return Verdict::NoSuchRow;
	}
	if (table[id.mapIndex] != id.nameHash) {
		return Verdict::NameMismatch;
	}
	if (current_ && id.opening < current_->opening) {
		return Verdict::Stale;
	}
	if (current_ && *current_ == id) {
		return Verdict::Unchanged;
	}
	current_ = id;
	return Verdict::Accepted;
}

PeerBook::Add PeerBook::add(uint64_t peer) {
	if (has(peer)) {
		return Add::Known;
	}
	if (peers_.size() >= kMaxPeers) {
		peers_.erase(peers_.begin());
		peers_.push_back(peer);
		return Add::AddedDroppedOldest;
	}
	peers_.push_back(peer);
	return Add::Added;
}

bool PeerBook::remove(uint64_t peer) {
	const auto it = std::find(peers_.begin(), peers_.end(), peer);
	if (it == peers_.end()) {
		return false;
	}
	peers_.erase(it);
	return true;
}

bool PeerBook::has(uint64_t peer) const { return std::find(peers_.begin(), peers_.end(), peer) != peers_.end(); }

void begin(ConnectSchedule &s, uint64_t nowMs) {
	s = ConnectSchedule{};
	s.started = true;
	s.nextAtMs = nowMs + kFirstDelayMs;
}

Step step(ConnectSchedule &s, uint64_t nowMs) {
	if (!s.started || s.done) {
		return Step::GiveUp;
	}
	if (s.attempts >= kMaxAttempts) {
		s.done = true;
		return Step::GiveUp;
	}
	if (nowMs < s.nextAtMs) {
		return Step::Wait;
	}
	++s.attempts;
	s.nextAtMs = nowMs + kRetryMs;
	return Step::Try;
}

void finish(ConnectSchedule &s) { s.done = true; }

bool onRejected(ConnectSchedule &s, bool hostUnavailable) {
	if (!hostUnavailable) {
		finish(s);
	}
	return !s.done;
}

const char *describe(Skip s)
{
	switch (s) {
	case Skip::None: return "applied";
	case Skip::SlotMismatch: return "this client's slot level for the map is not the host's (the tables or the mod differ)";
	case Skip::KindMismatch: return "the host's map kind (maze or preset) is not this client's (maps.txt differs)";
	case Skip::SettingOff: return "MapNames is off here";
	case Skip::NotMaze: return "a preset map has no layout seed";
	case Skip::NoSeed: return "the host sent no layout seed";
	case Skip::SteeringOff: return "the maze steering is off here";
	case Skip::TilesetMismatch: return "this client's Maze slot builds the other tileset";
	case Skip::AlreadyBuilt: return "the slot level was already built here before the host's map arrived, so its layout is not changed";
	case Skip::NotOutdoor: return "this map has no outdoor layout";
	case Skip::OutdoorOff: return "the outdoor build is off here";
	}
	return "?";
}

ApplyPlan decideApply(const MapIdentity &id, const ApplyFacts &f)
{
	ApplyPlan p;
	if (!f.slotLevelMatches) {
		p.namesWhy = p.mazeWhy = p.outdoorWhy = Skip::SlotMismatch;
		return p;
	}
	if (id.maze != f.clientMazeMap || id.outdoor != f.clientOutdoorMap) {
		p.namesWhy = p.mazeWhy = p.outdoorWhy = Skip::KindMismatch;
		return p;
	}
	p.names = f.namesSetting;
	p.namesWhy = f.namesSetting ? Skip::None : Skip::SettingOff;
	if (!id.outdoor) {
		p.outdoorWhy = Skip::NotOutdoor;
	} else if (id.seed == 0) {
		p.outdoorWhy = Skip::NoSeed;
	} else if (!f.outdoorBuildLive) {
		p.outdoorWhy = Skip::OutdoorOff;
	} else if (f.outdoorSlotBuilt) {
		p.outdoorWhy = Skip::AlreadyBuilt;
	} else {
		p.outdoor = true;
	}
	if (!id.maze) {
		p.mazeWhy = Skip::NotMaze;
	} else if (id.seed == 0) {
		p.mazeWhy = Skip::NoSeed;
	} else if (!f.mazeSteeringLive) {
		p.mazeWhy = Skip::SteeringOff;
	} else if (!f.tilesetFits) {
		p.mazeWhy = Skip::TilesetMismatch;
	} else if (f.slotBuilt) {
		p.mazeWhy = Skip::AlreadyBuilt;
	} else {
		p.maze = true;
	}
	return p;
}

}  // namespace cartographer::mapsync
