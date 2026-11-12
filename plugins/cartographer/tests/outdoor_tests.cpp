// Host tests for the Cartographer outdoor piece-picker. No game, no SDK: the picker is pure C++.
// Catalogue data: tests/fixtures/jungle_catalogue.tsv holds the Jungle rows of tools/catalogue.tsv (PR #28, commit
// 13d6e91): derived rows, no game files.
#define _CRT_SECURE_NO_WARNINGS
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "layout.h"
#include "outdoor.h"
#include "rng.h"
#include "stamp.h"

using namespace cartographer;
using namespace cartographer::outdoor;
using layout::Layout;
using layout::Role;

namespace {

constexpr int kSeeds = 10000;
// Goldens: run with CARTOGRAPHER_GOLDEN=1 to print the current values when a change is meant to move them.
const std::string GOLDEN_SHORELINES =
	"2,0=535/0 3,0=530/0 2,1=541/0 2,2=541/2 2,3=541/2 2,4=541/3 8,4=598/0 2,5=543/0 3,5=530/0 8,5=541/4 2,6=541/3 8,6=541/1 "
	"2,7=583/1 3,7=532/0 4,7=532/0 5,7=532/1 6,7=532/1 7,7=532/0 8,7=538/1 ";
const std::string GOLDEN_HASHES =
	"64ece3af063ed751 fd3e0fc0f6d54272 26b1a6ce1f6541c2 68068c76a2578772 ce4981cb9a1e643a 2c762be1dbd7e208 c79697c5b4021c65 "
	"1215a7942077a2ae 19fecd3b91d093a6 2c520c8edae359ff a5e29bd842850fc3 901fab347c753ae8 763e37c95d92c752 08e5bbf643a10c8c "
	"74b3fce4810fad1e ";

const std::string GOLDEN_STAMP_LINE =
	"224x256 0,0=535 4,0=530 0,4=541 0,8=541 0,12=541 0,16=541 24,16=598 0,20=543 4,20=530 24,20=541 0,24=541 24,24=541 0,28=583 4,28=532 "
	"8,28=532 12,28=532 16,28=532 20,28=532 24,28=538 ";
const std::string GOLDEN_STAMP_HASHES =
	"1a4771abad5cde3d 2c9bfd4b0c19b20c 0ea6ab53f22982a7 da62d59a94796e97 5575e6746fb2e8b8 8f2979a836716794 485a1e0bb7e9ba70 "
	"26c18868df38eaa1 51585abd796d49ea f65c9d35dda89b61 c77ebfdc7deca837 eddf55fdbabafa69 9225d7578be89530 1e448733c0b64cff 87eb091d4f761c90 ";

// The fixtures sit beside this file, so the test needs no build-time path or working directory.
std::string fixturePath(const char *name)
{
	std::string here = __FILE__;
	return here.substr(0, here.find_last_of("/\\") + 1) + "fixtures/" + name;
}

LoadResult loadJungle()
{
	return loadCatalogueFile(fixturePath("jungle_catalogue.tsv"));
}

// A 1-row layout from masks and roles, linked as given.
Layout lineLayout(const std::vector<uint8_t> &masks, const std::vector<Role> &roles)
{
	Layout l;
	l.width = static_cast<int>(masks.size());
	l.height = 1;
	l.cells.resize(masks.size());
	for (size_t i = 0; i < masks.size(); ++i) {
		l.cells[i].mask = masks[i];
		l.cells[i].role = roles[i];
	}
	return l;
}

// The Shorelines S1 line [E][EW][EW][EW][W]: entrance, path, event, path, boss.
Layout s1Line()
{
	using namespace layout;
	return lineLayout({kEast, static_cast<uint8_t>(kEast | kWest), static_cast<uint8_t>(kEast | kWest), static_cast<uint8_t>(kEast | kWest), kWest},
		{Role::Entrance, Role::Path, Role::Event, Role::Path, Role::Boss});
}

bool isClearing(const Catalogue &c, int lvlprest)
{
	for (const Piece &p : c.pieces)
		if (p.lvlPrest == lvlprest) return p.role == PieceRole::Clearing;
	return false;
}

} // namespace

