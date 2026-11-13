// Host tests for the Cartographer outdoor piece-picker. No game, no SDK: the picker is pure C++.
// Catalogue data: tests/fixtures/jungle_catalogue.tsv holds the Jungle rows of tools/catalogue.tsv (PR #28, commit
// 13d6e91): derived rows, no game files.
#define _CRT_SECURE_NO_WARNINGS
#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "layout.h"
#include "outdoor.h"
#include "outdoor_plan.h"
#include "outdoor_tilesets.h"
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
	"907cf8217d7b7829 64ece3af063ed751 fd3e0fc0f6d54272 26b1a6ce1f6541c2 ee21102abe478a95 68068c76a2578772 ce4981cb9a1e643a "
	"2c762be1dbd7e208 606db446a5eb7047 c79697c5b4021c65 1215a7942077a2ae 19fecd3b91d093a6 5c0b71422478a02a 2c520c8edae359ff "
	"a5e29bd842850fc3 901fab347c753ae8 49c9cba06771a0c0 763e37c95d92c752 08e5bbf643a10c8c 74b3fce4810fad1e "
	"36bb32f5285e6d5f 3766511a9a24892c ef66469bffdeeff7 00f38885818a84b6 ";

const std::string GOLDEN_STAMP_LINE =
	"224x256 0,0=535 4,0=530 0,4=541 0,8=541 0,12=541 0,16=541 24,16=598 0,20=543 4,20=530 24,20=541 0,24=541 24,24=541 0,28=583 4,28=532 "
	"8,28=532 12,28=532 16,28=532 20,28=532 24,28=538 ";
const std::string GOLDEN_STAMP_HASHES =
	"bbc9e79c79924f3f 1a4771abad5cde3d 2c9bfd4b0c19b20c 0ea6ab53f22982a7 151c08742a7209c8 da62d59a94796e97 5575e6746fb2e8b8 "
	"8f2979a836716794 7d42d055239584b1 485a1e0bb7e9ba70 26c18868df38eaa1 51585abd796d49ea a42cec83cc16fdf4 f65c9d35dda89b61 "
	"c77ebfdc7deca837 eddf55fdbabafa69 e45b406a6be50916 9225d7578be89530 1e448733c0b64cff 87eb091d4f761c90 "
	"06c76a6eddf1c7c3 2cb9cb858214889e 5c05387d54e3dfd8 8ae65ae4e7d7937a ";

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
			if (shape != layout::Shape::Open) CHECK(clearingBoss == bossRooms); // Open rooms have up to four exits, and Jungle has no such clearing: they take plain path pieces
			if (shape != layout::Shape::Open) CHECK(clearingEvent == eventRooms);
			if (std::getenv("CARTOGRAPHER_STATS"))
				std::printf("%-9s %-6s seeds %d  failures %d  boss rooms %d (clearing %d)  event rooms %d (clearing %d)\n", layout::shapeName(shape),
					layout::sizeName(size), kSeeds, failures, bossRooms, clearingBoss, eventRooms, clearingEvent);
		}
}

