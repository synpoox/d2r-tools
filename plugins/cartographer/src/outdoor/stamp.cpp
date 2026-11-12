#include "stamp.h"

#include <algorithm>

namespace cartographer::outdoor {

namespace {

constexpr int kJungleFirst = 530; // 0x211 + 1, Jungle W
constexpr int kJungleLast = 544;  // 0x211 + 15, Jungle NSEW

bool inJungleMaskRange(int lvlPrest)
{
	return lvlPrest >= kJungleFirst && lvlPrest <= kJungleLast;
}

TileRect macroRect(const StampPlan &plan, const StampOptions &o, int macroX, int macroY)
{
	TileRect r;
	r.known = true;
	r.x = o.originX + (macroX - plan.macroMinX) * o.macroTiles;
	r.y = o.originY + (macroY - plan.macroMinY) * o.macroTiles;
	r.w = o.macroTiles;
	r.h = o.macroTiles;
	return r;
}

void setRect(const layout::Layout &layout, const StampPlan &plan, const StampOptions &o, layout::Role role, TileRect &out, bool &duplicate)
{
	bool found = false;
	for (int y = 0; y < layout.height; ++y) {
		for (int x = 0; x < layout.width; ++x) {
			if (layout.at(x, y).mask == 0 || layout.at(x, y).role != role) continue;
			if (found) duplicate = true;
			if (!found) out = macroRect(plan, o, x, y);
			found = true;
		}
	}
}

std::string where(int x, int y)
{
	return "cell (" + std::to_string(x) + "," + std::to_string(y) + ")";
}

} // namespace

int jungleLvlPrest(uint8_t mask)
{
	if (mask == 0 || mask > 15) return -1;
	return 0x211 + mask;
}

StampPlan planStamps(const layout::Layout &layout, const PickResult &picks, const StampOptions &o)
{
	StampPlan plan;
	plan.originX = o.originX;
	plan.originY = o.originY;
	auto fail = [&plan](const std::string &why) {
		plan.errors.push_back(why);
	};

	if (o.gridTiles <= 0 || o.macroTiles <= 0 || o.macroTiles % o.gridTiles != 0) {
		fail("the macro-cell size must be a whole number of grid cells (" + std::to_string(o.macroTiles) + " tiles over " + std::to_string(o.gridTiles) + ").");
		return plan;
	}
	if (!picks.ok()) {
		fail("the piece picks are incomplete (" + picks.errors.front() + "), so the map cannot be stamped.");
		return plan;
	}
	if (picks.width != layout.width || picks.height != layout.height || picks.cells.size() != layout.cells.size()) {
		fail("the piece picks were made for a different layout.");
		return plan;
	}

	int minX = layout.width, minY = layout.height, maxX = -1, maxY = -1;
	for (int y = 0; y < layout.height; ++y) {
		for (int x = 0; x < layout.width; ++x) {
			if (layout.at(x, y).mask == 0) continue;
			minX = std::min(minX, x);
			minY = std::min(minY, y);
			maxX = std::max(maxX, x);
			maxY = std::max(maxY, y);
		}
	}
	if (maxX < 0) {
		fail("the layout has no cell, so there is nothing to stamp.");
		return plan;
	}
	plan.macroMinX = minX;
	plan.macroMinY = minY;
	plan.macroW = maxX - minX + 1;
	plan.macroH = maxY - minY + 1;
	plan.tilesW = plan.macroW * o.macroTiles;
	plan.tilesH = plan.macroH * o.macroTiles;
	if (o.maxMacroCells > 0 && (plan.macroW > o.maxMacroCells || plan.macroH > o.maxMacroCells)) {
		fail("the map is " + std::to_string(plan.macroW) + " x " + std::to_string(plan.macroH) + " blocks, more than the " +
			std::to_string(o.maxMacroCells) + " x " + std::to_string(o.maxMacroCells) + " the outdoor builder allows.");
	}
	if ((o.maxTilesX > 0 && plan.tilesW > o.maxTilesX) || (o.maxTilesY > 0 && plan.tilesH > o.maxTilesY)) {
		fail("the map needs " + std::to_string(plan.tilesW) + " x " + std::to_string(plan.tilesH) + " tiles, more than the slot level's " +
			std::to_string(o.maxTilesX) + " x " + std::to_string(o.maxTilesY) + ".");
	}
	if (!plan.ok()) return plan;

	const int per = o.macroTiles / o.gridTiles; // grid cells per macro-cell side
	plan.cellsW = plan.macroW * per;
	plan.cellsH = plan.macroH * per;

	for (int y = minY; y <= maxY; ++y) {
		for (int x = minX; x <= maxX; ++x) {
			const layout::Cell &cell = layout.at(x, y);
			if (cell.mask == 0) continue;
			const PickedCell &pick = picks.at(x, y);
			if (pick.lvlPrest < 0) {
				fail(where(x, y) + " has no piece.");
				continue;
			}
			if (cell.mask > 15) {
				fail(where(x, y) + " has an exit mask over 15.");
				continue;
			}
			if (inJungleMaskRange(pick.lvlPrest) && pick.lvlPrest != jungleLvlPrest(cell.mask)) {
				fail(where(x, y) + " has exits " + maskName(cell.mask) + " but piece " + std::to_string(pick.lvlPrest) + ", which breaks the jungle edge rule.");
				continue;
			}
			Stamp s;
			s.cx = (x - minX) * per;
			s.cy = (y - minY) * per;
			s.lvlPrest = pick.lvlPrest;
			s.file = (o.pinFiles && pick.ds1Variant >= 0) ? pick.ds1Variant : -1;
			s.macroX = x;
			s.macroY = y;
			s.mask = cell.mask;
			s.role = cell.role;
			plan.stamps.push_back(s);
		}
	}
	if (!plan.ok()) {
		plan.stamps.clear();
		return plan;
	}

	bool dupEntrance = false, dupEvent = false, dupBoss = false;
	setRect(layout, plan, o, layout::Role::Entrance, plan.entrance, dupEntrance);
	setRect(layout, plan, o, layout::Role::Event, plan.event, dupEvent);
	setRect(layout, plan, o, layout::Role::Boss, plan.boss, dupBoss);
	if (!plan.entrance.known) fail("the layout has no entrance cell.");
	if (dupEntrance) fail("the layout has more than one entrance cell.");
	if (dupEvent) fail("the layout has more than one event cell.");
	if (dupBoss) fail("the layout has more than one boss cell.");
	if (!plan.ok()) {
		plan.stamps.clear();
		plan.entrance = plan.event = plan.boss = TileRect{};
		return plan;
	}

	if (o.listVoid) {
		std::vector<char> covered(static_cast<size_t>(plan.cellsW) * plan.cellsH, 0);
		for (const Stamp &s : plan.stamps)
			for (int dy = 0; dy < per; ++dy)
				for (int dx = 0; dx < per; ++dx) covered[static_cast<size_t>(s.cy + dy) * plan.cellsW + (s.cx + dx)] = 1;
		for (int cy = 0; cy < plan.cellsH; ++cy)
			for (int cx = 0; cx < plan.cellsW; ++cx)
				if (!covered[static_cast<size_t>(cy) * plan.cellsW + cx]) plan.voidCells.push_back({cx, cy});
	}
	return plan;
}

std::vector<std::string> checkStampPlan(const layout::Layout &layout, const PickResult &picks, const StampOptions &o, const StampPlan &plan)
{
	std::vector<std::string> bad;
	if (o.gridTiles <= 0 || o.macroTiles <= 0 || o.macroTiles % o.gridTiles != 0) {
		bad.push_back("bad cell sizes in the options");
		return bad;
	}
	const int per = o.macroTiles / o.gridTiles;
	if (plan.cellsW != plan.macroW * per || plan.cellsH != plan.macroH * per || plan.tilesW != plan.macroW * o.macroTiles ||
		plan.tilesH != plan.macroH * o.macroTiles) {
		bad.push_back("the level size does not match the cropped box");
		return bad;
	}
	if (plan.tilesW % o.gridTiles != 0 || plan.tilesH % o.gridTiles != 0) bad.push_back("the level size is not a whole number of grid cells");

	int used = 0;
	for (int y = 0; y < layout.height; ++y)
		for (int x = 0; x < layout.width; ++x)
			if (layout.at(x, y).mask != 0) {
				++used;
				if (x < plan.macroMinX || y < plan.macroMinY || x >= plan.macroMinX + plan.macroW || y >= plan.macroMinY + plan.macroH)
					bad.push_back(where(x, y) + " lies outside the cropped box");
			}
	if (static_cast<int>(plan.stamps.size()) != used) bad.push_back("the plan has " + std::to_string(plan.stamps.size()) + " stamps for " + std::to_string(used) + " used cells");

	std::vector<int> owner(static_cast<size_t>(plan.cellsW) * plan.cellsH, 0);
	int lastRow = -1, lastCol = -1;
	for (const Stamp &s : plan.stamps) {
		const std::string at = where(s.macroX, s.macroY);
		if (!layout.inside(s.macroX, s.macroY) || layout.at(s.macroX, s.macroY).mask == 0) {
			bad.push_back("a stamp is for " + at + ", which is empty or outside the layout");
			continue;
		}
		if (s.mask != layout.at(s.macroX, s.macroY).mask || s.role != layout.at(s.macroX, s.macroY).role) bad.push_back(at + " stamp has the wrong mask or role");
		if (s.cx != (s.macroX - plan.macroMinX) * per || s.cy != (s.macroY - plan.macroMinY) * per) bad.push_back(at + " is not aligned to the macro-cell grid");
		if (s.lvlPrest != picks.at(s.macroX, s.macroY).lvlPrest) bad.push_back(at + " does not use its picked piece");
		if (s.lvlPrest < 0 || s.lvlPrest == 529) bad.push_back(at + " has no piece or the Act 3 town piece");
		if (inJungleMaskRange(s.lvlPrest) && s.lvlPrest != jungleLvlPrest(s.mask)) bad.push_back(at + " breaks the jungle edge rule");
		if (s.file < -1 || s.file > 15) bad.push_back(at + " has a file index the 4-bit flag cannot hold");
		if (s.macroY < lastRow || (s.macroY == lastRow && s.macroX <= lastCol)) bad.push_back(at + " is out of row-major order");
		lastRow = s.macroY;
		lastCol = s.macroX;
		for (int dy = 0; dy < per; ++dy)
			for (int dx = 0; dx < per; ++dx) {
				const int cx = s.cx + dx, cy = s.cy + dy;
				if (cx < 0 || cy < 0 || cx >= plan.cellsW || cy >= plan.cellsH) {
					bad.push_back(at + " reaches outside the level");
					continue;
				}
				++owner[static_cast<size_t>(cy) * plan.cellsW + cx];
			}
	}
	for (int v : owner)
		if (v > 1) {
			bad.push_back("two stamps cover the same grid cell");
			break;
		}

	size_t next = 0;
	for (int cy = 0; cy < plan.cellsH; ++cy)
		for (int cx = 0; cx < plan.cellsW; ++cx) {
			if (owner[static_cast<size_t>(cy) * plan.cellsW + cx] != 0 || !o.listVoid) continue;
			if (next >= plan.voidCells.size() || plan.voidCells[next] != layout::Point{cx, cy}) {
				bad.push_back("grid cell (" + std::to_string(cx) + "," + std::to_string(cy) + ") is not stamped and not listed void");
				return bad;
			}
			++next;
		}
	if (o.listVoid && next != plan.voidCells.size()) bad.push_back("the void list holds cells that are stamped");
	if (!o.listVoid && !plan.voidCells.empty()) bad.push_back("a void list was made though it was not asked for");

	struct Want { layout::Role role; const TileRect *rect; const char *name; };
	for (const Want &w : {Want{layout::Role::Entrance, &plan.entrance, "entrance"}, Want{layout::Role::Event, &plan.event, "event"}, Want{layout::Role::Boss, &plan.boss, "boss"}}) {
		int count = 0;
		for (int y = 0; y < layout.height; ++y)
			for (int x = 0; x < layout.width; ++x) {
				if (layout.at(x, y).mask == 0 || layout.at(x, y).role != w.role) continue;
				++count;
				const TileRect want = macroRect(plan, o, x, y);
				if (!w.rect->known || w.rect->x != want.x || w.rect->y != want.y || w.rect->w != want.w || w.rect->h != want.h)
					bad.push_back(std::string("the ") + w.name + " rectangle is not on " + where(x, y));
			}
		if (count == 0 && w.rect->known) bad.push_back(std::string("the plan has an ") + w.name + " rectangle but the layout has no such cell");
	}
	return bad;
}

uint64_t stampPlanHash(const StampPlan &plan)
{
	uint64_t h = 1469598103934665603ull;
	auto mix = [&h](int64_t v) {
		const uint64_t u = static_cast<uint64_t>(v);
		for (int i = 0; i < 8; ++i) {
			h ^= (u >> (8 * i)) & 0xFFu;
			h *= 1099511628211ull;
		}
	};
	mix(plan.tilesW);
	mix(plan.tilesH);
	for (const Stamp &s : plan.stamps) {
		mix(s.cx);
		mix(s.cy);
		mix(s.lvlPrest);
		mix(s.file);
	}
	mix(-1);
	for (const layout::Point &p : plan.voidCells) {
		mix(p.x);
		mix(p.y);
	}
	return h;
}

std::string stampPlanAscii(const StampPlan &plan)
{
	std::vector<char> grid(static_cast<size_t>(plan.cellsW) * plan.cellsH, '.');
	const int per = plan.macroW > 0 ? plan.cellsW / plan.macroW : 0;
	for (const Stamp &s : plan.stamps)
		for (int dy = 0; dy < per; ++dy)
			for (int dx = 0; dx < per; ++dx) grid[static_cast<size_t>(s.cy + dy) * plan.cellsW + (s.cx + dx)] = '#';
	std::string out;
	for (int cy = 0; cy < plan.cellsH; ++cy) {
		out.append(grid.begin() + static_cast<std::ptrdiff_t>(cy) * plan.cellsW, grid.begin() + static_cast<std::ptrdiff_t>(cy + 1) * plan.cellsW);
		out += '\n';
	}
	return out;
}

} // namespace cartographer::outdoor