TEST_CASE("the committed Jungle rows load with no warnings")
{
	const LoadResult r = loadJungle();
	for (const std::string &w : r.warnings) MESSAGE(w);
	CHECK(r.warnings.empty());
	CHECK(r.catalogue.pieces.size() == 75);
	const Piece &w = r.catalogue.pieces[0]; // Jungle W
	CHECK(w.lvlPrest == 530);
	CHECK(w.role == PieceRole::Path);
	CHECK(w.openMask == layout::kWest);
	CHECK(w.ds1.size() == 3);
	const Piece &head = r.catalogue.pieces[43]; // Jungle Head, 2x1
	CHECK(head.lvlPrest == 573);
	CHECK(head.w == 2);
	CHECK(head.edge[0] == "xp");
}

TEST_CASE("the loader is header-driven and skips bad rows with plain-English warnings")
{
	// Columns in a different order, an extra column, CRLF line ends, and one bad row of each kind.
	const std::string text =
		"weight\trole\tlvlprest\tedgeW\tedgeS\tedgeE\tedgeN\ttileset\tw\th\tnotes\r\n"
		"1\tpath\t10\tp\tx\tx\tx\tJungle\t1\t1\tfine\r\n"
		"1\tpath\tabc\tp\tx\tx\tx\tJungle\t1\t1\tbad id\r\n"
		"1\tcorridor\t11\tp\tx\tx\tx\tJungle\t1\t1\tbad role\r\n"
		"1\tpath\t12\tpp\tx\tx\tx\tJungle\t1\t1\tbad socket length\r\n"
		"1\tpath\t13\tq\tx\tx\tx\tJungle\t1\t1\tbad socket char\r\n"
		"x\tpath\t14\tp\tx\tx\tx\tJungle\t1\t1\tbad weight\r\n"
		"1\tpath\t10\tp\tx\tx\tx\tJungle\t1\t1\tduplicate\r\n"
		"1\tpath\t15\tp\tx\tx\tx\t\t1\t1\tno tileset\r\n"
		"2\tclearing\t16\tx\tx\tp\tx\tJungle\t1\t1\tfine too\r\n";
	const LoadResult r = parseCatalogue(text, "test.tsv");
	CHECK(r.catalogue.pieces.size() == 2);
	CHECK(r.warnings.size() == 7);
	CHECK(r.catalogue.pieces[0].openMask == layout::kWest);
	CHECK(r.catalogue.pieces[1].openMask == layout::kEast);
	CHECK(r.catalogue.pieces[1].role == PieceRole::Clearing);
	CHECK(r.catalogue.pieces[1].weight == 2);
	for (const std::string &w : r.warnings) CHECK(w.rfind("test.tsv line ", 0) == 0);

	const LoadResult none = parseCatalogue("tileset\tlvlprest\nJungle\t1\n");
	CHECK(!none.ok());
	REQUIRE(none.warnings.size() == 1);
	CHECK(none.warnings[0].find("missing the column(s)") != std::string::npos);
	CHECK(!parseCatalogue("").ok());
	CHECK(!loadCatalogueFile("no/such/file.tsv").ok());
}

TEST_CASE("the S1 line gets path pieces in the middle, clearings for the event and boss")
{
	const LoadResult cat = loadJungle();
	const Layout l = s1Line();
	for (uint64_t seed = 1; seed <= 50; ++seed) {
		const PickResult r = pickPieces(l, cat.catalogue, "Jungle", seed);
		REQUIRE(r.ok());
		CHECK(checkPicks(l, cat.catalogue, "Jungle", r).empty());
		CHECK(r.at(0, 0).lvlPrest == 531);      // entrance: Jungle E
		CHECK(r.at(1, 0).lvlPrest == 532);      // Jungle EW
		CHECK(r.at(3, 0).lvlPrest == 532);
		CHECK(isClearing(cat.catalogue, r.at(2, 0).lvlPrest)); // event: a clearing EW (577, 587 or 597)
		CHECK((r.at(2, 0).lvlPrest == 577 || r.at(2, 0).lvlPrest == 587 || r.at(2, 0).lvlPrest == 597));
		CHECK((r.at(4, 0).lvlPrest == 575 || r.at(4, 0).lvlPrest == 585 || r.at(4, 0).lvlPrest == 595)); // boss: clearing W
	}
}

