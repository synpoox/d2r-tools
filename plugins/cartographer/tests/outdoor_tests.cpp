// Host tests for the Cartographer outdoor piece-picker. No game, no SDK: the picker is pure C++.
// Catalogue data: tests/fixtures/jungle_catalogue.tsv holds the Jungle rows of tools/catalogue.tsv (PR #28, commit
// f62af4f): derived rows, no game files.
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

LoadResult loadJungle()
{
	return loadCatalogueFile(std::string(CARTOGRAPHER_FIXTURE_DIR) + "/jungle_catalogue.tsv");
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
