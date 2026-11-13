// Cartographer outdoor Siege (Act 5 Bloody Foothills) plan. Pure C++: no engine headers, no SDK, no floats, no <random>.
//
// The Siege tileset is one line of 16 x 48 tile strips (LvlPrest 865..879, LevelType 30), read west to east: the barricade gate (879), strip 13
// (878), the free wide strips, a fixed funnel of eight strips (875 down to 868), more free wide strips and the town strip (865) at the east end.
// The strips are not Wang tiles: each one has a west and an east seam, a corridor of open rows that has to meet the next strip's corridor.
// The seams were read from the strips' DS1 walk flags (docs/reverse-engineering/cartographer-siege.md); only seams that match are chained.
// The funnel and the two end strips are fixed. The four wide strips 866, 867, 876 and 877 are the only ones that vary: a seed picks how many
// of them the map uses (the size) and their order, from the orders whose seams all match.
//
// The plan is the same StampPlan the Jungle and Snow plans produce, so the outdoor build hook replays it unchanged.
//
// Seen in game (#288): the stamper accepts these ids, the strips are built as 2 x 6 rooms of 8 tiles,
// the Boss and event land in their rooms. Not settled: how the seams look on screen (the funnel pair 874/873 differs by one row at the wall) and the
// north and south edge (KillEdge).
#pragma once
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "stamp.h"

namespace cartographer::outdoor::siege {

constexpr int kLevelType = 30;     // lvltypes.txt "Act 5 - Siege"
constexpr int kStripWidth = 16;    // tiles, west to east
constexpr int kStripHeight = 48;   // tiles
constexpr int kGridTiles = 8;      // the engine's grid cell
constexpr int kLevelTilesX = 240;  // the slot level: 15 strips
constexpr int kLevelTilesY = 48;
constexpr int kFirstLvlPrest = 865;
constexpr int kLastLvlPrest = 879;
constexpr int kFixedStrips = 11;   // the two ends, strip 13 and the eight strips of the funnel block
constexpr int kMinStrips = 12;     // at least one free strip is needed to match 878 to the funnel block
constexpr int kMaxStrips = 15;     // every strip once: the game's own Bloody Foothills

// Where a strip may stand.
enum class Role : uint8_t {
	WestCap,  // 879: the barricade gate, the west end
	EastCap,  // 865: the town, the east end
	Fixed,    // 878 and the funnel block 875..868: always in the same places
	Free,     // 866, 867, 876, 877: the strips the seed places
};
const char *roleName(Role role);

// Seam sockets are letters: the west and the east edge of a strip each carry one, and two neighbours must carry the same letter. A to E are the
// wide seams (open rows 5..40, 5..39, 5..37, 7..40 and 10..38), F to J the funnel's narrower ones. '\0' is the closed side of a cap.
struct Piece {
	int lvlPrest;      // 865..879
	const char *name;  // the lvlprest.txt name
	const char *ds1;   // the DS1 file under data/global/tiles
	Role role;
	char west;         // seam socket on the west edge
	char east;         // seam socket on the east edge
};

// The 15 rows, in lvlPrest order.
const std::vector<Piece> &pieces();
const Piece *findPiece(int lvlPrest);

// The fixed order of the funnel block, west to east (875, 874, 873, 872, 871, 870, 869, 868).
const std::vector<int> &funnelBlock();

struct SiegeOptions {
	int originX = 0;       // the level's origin in tiles (the slot row's OffsetX / OffsetY)
	int originY = 0;
	int maxTilesX = 0;     // the slot row's declared size limit in tiles; 0 means no limit
	int maxTilesY = 0;
	bool pinFiles = false; // true: pass the drawn DS1 file index (always 0, every strip has one file); false: -1
};

struct SiegeRequest {
	uint64_t seed = 0;
	int strips = 14;       // 12..15 strips of 16 tiles: 192 to 240 tiles
	SiegeOptions options;
};

// Small, Medium, Large or ExtraLarge (blank means Large), case-insensitive. False for any other word. 12, 13, 14 and 15 strips.
bool siegeSizeStrips(std::string_view size, int *strips);

struct SiegePlan {
	bool ok = false;
	std::string reason;                 // plain English, when !ok: why the game must build its own level
	std::vector<int> strips;            // the LvlPrest ids, west to east
	StampPlan stamps;                   // complete only when ok
	uint64_t hash = 0;                  // stampPlanHash of the stamps
};

// Plans a Siege map for a seed: places the free strips, stamps the line and checks the result with checkSiegePlan. Any failure leaves ok false with
// the reason. Never throws for bad input.
SiegePlan planSiegeMap(const SiegeRequest &request);

// Independent check of a plan against its request (does not use planSiegeMap). Empty means: the size is within the limits, the line is the cap,
// 878, the free strips, the funnel block, the free strips and the cap, the free strips are distinct and not repeated, every seam matches, the
// stamps are in order on the 8-tile grid, pinned files are in range and the entrance, boss and event rectangles sit on their cells.
std::vector<std::string> checkSiegePlan(const SiegeRequest &request, const SiegePlan &plan);

// Two characters per strip (west and east socket) for logs and failed tests, strips separated by a space; '-' is the closed side of a cap.
std::string siegePlanAscii(const SiegePlan &plan);

} // namespace cartographer::outdoor::siege