TEST_CASE("an entrance, boss or event with no special piece falls back to a path piece")
{
	// A tileset with path pieces only.
	const std::string text =
		"tileset\tlvlprest\tw\th\tedgeN\tedgeE\tedgeS\tedgeW\trole\tweight\n"
		"T\t1\t1\t1\tx\tp\tx\tx\tpath\t1\n"
		"T\t2\t1\t1\tx\tp\tx\tp\tpath\t1\n"
		"T\t3\t1\t1\tx\tx\tx\tp\tpath\t1\n";
	const LoadResult cat = parseCatalogue(text);
	const Layout l = s1Line();
	const PickResult r = pickPieces(l, cat.catalogue, "t", 5); // the tileset name is case-insensitive
	REQUIRE(r.ok());
	CHECK(r.at(0, 0).lvlPrest == 1);
	CHECK(r.at(2, 0).lvlPrest == 2);
	CHECK(r.at(4, 0).lvlPrest == 3);
}

TEST_CASE("a dedicated entry, boss or event piece wins over clearings and paths")
{
	const std::string text =
		"tileset\tlvlprest\tw\th\tedgeN\tedgeE\tedgeS\tedgeW\trole\tweight\n"
		"T\t1\t1\t1\tx\tp\tx\tx\tpath\t1\n"
		"T\t2\t1\t1\tx\tp\tx\tp\tpath\t1\n"
		"T\t3\t1\t1\tx\tx\tx\tp\tpath\t1\n"
		"T\t4\t1\t1\tx\tp\tx\tx\tentry\t1\n"
		"T\t5\t1\t1\tx\tp\tx\tp\tevent\t1\n"
		"T\t6\t1\t1\tx\tp\tx\tp\tclearing\t1\n"
		"T\t7\t1\t1\tx\tx\tx\tp\tboss\t1\n";
	const LoadResult cat = parseCatalogue(text);
	const PickResult r = pickPieces(s1Line(), cat.catalogue, "T", 9);
	REQUIRE(r.ok());
	CHECK(r.at(0, 0).lvlPrest == 4);
	CHECK(r.at(1, 0).lvlPrest == 2);
	CHECK(r.at(2, 0).lvlPrest == 5);
	CHECK(r.at(4, 0).lvlPrest == 7);
}

TEST_CASE("a missing piece gives a plain-English error naming the exits, once per kind")
{
	// Jungle without any piece that has only a west exit: the S1 line's boss room and any W-only dead end.
	LoadResult cat = loadJungle();
	std::vector<Piece> kept;
	for (const Piece &p : cat.catalogue.pieces)
		if (p.openMask != layout::kWest) kept.push_back(p);
	cat.catalogue.pieces = kept;
	const PickResult r = pickPieces(s1Line(), cat.catalogue, "Jungle", 3);
	REQUIRE(!r.ok());
	REQUIRE(r.errors.size() == 1);
	CHECK(r.errors[0] == "The tileset 'Jungle' has no piece with exits W for the boss room at 4,0. "
	                     "Add one to the catalogue, or pick a tileset or shape that does not need it.");
	CHECK(r.at(4, 0).lvlPrest == -1);
	CHECK(r.at(0, 0).lvlPrest == 531); // the rest still got pieces

	const PickResult unknown = pickPieces(s1Line(), loadJungle().catalogue, "Snowy", 3);
	REQUIRE(unknown.errors.size() == 1);
	CHECK(unknown.errors[0].find("no usable 1x1 pieces for the tileset 'Snowy'") != std::string::npos);

	// Only border pieces and 2x1 pieces: nothing usable.
	Catalogue onlyBorders;
	for (const Piece &p : loadJungle().catalogue.pieces)
		if (p.role == PieceRole::Border) onlyBorders.pieces.push_back(p);
	CHECK(!pickPieces(s1Line(), onlyBorders, "Jungle", 3).ok());
}