TEST_CASE("picks are stable: same inputs give the same picks, and goldens do not move")
{
	const LoadResult cat = loadJungle();
	const Layout l = layout::generate(layout::Shape::Corridor, layout::Size::Large, 1);
	const PickResult a = pickPieces(l, cat.catalogue, "Jungle", 1), b = pickPieces(l, cat.catalogue, "Jungle", 1);
	CHECK(picksHash(a) == picksHash(b));
	CHECK(picksHash(a) != picksHash(pickPieces(l, cat.catalogue, "Jungle", 2)));

	// Golden: the Shorelines (Jungle / Corridor, large) layout for map seed 1, piece by piece.
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

TEST_CASE("the planner itself refuses piece 529 and a pinned DS1 variant over 15")
{
	const LoadResult cat = loadJungle();
	const Layout l = s1Line();
	PickResult r = pickPieces(l, cat.catalogue, "Jungle", 7);
	REQUIRE(r.ok());

	PickResult town = r;
	town.cells[1].lvlPrest = 529;
	StampPlan p = planStamps(l, town, StampOptions{});
	CHECK(!p.ok());
	CHECK(p.stamps.empty());
	REQUIRE(!p.errors.empty());
	CHECK(p.errors[0].find("Act 3 town") != std::string::npos);

	PickResult wide = r;
	wide.cells[1].ds1Variant = 16;
	StampOptions pin;
	pin.pinFiles = true;
	p = planStamps(l, wide, pin);
	CHECK(!p.ok());
	REQUIRE(!p.errors.empty());
	CHECK(p.errors[0].find("4-bit file index") != std::string::npos);
	CHECK(planStamps(l, wide, StampOptions{}).ok()); // not pinned: the variant is not passed on
}

TEST_CASE("checkStampPlan names the fault in a corrupted plan")
{
	const LoadResult cat = loadJungle();
	Layout l;
	l.width = 4;
	l.height = 4;
	l.cells.resize(16);
	l.at(1, 1) = {layout::kEast, Role::Entrance};
	l.at(2, 1) = {static_cast<uint8_t>(layout::kWest | layout::kSouth), Role::Path};
	l.at(2, 2) = {layout::kNorth, Role::Boss};
	const PickResult r = pickPieces(l, cat.catalogue, "Jungle", 3);
	REQUIRE(r.ok());
	const StampOptions o;
	const StampPlan good = planStamps(l, r, o);
	REQUIRE(good.ok());
	REQUIRE(checkStampPlan(l, r, o, good).empty());

	auto expectFault = [&](const StampPlan &bad, const char *text) {
		const std::vector<std::string> problems = checkStampPlan(l, r, o, bad);
		INFO("expected: " << text);
		REQUIRE(!problems.empty());
		bool found = false;
		for (const std::string &m : problems) found = found || m.find(text) != std::string::npos;
		CHECK(found);
	};

	StampPlan bad = good;
	bad.stamps[1].cx += 1; // off the macro-cell grid
	expectFault(bad, "is not aligned to the macro-cell grid");

	bad = good;
	bad.voidCells.pop_back(); // the last void cell dropped
	expectFault(bad, "is not stamped and not listed void");

	bad = good;
	bad.voidCells.erase(bad.voidCells.begin()); // the first void cell dropped
	expectFault(bad, "is not stamped and not listed void");

	bad = good;
	bad.voidCells.push_back({0, 0}); // a stamped cell listed as void
	expectFault(bad, "the void list holds cells that are stamped");

	bad = good;
	bad.entrance.x += 8; // the entrance rectangle shifted
	expectFault(bad, "rectangle is not on");

	bad = good;
	std::swap(bad.stamps[0], bad.stamps[1]); // two stamps out of order
	expectFault(bad, "out of row-major order");

	bad = good;
	bad.stamps.pop_back(); // a stamp missing
	expectFault(bad, "stamps for 3 used cells");

	bad = good;
	bad.stamps[0].lvlPrest = 529; // the Act 3 town
	expectFault(bad, "Act 3 town piece");

	bad = good;
	bad.stamps[0].file = 16; // does not fit the 4-bit flag
	expectFault(bad, "4-bit flag");

	bad = good;
	bad.tilesW += 8; // the level size no longer matches the box
	expectFault(bad, "does not match the cropped box");
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

	StampOptions onlyX;
	onlyX.maxTilesX = 128; // no limit on the other axis: the message must not say 0
	p = planStamps(l, r, onlyX);
	REQUIRE(!p.errors.empty());
	CHECK(p.errors[0].find("128 x unlimited") != std::string::npos);

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
			CHECK(tooBig == 0); // no layout of any preset is wider or taller than 12 blocks
			if (std::getenv("CARTOGRAPHER_STATS"))
				std::printf("stamp %-9s %-6s seeds %d  failures %d  over 12x12 %d\n", layout::shapeName(shape), layout::sizeName(size), kSeeds, failures, tooBig);
		}
}

