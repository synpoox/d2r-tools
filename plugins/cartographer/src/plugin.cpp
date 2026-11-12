// Cartographer: D2RLoader plugin entry point. Reads maps.txt and cartographer.txt, writes cartographer-report.txt, and
// opens a map through the map's own item (or "cartographer open <Name>"). Global: no mod needs to ship layouts; the installer
// (tools\install.ps1) adds the slot level to the mod's tables. The items belong to the mod.
//
// Console command "cartographer": "check" re-reads the tables and prints what is wrong; "open <Name>" opens a map without an item.
#include <D2RLPlugin/api.h>

#include "core/coexist.h"
#include "core/tables.h"
#include "core/text.h"
#include "core/tsv.h"
#include "event6b.h"
#include "crash_guard.h"
#include "engine/boss_names.h"
#include "engine/event_apply.h"
#include "engine/item_stats.h"
#include "engine/level_row.h"
#include "engine/map_names.h"
#include "engine/map_sync.h"
#include "engine/maze_hooks.h"
#include "engine/portal.h"
#include "engine/stat_apply.h"
#include "log.h"

#include <windows.h>

#include <cstdio>
#include <cstdlib>
#include <cwchar>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>

namespace {

constexpr const char* PluginName = "cartographer";
constexpr const char* PluginVersion = "0.1.0";  // PluginInfo::version too; CHANGELOG.md

const D2RL::PluginContext* g_context = nullptr;
std::wstring g_tableDir;   // <Mod>.mpq\data\global\excel\d2rloader\cartographer
std::wstring g_excelDir;   // ...\excel: the game's own tables, for the name checks
std::wstring g_reportPath;
std::vector<carto::SignatureOverride> g_overrides;
bool g_loadFailed = false;  // loading threw: the portal was not set up

auto Widen(const char* s) -> std::wstring {
	std::wstring out;
	for (; s != nullptr && *s != '\0'; ++s) {
		out.push_back(static_cast<unsigned char>(*s));
	}
	return out;
}

auto Narrow(const std::wstring& s) -> std::string {
	std::string out;
	for (const wchar_t c : s) {
		out.push_back(c < 0x80 ? static_cast<char>(c) : '?');
	}
	return out;
}

// core-maps and Cartographer are one-or-the-other (ADR-0001). Detected by module name; nothing is changed either way.
auto CoreMapsNotice() -> std::string {
	return carto::CoreMapsNotice(GetModuleHandleW(L"core-maps.dll") != nullptr);
}

auto ReadFile(const std::wstring& path, std::string* text) -> bool {
	const bool drive = path.size() > 2 && path[1] == L':' && (path[2] == L'\\' || path[2] == L'/');
	if (!drive && path.rfind(L"\\\\", 0) != 0) {
		return false;  // only absolute paths: never relative to the current folder or the drive root
	}
	std::ifstream in(path, std::ios::binary);
	if (!in) {
		return false;
	}
	std::ostringstream buffer;
	buffer << in.rdbuf();
	*text = buffer.str();
	return true;
}

// A Cartographer table (d2rloader\cartographer) as UTF-8: a file that is not valid UTF-8 is taken as Latin-1 and converted, with one note.
auto ReadTable(const std::wstring& path, const char* name, std::string* text, carto::Report& report) -> bool {
	std::string raw;
	if (!ReadFile(path, &raw)) {
		return false;
	}
	bool latin1 = false;
	*text = carto::NormalizeTableText(raw, &latin1);
	if (latin1) {
		report.Note(name, 0, "the file is not valid UTF-8, so it was read as Latin-1 and converted. Save it as UTF-8 to keep characters outside Latin-1.");
	}
	return true;
}

// One column of a game table, as a list of cells. False when the table or the column is missing.
auto ReadColumn(const std::wstring& file, const char* column, std::vector<std::string>* out) -> bool {
	std::string text;
	if (!ReadFile(file, &text)) {
		return false;
	}
	const carto::Tsv table = carto::ParseTsv(text);
	const int c = table.Column(column);
	if (c < 0) {
		return false;
	}
	for (const carto::TsvRow& row : table.rows) {
		const std::string cell = table.Cell(row, c);
		if (!cell.empty()) {
			out->push_back(cell);
		}
	}
	return true;
}

// One column of a game table by row: an empty cell keeps its row, because the engine is asked for the row number.
auto ReadColumnRows(const std::wstring& file, const char* column, std::vector<std::string>* out) -> bool {
	std::string text;
	if (!ReadFile(file, &text)) {
		return false;
	}
	const carto::Tsv table = carto::ParseTsv(text);
	const int c = table.Column(column);
	if (c < 0) {
		return false;
	}
	for (const carto::TsvRow& row : table.rows) {
		out->push_back(table.Cell(row, c));
	}
	return true;
}

// monstats.txt Id and superuniques.txt Superunique and Class, by row, for the event spawns, read from the active mod's excel folder.
// The `Expansion` marker line (monstats row 410, superuniques row 42) is dropped from both. This is the fallback: at map open the game's own
// loaded tables are tried first (engine/event_apply.cpp). The row index is the parsed row count, which is the game's row number only if the
// file has no blank lines (ParseTsv drops them): an assumption, unchecked.
// Empty when a table is missing.
auto ReadSpawnNames() -> cartographer::eventplan::SpawnTable {
	cartographer::eventplan::SpawnTable out;
	if (g_excelDir.empty()) {
		return out;
	}
	std::vector<std::string> supers, classes, ids;
	if (ReadColumnRows(g_excelDir + L"\\monstats.txt", "Id", &ids)) {
		out.monsters = cartographer::event6b::dropMarkerLines(ids, ids);
	}
	if (ReadColumnRows(g_excelDir + L"\\superuniques.txt", "Superunique", &supers) && ReadColumnRows(g_excelDir + L"\\superuniques.txt", "Class", &classes)) {
		classes.resize(supers.size());
		const std::vector<std::string> keptClasses = cartographer::event6b::dropMarkerLines(supers, classes);
		const std::vector<std::string> keptSupers = cartographer::event6b::dropMarkerLines(supers, supers);
		for (size_t i = 0; i < keptSupers.size(); ++i) {
			out.supers.push_back({keptSupers[i], keptClasses[i]});
		}
	}
	return out;
}

// misc.txt code and pSpell for every item that has a code (a blank pSpell is 0). False when the table or a column is missing.
auto ReadItemSpells(const std::wstring& file, std::vector<std::pair<std::string, int>>* out) -> bool {
	std::string text;
	if (!ReadFile(file, &text)) {
		return false;
	}
	const carto::Tsv table = carto::ParseTsv(text);
	const int cCode = table.Column("code"), cSpell = table.Column("pSpell");
	if (cCode < 0 || cSpell < 0) {
		return false;
	}
	for (const carto::TsvRow& row : table.rows) {
		const std::string code = table.Cell(row, cCode);
		if (!code.empty()) {
			out->emplace_back(code, std::atoi(table.Cell(row, cSpell).c_str()));
		}
	}
	return true;
}

// monstats.txt Ids whose boss column is 1. False when the table or either column is missing.
auto ReadBossIds(const std::wstring& file, std::vector<std::string>* out) -> bool {
	std::string text;
	if (!ReadFile(file, &text)) {
		return false;
	}
	const carto::Tsv table = carto::ParseTsv(text);
	const int cId = table.Column("Id"), cBoss = table.Column("boss");
	if (cId < 0 || cBoss < 0) {
		return false;
	}
	for (const carto::TsvRow& row : table.rows) {
		if (table.Cell(row, cBoss) == "1" && !table.Cell(row, cId).empty()) {
			out->push_back(table.Cell(row, cId));
		}
	}
	return true;
}

auto ReadGameNames() -> carto::GameNames {
	carto::GameNames names;
	if (g_excelDir.empty()) {
		return names;
	}
	names.haveMonsters = ReadColumn(g_excelDir + L"\\monstats.txt", "Id", &names.monsters);
	names.haveSuperUniques = ReadColumn(g_excelDir + L"\\superuniques.txt", "Superunique", &names.superUniques);
	names.haveBossFlags = ReadBossIds(g_excelDir + L"\\monstats.txt", &names.bossMonsters);
	std::vector<std::string> modIds;
	if (ReadColumn(g_excelDir + L"\\monumod.txt", "id", &modIds)) {
		for (const std::string& id : modIds) {
			char* end = nullptr;
			const long v = std::strtol(id.c_str(), &end, 10);
			if (end != id.c_str() && *end == '\0' && v >= 0 && v <= 255) {
				names.monsterMods.push_back(static_cast<int>(v));
			}
		}
		names.haveMods = !names.monsterMods.empty();
	}
	names.haveItems = true;
	for (const wchar_t* file : {L"\\misc.txt", L"\\weapons.txt", L"\\armor.txt"}) {
		names.haveItems = ReadColumn(g_excelDir + file, "code", &names.itemCodes) && names.haveItems;
	}
	names.haveItemSpells = ReadItemSpells(g_excelDir + L"\\misc.txt", &names.itemSpells);
	return names;
}

struct Loaded {
	carto::Report report;
	std::shared_ptr<carto::Tables> tables = std::make_shared<carto::Tables>();
};

// Reads the tables. A missing maps.txt or cartographer.txt is fine: there are no maps, and the defaults apply.
// shapes.txt, mapstats.txt and mapevents.txt are optional too.
auto LoadAll() -> Loaded {
	Loaded loaded;
	if (g_tableDir.empty()) {
		// No mod: no tables are read. There are no maps, and the portal has no level to open.
		loaded.report.Note("maps.txt", 0, "no mod is active, so no tables were read. There are no maps; only the default settings apply.");
		return loaded;
	}
	const carto::GameNames names = ReadGameNames();
	if (!names.haveMonsters || !names.haveSuperUniques || !names.haveItems) {
		loaded.report.Note("maps.txt", 0, "some of the game's tables could not be read, so Monsters, Boss and Item names are not checked.");
	}
	if (!names.haveItemSpells) {
		loaded.report.Note("maps.txt", 0, "misc.txt or its pSpell column could not be read, so the check that each Item has item spell 16 was skipped. An item without spell 16 will never open its map.");
	}
	std::string text;
	// shapes.txt first: a map's Shape may name one of its rows. shapes.txt, mapstats.txt and mapevents.txt are all optional.
	if (ReadTable(g_tableDir + L"\\" + Widen(carto::ShapesFile), carto::ShapesFile, &text, loaded.report)) {
		loaded.tables->shapes = carto::LoadShapes(text, loaded.report);
	}
	text.clear();
	if (ReadTable(g_tableDir + L"\\" + Widen(carto::MapStatsFile), carto::MapStatsFile, &text, loaded.report)) {
		loaded.tables->pool = carto::LoadMapStats(text, loaded.report);
	}
	// The stat names are checked against itemstatcost.txt, which also gives the ids the game is asked for.
	text.clear();
	if (ReadFile(g_excelDir + L"\\itemstatcost.txt", &text)) {
		loaded.tables->statIds = cartographer::statplan::parseItemStatCost(text);
	}
	if (!loaded.tables->statIds.loaded) {
		loaded.report.Note(carto::MapStatsFile, 0, "itemstatcost.txt could not be read, so Stat names are not checked and no map stats can be applied.");
	} else {
		const bool builtIn = loaded.tables->pool.empty();
		const auto& checked = builtIn ? cartographer::stats::builtInPool() : loaded.tables->pool;
		for (const cartographer::stats::PoolIssue& issue : cartographer::statplan::checkPoolStats(checked, loaded.tables->statIds)) {
			loaded.report.Error(carto::MapStatsFile, 0, (builtIn ? "built-in pool " : "") + issue.message);
		}
	}
	text.clear();
	if (ReadTable(g_tableDir + L"\\" + Widen(carto::MapEventsFile), carto::MapEventsFile, &text, loaded.report)) {
		loaded.tables->events = carto::LoadMapEvents(text, loaded.report);
	}
	loaded.tables->spawnNames = ReadSpawnNames();
	// objects.txt Name by row, for a Shrine. ParseTsv drops blank lines, so a file with one fails the line-count check against the
	// game's row count (engine/event_6b.cpp) and its Shrine is then given by row number instead.
	// The `Expansion` marker line has no compiled row (588 lines, 587 rows in the stock file), so it is dropped here (issue #135).
	{
		std::vector<std::string> classes, names;
		if (!g_excelDir.empty() && ReadColumnRows(g_excelDir + L"\\objects.txt", "Class", &classes) && ReadColumnRows(g_excelDir + L"\\objects.txt", "Name", &names)) {
			loaded.tables->objectNames = cartographer::event6b::objectRowNames(classes, names);
			loaded.tables->objectClasses = cartographer::event6b::dropMarkerLines(classes, classes);
		} else {
			loaded.tables->objectNames.clear();
			loaded.tables->objectClasses.clear();
		}
	}
	// Boss and Horde names are looked up in the game's loaded tables at map open; these two files are only the fallback for when that
	// route is off (checked then, not here). Without monstats.txt that fallback has nothing; without superuniques.txt it has no superunique.
	if (!loaded.tables->events.empty()) {
		if (!loaded.tables->spawnNames.loaded()) {
			loaded.report.Note("mapevents.txt", 0, "monstats.txt could not be read from the mod's excel folder. Boss and Horde events use the game's own tables when they check out (see the plugin log); otherwise they will not be spawned or announced.");
		} else if (loaded.tables->spawnNames.supers.empty()) {
			loaded.report.Note("mapevents.txt", 0, "superuniques.txt could not be read from the mod's excel folder. Boss and Horde events with a superunique name use the game's own tables when they check out (see the plugin log); otherwise only monsters from monstats.txt are spawned.");
		}
	}
	text.clear();
	if (ReadTable(g_tableDir + L"\\" + Widen(carto::BossesFile), carto::BossesFile, &text, loaded.report)) {
		loaded.tables->bosses = carto::LoadBosses(text, names, loaded.report);
	}
	text.clear();
	std::vector<carto::MapDef> fromFile;
	if (ReadTable(g_tableDir + L"\\" + Widen(carto::MapsFile), carto::MapsFile, &text, loaded.report)) {
		fromFile = carto::LoadMaps(text, names, loaded.report, &loaded.tables->shapes, &loaded.tables->bosses);
	}
	loaded.tables->maps = std::move(fromFile);
	text.clear();
	if (ReadTable(g_tableDir + L"\\" + Widen(carto::SettingsFile), carto::SettingsFile, &text, loaded.report)) {
		loaded.tables->settings = carto::LoadSettings(text, loaded.report);
	}
	carto::StatApplySetPlayerStats(loaded.tables->settings.playerStats != 0);
	text.clear();
	if (ReadTable(g_tableDir + L"\\" + Widen(carto::SlotsFile), carto::SlotsFile, &text, loaded.report)) {
		loaded.tables->slots = carto::LoadSlots(text, loaded.report);
	} else {
		loaded.report.Error(carto::SlotsFile, 0, "the file is missing, so the map item has no level to open. Run tools\\install.ps1 and restart the game.");
	}
	// A slot must be a level the installer added: drop the others (a vanilla level, a town, no row) before any hook sees them.
	text.clear();
	ReadFile(g_excelDir + L"\\levels.txt", &text);
	carto::ValidateSlots(loaded.tables->slots, text, loaded.report);
	// A Maze slot is one LevelType (3 Cave or 4 Crypt): name each Cave/Crypt map it cannot build. An Outdoor slot's origin, declared size and
	// act are read from the same file for the outdoor build hook.
	if (carto::SlotLevel(loaded.tables->slots, carto::MazeSlot) > 0 || carto::HasOutdoorSlot(loaded.tables->slots)) {
		carto::ReadSlotLevelTypes(loaded.tables->slots, text);
		for (const carto::MapDef& map : loaded.tables->maps) {
			const std::string problem = carto::MapIsOutdoor(map) ? carto::OutdoorSlotProblem(map, loaded.tables->slots) : carto::MazeTilesetProblem(map, *loaded.tables);
			if (!problem.empty()) {
				loaded.report.Error(carto::MapsFile, map.line, problem.substr(problem.rfind("Cartographer: ") == 0 ? 14 : 0));
			}
		}
	}
	return loaded;
}

// signatures.txt (Part, Signature): signatures for engine parts, so a new game build needs no rebuild.
void LoadOverrides() {
	g_overrides.clear();
	std::string text;
	if (g_tableDir.empty() || !ReadFile(g_tableDir + L"\\signatures.txt", &text)) {
		return;
	}
	const carto::Tsv table = carto::ParseTsv(text);
	const int part = table.Column("Part"), signature = table.Column("Signature");
	for (const carto::TsvRow& row : table.rows) {
		if (part >= 0 && signature >= 0 && !table.Cell(row, part).empty() && !table.Cell(row, signature).empty()) {
			g_overrides.push_back({table.Cell(row, part), table.Cell(row, signature)});
		}
	}
}

void WriteReport(const Loaded& loaded) {
	if (g_reportPath.empty()) {
		return;
	}
	std::ofstream out(g_reportPath, std::ios::binary | std::ios::trunc);
	if (!out) {
		plog::Warn("could not write the report to %s", Narrow(g_reportPath).c_str());
		return;
	}
	out << "Cartographer " << PluginVersion << " report\r\n";
	out << loaded.tables->maps.size() << " map(s) loaded, " << loaded.report.Errors() << " problem(s).\r\n";
	const std::string coreMaps = CoreMapsNotice();
	if (!coreMaps.empty()) {
		out << "WARNING: " << coreMaps << "\r\n";
	}
	out << loaded.tables->shapes.shapes.size() << " shape(s), " << loaded.tables->pool.size() << " property row(s) and " << loaded.tables->events.size() << " event(s) loaded. Shapes and events are read and checked only; the properties are applied when a map opens (" << carto::StatApplySummary() << ").\r\n\r\n";
	for (const carto::Problem& p : loaded.report.problems) {
		out << carto::Report::Line(p) << "\r\n";
	}
	if (loaded.report.problems.empty()) {
		out << "Nothing to fix.\r\n";
	}
}

// Reads the tables, writes the report, hands the tables to the hooks.
auto Reload() -> Loaded {
	Loaded loaded = LoadAll();
	WriteReport(loaded);
	carto::SetTables(loaded.tables);
	carto::BossNamesSetTables(loaded.tables);
	for (const carto::Problem& p : loaded.report.problems) {
		plog::Warn("%s", carto::Report::Line(p).c_str());
	}
	if (const std::string coreMaps = CoreMapsNotice(); !coreMaps.empty()) {
		plog::Warn("%s", coreMaps.c_str());
	}
	return loaded;
}

// core-maps may load after Cartographer, so the load-time check can miss it. The first game start re-checks, once.
void OnFirstGameStart() noexcept {
	try {
		if (!CoreMapsNotice().empty()) {
			(void)Reload();
		}
	} catch (...) {
	}
}

void Say(const D2RL::ConsoleCommandContext* command, const std::string& line) {
	command->plugin->WriteConsoleMessage(line.c_str());
	plog::Info("%s", line.c_str());
}

auto Command(D2R::Game::Client* client, const D2RL::ConsoleCommandContext* command, void* userData) noexcept -> D2RL::ConsoleCommandResult {
	(void)client;
	(void)userData;
	if (command == nullptr || command->plugin == nullptr) {
		return D2RL::ConsoleCommandResult::Failed;
	}
	const std::string args = carto::Trim(command->args != nullptr ? command->args : "");
	const size_t space = args.find(' ');
	const std::string verb = args.substr(0, space);
	const std::string rest = space == std::string::npos ? std::string() : carto::Trim(args.substr(space + 1));
	try {
		if (carto::EqualsNoCase(verb, "check")) {
			const Loaded loaded = Reload();
			(void)carto::RescanPortal();
			Say(command, std::string("Cartographer ") + PluginVersion + ": " + std::to_string(loaded.tables->maps.size()) + " map(s), " +
			                 std::to_string(loaded.report.Errors()) + " problem(s). The portal is " + (carto::PortalLive() ? "on." : g_loadFailed ? "off; loading failed, see the plugin log." : carto::NeedsRestart() ? "off; a part it hooks was missing at load, so a restart is needed once it is fixed." : "off."));
			if (const std::string coreMaps = CoreMapsNotice(); !coreMaps.empty()) {
				Say(command, "WARNING: " + coreMaps);
			}
			int shown = 0;
			for (const carto::Problem& p : loaded.report.problems) {
				if (++shown > 10) {
					Say(command, "...and more. See " + Narrow(g_reportPath));
					break;
				}
				Say(command, carto::Report::Line(p));
			}
			for (const carto::PartStatus& part : carto::PartStatuses()) {
				Say(command, "part " + part.name + ": " + part.detail);
			}
			Say(command, "Cartographer " + carto::StatApplySummary() + ". These parts are settled at load; fix and restart to change them.");
			for (const carto::PartStatus& part : carto::StatApplyPartStatuses()) {
				Say(command, "part " + part.name + ": " + part.detail);
			}
			for (const std::string& line : carto::StatApplyStateReport()) {
				Say(command, line);
			}
			for (const carto::PartStatus& part : carto::ItemStatsPartStatuses()) {
				Say(command, "part " + part.name + ": " + part.detail);
			}
			Say(command, "Cartographer " + carto::EventsSummary() + ". These parts are settled at load; fix and restart to change them.");
			for (const carto::PartStatus& part : carto::EventPartStatuses()) {
				Say(command, "part " + part.name + ": " + part.detail);
			}
			Say(command, "Cartographer " + carto::MapNamesSummary() + ".");
			for (const carto::PartStatus& part : carto::MapNamesPartStatuses()) {
				Say(command, "part " + part.name + ": " + part.detail);
			}
			Say(command, "Cartographer " + carto::BossNamesSummary() + ".");
			Say(command, "Cartographer " + carto::LevelRowSummary() + ".");
			Say(command, "Cartographer " + carto::MapSyncSummary() + ".");
			Say(command, "Report: " + Narrow(g_reportPath));
			return D2RL::ConsoleCommandResult::Handled;
		}
		if (carto::EqualsNoCase(verb, "open")) {
			if (rest.empty()) {
				Say(command, "Usage: cartographer open <Name>");
				return D2RL::ConsoleCommandResult::InvalidArguments;
			}
			std::string mapName;
			switch (carto::RequestOpen(rest, &mapName)) {
			case carto::OpenStart::NoSuchMap:
				Say(command, "Cartographer: there is no map called '" + rest + "'. Run 'cartographer check'.");
				return D2RL::ConsoleCommandResult::Failed;
			case carto::OpenStart::NotInGame:
				Say(command, "Cartographer: join a game first.");
				return D2RL::ConsoleCommandResult::Failed;
			case carto::OpenStart::Unavailable:
				Say(command, "Cartographer: opening without an item is not available (a game part is missing). Run 'cartographer check' to see which.");
				return D2RL::ConsoleCommandResult::Failed;
			case carto::OpenStart::NoServer:
				Say(command, "Cartographer: a map can only be opened in a game you host, once the game is running.");
				return D2RL::ConsoleCommandResult::Failed;
			case carto::OpenStart::Posted:
				Say(command, "Cartographer: opening " + mapName + "...");
				break;
			}
			return D2RL::ConsoleCommandResult::Handled;
		}
	} catch (...) {
		plog::Error("the cartographer command failed");
		return D2RL::ConsoleCommandResult::Failed;
	}
	Say(command, "Usage: cartographer check | cartographer open <Name>");
	return D2RL::ConsoleCommandResult::InvalidArguments;
}

constexpr D2RL::PluginInfo Info{
	.infoSize = D2RL::PluginInfoSize,
	.abiVersion = D2RL_PLUGIN_ABI_VERSION,
	.id = "cartographer",
	.name = "Cartographer",
	.version = PluginVersion,
	.author = "Synpoo",
	.description = "Opens a map from a map item in Harrogath: a red portal to a level made from maps.txt.",
	.flags = D2RL::PluginFlags::Shared | D2RL::PluginFlags::NativeHooks,
};

}  // namespace