TEST_CASE("a broken layout is reported, never given mask 0 or a mismatched piece")
{
	const LoadResult cat = loadJungle();
	Layout noExits = lineLayout({0, layout::kEast}, {Role::Entrance, Role::Boss});
	PickResult r = pickPieces(noExits, cat.catalogue, "Jungle", 1);
	CHECK(!r.ok());
	CHECK(r.at(0, 0).lvlPrest == -1);
	Layout oneSided = lineLayout({layout::kEast, 0}, {Role::Entrance, Role::Empty});
	r = pickPieces(oneSided, cat.catalogue, "Jungle", 1);
	CHECK(!r.ok());
	CHECK(r.errors[0].find("does not face back") != std::string::npos);
}

TEST_CASE("weights bias the pick and weight 0 is never picked; the catalogue's row order does not matter")
{
	const std::string text =
		"tileset\tlvlprest\tw\th\tedgeN\tedgeE\tedgeS\tedgeW\trole\tweight\tds1\n"
		"T\t1\t1\t1\tx\tp\tx\tx\tpath\t3\ta.ds1;b.ds1\n"
		"T\t2\t1\t1\tx\tp\tx\tx\tpath\t1\tc.ds1\n"
		"T\t3\t1\t1\tx\tp\tx\tx\tpath\t0\td.ds1\n"
		"T\t4\t1\t1\tx\tx\tx\tp\tpath\t1\te.ds1\n";
	const LoadResult cat = parseCatalogue(text);
	LoadResult rev = cat;
	std::vector<Piece> flipped(cat.catalogue.pieces.rbegin(), cat.catalogue.pieces.rend());
	rev.catalogue.pieces = flipped;
	const Layout l = lineLayout({layout::kEast, layout::kWest}, {Role::Entrance, Role::Boss});
	int one = 0, two = 0, variantB = 0;
	for (uint64_t seed = 0; seed < 4000; ++seed) {
		const PickResult a = pickPieces(l, cat.catalogue, "T", seed), b = pickPieces(l, rev.catalogue, "T", seed);
		REQUIRE(a.ok());
		CHECK(a.at(0, 0).lvlPrest == b.at(0, 0).lvlPrest);
		CHECK(a.at(0, 0).ds1Variant == b.at(0, 0).ds1Variant);
		CHECK(a.at(0, 0).lvlPrest != 3);
		if (a.at(0, 0).lvlPrest == 1) {
			++one;
			if (a.at(0, 0).ds1Variant == 1) ++variantB;
		}
		if (a.at(0, 0).lvlPrest == 2) {
			++two;
			CHECK(a.at(0, 0).ds1Variant == 0);
		}
	}
	CHECK(one > 2800);
	CHECK(one < 3200); // about 3000 of 4000
	CHECK(two > 800);
	CHECK(variantB > 1200); // about half of the weight-3 picks
}

