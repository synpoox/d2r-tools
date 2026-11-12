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
#include "crash_guard.h"
#include "engine/portal.h"
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

auto ReadGameNames() -> carto::GameNames {
	carto::GameNames names;
	if (g_excelDir.empty()) {
		return names;
	}
	names.haveMonsters = ReadColumn(g_excelDir + L"\\monstats.txt", "Id", &names.monsters);
	names.haveSuperUniques = ReadColumn(g_excelDir + L"\\superuniques.txt", "Superunique", &names.superUniques);
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
	if (ReadFile(g_tableDir + L"\\" + Widen(carto::ShapesFile), &text)) {
		loaded.tables->shapes = carto::LoadShapes(text, loaded.report);
	}
	text.clear();
	if (ReadFile(g_tableDir + L"\\" + Widen(carto::MapStatsFile), &text)) {
		loaded.tables->pool = carto::LoadMapStats(text, loaded.report);
	}
	text.clear();
	if (ReadFile(g_tableDir + L"\\" + Widen(carto::MapEventsFile), &text)) {
		loaded.tables->events = carto::LoadMapEvents(text, loaded.report);
	}
	text.clear();
	std::vector<carto::MapDef> fromFile;
	if (ReadFile(g_tableDir + L"\\" + Widen(carto::MapsFile), &text)) {
		fromFile = carto::LoadMaps(text, names, loaded.report, &loaded.tables->shapes);
	}
	loaded.tables->maps = std::move(fromFile);
	text.clear();
	if (ReadFile(g_tableDir + L"\\" + Widen(carto::SettingsFile), &text)) {
		loaded.tables->settings = carto::LoadSettings(text, loaded.report);
	}
	text.clear();
	if (ReadFile(g_tableDir + L"\\" + Widen(carto::SlotsFile), &text)) {
		loaded.tables->slots = carto::LoadSlots(text, loaded.report);
	} else {
		loaded.report.Error(carto::SlotsFile, 0, "the file is missing, so the map item has no level to open. Run tools\\install.ps1 and restart the game.");
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
	out << loaded.tables->shapes.shapes.size() << " shape(s), " << loaded.tables->pool.size() << " property row(s) and " << loaded.tables->events.size() << " event(s) loaded (read and checked; not applied in game yet).\r\n\r\n";
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
	plog::Info("%s loaded (mod %s, %s)", PluginVersion, context->activeMod != nullptr ? context->activeMod : "none", image);
	try {
		if (context->modDirectory != nullptr && context->modDirectory[0] != L'\0' && context->activeMod != nullptr) {
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
		std::vector<carto::PartStatus> parts;
		carto::SetFirstGameStartNotify(&OnFirstGameStart);
		(void)carto::InstallPortal(context, g_overrides, &parts);
	} catch (...) {
		g_loadFailed = true;
		plog::Error("loading failed; the plugin stays loaded with the portal off");
	}
	// Stay loaded even when parts are off, so "cartographer check" can say why.
	return context->RegisterConsoleCommand(PluginName, Command, "Cartographer: 'check' re-reads the tables, 'open <Name>' opens a map without an item.");
}

D2RL_PLUGIN_EXPORT void D2RLoaderUnloadPlugin() noexcept {
	crash_guard::Remove();
	g_context = nullptr;
}