D2RL_PLUGIN_EXPORT auto D2RLoaderGetPluginInfo() noexcept -> const D2RL::PluginInfo* {
	return &Info;
}

D2RL_PLUGIN_EXPORT auto D2RLoaderLoadPlugin(const D2RL::PluginContext* context) noexcept -> bool {
	if (context == nullptr) {
		return false;
	}
	g_context = context;
	plog::Init(context, PluginName);
	crash_guard::Install(PluginVersion, context->exeBase);
	char image[200];
	crash_guard::Describe(image, sizeof(image), D2RL::GetBuildVersion(context));
	plog::Info("%s loaded (mod %s, %s)", PluginVersion, context->activeMod != nullptr && context->activeMod[0] != '\0' ? context->activeMod : "none", image);
	try {
		if (context->modDirectory != nullptr && context->modDirectory[0] != L'\0' && context->activeMod != nullptr && context->activeMod[0] != '\0') {
			// Older loaders pass the mod folder, which holds <Mod>.mpq; D2RLoader 1.3.1 passes the <Mod>.mpq folder itself.
			// <Mod>.mpq is probed first: the 1.3.1 folder does not hold a nested <Mod>.mpq, so neither layout picks the other's path.
			g_excelDir = std::wstring(context->modDirectory) + L"\\" + Widen(context->activeMod) + L".mpq\\data\\global\\excel";
			if (GetFileAttributesW(g_excelDir.c_str()) == INVALID_FILE_ATTRIBUTES) {
				g_excelDir = std::wstring(context->modDirectory) + L"\\data\\global\\excel";
			}
			g_tableDir = g_excelDir + L"\\d2rloader\\cartographer";
			plog::Info("mod folder %s, tables read from %s", Narrow(context->modDirectory).c_str(), Narrow(g_tableDir).c_str());
		}
		std::wstring logDir = context->pluginLogPath != nullptr ? context->pluginLogPath : L"";
		const size_t slash = logDir.find_last_of(L"\\/");
		if (slash != std::wstring::npos && slash > 0) {
			g_reportPath = logDir.substr(0, slash) + L"\\cartographer-report.txt";
		} else {
			plog::Warn("no log folder, so no report file is written; use 'cartographer check'");
		}
		LoadOverrides();
		const Loaded loaded = Reload();
		plog::Info("%zu map(s), %d problem(s); report at %s", loaded.tables->maps.size(), loaded.report.Errors(), Narrow(g_reportPath).c_str());
		carto::InstallBossNames(context);  // first: the boss strings must be there before the game compiles superuniques.txt
		std::vector<carto::PartStatus> parts;
		carto::SetFirstGameStartNotify(&OnFirstGameStart);
		(void)carto::InstallPortal(context, g_overrides, &parts);
		std::vector<carto::PartStatus> statParts;
		(void)carto::InstallStatApply(context, g_overrides, &statParts);
		std::vector<carto::PartStatus> itemParts;
		(void)carto::InstallItemStats(context, g_overrides, &itemParts);
		std::vector<carto::PartStatus> eventParts;
		(void)carto::InstallEventApply(context, g_overrides, &eventParts);
		std::vector<carto::PartStatus> nameParts;
		(void)carto::InstallMapNames(context, g_overrides, &nameParts);
		(void)carto::InstallMapSync(context);  // SDK only, no engine part: nothing here can turn the portal off
	} catch (...) {
		g_loadFailed = true;
		plog::Error("loading failed; the plugin stays loaded with the portal off");
	}
	// Stay loaded even when parts are off, so "cartographer check" can say why.
	return context->RegisterConsoleCommand(PluginName, Command, "Cartographer: 'check' re-reads the tables, 'open <Name>' opens a map without an item.");
}

D2RL_PLUGIN_EXPORT void D2RLoaderUnloadPlugin() noexcept {
	carto::BossNamesUnload();
	carto::MapNamesUnload();  // the slot level's own text back in the game's table
	carto::LevelRowUnload();  // ... and its own monster fields
	carto::UnloadMapSync();
	crash_guard::Remove();
	g_context = nullptr;
}