TEST_CASE("10k seeds of every shape preset on Jungle: every cell gets a matching piece")
{
	const LoadResult cat = loadJungle();
	REQUIRE(cat.ok());
	for (int s = 0; s < layout::kShapeCount; ++s)
		for (int z = 0; z < layout::kSizeCount; ++z) {
			const layout::Shape shape = static_cast<layout::Shape>(s);
			const layout::Size size = static_cast<layout::Size>(z);
			const layout::Rules rules = layout::presetRules(shape, size);
			int failures = 0, clearingBoss = 0, clearingEvent = 0, bossRooms = 0, eventRooms = 0;
			for (int i = 0; i < kSeeds; ++i) {
				const uint64_t seed = layout::splitmix64(static_cast<uint64_t>(i) + 0x2000);
				const Layout l = layout::generate(rules, seed);
				const PickResult r = pickPieces(l, cat.catalogue, "Jungle", seed);
				const std::vector<std::string> problems = checkPicks(l, cat.catalogue, "Jungle", r);
				if (!r.ok() || !problems.empty()) {
					if (failures++ < 3)
						MESSAGE(layout::shapeName(shape) << " " << layout::sizeName(size) << " seed " << seed << ": "
							<< (r.errors.empty() ? problems[0] : r.errors[0]) << "\n" << layout::toAscii(l));
					continue;
				}
				for (int y = 0; y < l.height; ++y)
					for (int x = 0; x < l.width; ++x) {
						if (l.at(x, y).role == Role::Boss) { ++bossRooms; clearingBoss += isClearing(cat.catalogue, r.at(x, y).lvlPrest); }
						if (l.at(x, y).role == Role::Event) { ++eventRooms; clearingEvent += isClearing(cat.catalogue, r.at(x, y).lvlPrest); }
					}
			}
			INFO(layout::shapeName(shape) << " " << layout::sizeName(size));
			CHECK(failures == 0);
			// Jungle has clearings for one or two exits; the event and boss rooms have one or two on these presets.
			CHECK(clearingBoss == bossRooms);
			CHECK(clearingEvent == eventRooms);
			if (std::getenv("CARTOGRAPHER_STATS"))
				std::printf("%-9s %-6s seeds %d  failures %d  boss rooms %d (clearing %d)  event rooms %d (clearing %d)\n", layout::shapeName(shape),
					layout::sizeName(size), kSeeds, failures, bossRooms, clearingBoss, eventRooms, clearingEvent);
		}
}

TEST_CASE("picks are stable: same inputs give the same picks, and goldens do not move")
{
	const LoadResult cat = loadJungle();
	const Layout l = layout::generate(layout::Shape::Corridor, layout::Size::Medium, 1);
	const PickResult a = pickPieces(l, cat.catalogue, "Jungle", 1), b = pickPieces(l, cat.catalogue, "Jungle", 1);
	CHECK(picksHash(a) == picksHash(b));
	CHECK(picksHash(a) != picksHash(pickPieces(l, cat.catalogue, "Jungle", 2)));

	// Golden: the Shorelines (Jungle / Corridor, medium) layout for map seed 1, piece by piece.
	std::string line;
	for (int y = 0; y < a.height; ++y)
		for (int x = 0; x < a.width; ++x)
			if (a.at(x, y).lvlPrest >= 0)
				line += std::to_string(x) + "," + std::to_string(y) + "=" + std::to_string(a.at(x, y).lvlPrest) + "/" + std::to_string(a.at(x, y).ds1Variant) + " ";
	if (std::getenv("CARTOGRAPHER_GOLDEN")) std::printf("GOLDEN_LINE %s\n", line.c_str());
	CHECK(line == GOLDEN_SHORELINES);

	// Golden hashes for the 15 presets at map seed 12345.
	std::string hashes;
	for (int s = 0; s < layout::kShapeCount; ++s)
		for (int z = 0; z < layout::kSizeCount; ++z) {
			const Layout g = layout::generate(static_cast<layout::Shape>(s), static_cast<layout::Size>(z), 12345);
			char buf[32];
			std::snprintf(buf, sizeof buf, "%016llx ", static_cast<unsigned long long>(picksHash(pickPieces(g, cat.catalogue, "Jungle", 12345))));
			hashes += buf;
		}
	if (std::getenv("CARTOGRAPHER_GOLDEN")) std::printf("GOLDEN_HASHES %s\n", hashes.c_str());
	CHECK(hashes == GOLDEN_HASHES);
}

TEST_CASE("check column: bad rows are skipped with a warning, variants rows get no DS1 variant, a BOM is accepted")
{
	const std::string text =
		"\xEF\xBB\xBF"
		"tileset\tlvlprest\tw\th\tedgeN\tedgeE\tedgeS\tedgeW\trole\tweight\tcheck\tds1\n"
		"T\t1\t1\t1\tx\tp\tx\tx\tpath\t1\tok\ta.ds1;b.ds1\n"
		"T\t2\t1\t1\tx\tp\tx\tp\tpath\t1\tvariants\tc.ds1;d.ds1\n"
		"T\t3\t1\t1\tx\tx\tx\tp\tpath\t1\tmismatch\te.ds1\n"
		"T\t4\t1\t1\tx\tx\tx\tp\tpath\t1\t\te.ds1\n";
	const LoadResult cat = parseCatalogue(text);
	REQUIRE(cat.catalogue.pieces.size() == 3); // the 'mismatch' row is skipped
	CHECK(cat.warnings.size() == 1);
	CHECK(cat.catalogue.pieces[1].ds1.empty());
	const PickResult r = pickPieces(s1Line(), cat.catalogue, "T", 3);
	REQUIRE(r.ok());
	CHECK(r.at(1, 0).ds1Variant == -1);
}

