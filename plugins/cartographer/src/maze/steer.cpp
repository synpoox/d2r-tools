// Cartographer maze steering, the pure part of the hooks. See steer.h.
#include "steer.h"

#include <algorithm>
#include <optional>

#include "ascii.h"
#include "maze_tilesets.h"
#include "rng.h"

namespace cartographer::maze {

namespace {

using layout::equalsNoCase;

} // namespace

bool builtinTileset(std::string_view name, Tileset &out)
{
	const MazeTileset *row = findMazeTileset(name);
	if (row == nullptr) return false;
	Tileset t;
	t.name = row->name;
	t.levelType = row->levelType;
	t.roomTiles = row->roomTiles;
	t.pieceBase = row->pieceBase;
	t.callsRingGrower = row->callsRingGrower;
	t.prev.present = row->prevPiece[0] != 0;
	for (int i = 0; i < 4; ++i) t.prev.piece[i] = row->prevPiece[i];  // Prev W, E, S, N
	out = std::move(t);
	return true;
}

SlotFit clampToSlot(Tileset tileset, int declaredX, int declaredY)
{
	SlotFit fit;
	const std::string who = tileset.name.empty() ? std::string("The tileset") : tileset.name;
	const int side = std::min(declaredX, declaredY);
	if (tileset.roomTiles <= 0 || side < tileset.roomTiles) {
		fit.reason = "The Maze slot level declares " + std::to_string(declaredX) + " by " + std::to_string(declaredY) +
		             " tiles, which is less than one " + who + " room (" + std::to_string(tileset.roomTiles) +
		             "). Set its SizeX and SizeY in levels.txt (see the Cartographer README).";
		return fit;
	}
	tileset.maxLevelTiles = std::min({tileset.maxLevelTiles, side, kMaxLevelTiles});
	fit.ok = true;
	fit.tileset = std::move(tileset);
	return fit;
}

DeclaredSizes::Size DeclaredSizes::record(int levelId, int x, int y)
{
	if (x < 1 || y < 1) return {x, y};
	return sizes_.emplace(levelId, Size{x, y}).first->second;
}

std::string slotTypeProblem(const Tileset &tileset, int slotLevelType, std::string_view mapName)
{
	if (slotLevelType == tileset.levelType) return {};
	const std::string who = "the " + tileset.name + " map '" + std::string(mapName) + "'";
	const MazeTileset *row = findMazeTileset(tileset.name);
	const std::string slotName = row != nullptr ? row->slot : "Maze";
	if (slotLevelType < 0) {
		return "Cartographer: could not read the LevelType of the " + slotName + " slot level from the mod's levels.txt, so " + who +
		       " is not opened. Check that the " + slotName + " row in slots.txt names a level that exists in levels.txt (see the Cartographer README).";
	}
	std::string slotIs = "LevelType " + std::to_string(slotLevelType) + ", which is none of the maze tilesets";
	for (const MazeTileset &t : MazeTilesets)
		if (t.levelType == slotLevelType) slotIs = std::string("a ") + t.name + " level (LevelType " + std::to_string(slotLevelType) + ")";
	return "Cartographer: the " + slotName + " slot level is " + slotIs + ", so it cannot build " + who + " (needs LevelType " + std::to_string(tileset.levelType) +
	       "). Each maze tileset has a slot level of its own: run tools\\install.ps1 again, and set its LevelType back if it was changed (see the Cartographer README).";
}

uint64_t mixSeed(uint64_t a, uint64_t b)
{
	return layout::splitmix64(a ^ layout::splitmix64(b));
}

MapPlan planForMap(std::string_view tileset, const layout::ShapeTable &shapes, std::string_view shape, std::string_view size,
                   uint64_t seed, int declaredX, int declaredY)
{
	MapPlan out;
	Tileset base;
	if (!builtinTileset(tileset, base)) {
		out.reason = "Cartographer has no maze numbers for the tileset '" + std::string(tileset) + "'. Use " + mazeTilesetNames() + ".";
		return out;
	}
	if (!base.steerable) {
		out.reason = base.notSteerableReason.empty() ? base.name + " cannot be steered." : base.notSteerableReason;
		return out;
	}
	SlotFit fit = clampToSlot(base, declaredX, declaredY);
	if (!fit.ok) {
		out.reason = fit.reason;
		return out;
	}
	const std::optional<layout::Size> sz = layout::parseSizeName(size.empty() ? std::string_view("Large") : size);
	if (!sz) {
		out.reason = "The size '" + std::string(size) + "' is not Small, Medium, Large or ExtraLarge." + layout::sizeRenameNote(size);
		return out;
	}
	layout::Rules rules;
	std::string error;
	if (!layout::resolveRules(shapes, shape, *sz, rules, error)) {
		out.reason = "The shape '" + std::string(shape) + "' cannot be used: " + error;
		return out;
	}
	out.tileset = fit.tileset;
	out.seedPlan = planForSeed(out.tileset, rules, seed);
	out.hash = planHash(out.seedPlan.plan);
	if (!out.seedPlan.plan.steer) {
		out.reason = out.seedPlan.plan.reason;
		return out;
	}
	if (out.seedPlan.plan.ring.graphInRing) {
		out.reason = base.name + " builds its graph inside the ring grower, which the hooks do not do yet.";
		return out;
	}
	out.steer = true;
	return out;
}

bool isPrevPiece(const Tileset &ts, int toPiece)
{
	if (!ts.prev.present) return false;
	return std::find(ts.prev.piece.begin(), ts.prev.piece.end(), toPiece) != ts.prev.piece.end();
}

SpecialAnswer answerSpecial(const Tileset &ts, const Plan &plan, SpecialState &state, const Group *group, uint32_t rot)
{
	SpecialAnswer a;
	if (!plan.steer) return a; // Original
	a.kind = SpecialAnswer::Kind::Suppress;
	if (group == nullptr || rot > 3) {
		// The called entry cannot be read, so it cannot be told from the entrance's: place nothing.
		if (state.fault.empty()) state.fault = "a special-room call came with a rotation outside 0..3, so its table could not be read";
		return a;
	}
	const TableEntry &called = (*group)[rot];
	const bool prev = isPrevPiece(ts, called.to);
	const SpecialSteer s = steerSpecial(ts, plan, called.to, state.prevCalls);
	if (prev) ++state.prevCalls;
	if (s.action != SpecialSteer::Action::Redirect) return a;
	for (int j = 0; j < 4; ++j) {
		const TableEntry &e = (*group)[static_cast<size_t>(j)];
		if (e.from == s.fromPiece && e.to == plan.entrance.toPiece) {
			a.kind = SpecialAnswer::Kind::Redirect;
			a.index = j;
			state.placed = true;
			return a;
		}
	}
	if (state.fault.empty()) {
		state.fault = "the game's Prev table has no entry from piece " + std::to_string(s.fromPiece) + " to piece " +
		              std::to_string(plan.entrance.toPiece) + ", which the plan needs for the entrance";
	}
	return a;
}

std::string specialProblem(const Plan &plan, const SpecialState &state)
{
	if (!state.fault.empty()) return state.fault;
	if (plan.steer && plan.entrance.place && !state.placed) {
		return "the level's own code never asked for an entrance room (no Prev call), so the plan's entrance was not placed";
	}
	return {};
}

std::array<int32_t, 7> lvlMazeRow(int levelId, const Plan &plan)
{
	const LvlMazeRow &r = plan.lvlMaze;
	return {levelId, r.rooms, r.rooms, r.rooms, r.sizeX, r.sizeY, r.merge};
}

int pieceAfterWalk(const Plan &plan, int roomIndex)
{
	if (roomIndex < 0 || roomIndex >= static_cast<int>(plan.rooms.size())) return 0;
	const Room &r = plan.rooms[static_cast<size_t>(roomIndex)];
	return r.special >= 0 && r.fix == Room::Fix::ByPlacer ? plan.entrance.fromPiece : r.piece;
}

bool fixAfterWalk(const Plan &plan, int roomIndex)
{
	if (roomIndex < 0 || roomIndex >= static_cast<int>(plan.rooms.size())) return false;
	return plan.rooms[static_cast<size_t>(roomIndex)].fix == Room::Fix::AfterWalk;
}

CellRoom planCellRoom(const Plan &plan, int unit, int planRoom, const std::vector<RoomRect> &rects)
{
	CellRoom out;
	const auto refuse = [&out](const char *why) {
		out.why = why;
		return out;
	};
	if (!plan.steer || plan.rooms.empty()) return refuse("the plan places no rooms");
	if (planRoom < 0 || static_cast<size_t>(planRoom) >= plan.rooms.size()) return refuse("the plan has no such room");
	if (unit <= 0) return refuse("the plan cell size is unknown");
	if (rects.empty()) return refuse("the level has no rooms");
	int pMinX = plan.rooms[0].dx, pMinY = plan.rooms[0].dy;
	for (const Room &r : plan.rooms) {
		pMinX = std::min(pMinX, r.dx);
		pMinY = std::min(pMinY, r.dy);
	}
	const Room &cell = plan.rooms[static_cast<size_t>(planRoom)];
	out.cellX = cell.dx - pMinX;
	out.cellY = cell.dy - pMinY;
	int64_t minX = 0, minY = 0, maxX = 0, maxY = 0;
	bool any = false;
	for (const RoomRect &r : rects) {
		if (r.w <= 0 || r.h <= 0) continue;  // a room with no area is not a room: it neither joins the box nor is chosen
		if (!any) {
			minX = r.x;
			minY = r.y;
			maxX = static_cast<int64_t>(r.x) + r.w;
			maxY = static_cast<int64_t>(r.y) + r.h;
			any = true;
			continue;
		}
		minX = std::min<int64_t>(minX, r.x);
		minY = std::min<int64_t>(minY, r.y);
		maxX = std::max<int64_t>(maxX, static_cast<int64_t>(r.x) + r.w);
		maxY = std::max<int64_t>(maxY, static_cast<int64_t>(r.y) + r.h);
	}
	if (!any) return refuse("the level has no rooms");
	// The built box must be the plan's box exactly (every planned cell is built whole), or the cell numbers mean nothing: a
	// partly missing edge column or row would move minX/minY by a part of a cell and shift the chosen cell.
	const int64_t w = maxX - minX, h = maxY - minY;
	if (w != static_cast<int64_t>(plan.widthRooms) * unit || h != static_cast<int64_t>(plan.heightRooms) * unit) {
		return refuse("the built rooms do not span the plan's box");
	}
	out.minX = static_cast<int32_t>(minX);
	out.minY = static_cast<int32_t>(minY);
	out.originX = out.minX + out.cellX * unit;
	out.originY = out.minY + out.cellY * unit;
	// The room whose top-left corner is in the cell and whose centre is nearest the cell's centre (doubled coordinates, so
	// there are no halves); ties go to the smaller y, then the smaller x.
	int64_t best = -1;
	for (size_t i = 0; i < rects.size(); ++i) {
		const RoomRect &r = rects[i];
		if (r.w <= 0 || r.h <= 0) continue;  // a room with no area is not a room of the cell
		if (r.x < out.originX || r.y < out.originY || r.x >= out.originX + unit || r.y >= out.originY + unit) continue;
		const int64_t dx = 2 * static_cast<int64_t>(r.x) + r.w - (2 * static_cast<int64_t>(out.originX) + unit);
		const int64_t dy = 2 * static_cast<int64_t>(r.y) + r.h - (2 * static_cast<int64_t>(out.originY) + unit);
		const int64_t d = dx * dx + dy * dy;
		bool better = best < 0 || d < best;
		if (!better && d == best) {
			const RoomRect &cur = rects[static_cast<size_t>(out.room)];
			better = r.y < cur.y || (r.y == cur.y && r.x < cur.x);
		}
		if (better) {
			best = d;
			out.room = static_cast<int>(i);
		}
	}
	if (out.room < 0) return refuse("the cell holds no room");
	return out;
}

ArrivalMark planArrival(const Plan &plan, int unit, const std::vector<RoomRect> &rects)
{
	ArrivalMark out;
	if (!plan.steer || plan.rooms.empty()) {
		out.why = "the plan places no rooms";
		return out;
	}
	if (plan.entrance.room < 0 || static_cast<size_t>(plan.entrance.room) >= plan.rooms.size()) {
		out.why = "the plan has no entrance cell";
		return out;
	}
	const CellRoom c = planCellRoom(plan, unit, plan.entrance.room, rects);
	if (c.room < 0) {
		out.why = c.why == "the cell holds no room" ? "the entrance cell holds no room" : c.why;
		return out;
	}
	out.room = c.room;
	out.cellX = c.cellX;
	out.cellY = c.cellY;
	out.originX = c.originX;
	out.originY = c.originY;
	out.minX = c.minX;
	out.minY = c.minY;
	for (size_t i = 0; i < rects.size(); ++i) {
		if (static_cast<int>(i) != out.room && (rects[i].flags & kArrivalFlags) != 0) out.clear.push_back(static_cast<int>(i));
	}
	out.mark = true;
	return out;
}

} // namespace cartographer::maze
