#include "outdoor_plan.h"

#include <algorithm>
#include <optional>
#include <utility>

#include "outdoor_tilesets.h"
#include "siege_plan.h"
#include "snow_plan.h"

namespace cartographer::outdoor {

std::string capJungleOpenRules(layout::Rules &rules) {
	if (!rules.open) return std::string();
	rules.width = std::min(rules.width, kJungleOpenMaxCells);
	rules.height = std::min(rules.height, kJungleOpenMaxCells);
	const std::string grid = std::to_string(rules.width) + " x " + std::to_string(rules.height);
	const int cells = rules.width * rules.height;
	if (rules.pathMin > cells) {
		return "an Open map on Jungle is at most " + grid + " cells, so MinPath can't be more than " + std::to_string(cells) + ".";
	}
	rules.pathMax = std::min(rules.pathMax, cells);
	std::string problem = layout::validateRules(rules);
	// An Open path is the shortest walk across the grid, so a MinPath that fit the uncapped grid may be out of reach on the capped one.
	for (uint64_t seed = 1; problem.empty() && seed <= 16; ++seed) {
		if (!layout::generate(rules, seed).rulesMet) problem = "MinPath " + std::to_string(rules.pathMin) + " is too long for it.";
	}
	if (!problem.empty()) {
		return "an Open map on Jungle is capped at " + grid + " cells, and " + problem;
	}
	return std::string();
}

namespace {

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
	return findOutdoorTileset(tileset) != nullptr;
}

std::string planCacheKey(const MapRequest &request) {
	const StampOptions &o = request.options;
	std::string key = std::to_string(request.seed);
	key += '|' + lowerAscii(request.tileset) + '|' + lowerAscii(request.shape) + '|' + lowerAscii(request.size);
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
	const OutdoorTileset *tileset = findOutdoorTileset(request.tileset);
	if (tileset == nullptr) {
		return fail("Cartographer has no outdoor pieces for the tileset '" + request.tileset + "'. Use " + outdoorTilesetNames() + ".");
	}
	if (tileset->planner == Planner::Snow) {
		// Snow is lines of fence pieces, not a layout of exit-mask pieces: only the stamps reach the hook and the grid comparison.
		int cellsW = 0, cellsH = 0;
		if (!snow::snowSizeCells(request.size, &cellsW, &cellsH)) {
			return fail("The size '" + request.size + "' is not ExtraSmall, Small, Medium or Large.");
		}
		snow::SnowRequest snowRequest;
		snowRequest.seed = request.seed;
		snowRequest.cellsW = cellsW;
		snowRequest.cellsH = cellsH;
		snowRequest.options.originX = request.options.originX;
		snowRequest.options.originY = request.options.originY;
		snowRequest.options.maxTilesX = request.options.maxTilesX;
		snowRequest.options.maxTilesY = request.options.maxTilesY;
		snowRequest.options.pinFiles = request.options.pinFiles;
		snow::SnowPlan snowPlan = snow::planSnowMap(snowRequest);
		if (!snowPlan.ok) {
			return fail("Cartographer could not plan the Snow map: " + snowPlan.reason);
		}
		out.stamps = std::move(snowPlan.stamps);
		out.hash = mix(0xcbf29ce484222325ull, stampPlanHash(out.stamps));
		out.ok = true;
		return out;
	}
	if (tileset->planner == Planner::Siege) {
		// Siege is one line of 16 x 48 strips: only the stamps reach the hook and the grid comparison, as for Snow.
		siege::SiegeRequest siegeRequest;
		if (!siege::siegeSizeStrips(request.size, &siegeRequest.strips)) {
			return fail("The size '" + request.size + "' is not ExtraSmall, Small, Medium or Large.");
		}
		siegeRequest.seed = request.seed;
		siegeRequest.options.originX = request.options.originX;
		siegeRequest.options.originY = request.options.originY;
		siegeRequest.options.maxTilesX = request.options.maxTilesX;
		siegeRequest.options.maxTilesY = request.options.maxTilesY;
		siegeRequest.options.pinFiles = request.options.pinFiles;
		siege::SiegePlan siegePlan = siege::planSiegeMap(siegeRequest);
		if (!siegePlan.ok) {
			return fail("Cartographer could not plan the Siege map: " + siegePlan.reason);
		}
		out.stamps = std::move(siegePlan.stamps);
		out.hash = mix(0xcbf29ce484222325ull, stampPlanHash(out.stamps));
		out.ok = true;
		return out;
	}
	const std::optional<layout::Size> size = layout::parseSizeName(request.size.empty() ? std::string_view("Medium") : std::string_view(request.size));
	if (!size) {
		return fail("The size '" + request.size + "' is not ExtraSmall, Small, Medium or Large.");
	}
	const layout::ShapeTable none;
	layout::Rules rules;
	std::string error;
	if (!layout::resolveRules(request.shapes != nullptr ? *request.shapes : none, request.shape.empty() ? std::string_view("Winding") : std::string_view(request.shape), *size, rules, error)) {
		return fail("The shape '" + request.shape + "' cannot be used: " + error);
	}
	// An Open map stamps every block (no void), so a Large one is 144 blocks, 2304 rooms: 6 to 8 times the other Jungle maps. Cap it at
	// the 9 x 9 grid of Medium (81 blocks, 1296 rooms). The cap applies to any outdoor Open layout this planner builds, and the capped
	// rules are checked again (maps.txt checks them at load with capJungleOpenRules).
	if (const std::string capped = capJungleOpenRules(rules); !capped.empty()) {
		return fail("The shape '" + request.shape + "' cannot be used: " + capped);
	}
	out.layout = layout::generate(rules, request.seed);
	out.picks = pickPieces(out.layout, catalogue, tileset->name, request.seed);
	if (!out.picks.ok()) {
		return fail("Cartographer could not pick " + std::string(tileset->name) + " pieces for the layout: " + joinErrors(out.picks.errors));
	}
	if (const std::vector<std::string> bad = checkPicks(out.layout, catalogue, tileset->name, out.picks); !bad.empty()) {
		return fail("The " + std::string(tileset->name) + " pieces picked do not fit the layout: " + joinErrors(bad));
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

bool pickFarthestCell(const std::vector<GridCell> &candidates, const std::vector<GridCell> &anchors, layout::Pcg32 &rng, GridCell &out) {
	if (anchors.empty()) return false;  // nothing to be far from: no draw from rng, `out` untouched
	int top = -1, ties = 0;
	bool found = false;
	for (const GridCell &c : candidates) {
		int v = -1;  // squared distance to the nearest anchor
		for (const GridCell &a : anchors) {
			const int d = (c.x - a.x) * (c.x - a.x) + (c.y - a.y) * (c.y - a.y);
			if (v < 0 || d < v) v = d;
		}
		if (v > top) {
			top = v;
			ties = 1;
			out = c;
			found = true;
		} else if (v == top && rng.below(static_cast<uint32_t>(++ties)) == 0) {
			out = c;
		}
	}
	return found;
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
	const int macroGridX = plan.macroW > 0 ? plan.cellsW / plan.macroW : 0; // grid cells per macro-cell, across and down (a Siege strip is 2 x 6)
	const int macroGridY = plan.macroH > 0 ? plan.cellsH / plan.macroH : 0;
	for (const Stamp &s : plan.stamps) {
		if (s.cx < 0 || s.cy < 0 || s.cx >= plan.cellsW || s.cy >= plan.cellsH) {
			problems.push_back("a stamp at grid cell (" + std::to_string(s.cx) + "," + std::to_string(s.cy) + ") is outside the grid");
			continue;
		}
		const size_t at = static_cast<size_t>(s.cy) * plan.cellsW + s.cx;
		if (grid.piece[at] == 0) {
			problems.push_back("the stamp for piece " + std::to_string(s.lvlPrest) + " at grid cell (" + std::to_string(s.cx) + "," + std::to_string(s.cy) + ") left no piece there");
		}
		for (int dy = 0; dy < macroGridY; ++dy) {
			for (int dx = 0; dx < macroGridX; ++dx) {
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