// --- The stamp plan (phase 5, part 1) ---------------------------------------------------------------------------------

namespace {

std::string stampLine(const StampPlan &p)
{
	std::string out = std::to_string(p.tilesW) + "x" + std::to_string(p.tilesH) + " ";
	for (const Stamp &s : p.stamps) out += std::to_string(s.cx) + "," + std::to_string(s.cy) + "=" + std::to_string(s.lvlPrest) + " ";
	return out;
}

} // namespace

TEST_CASE("the S1 line becomes five stamps 4 grid cells apart, a 160 x 32 level, no void")
{
	const LoadResult cat = loadJungle();
	const Layout l = s1Line();
	const PickResult r = pickPieces(l, cat.catalogue, "Jungle", 7);
	REQUIRE(r.ok());
	StampOptions o;
	o.originX = 6000;
	o.originY = 3000;
	const StampPlan p = planStamps(l, r, o);
	REQUIRE(p.ok());
	CHECK(p.tilesW == 160);
	CHECK(p.tilesH == 32);
	CHECK(p.cellsW == 20);
	CHECK(p.cellsH == 4);
	REQUIRE(p.stamps.size() == 5);
	for (int i = 0; i < 5; ++i) {
		CHECK(p.stamps[i].cx == i * 4);
		CHECK(p.stamps[i].cy == 0);
		CHECK(p.stamps[i].file == -1); // the engine draws it, as in the S1 spike
	}
	CHECK(p.stamps[0].lvlPrest == 531);
	CHECK(p.voidCells.empty());
	CHECK(p.entrance.known);
	CHECK(p.entrance.x == 6000);
	CHECK(p.entrance.centreX() == 6016);
	CHECK(p.entrance.centreY() == 3016);
	CHECK(p.event.x == 6000 + 2 * 32);
	CHECK(p.boss.x == 6000 + 4 * 32);
	CHECK(checkStampPlan(l, r, o, p).empty());
}

TEST_CASE("the jungle edge rule: a mask piece is 0x211 + mask, and mask 0 and 529 are never emitted")
{
	CHECK(jungleLvlPrest(layout::kWest) == 530);
	CHECK(jungleLvlPrest(layout::kEast | layout::kWest) == 532);
	CHECK(jungleLvlPrest(15) == 544);
	CHECK(jungleLvlPrest(0) == -1);
	CHECK(jungleLvlPrest(16) == -1);

	// A pick whose jungle piece does not match the cell's mask is an error, not a stamp.
	const LoadResult cat = loadJungle();
	const Layout l = s1Line();
	PickResult r = pickPieces(l, cat.catalogue, "Jungle", 7);
	REQUIRE(r.ok());
	r.cells[1].lvlPrest = 533; // Jungle S on an EW cell
	const StampPlan p = planStamps(l, r, StampOptions{});
	CHECK(!p.ok());
	CHECK(p.stamps.empty());
	REQUIRE(!p.errors.empty());
	CHECK(p.errors[0].find("jungle edge rule") != std::string::npos);
}