TEST_CASE("stamp plans are stable: same inputs give the same plan, and the goldens do not move")
{
	const LoadResult cat = loadJungle();
	const Layout l = layout::generate(layout::Shape::Corridor, layout::Size::Large, 1);
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

// --- The build plan (outdoor_plan.h): what the engine hook replays ---------------------------------------------------

namespace {

MapRequest jungleRequest(uint64_t seed)
{
	MapRequest r;
	r.tileset = "Jungle";
	r.shape = "Winding";
	r.size = "Large";
	r.seed = seed;
	r.options.originX = 6000;
	r.options.originY = 3000;
	r.options.maxTilesX = 384;
	r.options.maxTilesY = 384;
	return r;
}

// What the engine's grids would hold after the plan is replayed: each stamp's origin cell carries a piece, every cell it covers is
// flagged 0x200, each listed void cell is 0x100 with no piece.
GridSample gridOf(const StampPlan &plan)
{
	GridSample g;
	const size_t n = static_cast<size_t>(plan.cellsW) * plan.cellsH;
	g.piece.assign(n, 0);
	g.flags.assign(n, 0);
	const int span = plan.cellsW / plan.macroW;
	for (const Stamp &s : plan.stamps) {
		for (int dy = 0; dy < span; ++dy)
			for (int dx = 0; dx < span; ++dx) g.flags[static_cast<size_t>(s.cy + dy) * plan.cellsW + s.cx + dx] = 0x200;
		g.piece[static_cast<size_t>(s.cy) * plan.cellsW + s.cx] = static_cast<uint64_t>(s.lvlPrest);
	}
	for (const layout::Point &p : plan.voidCells) g.flags[static_cast<size_t>(p.y) * plan.cellsW + p.x] = 0x100;
	return g;
}

} // namespace

TEST_CASE("the built-in Jungle catalogue is the fixture's rows, column for column")
{
	const LoadResult fixture = loadJungle();
	REQUIRE(fixture.ok());
	const Catalogue &builtin = builtinJungleCatalogue();
	REQUIRE(builtin.pieces.size() == fixture.catalogue.pieces.size());
	for (size_t i = 0; i < builtin.pieces.size(); ++i) {
		const Piece &a = builtin.pieces[i], &b = fixture.catalogue.pieces[i];
		CAPTURE(i);
		CHECK(a.tileset == b.tileset);
		CHECK(a.builder == b.builder);
		CHECK(a.levelType == b.levelType);
		CHECK(a.lvlPrest == b.lvlPrest);
		CHECK(a.name == b.name);
		CHECK(a.w == b.w);
		CHECK(a.h == b.h);
		for (int e = 0; e < 4; ++e) CHECK(a.edge[e] == b.edge[e]);
		CHECK(a.role == b.role);
		CHECK(a.weight == b.weight);
		CHECK(a.tags == b.tags);
		CHECK(a.check == b.check);
		CHECK(a.ds1 == b.ds1);
		CHECK(a.openMask == b.openMask);
	}
	// The picker gives the same answer on both, so nothing the picker reads was lost in the copy.
	const Layout l = layout::generate(layout::Shape::Branching, layout::Size::ExtraLarge, 77);
	CHECK(picksHash(pickPieces(l, builtin, "Jungle", 77)) == picksHash(pickPieces(l, fixture.catalogue, "Jungle", 77)));
}

TEST_CASE("a Jungle map plans: layout, picks and stamps agree, the rect is the slot's origin plus the plan's size")
{
	const MapPlan plan = planOutdoorMap(jungleRequest(4242), builtinJungleCatalogue());
	REQUIRE(plan.ok);
	CHECK(plan.reason.empty());
	CHECK(plan.stamps.ok());
	CHECK(!plan.stamps.stamps.empty());
	CHECK(plan.hash != 0);
	const LevelRect rect = levelRectOf(plan.stamps);
	CHECK(rect.x == 6000);
	CHECK(rect.y == 3000);
	CHECK(rect.w == plan.stamps.macroW * 32);
	CHECK(rect.h == plan.stamps.macroH * 32);
	CHECK(rect.w <= 384);
	CHECK(rect.h <= 384);
	CHECK(plan.stamps.entrance.known);
	CHECK(plan.stamps.entrance.x >= 6000);
	// The same request plans the same level.
	const MapPlan again = planOutdoorMap(jungleRequest(4242), builtinJungleCatalogue());
	REQUIRE(again.ok);
	CHECK(again.hash == plan.hash);
	CHECK(again.cacheKey == plan.cacheKey);
	CHECK(planOutdoorMap(jungleRequest(4243), builtinJungleCatalogue()).hash != plan.hash);
	// Every stamp is a jungle mask piece (530..544) or a clearing (575..604); never 529 (Act 3 town) or the Docks and Travincal transitions.
	for (const Stamp &s : plan.stamps.stamps) {
		CHECK(s.lvlPrest >= 530);
		CHECK(s.lvlPrest <= 604);
		CHECK(s.lvlPrest != 573);
		CHECK(s.lvlPrest != 574);
	}
}

TEST_CASE("the build plan builds the Open shape on Jungle at every size, and a plan is the same twice")
{
	const Catalogue &cat = builtinJungleCatalogue();
	for (const char *size : {"Small", "Medium", "Large", "ExtraLarge"})
		for (uint64_t seed = 1; seed <= 25; ++seed) {
			MapRequest r = jungleRequest(seed);
			r.shape = "Open";
			r.size = size;
			const MapPlan plan = planOutdoorMap(r, cat);
			INFO(size << " seed " << seed << ": " << plan.reason);
			REQUIRE(plan.ok);
			CHECK(plan.layout.rulesMet);
			CHECK(!plan.stamps.stamps.empty());
			CHECK(planOutdoorMap(r, cat).hash == plan.hash);
			// Open stamps every block, so ExtraLarge is capped at the Large grid (9 x 9 blocks).
			if (std::string(size) == "ExtraLarge") {
				CHECK(plan.layout.width == 9);
				CHECK(plan.layout.height == 9);
				CHECK(plan.stamps.stamps.size() == 81);
			}
		}
}

TEST_CASE("a shapes.txt Open row with a large Width and Height is capped at 81 stamps, and a MinPath the capped grid cannot reach is refused")
{
	const Catalogue &cat = builtinJungleCatalogue();
	const layout::ParseResult parsed = layout::parseShapesTxt("Name\tBase\tMinPath\tMaxPath\tWidth\tHeight\nHuge Plains\tOpen\t\t\t20\t20\nLong Plains\tOpen\t20\t60\t20\t20\n");
	REQUIRE(parsed.errors.empty());
	MapRequest r = jungleRequest(7);
	r.shape = "Huge Plains";
	r.size = "ExtraLarge";
	r.shapes = &parsed.table;
	const MapPlan plan = planOutdoorMap(r, cat);
	REQUIRE_MESSAGE(plan.ok, plan.reason);
	CHECK(plan.layout.width == 9);
	CHECK(plan.layout.height == 9);
	CHECK(plan.stamps.stamps.size() == 81);
	r.shape = "Long Plains";
	const MapPlan refused = planOutdoorMap(r, cat);
	CHECK_FALSE(refused.ok);
	CHECK(refused.stamps.stamps.empty());
	CHECK(refused.reason.find("MinPath") != std::string::npos);
}

TEST_CASE("capJungleOpenRules names the 81-cell limit for a MinPath above it, and no seed or Width/Height advice")
{
	layout::Rules rules;
	std::string ignored;
	const layout::ShapeTable none;
	REQUIRE(layout::resolveRules(none, "Open", layout::Size::ExtraLarge, rules, ignored));
	rules.width = 20;
	rules.height = 20;
	rules.pathMin = 90;
	rules.pathMax = 120;
	const std::string problem = capJungleOpenRules(rules);
	CHECK(problem == "an Open map on Jungle is at most 9 x 9 cells, so MinPath can't be more than 81.");
	CHECK(problem.find("MaxPath") == std::string::npos);

	layout::Rules reach;
	REQUIRE(layout::resolveRules(none, "Open", layout::Size::ExtraLarge, reach, ignored));
	reach.width = 20;
	reach.height = 20;
	reach.pathMin = 20;
	reach.pathMax = 60;
	const std::string unreachable = capJungleOpenRules(reach);
	CHECK(unreachable == "an Open map on Jungle is capped at 9 x 9 cells, and MinPath 20 is too long for it.");
	CHECK(unreachable.find("seed") == std::string::npos);
}

TEST_CASE("the build plan refuses what the game must build itself: other tilesets, bad shape or size, a level past the declared size")
{
	const Catalogue &cat = builtinJungleCatalogue();
	MapRequest lava = jungleRequest(1);
	lava.tileset = "Lava";
	const MapPlan s = planOutdoorMap(lava, cat);
	CHECK(!s.ok);
	CHECK(s.reason.find("Lava") != std::string::npos);
	CHECK(s.stamps.stamps.empty());

	MapRequest size = jungleRequest(1);
	size.size = "Huge";
	CHECK(planOutdoorMap(size, cat).reason.find("Huge") != std::string::npos);

	MapRequest shape = jungleRequest(1);
	shape.shape = "Zigzag";
	const MapPlan z = planOutdoorMap(shape, cat);
	CHECK(!z.ok);
	CHECK(z.reason.find("Zigzag") != std::string::npos);

	MapRequest tight = jungleRequest(1);
	tight.options.maxTilesX = 32;
	tight.options.maxTilesY = 32;
	const MapPlan t = planOutdoorMap(tight, cat);
	CHECK(!t.ok);
	CHECK(!t.reason.empty());
	CHECK(t.stamps.stamps.empty());
	CHECK(t.stamps.voidCells.empty());

	// A blank shape and size mean Winding and Large; a blank tileset is not Jungle.
	MapRequest blank = jungleRequest(9);
	blank.shape.clear();
	blank.size.clear();
	CHECK(planOutdoorMap(blank, cat).ok);
	blank.tileset.clear();
	CHECK(!planOutdoorMap(blank, cat).ok);

	// An empty catalogue picks nothing: a plain refusal, not a crash.
	const Catalogue empty;
	const MapPlan e = planOutdoorMap(jungleRequest(1), empty);
	CHECK(!e.ok);
	CHECK(e.reason.find("pieces") != std::string::npos);
}

TEST_CASE("every shape and size preset on Jungle plans for a range of seeds, or says why not")
{
	const Catalogue &cat = builtinJungleCatalogue();
	int planned = 0;
	for (int s = 0; s < layout::kShapeCount; ++s)
		for (int z = 0; z < layout::kSizeCount; ++z)
			for (uint64_t seed = 1; seed <= 40; ++seed) {
				MapRequest r = jungleRequest(seed);
				r.shape = layout::shapeName(static_cast<layout::Shape>(s));
				r.size = layout::sizeName(static_cast<layout::Size>(z));
				r.options.maxMacroCells = 64;
				r.options.maxTilesX = 0;
				r.options.maxTilesY = 0;
				const MapPlan p = planOutdoorMap(r, cat);
				CAPTURE(r.shape);
				CAPTURE(r.size);
				CAPTURE(seed);
				CAPTURE(p.reason);
				REQUIRE(p.ok);
				++planned;
			}
	CHECK(planned == layout::kShapeCount * layout::kSizeCount * 40);
}

TEST_CASE("the plan cache keys by seed and options, shares plans, drops the oldest and remembers failures")
{
	const Catalogue &cat = builtinJungleCatalogue();
	PlanCache cache(2);
	bool built = false;
	const auto a = cache.get(jungleRequest(1), cat, &built);
	CHECK(built);
	CHECK(a->ok);
	const auto a2 = cache.get(jungleRequest(1), cat, &built);
	CHECK(!built);
	CHECK(a2 == a);  // the same plan object: the server's build and the client's build share it
	CHECK(cache.size() == 1);

	MapRequest moved = jungleRequest(1);
	moved.options.originX = 7000;
	const auto m = cache.get(moved, cat, &built);
	CHECK(built);
	CHECK(m != a);
	CHECK(m->stamps.originX == 7000);
	CHECK(cache.size() == 2);

	cache.get(jungleRequest(2), cat, &built);  // third entry: the oldest (seed 1, origin 6000) is dropped
	CHECK(built);
	CHECK(cache.size() == 2);
	cache.get(jungleRequest(1), cat, &built);
	CHECK(built);

	MapRequest bad = jungleRequest(3);
	bad.tileset = "Lava";
	const auto f = cache.get(bad, cat, &built);
	CHECK(built);
	CHECK(!f->ok);
	const auto f2 = cache.get(bad, cat, &built);
	CHECK(!built);  // the same seed fails the same way
	CHECK(f2 == f);

	// Case and the shape's spelling do not make a second plan.
	PlanCache wide(8);
	MapRequest upper = jungleRequest(5);
	upper.tileset = "JUNGLE";
	upper.shape = "winding";
	wide.get(jungleRequest(5), cat);
	wide.get(upper, cat, &built);
	CHECK(!built);
	CHECK(wide.size() == 1);
	wide.clear();
	CHECK(wide.size() == 0);
	CHECK(PlanCache(0).size() == 0);  // a capacity of 0 is raised to 1, not a crash
}

TEST_CASE("compareGrid agrees with a replayed plan and names every way a grid can differ")
{
	// An L-shaped layout has void cells in its empty macro-cell; a line does not.
	Layout l;
	l.width = 2;
	l.height = 2;
	l.cells.resize(4);
	l.cells[0] = {static_cast<uint8_t>(layout::kEast | layout::kSouth), Role::Entrance};
	l.cells[1] = {layout::kWest, Role::Boss};
	l.cells[2] = {layout::kNorth, Role::Path};
	const LoadResult cat = loadJungle();
	const PickResult picks = pickPieces(l, cat.catalogue, "Jungle", 3);
	REQUIRE(picks.ok());
	const StampPlan plan = planStamps(l, picks, StampOptions{});
	REQUIRE(plan.ok());
	REQUIRE(!plan.voidCells.empty());

	GridSample g = gridOf(plan);
	GridCounts counts;
	CHECK(compareGrid(plan, g, &counts).empty());
	CHECK(counts.cells == plan.cellsW * plan.cellsH);
	CHECK(counts.pieces == static_cast<int>(plan.stamps.size()));
	CHECK(counts.voided == static_cast<int>(plan.voidCells.size()));
	CHECK(counts.other == 0);
	CHECK(counts.covered == counts.pieces * (plan.cellsW / plan.macroW) * (plan.cellsW / plan.macroW));

	GridSample noPiece = g;
	noPiece.piece[static_cast<size_t>(plan.stamps[0].cy) * plan.cellsW + plan.stamps[0].cx] = 0;
	CHECK(compareGrid(plan, noPiece)[0].find("left no piece") != std::string::npos);

	GridSample notVoid = g;
	const layout::Point v = plan.voidCells[0];
	notVoid.flags[static_cast<size_t>(v.y) * plan.cellsW + v.x] = 0;
	CHECK(compareGrid(plan, notVoid)[0].find("should be void") != std::string::npos);

	GridSample voidInside = g;
	voidInside.flags[static_cast<size_t>(plan.stamps[0].cy) * plan.cellsW + plan.stamps[0].cx + 1] |= 0x100;
	CHECK(compareGrid(plan, voidInside)[0].find("void inside") != std::string::npos);

	GridSample shortGrid = g;
	shortGrid.piece.pop_back();
	CHECK(compareGrid(plan, shortGrid)[0].find("expects") != std::string::npos);
	CHECK(!compareGrid(StampPlan{}, GridSample{}).empty());
}

namespace {
// The rooms the engine builds for a plan: 8 x 8 tiles for every stamped cell, row-major, absolute.
std::vector<BuiltRoom> roomsOf(const StampPlan &plan)
{
	std::vector<BuiltRoom> rooms;
	for (const Stamp &s : plan.stamps) {
		for (int ry = 0; ry < 4; ++ry) {
			for (int rx = 0; rx < 4; ++rx) rooms.push_back({plan.originX + s.cx * 8 + rx * 8, plan.originY + s.cy * 8 + ry * 8, 8, 8, 0});
		}
	}
	return rooms;
}
}

TEST_CASE("the entry room is the room at the entrance cell's centre, and the boss and event cells reduce to one exact room")
{
	for (uint64_t seed = 1; seed <= 200; ++seed) {
		const MapPlan plan = planOutdoorMap(jungleRequest(seed), builtinJungleCatalogue());
		if (!plan.ok) continue;
		CAPTURE(seed);
		const std::vector<BuiltRoom> rooms = roomsOf(plan.stamps);
		const ArrivalMark m = planArrival(plan.stamps, rooms);
		REQUIRE(m.mark);
		const BuiltRoom &e = rooms[static_cast<size_t>(m.room)];
		const TileRect &c = plan.stamps.entrance;
		CHECK(e.x >= c.x);
		CHECK(e.x + e.w <= c.x + c.w);
		CHECK(e.y >= c.y);
		CHECK(e.y + e.h <= c.y + c.h);
		CHECK(e.x + e.w / 2 >= c.centreX() - 8);
		CHECK(e.x + e.w / 2 <= c.centreX() + 8);
		CHECK(m.clear.empty());
		// The boss cell: a 32 x 32 clearing inside the level that yields one room; the event cell likewise.
		const TileRect &b = plan.stamps.boss;
		if (b.known) {
			CHECK(b.w == 32);
			CHECK(b.h == 32);
			CHECK(b.x >= plan.stamps.originX);
			CHECK(b.x + b.w <= plan.stamps.originX + plan.stamps.tilesW);
			const int br = chooseCellRoom(b, rooms);
			REQUIRE(br >= 0);
			CHECK(br != m.room);
			if (plan.stamps.event.known) CHECK(br != chooseCellRoom(plan.stamps.event, rooms));
		}
		if (plan.stamps.event.known) CHECK(chooseCellRoom(plan.stamps.event, rooms) >= 0);
	}
}

TEST_CASE("the arrival mark moves: other marked rooms are cleared, and a build that is not the plan is left to the game")
{
	const MapPlan plan = planOutdoorMap(jungleRequest(4242), builtinJungleCatalogue());
	REQUIRE(plan.ok);
	std::vector<BuiltRoom> rooms = roomsOf(plan.stamps);
	rooms[0].flags = 0x10000;
	rooms[1].flags = 0x20000 | 4;
	const ArrivalMark m = planArrival(plan.stamps, rooms);
	REQUIRE(m.mark);
	CHECK(m.clear == std::vector<int>({0, 1}));

	// An entrance cell with no room in it: the game keeps its own arrival.
	std::vector<BuiltRoom> hollow;
	for (const BuiltRoom &r : rooms) {
		const TileRect &e = plan.stamps.entrance;
		if (!(r.x >= e.x && r.y >= e.y && r.x + r.w <= e.x + e.w && r.y + r.h <= e.y + e.h)) hollow.push_back(r);
	}
	const ArrivalMark none = planArrival(plan.stamps, hollow);
	CHECK(!none.mark);
	CHECK(none.why.find("the entrance cell holds no room") != std::string::npos);

	std::vector<BuiltRoom> outside = rooms;
	outside.push_back({plan.stamps.originX - 8, plan.stamps.originY, 8, 8, 0});
	const ArrivalMark bad = planArrival(plan.stamps, outside);
	CHECK(!bad.mark);
	CHECK(bad.why.find("outside the plan's level rectangle") != std::string::npos);
	CHECK(!planArrival(plan.stamps, {}).mark);

	// A room that straddles the cell's edge is not in the cell; an unknown cell gives no room.
	TileRect cell;
	cell.known = true;
	cell.w = 32;
	cell.h = 32;
	CHECK(chooseCellRoom(cell, {{28, 0, 8, 8, 0}}) == -1);
	CHECK(chooseCellRoom(TileRect{}, {{0, 0, 8, 8, 0}}) == -1);
	// Nearest the centre wins; a tie goes to the smaller y, then the smaller x.
	CHECK(chooseCellRoom(cell, {{0, 0, 8, 8, 0}, {12, 12, 8, 8, 0}}) == 1);
	CHECK(chooseCellRoom(cell, {{16, 16, 8, 8, 0}, {8, 8, 8, 8, 0}, {16, 8, 8, 8, 0}, {8, 16, 8, 8, 0}}) == 1);
}

TEST_CASE("a Snow map plans through the same entry point: stamps only, on the 16-tile grid, and the grid comparison agrees")
{
	const Catalogue &cat = builtinJungleCatalogue();
	CHECK(outdoorTilesetKnown("snow"));
	CHECK(outdoorTilesetKnown("Jungle"));
	CHECK_FALSE(outdoorTilesetKnown("Lava"));
	MapRequest r;
	r.tileset = "Snow";
	r.shape = "Winding";  // ignored: Snow has no shapes
	r.size = "";          // Large: 8 x 5 cells of 16 tiles
	r.seed = 4242;
	r.options.originX = 6000;
	r.options.originY = 3600;
	r.options.maxTilesX = 192;
	r.options.maxTilesY = 192;
	const MapPlan plan = planOutdoorMap(r, cat);
	REQUIRE_MESSAGE(plan.ok, plan.reason);
	CHECK(plan.layout.cells.empty());
	CHECK(plan.picks.cells.empty());
	CHECK(plan.stamps.stamps.size() == 40);
	CHECK(plan.stamps.tilesW == 128);
	CHECK(plan.stamps.tilesH == 80);
	CHECK(plan.stamps.originX == 6000);
	CHECK(plan.stamps.originY == 3600);
	CHECK(plan.stamps.entrance.known);
	for (const Stamp &st : plan.stamps.stamps) {
		CHECK(st.lvlPrest >= 987);
		CHECK(st.lvlPrest <= 1002);
	}
	GridSample g = gridOf(plan.stamps);
	CHECK(compareGrid(plan.stamps, g).empty());
	const LevelRect rect = levelRectOf(plan.stamps);
	CHECK(rect.w == 128);
	CHECK(rect.h == 80);

	const MapPlan again = planOutdoorMap(r, cat);
	CHECK(again.hash == plan.hash);
	MapRequest other = r;
	other.seed = 4243;
	CHECK(planOutdoorMap(other, cat).hash != plan.hash);

	MapRequest big = r;
	big.size = "ExtraLarge";  // 160 x 112 tiles
	big.options.maxTilesY = 100;
	const MapPlan tooBig = planOutdoorMap(big, cat);
	CHECK_FALSE(tooBig.ok);
	CHECK(tooBig.reason.find("112") != std::string::npos);
	big.options.maxTilesY = 192;
	CHECK(planOutdoorMap(big, cat).ok);

	MapRequest huge = r;
	huge.size = "Huge";
	CHECK(planOutdoorMap(huge, cat).reason.find("Huge") != std::string::npos);

	PlanCache cache(4);
	bool built = false;
	const auto first = cache.get(r, cat, &built);
	CHECK(built);
	CHECK(first->ok);
	const auto second = cache.get(r, cat, &built);
	CHECK_FALSE(built);
	CHECK(second == first);
	MapRequest jungle = jungleRequest(4242);
	CHECK(cache.get(jungle, cat, &built)->hash != first->hash);  // Jungle and Snow never share a cache entry
	CHECK(built);
}

// --- The tileset table (outdoor_tilesets.h) ----------------------------------------------------------------------------------

TEST_CASE("the outdoor tileset table holds Jungle, Snow then Siege, with the values the hooks and the installer rely on")
{
	// Pinned: these are the slot rows, levels.txt names and LevelTypes of shipped installs. A change here is a compatibility change.
	REQUIRE(OutdoorTilesetCount == 3);
	CHECK(std::string(OutdoorTilesets[0].name) == "Jungle");
	CHECK(OutdoorTilesets[0].levelType == 21);
	CHECK(std::string(OutdoorTilesets[0].slot) == "Outdoor");
	CHECK(std::string(OutdoorTilesets[0].levelName) == "Cartographer Outdoor");
	CHECK(OutdoorTilesets[0].planner == Planner::Jungle);
	CHECK(std::string(OutdoorTilesets[1].name) == "Snow");
	CHECK(OutdoorTilesets[1].levelType == 31);
	CHECK(std::string(OutdoorTilesets[1].slot) == "OutdoorSnow");
	CHECK(std::string(OutdoorTilesets[1].levelName) == "Cartographer Snow");
	CHECK(OutdoorTilesets[1].planner == Planner::Snow);
	CHECK(std::string(OutdoorTilesets[2].name) == "Siege");
	CHECK(OutdoorTilesets[2].levelType == 30);
	CHECK(std::string(OutdoorTilesets[2].slot) == "OutdoorSiege");
	CHECK(std::string(OutdoorTilesets[2].levelName) == "Cartographer Siege");
	CHECK(OutdoorTilesets[2].planner == Planner::Siege);
	// Lookups ignore case; an unknown name has no row and slot index 0 (Jungle's), as the hooks always assumed.
	CHECK(findOutdoorTileset("sNoW") == &OutdoorTilesets[1]);
	CHECK(findOutdoorTileset("JUNGLE") == &OutdoorTilesets[0]);
	CHECK(findOutdoorTileset("siege") == &OutdoorTilesets[2]);
	CHECK(findOutdoorTileset("Lava") == nullptr);
	CHECK(findOutdoorTilesetBySlot("outdoorsnow") == &OutdoorTilesets[1]);
	CHECK(findOutdoorTilesetBySlot("Maze") == nullptr);
	CHECK(outdoorTilesetIndex("snow") == 1);
	CHECK(outdoorTilesetIndex("Siege") == 2);
	CHECK(outdoorTilesetIndex("Jungle") == 0);
	CHECK(outdoorTilesetIndex("Lava") == 0);
	CHECK(outdoorTilesetNames() == "Jungle, Snow or Siege");
	MapRequest r;
	r.tileset = "Lava";
	const MapPlan refused = planOutdoorMap(r, builtinJungleCatalogue());
	CHECK_FALSE(refused.ok);
	CHECK(refused.reason == "Cartographer has no outdoor pieces for the tileset 'Lava'. Use Jungle, Snow or Siege.");
}

TEST_CASE("tools/install.ps1 lists the same outdoor tilesets as the table, in the same order")
{
	std::string here = __FILE__;
	std::ifstream in(here.substr(0, here.find_last_of("/\\") + 1) + "../tools/install.ps1", std::ios::binary);
	REQUIRE(in.good());
	std::stringstream all;
	all << in.rdbuf();
	const std::string text = all.str();
	const size_t start = text.find("$OutdoorTilesets = @(");
	REQUIRE(start != std::string::npos);
	const std::string block = text.substr(start, text.find("\n)", start) - start);
	// Each row reads  @{ Tileset = 'Jungle'; Slot = 'Outdoor'; LevelName = '...'; LevelType = '21'; ...
	const auto field = [](const std::string &row, const char *key) {
		const size_t at = row.find(std::string(key) + " = '");
		REQUIRE(at != std::string::npos);
		const size_t from = at + std::string(key).size() + 4;
		return row.substr(from, row.find('\'', from) - from);
	};
	size_t row = 0, at = 0;
	while ((at = block.find("@{", at)) != std::string::npos) {
		const std::string body = block.substr(at, block.find('}', at) - at);
		REQUIRE(row < OutdoorTilesetCount);
		CHECK(field(body, "Tileset") == OutdoorTilesets[row].name);
		CHECK(field(body, "Slot") == OutdoorTilesets[row].slot);
		CHECK(field(body, "LevelName") == OutdoorTilesets[row].levelName);
		CHECK(field(body, "LevelType") == std::to_string(OutdoorTilesets[row].levelType));
		++row;
		at += 2;
	}
	CHECK(row == OutdoorTilesetCount);
}

TEST_CASE("pickFarthestCell with no anchors or no candidates picks nothing and draws nothing from the rng")
{
	const std::vector<GridCell> cells = {{0, 0}, {1, 0}, {2, 0}};
	layout::Pcg32 rng(77);
	layout::Pcg32 untouched(77);
	GridCell out{9, 9};
	CHECK_FALSE(pickFarthestCell(cells, {}, rng, out));
	CHECK(out == GridCell{9, 9});
	CHECK_FALSE(pickFarthestCell({}, {{0, 0}}, rng, out));
	CHECK(out == GridCell{9, 9});
	CHECK(rng.below(1000000) == untouched.below(1000000));  // the stream did not move
	// With an anchor the farthest cell wins.
	CHECK(pickFarthestCell(cells, {{0, 0}}, rng, out));
	CHECK(out == GridCell{2, 0});
}
