// Cartographer maze steering, the pure part of the hooks. See steer.h.
#include "steer.h"

#include <algorithm>
#include <optional>

#include "rng.h"

namespace cartographer::maze {

namespace {

char lower(char c) { return c >= 'A' && c <= 'Z' ? static_cast<char>(c + 32) : c; }

bool sameNoCase(std::string_view a, std::string_view b)
{
	if (a.size() != b.size()) return false;
	for (size_t i = 0; i < a.size(); ++i) {
		if (lower(a[i]) != lower(b[i])) return false;
	}
	return true;
}

} // namespace

bool builtinTileset(std::string_view name, Tileset &out)
{
	Tileset t;
	if (sameNoCase(name, "Cave")) {
		t.name = "Cave";
		t.levelType = 3;
		t.roomTiles = 24;
		t.pieceBase = 52;
		t.prev = {true, {83, 84, 85, 86}}; // Prev W, E, S, N
	} else if (sameNoCase(name, "Crypt")) {
		t.name = "Crypt";
		t.levelType = 4;
		t.roomTiles = 8;
		t.pieceBase = 108;
		t.prev = {true, {139, 140, 141, 142}};
	} else {
		return false;
	}
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
	if (slotLevelType < 0) {
		return "Cartographer: could not read the LevelType of the Maze slot level from the mod's levels.txt, so " + who +
		       " is not opened. Check that the Maze row in slots.txt names a level that exists in levels.txt (see the Cartographer README).";
	}
	const std::string slotIs = slotLevelType == 3   ? "a Cave level (LevelType 3)"
	                           : slotLevelType == 4 ? "a Crypt level (LevelType 4)"
	                                                : "LevelType " + std::to_string(slotLevelType) + ", neither Cave (3) nor Crypt (4)";
	return "Cartographer: the Maze slot level is " + slotIs + ", so it cannot build " + who + " (needs LevelType " + std::to_string(tileset.levelType) +
	       "). One Maze slot serves one tileset: use a map of that tileset, or give the slot a level of the other type (see the Cartographer README).";
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
		out.reason = "Cartographer has no maze numbers for the tileset '" + std::string(tileset) + "'. Use Cave or Crypt.";
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
	const std::optional<layout::Size> sz = layout::parseSizeName(size.empty() ? std::string_view("Medium") : size);
	if (!sz) {
		out.reason = "The size '" + std::string(size) + "' is not Small, Medium or Large.";
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

} // namespace cartographer::maze