TEST_CASE("an L-shape is cropped to its box and the empty macro-cell becomes void grid cells")
{
	const LoadResult cat = loadJungle();
	Layout l;
	l.width = 4;
	l.height = 4;
	l.cells.resize(16);
	// (1,1) E | (2,1) SW | (2,2) N: an L in a 4 x 4 grid, so the box is 2 x 2 blocks and (1,2) is empty.
	l.at(1, 1) = {layout::kEast, Role::Entrance};
	l.at(2, 1) = {static_cast<uint8_t>(layout::kWest | layout::kSouth), Role::Path};
	l.at(2, 2) = {layout::kNorth, Role::Boss};
	const PickResult r = pickPieces(l, cat.catalogue, "Jungle", 3);
	REQUIRE(r.ok());
	StampOptions o;
	const StampPlan p = planStamps(l, r, o);
	REQUIRE(p.ok());
	CHECK(p.macroMinX == 1);
	CHECK(p.macroMinY == 1);
	CHECK(p.tilesW == 64);
	CHECK(p.tilesH == 64);
	REQUIRE(p.stamps.size() == 3);
	CHECK(p.stamps[0].cx == 0);
	CHECK(p.stamps[1].cx == 4);
	CHECK(p.stamps[2].cx == 4);
	CHECK(p.stamps[2].cy == 4);
	CHECK(p.voidCells.size() == 16); // the empty bottom-left block, 4 x 4 grid cells
	CHECK(p.voidCells.front() == layout::Point{0, 4});
	CHECK(p.voidCells.back() == layout::Point{3, 7});
	CHECK(p.event.known == false);
	CHECK(p.boss.x == 32);
	CHECK(p.boss.y == 32);
	CHECK(stampPlanAscii(p) == "########\n########\n########\n########\n....####\n....####\n....####\n....####\n");
	CHECK(checkStampPlan(l, r, o, p).empty());

	o.listVoid = false;
	const StampPlan quiet = planStamps(l, r, o);
	REQUIRE(quiet.ok());
	CHECK(quiet.voidCells.empty());
	CHECK(checkStampPlan(l, r, o, quiet).empty());
}

TEST_CASE("size limits and bad inputs give plain-English errors and no stamps")
{
	const LoadResult cat = loadJungle();
	const Layout l = s1Line();
	const PickResult r = pickPieces(l, cat.catalogue, "Jungle", 7);
	REQUIRE(r.ok());

	StampOptions narrow;
	narrow.maxMacroCells = 4; // the line is 5 blocks long
	StampPlan p = planStamps(l, r, narrow);
	CHECK(!p.ok());
	CHECK(p.stamps.empty());
	CHECK(p.errors[0].find("5 x 1 blocks") != std::string::npos);

	StampOptions small;
	small.maxTilesX = 128;
	small.maxTilesY = 32;
	p = planStamps(l, r, small);
	CHECK(!p.ok());
	CHECK(p.errors[0].find("160 x 32 tiles") != std::string::npos);

	StampOptions odd;
	odd.macroTiles = 30;
	CHECK(!planStamps(l, r, odd).ok());

	PickResult bad = r;
	bad.errors.push_back("no piece for the boss");
	CHECK(!planStamps(l, bad, StampOptions{}).ok());

	Layout two = l;
	two.at(3, 0).role = Role::Entrance;
	p = planStamps(two, r, StampOptions{});
	CHECK(!p.ok());
	CHECK(p.stamps.empty());
	REQUIRE(!p.errors.empty());
	CHECK(p.errors[0].find("more than one entrance") != std::string::npos);

	Layout empty;
	empty.width = 3;
	empty.height = 3;
	empty.cells.resize(9);
	PickResult none;
	none.width = 3;
	none.height = 3;
	none.cells.resize(9);
	CHECK(!planStamps(empty, none, StampOptions{}).ok());
}

TEST_CASE("pinFiles passes the picked DS1 variant as the file index")
{
	const LoadResult cat = loadJungle();
	const Layout l = s1Line();
	bool sawVariant = false;
	for (uint64_t seed = 1; seed <= 40; ++seed) {
		const PickResult r = pickPieces(l, cat.catalogue, "Jungle", seed);
		REQUIRE(r.ok());
		StampOptions o;
		o.pinFiles = true;
		const StampPlan p = planStamps(l, r, o);
		REQUIRE(p.ok());
		REQUIRE(p.stamps.size() == 5);
		for (size_t i = 0; i < p.stamps.size(); ++i) {
			CHECK(p.stamps[i].file == r.at(p.stamps[i].macroX, p.stamps[i].macroY).ds1Variant);
			sawVariant = sawVariant || p.stamps[i].file > 0;
		}
		CHECK(checkStampPlan(l, r, o, p).empty());
	}
	CHECK(sawVariant);
}

