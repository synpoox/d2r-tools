#include "outdoor_plan.h"

#include <cctype>
#include <optional>
#include <utility>

namespace cartographer::outdoor {

namespace {

std::string lower(std::string_view text) {
	std::string out(text);
	for (char &c : out) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
	return out;
}

std::string joinErrors(const std::vector<std::string> &errors) {
	std::string out = errors.empty() ? std::string("unknown problem") : errors.front();
	if (errors.size() > 1) out += " (and " + std::to_string(errors.size() - 1) + " more)";
	return out;
}

uint64_t mix(uint64_t h, uint64_t v) {
	for (int i = 0; i < 8; ++i) {
		h ^= (v >> (i * 8)) & 0xffu;
		h *= 0x100000001b3ull;
	}
	return h;
}

} // namespace

const Catalogue &builtinJungleCatalogue() {
	static const Catalogue catalogue = parseCatalogue(builtinJungleCatalogueText(), "the built-in Jungle catalogue").catalogue;
	return catalogue;
}

bool outdoorTilesetKnown(std::string_view tileset) {
	return lower(tileset) == "jungle";
}

std::string planCacheKey(const MapRequest &request) {
	const StampOptions &o = request.options;
	std::string key = std::to_string(request.seed);
	key += '|' + lower(request.tileset) + '|' + lower(request.shape) + '|' + lower(request.size);
	key += '|' + std::to_string(o.macroTiles) + ',' + std::to_string(o.gridTiles) + ',' + std::to_string(o.originX) + ',' + std::to_string(o.originY);
	key += ',' + std::to_string(o.maxMacroCells) + ',' + std::to_string(o.maxTilesX) + ',' + std::to_string(o.maxTilesY);
	key += o.pinFiles ? ",pin" : ",free";
	key += o.listVoid ? ",void" : ",fill";
	return key;
}

MapPlan planOutdoorMap(const MapRequest &request, const Catalogue &catalogue) {
	MapPlan out;
	out.cacheKey = planCacheKey(request);
	const auto fail = [&out](std::string why) {
		out.ok = false;
		out.reason = std::move(why);
		return out;
	};
	if (!outdoorTilesetKnown(request.tileset)) {
		return fail("Cartographer has no outdoor pieces for the tileset '" + request.tileset + "'. Use Jungle.");
	}
	const std::optional<layout::Size> size = layout::parseSizeName(request.size.empty() ? std::string_view("Medium") : std::string_view(request.size));
	if (!size) {
		return fail("The size '" + request.size + "' is not Small, Medium or Large.");
	}
	const layout::ShapeTable none;
	layout::Rules rules;
	std::string error;
	if (!layout::resolveRules(request.shapes != nullptr ? *request.shapes : none, request.shape.empty() ? std::string_view("Winding") : std::string_view(request.shape), *size, rules, error)) {
		return fail("The shape '" + request.shape + "' cannot be used: " + error);
	}
	out.layout = layout::generate(rules, request.seed);
	out.picks = pickPieces(out.layout, catalogue, "Jungle", request.seed);
	if (!out.picks.ok()) {
		return fail("Cartographer could not pick Jungle pieces for the layout: " + joinErrors(out.picks.errors));
	}
	if (const std::vector<std::string> bad = checkPicks(out.layout, catalogue, "Jungle", out.picks); !bad.empty()) {
		return fail("The Jungle pieces picked do not fit the layout: " + joinErrors(bad));
	}
	out.stamps = planStamps(out.layout, out.picks, request.options);
	if (!out.stamps.ok()) {
		return fail("Cartographer could not plan the level: " + joinErrors(out.stamps.errors));
	}
	if (const std::vector<std::string> bad = checkStampPlan(out.layout, out.picks, request.options, out.stamps); !bad.empty()) {
		return fail("The stamp plan failed its own check: " + joinErrors(bad));
	}
	uint64_t h = 0xcbf29ce484222325ull;
	h = mix(h, layout::layoutHash(out.layout));
	h = mix(h, picksHash(out.picks));
	h = mix(h, stampPlanHash(out.stamps));
	out.hash = h;
	out.ok = true;
	return out;
}

std::shared_ptr<const MapPlan> PlanCache::get(const MapRequest &request, const Catalogue &catalogue, bool *built) {
	const std::string key = planCacheKey(request);
	{
		std::lock_guard<std::mutex> guard(lock_);
		const auto it = plans_.find(key);
		if (it != plans_.end()) {
			if (built != nullptr) *built = false;
			return it->second;
		}
	}
	// Computed outside the lock: two threads asking for the same new plan both build it, and the second one's result is dropped.
	auto plan = std::make_shared<const MapPlan>(planOutdoorMap(request, catalogue));
	std::lock_guard<std::mutex> guard(lock_);
	const auto it = plans_.find(key);
	if (it != plans_.end()) {
		if (built != nullptr) *built = false;
		return it->second;
	}
	plans_[key] = plan;
	order_.push_back(key);
	while (order_.size() > capacity_) {
		plans_.erase(order_.front());
		order_.erase(order_.begin());
	}
	if (built != nullptr) *built = true;
	return plan;
}

void PlanCache::clear() {
	std::lock_guard<std::mutex> guard(lock_);
	plans_.clear();
	order_.clear();
}

size_t PlanCache::size() const {
	std::lock_guard<std::mutex> guard(lock_);
	return plans_.size();
}

LevelRect levelRectOf(const StampPlan &plan) {
	return {plan.originX, plan.originY, plan.tilesW, plan.tilesH};
}

std::vector<std::string> compareGrid(const StampPlan &plan, const GridSample &grid, GridCounts *counts) {
	std::vector<std::string> problems;
	const size_t n = static_cast<size_t>(plan.cellsW) * static_cast<size_t>(plan.cellsH);
	if (plan.cellsW <= 0 || plan.cellsH <= 0 || grid.piece.size() != n || grid.flags.size() != n) {
		problems.push_back("the grid holds " + std::to_string(grid.piece.size()) + " cells, the plan expects " + std::to_string(plan.cellsW) + " x " + std::to_string(plan.cellsH));
		return problems;
	}
	constexpr uint64_t Covered = 0x200, Void = 0x100;
	GridCounts c;
	c.cells = static_cast<int>(n);
	for (size_t i = 0; i < n; ++i) {
		const bool piece = grid.piece[i] != 0;
		const bool covered = (grid.flags[i] & Covered) != 0;
		const bool voided = (grid.flags[i] & Void) != 0;
		c.pieces += piece ? 1 : 0;
		c.covered += covered ? 1 : 0;
		c.voided += voided ? 1 : 0;
		c.other += (!piece && !covered && !voided) ? 1 : 0;
	}
	// A stamped macro-cell: its origin grid cell holds a piece; no cell inside it is void.
	const int macroGrid = plan.macroW > 0 ? plan.cellsW / plan.macroW : 0; // grid cells per macro-cell side
	for (const Stamp &s : plan.stamps) {
		if (s.cx < 0 || s.cy < 0 || s.cx >= plan.cellsW || s.cy >= plan.cellsH) {
			problems.push_back("a stamp at grid cell (" + std::to_string(s.cx) + "," + std::to_string(s.cy) + ") is outside the grid");
			continue;
		}
		const size_t at = static_cast<size_t>(s.cy) * plan.cellsW + s.cx;
		if (grid.piece[at] == 0) {
			problems.push_back("the stamp for piece " + std::to_string(s.lvlPrest) + " at grid cell (" + std::to_string(s.cx) + "," + std::to_string(s.cy) + ") left no piece there");
		}
		for (int dy = 0; dy < macroGrid; ++dy) {
			for (int dx = 0; dx < macroGrid; ++dx) {
				const int x = s.cx + dx, y = s.cy + dy;
				if (x >= plan.cellsW || y >= plan.cellsH) continue;
				if ((grid.flags[static_cast<size_t>(y) * plan.cellsW + x] & Void) != 0) {
					problems.push_back("grid cell (" + std::to_string(x) + "," + std::to_string(y) + ") is void inside the stamped piece " + std::to_string(s.lvlPrest));
				}
			}
		}
	}
	for (const layout::Point &p : plan.voidCells) {
		if (p.x < 0 || p.y < 0 || p.x >= plan.cellsW || p.y >= plan.cellsH) {
			problems.push_back("a planned void cell (" + std::to_string(p.x) + "," + std::to_string(p.y) + ") is outside the grid");
			continue;
		}
		const size_t at = static_cast<size_t>(p.y) * plan.cellsW + p.x;
		if ((grid.flags[at] & Void) == 0 || grid.piece[at] != 0) {
			problems.push_back("grid cell (" + std::to_string(p.x) + "," + std::to_string(p.y) + ") should be void and is not");
		}
	}
	if (counts != nullptr) *counts = c;
	return problems;
}

int chooseCellRoom(const TileRect &cell, const std::vector<BuiltRoom> &rooms) {
	if (!cell.known || cell.w <= 0 || cell.h <= 0) return -1;
	// Twice the coordinates, so the centres are whole numbers.
	const int64_t cx = 2 * static_cast<int64_t>(cell.x) + cell.w, cy = 2 * static_cast<int64_t>(cell.y) + cell.h;
	int best = -1;
	int64_t bestDist = 0;
	for (size_t i = 0; i < rooms.size(); ++i) {
		const BuiltRoom &r = rooms[i];
		if (r.w <= 0 || r.h <= 0 || r.x < cell.x || r.y < cell.y || r.x + r.w > cell.x + cell.w || r.y + r.h > cell.y + cell.h) continue;
		const int64_t dx = 2 * static_cast<int64_t>(r.x) + r.w - cx, dy = 2 * static_cast<int64_t>(r.y) + r.h - cy;
		const int64_t d = dx * dx + dy * dy;
		bool better = best < 0 || d < bestDist;
		if (!better && d == bestDist) {
			const BuiltRoom &b = rooms[static_cast<size_t>(best)];
			better = r.y < b.y || (r.y == b.y && r.x < b.x);
		}
		if (better) {
			best = static_cast<int>(i);
			bestDist = d;
		}
	}
	return best;
}

ArrivalMark planArrival(const StampPlan &plan, const std::vector<BuiltRoom> &rooms) {
	ArrivalMark out;
	if (!plan.entrance.known) {
		out.why = "the plan has no entrance cell";
		return out;
	}
	if (rooms.empty()) {
		out.why = "the build made no rooms";
		return out;
	}
	for (const BuiltRoom &r : rooms) {
		if (r.x < plan.originX || r.y < plan.originY || r.x + r.w > plan.originX + plan.tilesW || r.y + r.h > plan.originY + plan.tilesH) {
			out.why = "a room at (" + std::to_string(r.x) + "," + std::to_string(r.y) + ") lies outside the plan's level rectangle";
			return out;
		}
	}
	const int room = chooseCellRoom(plan.entrance, rooms);
	if (room < 0) {
		out.why = "the entrance cell holds no room";
		return out;
	}
	out.mark = true;
	out.room = room;
	for (size_t i = 0; i < rooms.size(); ++i) {
		if (static_cast<int>(i) != room && (rooms[i].flags & kArrivalFlags) != 0) out.clear.push_back(static_cast<int>(i));
	}
	return out;
}

} // namespace cartographer::outdoor
