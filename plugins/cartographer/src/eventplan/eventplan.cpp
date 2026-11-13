#include "eventplan.h"

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <string>

#include "ascii.h"

namespace cartographer::eventplan {

namespace {

using layout::equalsNoCase;

bool same(std::string_view a, std::string_view b, bool exact) { return exact ? a == b : equalsNoCase(a, b); }

int findMonster(const SpawnTable &t, std::string_view name, bool exact) {
	for (size_t i = 0; i < t.monsters.size(); ++i) {
		if (!t.monsters[i].empty() && same(t.monsters[i], name, exact)) {
			return static_cast<int>(i);
		}
	}
	return -1;
}

int findSuper(const SpawnTable &t, std::string_view name, bool exact) {
	for (size_t i = 0; i < t.supers.size(); ++i) {
		if (!t.supers[i].name.empty() && same(t.supers[i].name, name, exact)) {
			return static_cast<int>(i);
		}
	}
	return -1;
}

int centreX(const Rect &r) { return r.x + r.w / 2; }
int centreY(const Rect &r) { return r.y + r.h / 2; }

} // namespace

std::string_view superClassOf(const SpawnTable &table, int superRow) {
	if (superRow < 0 || static_cast<size_t>(superRow) >= table.supers.size()) {
		return {};
	}
	return table.supers[static_cast<size_t>(superRow)].cls;
}

ClassCheck checkSuperClass(const SpawnTable &table, std::string_view name, std::string_view gameClassId) {
	if (name.empty()) {
		return ClassCheck::NoReference;
	}
	int s = findSuper(table, name, true);
	if (s < 0) {
		s = findSuper(table, name, false);
	}
	if (s < 0) {
		return ClassCheck::NoReference;
	}
	const std::string_view cls = superClassOf(table, s);
	if (cls.empty() || gameClassId.empty()) {
		return ClassCheck::NoReference;
	}
	return same(cls, gameClassId, false) ? ClassCheck::Agrees : ClassCheck::Differs;
}

bool idRowAccepted(const std::vector<int> &rowsWithId, int got) {
	return std::find(rowsWithId.begin(), rowsWithId.end(), got) != rowsWithId.end();
}

Resolved resolveSpawn(const SpawnTable &table, std::string_view spawn) {
	Resolved out;
	if (spawn.empty()) {
		return out;
	}
	for (const bool exact : {true, false}) {
		if (const int m = findMonster(table, spawn, exact); m >= 0) {
			out.kind = events::NameKind::Monster;
			out.row = out.classRow = m;
			return out;
		}
		if (const int s = findSuper(table, spawn, exact); s >= 0) {
			out.kind = events::NameKind::Unique;
			out.row = s;
			const std::string &cls = table.supers[static_cast<size_t>(s)].cls;
			out.classRow = findMonster(table, cls, true);
			if (out.classRow < 0) {
				out.classRow = findMonster(table, cls, false);
			}
			return out;
		}
	}
	return out;
}

// One pass of the grid: `inset` clear of the edge, spots `gap` apart (both in the room's own units). The centre is always first.
static std::vector<Spot> placeOnGrid(const Rect &room, int n, uint64_t seed, int inset, int gap) {
	std::vector<Spot> out;
	Rect in = room;
	if (room.w > 2 * inset + 1 && room.h > 2 * inset + 1) {
		in = {room.x + inset, room.y + inset, room.w - 2 * inset, room.h - 2 * inset};
	}
	const int cx = centreX(room), cy = centreY(room);
	// A grid gap apart, nearest the centre first; the seed orders the cells that are equally near.
	struct Cand {
		int x, y, d, tie;
	};
	std::vector<Cand> cands;
	for (int y = in.y; y < in.y + in.h; y += gap) {
		for (int x = in.x; x < in.x + in.w; x += gap) {
			const int d = std::max(std::abs(x - cx), std::abs(y - cy));
			const uint64_t h = (static_cast<uint64_t>(x) * 73856093u) ^ (static_cast<uint64_t>(y) * 19349663u) ^ seed;
			cands.push_back({x, y, d, static_cast<int>((h ^ (h >> 17)) & 0xFFFF)});
		}
	}
	out.push_back({cx, cy});  // the centre is always first, even when it is not on the grid
	std::sort(cands.begin(), cands.end(), [](const Cand &a, const Cand &b) {
		return a.d != b.d ? a.d < b.d : a.tie != b.tie ? a.tie < b.tie : a.y != b.y ? a.y < b.y : a.x < b.x;
	});
	for (const Cand &c : cands) {
		if (static_cast<int>(out.size()) >= n) {
			break;
		}
		bool apart = true;
		for (const Spot &s : out) {
			if (std::max(std::abs(s.x - c.x), std::abs(s.y - c.y)) < gap) {
				apart = false;
				break;
			}
		}
		if (apart) {
			out.push_back({c.x, c.y});
		}
	}
	return out;
}

std::vector<Spot> placements(const Rect &room, int n, uint64_t seed, int scale) {
	scale = std::max(1, scale);
	if (n <= 0 || room.w <= 0 || room.h <= 0) {
		return {};
	}
	std::vector<Spot> out = placeOnGrid(room, n, seed, kEdgeInset * scale, kSpotGap * scale);
	if (static_cast<int>(out.size()) < n) {
		// An outdoor event room is one 8 x 8 tile room: the usual spacing fits only 4 spots. Pack tighter (one tile) before giving spots up.
		// The tight pass replaces the whole layout (a Horde of 6 becomes a 3 x 3 block around the centre); it is not added to the normal one.
		std::vector<Spot> tight = placeOnGrid(room, n, seed, kTightInset * scale, kTightGap * scale);
		if (tight.size() > out.size()) {
			out = std::move(tight);
		}
	}
	return out;
}

int minEventDistance(int levelW, int levelH) {
	const int side = std::max(levelW, levelH);
	if (side <= 0) {
		return kMinEventDistance;
	}
	return std::clamp(side / 3, 1, kMinEventDistance);
}

std::optional<Rect> farthestRoom(const Rect &entry, const std::vector<Rect> &rooms) {
	std::optional<Rect> best;
	long long bestCheb = -1, bestSq = -1;
	for (const Rect &r : rooms) {
		if (r.w <= 0 || r.h <= 0 || (r.x == entry.x && r.y == entry.y && r.w == entry.w && r.h == entry.h)) {
			continue;
		}
		const long long dx = std::abs(centreX(r) - centreX(entry)), dy = std::abs(centreY(r) - centreY(entry));
		const long long cheb = std::max(dx, dy), sq = dx * dx + dy * dy;
		if (cheb > bestCheb || (cheb == bestCheb && sq > bestSq)) {
			best = r;
			bestCheb = cheb;
			bestSq = sq;
		}
	}
	return best;
}

int presetEventDistance(int levelW, int levelH, const Rect &entry, const std::vector<Rect> &rooms) {
	const int distance = minEventDistance(levelW, levelH);
	const std::optional<Rect> far = farthestRoom(entry, rooms);
	if (!far) {
		return distance;
	}
	const int cheb = std::max(std::abs(centreX(*far) - centreX(entry)), std::abs(centreY(*far) - centreY(entry)));
	return std::min(distance, cheb);
}

EventRoomTracker::EventRoomTracker(std::optional<Rect> planned, int minDistance) : planned_(planned), minDistance_(minDistance) {}

bool EventRoomTracker::onPopulated(const Rect &room) {
	++count_;
	if (fired_) {
		return false;
	}
	if (planned_) {
		if (room.x == planned_->x && room.y == planned_->y && room.w == planned_->w && room.h == planned_->h) {
			fired_ = true;
			return true;
		}
		return false;
	}
	if (!entry_) {
		entry_ = room;
		return false;
	}
	const int d = std::max(std::abs(centreX(room) - centreX(*entry_)), std::abs(centreY(room) - centreY(*entry_)));
	if (d >= minDistance_) {
		fired_ = true;
		return true;
	}
	return false;
}

BossRoomPicker::BossRoomPicker(std::vector<Rect> ranked) : ranked_(std::move(ranked)), populated_(ranked_.size(), false) {
	done_ = ranked_.empty();
}

int BossRoomPicker::indexOf(const Rect &room) const {
	for (size_t i = 0; i < ranked_.size(); ++i) {
		if (ranked_[i].x == room.x && ranked_[i].y == room.y && ranked_[i].w == room.w && ranked_[i].h == room.h) {
			return static_cast<int>(i);
		}
	}
	return -1;
}

bool BossRoomPicker::onPopulated(const Rect &room) {
	if (done_) {
		return false;
	}
	const int i = indexOf(room);
	if (i < 0) {
		return false;
	}
	populated_[static_cast<size_t>(i)] = true;
	return static_cast<size_t>(i) == current_;
}

int BossRoomPicker::refused() {
	if (done_) {
		return -1;
	}
	++current_;
	if (current_ >= ranked_.size()) {
		done_ = true;  // none left: given up
		return -1;
	}
	return populated_[current_] ? static_cast<int>(current_) : -1;
}

SpawnPlan planEvent(const events::Event &event, const Resolved &spawn, const Rect &room, uint64_t seed, int scale) {
	SpawnPlan plan;
	if (event.kind == events::Kind::Shrine) {
		plan.problem = "Shrine events: no monster spawn plan (placed as an object)";
		return plan;
	}
	if (event.kind == events::Kind::Invalid) {
		plan.problem = std::string(events::kindName(event.kind)) + " events: unknown kind";
		return plan;
	}
	if (spawn.kind != events::NameKind::Monster && spawn.kind != events::NameKind::Unique) {
		plan.problem = "Spawn '" + event.spawn + "' is not a monstats Id or a superuniques name";
		return plan;
	}
	if (spawn.classRow < 0) {
		plan.problem = "Spawn '" + event.spawn + "' has no monster to spawn (its Class is not in monstats.txt)";
		return plan;
	}
	// Ambush: one wave's worth (the engine part spawns it once per wave). Treasure: the one monster (a chest is placed by the engine part).
	const int total = event.kind == events::Kind::Boss       ? 1 + std::max(0, event.minions)
	                  : event.kind == events::Kind::Ambush   ? std::max(0, event.waveSize)
	                  : event.kind == events::Kind::Treasure ? 1
	                                                         : std::max(0, event.hordeSize);
	plan.requested = total;
	const std::vector<Spot> spots = placements(room, total, seed, scale);
	for (size_t i = 0; i < spots.size(); ++i) {
		Spawn s;
		s.spot = spots[i];
		s.classRow = spawn.classRow;
		if ((event.kind == events::Kind::Boss || event.kind == events::Kind::Treasure) && i == 0 && spawn.kind == events::NameKind::Unique) {
			s.superUnique = true;
			s.row = spawn.row;
		} else {
			s.row = spawn.classRow;  // minions and horde members: the monster, or the superunique's class
		}
		plan.spawns.push_back(s);
	}
	if (plan.spawns.empty()) {
		plan.problem = "the event room is too small to place anything";
	}
	return plan;
}

std::vector<Spot> bossSpots(const Rect &room, int scale) {
	scale = std::max(1, scale);
	std::vector<Spot> out;
	if (room.w <= 0 || room.h <= 0) {
		return out;
	}
	const int cx = centreX(room), cy = centreY(room);
	out.push_back({cx, cy});
	Rect in = room;
	if (room.w > 2 * scale + 1 && room.h > 2 * scale + 1) {
		in = {room.x + scale, room.y + scale, room.w - 2 * scale, room.h - 2 * scale};
	}
	std::vector<Spot> ring;
	for (int y = in.y; y < in.y + in.h; y += scale) {
		for (int x = in.x; x < in.x + in.w; x += scale) {
			if (x != cx || y != cy) ring.push_back({x, y});
		}
	}
	std::sort(ring.begin(), ring.end(), [&](const Spot &a, const Spot &b) {
		const long da = static_cast<long>(a.x - cx) * (a.x - cx) + static_cast<long>(a.y - cy) * (a.y - cy);
		const long db = static_cast<long>(b.x - cx) * (b.x - cx) + static_cast<long>(b.y - cy) * (b.y - cy);
		return da != db ? da < db : a.y != b.y ? a.y < b.y : a.x < b.x;
	});
	for (const Spot &s : ring) {
		if (static_cast<int>(out.size()) >= kBossSpots) break;
		out.push_back(s);
	}
	return out;
}

SpawnPlan planMapBoss(const std::string &name, const Resolved &boss, const Rect &room, int scale) {
	SpawnPlan plan;
	if (boss.kind != events::NameKind::Monster && boss.kind != events::NameKind::Unique) {
		plan.problem = "Boss '" + name + "' is not a monstats Id or a superuniques name";
		return plan;
	}
	if (boss.classRow < 0) {
		plan.problem = "Boss '" + name + "' has no monster to spawn (its Class is not in monstats.txt)";
		return plan;
	}
	const std::vector<Spot> spots = bossSpots(room, scale);
	if (spots.empty()) {
		plan.problem = "the boss room is too small to place anything";
		return plan;
	}
	for (const Spot &spot : spots) {  // the centre first; the caller tries them in order until the game accepts one
		Spawn s;
		s.spot = spot;
		s.classRow = boss.classRow;
		s.superUnique = boss.kind == events::NameKind::Unique;
		s.row = s.superUnique ? boss.row : boss.classRow;
		plan.spawns.push_back(s);
	}
	return plan;
}

int spotScale(const Rect &server, const Rect &drlg) {
	if (drlg.w <= 0 || drlg.h <= 0 || server.w < drlg.w || server.h < drlg.h || server.w % drlg.w != 0 || server.h % drlg.h != 0) {
		return 1;
	}
	const int k = server.w / drlg.w;
	return k == server.h / drlg.h ? k : 1;
}

ScaleKind scaleKind(const Rect &server, const Rect &drlg) {
	if (server.w == drlg.w && server.h == drlg.h) {
		return ScaleKind::Same;
	}
	return spotScale(server, drlg) > 1 ? ScaleKind::Multiple : ScaleKind::NotAMultiple;
}

const char *scaleKindName(ScaleKind kind) {
	switch (kind) {
	case ScaleKind::Same: return "the rects are the same size";
	case ScaleKind::Multiple: return "the server rect is a whole multiple";
	case ScaleKind::NotAMultiple: return "NOT a whole multiple, so 1 is a fallback";
	}
	return "";
}

std::optional<std::string_view> idText(const char *field, size_t fieldSize) {
	if (field == nullptr) {
		return std::nullopt;
	}
	for (size_t i = 0; i < fieldSize; ++i) {
		if (field[i] == '\0') {
			return std::string_view(field, i);
		}
	}
	return std::nullopt;
}

bool idMatches(const char *field, size_t fieldSize, std::string_view name) {
	const std::optional<std::string_view> text = idText(field, fieldSize);
	return text && !text->empty() && !name.empty() && equalsNoCase(*text, name);
}

int findMonsterRow(const uint8_t *rows, uint64_t rowCount, uint32_t rowSize, std::string_view name) {
	if (rows == nullptr || name.empty() || rowSize < kMonStatsIdOffset + kMonStatsIdSize) {
		return -1;
	}
	for (uint64_t i = 0; i < rowCount && i <= static_cast<uint64_t>(INT32_MAX); ++i) {
		if (idMatches(reinterpret_cast<const char *>(rows + i * rowSize + kMonStatsIdOffset), kMonStatsIdSize, name)) {
			return static_cast<int>(i);
		}
	}
	return -1;
}

std::string checkTable(const TableFacts &f, uint32_t wantRowSize) {
	if (f.viewRowSize != wantRowSize) {
		return "the SDK row size is " + std::to_string(f.viewRowSize) + ", not " + std::to_string(wantRowSize);
	}
	if (f.dtCount > 0xFFFFFFFFull || f.dtCount != f.viewCount) {
		return "the game's row count " + std::to_string(f.dtCount) + " is not the SDK's " + std::to_string(f.viewCount);
	}
	if (f.viewCount == 0) {
		return "the table has no rows";
	}
	if (f.viewRows == nullptr || f.dtRows == nullptr) {
		return "a row pointer is null";
	}
	if (f.viewRows != f.dtRows) {
		return "the SDK rows are not the game's rows for this data flag";
	}
	return {};
}

std::string checkRoundTrip(const RoundTrip &t) {
	if (t.mapNull) {
		return "the monstats name map is null";
	}
	if (t.checked <= 0) {
		return "no monstats Id could be looked up";
	}
	if (t.wrong > 0) {
		return std::to_string(t.wrong) + " of " + std::to_string(t.checked) + " monstats Ids did not map back to their own row (first: row " +
		       std::to_string(t.firstWrongRow) + " gave " + std::to_string(t.firstWrongGot) + ")";
	}
	return {};
}

int superRowFromLookup(int32_t result, uint64_t rowCount) {
	return result >= 0 && static_cast<uint64_t>(result) < rowCount ? result : -1;
}

const RouteVerdict *RouteCache::find(const RouteKey &key) const {
	for (const auto &e : entries_) {
		if (e.first == key) {
			return &e.second;
		}
	}
	return nullptr;
}

void RouteCache::store(const RouteKey &key, RouteVerdict verdict) {
	entries_.erase(std::remove_if(entries_.begin(), entries_.end(), [&](const auto &e) { return e.first.revision != key.revision || e.first == key; }), entries_.end());
	entries_.emplace_back(key, std::move(verdict));
}

bool superSeen(const uint8_t *bitmap, int row) {
	if (bitmap == nullptr || row < 0 || row > kMaxSuperRow) {
		return false;
	}
	return ((bitmap[row >> 3] >> (row & 7)) & 1) != 0;
}

std::vector<std::string> bannerLines(const std::string &mapName, const events::Event *event, bool announced, bool late) {
	(void)mapName;
	(void)late;
	return entryLines(std::string(), announced ? event : nullptr);  // the game's own "Entering <name>" is the map's title
}

std::vector<std::string> entryLines(const std::string &mapName, const events::Event *event) {
	std::vector<std::string> lines;
	if (!mapName.empty()) {
		lines.push_back(mapName);
	}
	if (event != nullptr) {
		if (!event->name.empty()) {
			lines.push_back(event->name);
		}
		if (!event->message.empty()) {
			lines.push_back(event->message);
		}
	}
	return lines;
}

bool EntryTracker::update(bool inMap) {
	const bool entered = inMap && !inside_;
	inside_ = inMap;
	return entered;
}

bool bannerVisible(int64_t elapsedMs) {
	return elapsedMs >= 0 && elapsedMs < kBannerTotalMs;
}

uint64_t eventSeed(uint64_t mapSeed) {
	uint64_t z = mapSeed ^ 0xE7E47C0FFEE5A11Dull;
	z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
	z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
	return z ^ (z >> 31);
}

} // namespace cartographer::eventplan