TEST_CASE("10k seeds of every shape preset on Jungle: the stamp plan is complete and passes its own check")
{
	const LoadResult cat = loadJungle();
	REQUIRE(cat.ok());
	for (int s = 0; s < layout::kShapeCount; ++s)
		for (int z = 0; z < layout::kSizeCount; ++z) {
			const layout::Shape shape = static_cast<layout::Shape>(s);
			const layout::Size size = static_cast<layout::Size>(z);
			const layout::Rules rules = layout::presetRules(shape, size);
			int failures = 0, tooBig = 0;
			for (int i = 0; i < kSeeds; ++i) {
				const uint64_t seed = layout::splitmix64(static_cast<uint64_t>(i) + 0x3000);
				const Layout l = layout::generate(rules, seed);
				const PickResult r = pickPieces(l, cat.catalogue, "Jungle", seed);
				if (!r.ok()) {
					++failures;
					continue;
				}
				StampOptions o;
				o.originX = 6000;
				o.originY = 3000;
				o.pinFiles = (i % 2) == 1;
				o.maxMacroCells = 64; // this test is about the plan; the 12 x 12 cap has its own test
				const StampPlan p = planStamps(l, r, o);
				const std::vector<std::string> problems = p.ok() ? checkStampPlan(l, r, o, p) : p.errors;
				if (!problems.empty()) {
					if (failures++ < 3)
						MESSAGE(layout::shapeName(shape) << " " << layout::sizeName(size) << " seed " << seed << ": " << problems[0] << "\n" << layout::toAscii(l));
					continue;
				}
				if (p.macroW > 12 || p.macroH > 12) ++tooBig;
				CHECK(p.entrance.known);
				CHECK(p.tilesW % 8 == 0);
			}
			INFO(layout::shapeName(shape) << " " << layout::sizeName(size));
			CHECK(failures == 0);
			if (std::getenv("CARTOGRAPHER_STATS"))
				std::printf("stamp %-9s %-6s seeds %d  failures %d  over 12x12 %d\n", layout::shapeName(shape), layout::sizeName(size), kSeeds, failures, tooBig);
		}
}

TEST_CASE("stamp plans are stable: same inputs give the same plan, and the goldens do not move")
{
	const LoadResult cat = loadJungle();
	const Layout l = layout::generate(layout::Shape::Corridor, layout::Size::Medium, 1);
	const PickResult r = pickPieces(l, cat.catalogue, "Jungle", 1);
	REQUIRE(r.ok());
	const StampPlan a = planStamps(l, r, StampOptions{}), b = planStamps(l, r, StampOptions{});
	REQUIRE(a.ok());
	CHECK(stampPlanHash(a) == stampPlanHash(b));
	StampOptions pinned;
	pinned.pinFiles = true;
	CHECK(stampPlanHash(a) != stampPlanHash(planStamps(l, r, pinned)));

	if (std::getenv("CARTOGRAPHER_GOLDEN")) std::printf("GOLDEN_STAMP_LINE %s\n", stampLine(a).c_str());
	CHECK(stampLine(a) == GOLDEN_STAMP_LINE);

	std::string hashes;
	for (int s = 0; s < layout::kShapeCount; ++s)
		for (int z = 0; z < layout::kSizeCount; ++z) {
			const Layout g = layout::generate(static_cast<layout::Shape>(s), static_cast<layout::Size>(z), 12345);
			StampOptions o;
			o.maxMacroCells = 64;
			char buf[32];
			std::snprintf(buf, sizeof buf, "%016llx ", static_cast<unsigned long long>(stampPlanHash(planStamps(g, pickPieces(g, cat.catalogue, "Jungle", 12345), o))));
			hashes += buf;
		}
	if (std::getenv("CARTOGRAPHER_GOLDEN")) std::printf("GOLDEN_STAMP_HASHES %s\n", hashes.c_str());
	CHECK(hashes == GOLDEN_STAMP_HASHES);
}
