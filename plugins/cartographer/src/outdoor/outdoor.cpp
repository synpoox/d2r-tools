#include "outdoor.h"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <utility>

#include "rng.h"

namespace cartographer::outdoor {

namespace {

using layout::kEast;
using layout::kNorth;
using layout::kSouth;
using layout::kWest;

// Edge order of Piece::edge and its mask bits.
constexpr uint8_t kEdgeBit[4] = {kNorth, kEast, kSouth, kWest};
constexpr int dxOf[4] = {0, 1, 0, -1}; // N E S W
constexpr int kMaxWeight = 1000000; // keeps the weight total in 64 bits for any catalogue size
constexpr int dyOf[4] = {-1, 0, 1, 0};

std::string trim(std::string_view s)
{
	size_t a = 0, b = s.size();
	while (a < b && (s[a] == ' ' || s[a] == '\t' || s[a] == '\r')) ++a;
	while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r')) --b;
	return std::string(s.substr(a, b - a));
}

std::vector<std::string> split(std::string_view s, char sep)
{
	std::vector<std::string> out;
	size_t start = 0;
	for (;;) {
		const size_t p = s.find(sep, start);
		if (p == std::string_view::npos) {
			out.push_back(std::string(s.substr(start)));
			return out;
		}
		out.push_back(std::string(s.substr(start, p - start)));
		start = p + 1;
	}
}

bool parseInt(const std::string &text, int &out)
{
	const std::string s = trim(text);
	if (s.empty() || s.size() > 9) return false;
	size_t i = 0;
	bool neg = false;
	if (s[0] == '-') {
		neg = true;
		i = 1;
		if (s.size() == 1) return false;
	}
	int v = 0;
	for (; i < s.size(); ++i) {
		if (s[i] < '0' || s[i] > '9') return false;
		v = v * 10 + (s[i] - '0');
	}
	out = neg ? -v : v;
	return true;
}

bool parseRole(const std::string &text, PieceRole &out)
{
	const std::string s = lowerAscii(trim(text));
	static const std::pair<const char *, PieceRole> names[] = {
		{"path", PieceRole::Path}, {"clearing", PieceRole::Clearing}, {"entry", PieceRole::Entry},
		{"boss", PieceRole::Boss}, {"event", PieceRole::Event}, {"filler", PieceRole::Filler}, {"border", PieceRole::Border}};
	for (const auto &n : names)
		if (s == n.first) {
			out = n.second;
			return true;
		}
	return false;
}

bool validSocket(const std::string &s, int len)
{
	if (static_cast<int>(s.size()) != len) return false;
	for (char c : s)
		if (c != 'x' && c != 'p' && c != 'c' && c != 'r') return false;
	return true;
}

std::string cellAt(const std::vector<std::string> &row, int idx)
{
	return idx >= 0 && idx < static_cast<int>(row.size()) ? trim(row[static_cast<size_t>(idx)]) : std::string();
}

// The role groups a cell may draw from, best first.
struct Tiers {
	PieceRole role[3];
	int count = 0;
};

Tiers tiersFor(layout::Role r)
{
	using R = PieceRole;
	switch (r) {
	case layout::Role::Entrance: return {{R::Entry, R::Path, R::Path}, 2};
	case layout::Role::Event: return {{R::Event, R::Clearing, R::Path}, 3};
	case layout::Role::Boss: return {{R::Boss, R::Clearing, R::Path}, 3};
	default: return {{R::Path, R::Path, R::Path}, 1};
	}
}

const char *roomName(layout::Role r)
{
	switch (r) {
	case layout::Role::Entrance: return "entrance";
	case layout::Role::Event: return "event room";
	case layout::Role::Boss: return "boss room";
	case layout::Role::Branch: return "side room";
	default: return "path cell";
	}
}

bool pickable(const Piece &p, const std::string &tilesetLower)
{
	const bool checked = p.check.empty() || p.check == "ok" || p.check == "n/a" || p.check == "variants";
	return checked && p.w == 1 && p.h == 1 && p.weight > 0 && p.role != PieceRole::Border && p.role != PieceRole::Filler &&
		lowerAscii(p.tileset) == tilesetLower;
}

// An exit needs a path socket, no exit needs a closed one.
bool socketsMatch(const Piece &p, uint8_t mask)
{
	for (int d = 0; d < 4; ++d) {
		const char c = p.edge[d][0];
		if (((mask & kEdgeBit[d]) != 0) ? c != 'p' : c != 'x') return false;
	}
	return true;
}

} // namespace

std::string lowerAscii(std::string_view s)
{
	std::string out(s);
	for (char &c : out)
		if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
	return out;
}

std::string cellName(int x, int y)
{
	return "cell (" + std::to_string(x) + "," + std::to_string(y) + ")";
}

const char *pieceRoleName(PieceRole role)
{
	switch (role) {
	case PieceRole::Path: return "path";
	case PieceRole::Clearing: return "clearing";
	case PieceRole::Entry: return "entry";
	case PieceRole::Boss: return "boss";
	case PieceRole::Event: return "event";
	case PieceRole::Filler: return "filler";
	case PieceRole::Border: return "border";
	}
	return "path";
}

std::string maskName(uint8_t mask)
{
	std::string s;
	if (mask & kNorth) s += 'N';
	if (mask & kSouth) s += 'S';
	if (mask & kEast) s += 'E';
	if (mask & kWest) s += 'W';
	return s.empty() ? "no exits" : s;
}

LoadResult parseCatalogue(std::string_view text, std::string_view fileName)
{
	LoadResult res;
	const std::string file(fileName);
	auto warn = [&](int line, const std::string &msg) {
		res.warnings.push_back(file + " line " + std::to_string(line) + ": " + msg);
	};

	enum { Tileset, Builder, LevelType, LvlPrest, Name, W, H, EdgeN, EdgeE, EdgeS, EdgeW, Role, Weight, Tags, Ds1, Check, kCols };
	static const char *colNames[kCols] = {"tileset", "builder", "leveltype", "lvlprest", "name", "w", "h", "edgen", "edgee",
		"edges", "edgew", "role", "weight", "tags", "ds1", "check"};
	static const char *colShown[kCols] = {"tileset", "builder", "leveltype", "lvlprest", "name", "w", "h", "edgeN", "edgeE",
		"edgeS", "edgeW", "role", "weight", "tags", "ds1", "check"};
	int col[kCols];
	for (int &c : col) c = -1;

	bool haveHeader = false;
	int lineNo = 0;
	size_t pos = 0;
	while (pos <= text.size()) {
		size_t end = text.find('\n', pos);
		if (end == std::string_view::npos) end = text.size();
		std::string line(text.substr(pos, end - pos));
		pos = end + 1;
		++lineNo;
		if (lineNo == 1 && line.size() >= 3 && line.compare(0, 3, "﻿") == 0) line.erase(0, 3); // UTF-8 byte order mark
		if (trim(line).empty() || line[0] == '#') continue;
		const std::vector<std::string> row = split(line, '\t');

		if (!haveHeader) {
			haveHeader = true;
			for (size_t i = 0; i < row.size(); ++i) {
				const std::string n = lowerAscii(trim(row[i]));
				for (int k = 0; k < kCols; ++k)
					if (n == colNames[k] && col[k] < 0) col[k] = static_cast<int>(i);
			}
			std::string missing;
			for (int k : {int(Tileset), int(LvlPrest), int(W), int(H), int(EdgeN), int(EdgeE), int(EdgeS), int(EdgeW), int(Role), int(Weight)})
				if (col[k] < 0) missing += (missing.empty() ? "" : ", ") + std::string(colShown[k]);
			if (!missing.empty()) {
				warn(lineNo, "the header is missing the column(s) " + missing + ", so no piece can be read");
				return res;
			}
			continue;
		}

		Piece p;
		p.tileset = cellAt(row, col[Tileset]);
		p.builder = cellAt(row, col[Builder]);
		p.name = cellAt(row, col[Name]);
		p.tags = cellAt(row, col[Tags]);
		if (p.tileset.empty()) {
			warn(lineNo, "no tileset name; row skipped");
			continue;
		}
		if (!parseInt(cellAt(row, col[LvlPrest]), p.lvlPrest) || p.lvlPrest < 0) {
			warn(lineNo, "lvlprest '" + cellAt(row, col[LvlPrest]) + "' is not a number; row skipped");
			continue;
		}
		const std::string who = "'" + p.name + "' (lvlprest " + std::to_string(p.lvlPrest) + ")";
		if (col[LevelType] >= 0 && !cellAt(row, col[LevelType]).empty() && !parseInt(cellAt(row, col[LevelType]), p.levelType)) {
			warn(lineNo, "leveltype of " + who + " is not a number; row skipped");
			continue;
		}
		if (!parseInt(cellAt(row, col[W]), p.w) || !parseInt(cellAt(row, col[H]), p.h) || p.w < 1 || p.h < 1) {
			warn(lineNo, "w and h of " + who + " must be whole numbers of at least 1; row skipped");
			continue;
		}
		if (!parseInt(cellAt(row, col[Weight]), p.weight)) {
			warn(lineNo, "weight of " + who + " is not a number; row skipped");
			continue;
		}
		if (p.weight > kMaxWeight) {
			warn(lineNo, "weight of " + who + " is above " + std::to_string(kMaxWeight) + "; using " + std::to_string(kMaxWeight));
			p.weight = kMaxWeight;
		}
		p.check = lowerAscii(cellAt(row, col[Check]));
		if (!p.check.empty() && p.check != "ok" && p.check != "n/a" && p.check != "variants") {
			warn(lineNo, "check of " + who + " is '" + p.check + "', not ok or n/a; row skipped (its DS1 does not match the catalogue)");
			continue;
		}
		if (!parseRole(cellAt(row, col[Role]), p.role)) {
			warn(lineNo, "role '" + cellAt(row, col[Role]) + "' of " + who + " is not one of path, clearing, entry, boss, event, filler, border; row skipped");
			continue;
		}
		const int edgeCols[4] = {col[EdgeN], col[EdgeE], col[EdgeS], col[EdgeW]};
		const int edgeLen[4] = {p.w, p.h, p.w, p.h};
		bool edgesOk = true;
		for (int d = 0; d < 4; ++d) {
			p.edge[d] = cellAt(row, edgeCols[d]);
			if (!validSocket(p.edge[d], edgeLen[d])) {
				warn(lineNo, std::string("the ") + "NESW"[d] + " sockets '" + p.edge[d] + "' of " + who + " must be " +
					std::to_string(edgeLen[d]) + " of x, p, c or r; row skipped");
				edgesOk = false;
				break;
			}
			if (edgeLen[d] == 1 && p.edge[d][0] == 'p') p.openMask = static_cast<uint8_t>(p.openMask | kEdgeBit[d]);
		}
		if (!edgesOk) continue;
		bool dup = false;
		for (const Piece &o : res.catalogue.pieces)
			if (o.lvlPrest == p.lvlPrest && lowerAscii(o.tileset) == lowerAscii(p.tileset)) dup = true;
		if (dup) {
			warn(lineNo, who + " is listed twice; the second row is skipped");
			continue;
		}
		if (col[Ds1] >= 0)
			for (const std::string &f : split(cellAt(row, col[Ds1]), ';'))
				if (!trim(f).empty()) p.ds1.push_back(trim(f));
		if (p.check == "variants") p.ds1.clear(); // the variants differ in shape: no DS1 variant is drawn for this piece
		res.catalogue.pieces.push_back(std::move(p));
	}
	if (!haveHeader) res.warnings.push_back(file + " is empty; it has no header row");
	return res;
}

LoadResult loadCatalogueFile(const std::string &path)
{
	std::ifstream in(path, std::ios::binary);
	if (!in) {
		LoadResult r;
		r.warnings.push_back("Cannot open the tileset catalogue '" + path + "'.");
		return r;
	}
	std::ostringstream ss;
	ss << in.rdbuf();
	return parseCatalogue(ss.str(), path);
}

PickResult pickPieces(const layout::Layout &layout, const Catalogue &catalogue, std::string_view tileset, uint64_t seed)
{
	PickResult res;
	res.width = layout.width;
	res.height = layout.height;
	res.cells.resize(static_cast<size_t>(layout.width) * static_cast<size_t>(layout.height));
	const std::string tsName(tileset);
	const std::string tsLower = lowerAscii(tileset);

	bool anyPiece = false;
	for (const Piece &p : catalogue.pieces)
		if (pickable(p, tsLower)) anyPiece = true;
	for (int y = 0; y < layout.height; ++y)
		for (int x = 0; x < layout.width; ++x) {
			PickedCell &c = res.cells[static_cast<size_t>(y) * layout.width + x];
			c.x = x;
			c.y = y;
		}
	if (!anyPiece) {
		res.errors.push_back("The catalogue has no usable 1x1 pieces for the tileset '" + tsName +
			"'. Check the tileset name, or add the tileset's pieces to the catalogue.");
		return res;
	}

	std::vector<std::pair<layout::Role, uint8_t>> reported; // (role, mask) already described
	for (int y = 0; y < layout.height; ++y)
		for (int x = 0; x < layout.width; ++x) {
			PickedCell &out = res.cells[static_cast<size_t>(y) * layout.width + x];
			const layout::Cell &cell = layout.at(x, y);
			const std::string where = std::to_string(x) + "," + std::to_string(y);
			if (cell.role == layout::Role::Empty) {
				if (cell.mask != 0)
					res.errors.push_back("The layout has an empty cell at " + where + " with exits " + maskName(cell.mask) + "; the layout is broken.");
				continue;
			}
			if (cell.mask == 0) {
				res.errors.push_back("The layout has a " + std::string(roomName(cell.role)) + " at " + where +
					" with no exits; it cannot be linked to anything.");
				continue;
			}
			bool agrees = true;
			for (int d = 0; d < 4; ++d) {
				if (!(cell.mask & kEdgeBit[d])) continue;
				const int nx = x + dxOf[d], ny = y + dyOf[d];
				const uint8_t opposite = kEdgeBit[(d + 2) % 4];
				if (!layout.inside(nx, ny) || !(layout.at(nx, ny).mask & opposite)) agrees = false;
			}
			if (!agrees) {
				res.errors.push_back("The layout cell at " + where + " has an exit that its neighbour does not face back; the layout is broken.");
				continue;
			}

			const Tiers tiers = tiersFor(cell.role);
			std::vector<int> candidates;
			for (int t = 0; t < tiers.count && candidates.empty(); ++t)
				for (size_t i = 0; i < catalogue.pieces.size(); ++i) {
					const Piece &p = catalogue.pieces[i];
					if (p.role == tiers.role[t] && pickable(p, tsLower) && socketsMatch(p, cell.mask))
						candidates.push_back(static_cast<int>(i));
				}
			if (candidates.empty()) {
				const std::pair<layout::Role, uint8_t> key{cell.role, cell.mask};
				if (std::find(reported.begin(), reported.end(), key) == reported.end()) {
					reported.push_back(key);
					res.errors.push_back("The tileset '" + tsName + "' has no piece with exits " + maskName(cell.mask) + " for the " +
						roomName(cell.role) + " at " + where + ". Add one to the catalogue, or pick a tileset or shape that does not need it.");
				}
				continue;
			}

			// Stable order (by LvlPrest id) so the pick does not depend on the catalogue's row order.
			std::sort(candidates.begin(), candidates.end(), [&](int a, int b) {
				return catalogue.pieces[static_cast<size_t>(a)].lvlPrest < catalogue.pieces[static_cast<size_t>(b)].lvlPrest;
			});
			layout::Pcg32 rng(seed ^ layout::splitmix64(static_cast<uint64_t>(y) * static_cast<uint64_t>(layout.width) + static_cast<uint64_t>(x) + 1u));
			uint64_t total = 0; // weights are capped at kMaxWeight when loaded; a hand-built catalogue is clamped here
			for (int i : candidates) total += static_cast<uint64_t>(std::min(catalogue.pieces[static_cast<size_t>(i)].weight, kMaxWeight));
			uint64_t roll = total <= 0xFFFFFFFFull ? rng.below(static_cast<uint32_t>(total))
				: ((static_cast<uint64_t>(rng.next()) << 32) | rng.next()) % total;
			int chosen = candidates.back();
			for (int i : candidates) {
				const uint64_t w = static_cast<uint64_t>(std::min(catalogue.pieces[static_cast<size_t>(i)].weight, kMaxWeight));
				if (roll < w) {
					chosen = i;
					break;
				}
				roll -= w;
			}
			const Piece &p = catalogue.pieces[static_cast<size_t>(chosen)];
			out.piece = chosen;
			out.lvlPrest = p.lvlPrest;
			out.ds1Variant = p.ds1.empty() ? -1 : static_cast<int>(rng.below(static_cast<uint32_t>(p.ds1.size())));
		}
	return res;
}

std::vector<std::string> checkPicks(const layout::Layout &layout, const Catalogue &catalogue, std::string_view tileset,
	const PickResult &result)
{
	std::vector<std::string> bad;
	const std::string tsLower = lowerAscii(tileset);
	if (result.width != layout.width || result.height != layout.height ||
		result.cells.size() != static_cast<size_t>(layout.width) * static_cast<size_t>(layout.height)) {
		bad.push_back("the picked grid is not the layout's size");
		return bad;
	}
	// The socket the picked piece shows on side d of cell (x,y); empty cells and the outside are closed.
	auto socket = [&](int x, int y, int d) -> char {
		if (!layout.inside(x, y)) return 'x';
		const PickedCell &c = result.at(x, y);
		if (c.piece < 0 || c.piece >= static_cast<int>(catalogue.pieces.size())) return 'x';
		return catalogue.pieces[static_cast<size_t>(c.piece)].edge[d][0];
	};
	for (int y = 0; y < layout.height; ++y)
		for (int x = 0; x < layout.width; ++x) {
			const PickedCell &c = result.at(x, y);
			const std::string where = " at " + std::to_string(x) + "," + std::to_string(y);
			if (layout.at(x, y).role == layout::Role::Empty) {
				if (c.piece >= 0 || c.lvlPrest != -1) bad.push_back("empty cell has a piece" + where);
				continue;
			}
			if (c.piece < 0 || c.piece >= static_cast<int>(catalogue.pieces.size())) {
				bad.push_back("used cell has no piece" + where);
				continue;
			}
			const Piece &p = catalogue.pieces[static_cast<size_t>(c.piece)];
			if (p.lvlPrest != c.lvlPrest || lowerAscii(p.tileset) != tsLower) bad.push_back("piece is not from the tileset" + where);
			if (p.openMask == 0) bad.push_back("mask 0 piece" + where);
			if (c.ds1Variant < -1 || c.ds1Variant >= static_cast<int>(p.ds1.size()) || (c.ds1Variant < 0 && !p.ds1.empty()))
				bad.push_back("ds1 variant out of range" + where);
			for (int d = 0; d < 4; ++d) {
				const bool exit = (layout.at(x, y).mask & kEdgeBit[d]) != 0;
				if (p.edge[d][0] != (exit ? 'p' : 'x')) bad.push_back(std::string("sockets differ from the facing bits on the ") + "NESW"[d] + " side" + where);
				// The neighbour's socket on the shared side must be the same.
				if (exit && socket(x + dxOf[d], y + dyOf[d], (d + 2) % 4) != p.edge[d][0])
					bad.push_back(std::string("socket differs from the neighbour on the ") + "NESW"[d] + " side" + where);
			}
		}
	return bad;
}

uint64_t picksHash(const PickResult &result)
{
	uint64_t h = 1469598103934665603ull;
	auto mix = [&h](uint64_t v) {
		for (int i = 0; i < 8; ++i) {
			h ^= (v >> (8 * i)) & 0xFFu;
			h *= 1099511628211ull;
		}
	};
	mix(static_cast<uint64_t>(result.width));
	mix(static_cast<uint64_t>(result.height));
	for (const PickedCell &c : result.cells) {
		mix(static_cast<uint64_t>(static_cast<int64_t>(c.lvlPrest)));
		mix(static_cast<uint64_t>(static_cast<int64_t>(c.ds1Variant)));
	}
	return h;
}

} // namespace cartographer::outdoor
