// MultiSpec: WoW-style dual spec for D2R 3.3 (docs/design.md). D2RLoader 1.3.1, PluginSDK 0.3.0.
//
// At load: one log line for the plugin (version, D2R version, image stamp, mod, DLL path), the off-file words, then one
// line per part. Parts (off-file words in multispec-off.txt next to the DLL):
//   envguard   environment guard from the toolkit template (other mod running, stray copies, other plugins)
//   specstore  where the spec files live (save folder from modinfo.json savepath, fallback folder)
//   specswap   every engine call: reading the character, reconciling the file, swapping. Needs
//              the entry bytes of every function it calls (d2r.cpp)
//   speccmd    the "spec" console command (spec, spec N)
//   specui     the spec panel beside the skill tree (spec_panel.h); a click runs the spec command
// The status command "multispec" is always registered.
//
// Threads: the console callback only queues work. The work runs on D2R's game thread (ThreadService::runOnGameThread),
// all of a swap in one callback, then the answer is printed from the UI thread.
#include <D2RLPlugin/api.h>

#include "crash_guard.h"
#include "d2r.h"
#include "env_guard.h"
#include "log.h"
#include "off_switch.h"
#include "spec_engine.h"
#include "spec_model.h"
#include "spec_store.h"
#include "swap.h"
#include "spec_panel.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <atomic>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>

#ifndef MULTISPEC_VERSION
#define MULTISPEC_VERSION "0.1.0"
#endif
#ifndef MULTISPEC_MOD_NAME
#define MULTISPEC_MOD_NAME ""  // the mod folder this plugin ships in (mods\<name>); "" skips the mod checks
#endif

namespace {

constexpr const char* PluginName = "multispec";
constexpr int32_t DefaultSpecCount = 2;

const D2RL::PluginContext* g_context = nullptr;
const D2RL::ThreadService* g_threads = nullptr;
bool g_engineOk = false;  // specswap installed: binding and entry bytes all good
bool g_storeOk = false;
int32_t g_specCount = DefaultSpecCount;
char g_selfPath[520] = "?";

constexpr D2RL::PluginInfo Info {
	.infoSize    = D2RL::PluginInfoSize,
	.abiVersion  = D2RL_PLUGIN_ABI_VERSION,
	.id          = "multispec",
	.name        = "MultiSpec",
	.version     = MULTISPEC_VERSION,
	.author      = "Synpoo",
	.description = "Saved skill allocations (specs) per character, swapped with the spec command in town.",
	.flags       = D2RL::PluginFlags::Shared,  // global: <game>\d2rloader\plugins, works with any mod or none
	.reserved    = {},
};

// --- Counters (status command) ------------------------------------------------------------------------------------
std::atomic<uint32_t> g_done {0};
std::atomic<uint32_t> g_refused {0};
std::atomic<uint32_t> g_rolledBack {0};
std::atomic<uint32_t> g_failed {0};
char g_lastError[600] = "none";
char g_lastSummary[300] = "no character seen yet";

// --- Parts --------------------------------------------------------------------------------------------------------
using InstallFn = void (*)(const D2RL::PluginContext* context, char* report, size_t size) noexcept;

struct Part {
	const char* word;
	InstallFn install;
	char report[600];
};

void InstallEnvironment(const D2RL::PluginContext* context, char* report, size_t size) noexcept {
	StartEnvironmentGuard(context, MULTISPEC_MOD_NAME);
	EnvironmentStatus(report, size);
}

void InstallStore(const D2RL::PluginContext* context, char* report, size_t size) noexcept {
	char paths[560];
	ms::InitStorePaths(context, paths, sizeof(paths));
	g_storeOk = true;
	std::snprintf(report, size, "on: %s", paths);
}

void InstallSwap(const D2RL::PluginContext* context, char* report, size_t size) noexcept {
	d2r::Bind(context->exeBase);
	char entries[480];
	const int bad = d2r::CheckEntries(context, entries, sizeof(entries));
	g_engineOk = bad == 0;
	std::snprintf(report, size, "%s: %s", g_engineOk ? "on" : "off", entries);
}

void InstallCommand(const D2RL::PluginContext* context, char* report, size_t size) noexcept;

void InstallPanel(const D2RL::PluginContext* context, char* report, size_t size) noexcept {
	if (!g_engineOk || !g_storeOk) {
		std::snprintf(report, size, "off: needs specswap and specstore");
		return;
	}
	ms::InstallSpecPanel(context, g_specCount, report, size);
}

Part g_parts[] = {
	{"envguard", InstallEnvironment, ""},
	{"specstore", InstallStore, ""},
	{"specswap", InstallSwap, ""},
	{"speccmd", InstallCommand, ""},
	{"specui", InstallPanel, ""},
};

// --- One request at a time: console -> game thread -> UI thread -------------------------------------------------
constexpr int MaxLines = 14;

struct Request {
	std::atomic<bool> busy {false};
	ULONGLONG started = 0;
	D2R::Game::Client* client = nullptr;
	int32_t swapTo = 0;  // 0 = status only
	int lineCount = 0;
	char lines[MaxLines][320] = {};
	int32_t active = 0;   // for the spec panel: 0 = not known
	bool inTown = false;
};
Request g_req;

void Say(const char* fmt, ...) noexcept {
	if (g_req.lineCount >= MaxLines) {
		return;
	}
	va_list args;
	va_start(args, fmt);
	std::vsnprintf(g_req.lines[g_req.lineCount], sizeof(g_req.lines[0]), fmt, args);
	va_end(args);
	++g_req.lineCount;
}

void __cdecl PrintAnswer(const D2RL::PluginContext* context, void*) noexcept {
	for (int i = 0; i < g_req.lineCount; ++i) {
		context->WriteConsoleMessage(g_req.lines[i]);
	}
	ms::UpdateSpecPanel(context, g_req.active, g_req.inTown);
	g_req.busy.store(false);
}

void Summarize(const ms::CharSpecs& file, const ms::LiveChar& live) noexcept {
	const int32_t total = ms::TotalPoints(live);
	Say("%s: spec %d of %d active, %d skill points in all", file.name, file.active, g_specCount, total);
	const int32_t shown = file.specCount < g_specCount ? file.specCount : g_specCount;
	for (int32_t k = 0; k < shown; ++k) {
		const ms::Spec& s = file.specs[k];
		const int32_t used = ms::SumLevels(s.levels, file.skillCount);
		Say("  spec %d: %d points, %d unspent%s%s", k + 1, used, total - used,
		    s.state == ms::SpecState::Ok ? "" : (s.state == ms::SpecState::Invalid ? " (INVALID: needs more points)" : " (adjusted to changed skills)"),
		    k + 1 == file.active ? "  <- active" : "");
	}
	if (file.specCount > g_specCount) {
		Say("  (%d more spec(s) kept in the file; raise spec_count to use them)", file.specCount - g_specCount);
	}
	std::snprintf(g_lastSummary, sizeof(g_lastSummary), "%s: spec %d of %d active, %d points", file.name, file.active,
	              g_specCount, total);
}

void __cdecl GameWork(const D2RL::PluginContext* context, void*) noexcept {
	char why[300] = "";
	ms::Player player {};
	if (!ms::ResolvePlayer(reinterpret_cast<d2r::Client*>(g_req.client), &player, why, sizeof(why))) {
		Say("MultiSpec: cannot find your character: %s", why);
		plog::Warn("player not resolved: %s", why);
	} else {
		ms::D2REngine engine(player);
		ms::LiveChar live {};
		ms::FileStore store;
		char note[160] = "";
		ms::CharSpecs file {};
		if (!engine.Read(&live)) {
			Say("MultiSpec: could not read the skills of %s", player.name);
			plog::Error("could not read the class skills of %s (class %d)", player.name, player.charClass);
		} else if (!store.Open(player.name, note, sizeof(note))) {
			Say("MultiSpec: no place for the spec file: %s", note);
			plog::Error("spec file for %s: %s", player.name, note);
		} else {
			char path[600];
			store.PathUtf8(path, sizeof(path));
			const ms::LoadResult loaded = store.Load(&file, why, sizeof(why));
			bool ok = loaded != ms::LoadResult::Failed;
			if (loaded == ms::LoadResult::Quarantined) {
				Say("MultiSpec: the spec file was damaged (%s); starting a new one from your current skills", why);
				plog::Warn("spec file %s: %s", path, why);
			} else if (!ok) {
				Say("MultiSpec: the spec file cannot be read (%s); nothing changed", why);
				plog::Error("spec file %s: %s", path, why);
			}
			if (ok) {
				const ms::ReconcileReport rep = ms::Reconcile(&file, loaded == ms::LoadResult::Loaded, live, g_specCount);
				if (rep.result != ms::Reconciled::Unchanged || rep.remapped != 0 || rep.newlyInvalid != 0) {
					plog::Info("%s: %s (remapped %d skill ids, %d specs now invalid), file %s (%s)", player.name,
					           ms::ReconciledName(rep.result), rep.remapped, rep.newlyInvalid, path, note);
				}
				if (rep.result == ms::Reconciled::Recovery) {
					Say("MultiSpec: your last swap did not finish, so your points may be unspent. Type spec N to load a spec");
				}
				if (rep.changed && !store.Write(file)) {
					Say("MultiSpec: the spec file could not be written; nothing changed");
					ok = false;
				}
			}
			if (ok && g_req.swapTo != 0) {
				ms::SwapReport sw {};
				bool allowed = true;
				if (!ms::Alive(player)) {
					std::snprintf(sw.detail, sizeof(sw.detail), "you cannot change specs while dead");
					allowed = false;
				} else if (!ms::InTown(player)) {
					std::snprintf(sw.detail, sizeof(sw.detail), "specs can only be changed in town");
					allowed = false;
				} else if (!ms::CostsAreOnePoint(player, live, sw.detail, sizeof(sw.detail))) {
					allowed = false;
				}
				ms::SwapResult result = ms::SwapResult::Refused;
				if (allowed) {
					plog::Info("%s: swapping spec %d -> %d", player.name, file.active, g_req.swapTo);
					result = ms::Swap(engine, store, &file, live, g_req.swapTo, g_specCount, &sw);
				}
				switch (result) {
				case ms::SwapResult::Done:
					++g_done;
					Say("MultiSpec: spec %d loaded (%d points, %d unspent)", sw.to, sw.pointsUsed, sw.unspent);
					plog::Info("%s: spec %d -> %d done: %d points, %d unspent", player.name, sw.from, sw.to, sw.pointsUsed,
					           sw.unspent);
					break;
				case ms::SwapResult::Refused:
					++g_refused;
					Say("MultiSpec: %s", sw.detail);
					plog::Info("%s: spec %d refused: %s", player.name, g_req.swapTo, sw.detail);
					break;
				case ms::SwapResult::RolledBack:
					++g_rolledBack;
					Say("MultiSpec: spec %d could not be loaded (%s); spec %d is back", sw.to, sw.detail, sw.from);
					plog::Warn("%s: spec %d -> %d rolled back: %s", player.name, sw.from, sw.to, sw.detail);
					std::snprintf(g_lastError, sizeof(g_lastError), "%s", sw.detail);
					break;
				case ms::SwapResult::LeftReset:
					++g_failed;
					Say("MultiSpec: swap FAILED, all your points are unspent (none lost); your specs are kept. See the log");
					plog::Error("%s: spec %d -> %d failed, character left reset: %s", player.name, sw.from, sw.to, sw.detail);
					std::snprintf(g_lastError, sizeof(g_lastError), "%s", sw.detail);
					break;
				}
				if (result != ms::SwapResult::Refused) {
					(void)engine.Read(&live);  // for the summary; the file is already right (Swap wrote it)
				}
			}
			if (ok) {
				Summarize(file, live);
				g_req.active = file.active;
				g_req.inTown = ms::InTown(player);
			}
		}
	}
	if (g_threads->runOnUiThread(context, PrintAnswer, nullptr) != D2RL::Threads::Result::Success) {
		for (int i = 0; i < g_req.lineCount; ++i) {
			plog::Info("%s", g_req.lines[i]);
		}
		g_req.busy.store(false);
	}
}

auto ParseSpecArgs(const char* args, int32_t* to) noexcept -> bool {
	*to = 0;
	if (args == nullptr) {
		return true;
	}
	while (*args == ' ') {
		++args;
	}
	if (*args == '\0' || std::strncmp(args, "status", 6) == 0 || std::strncmp(args, "list", 4) == 0) {
		return true;
	}
	int32_t v = 0;
	for (; *args >= '0' && *args <= '9'; ++args) {
		v = v * 10 + (*args - '0');
		if (v > 99) {
			return false;
		}
	}
	while (*args == ' ') {
		++args;
	}
	if (*args != '\0' || v == 0) {
		return false;
	}
	*to = v;
	return true;
}

auto SpecCommand(D2R::Game::Client* client, const D2RL::ConsoleCommandContext* command, void*) noexcept
	-> D2RL::ConsoleCommandResult {
	if (command == nullptr || command->plugin == nullptr) {
		return D2RL::ConsoleCommandResult::Failed;
	}
	const D2RL::PluginContext* ctx = command->plugin;
	int32_t to = 0;
	if (!ParseSpecArgs(command->args, &to)) {
		ctx->WriteConsoleMessage("usage: spec (show your specs) | spec N (load spec N, in town)");
		return D2RL::ConsoleCommandResult::InvalidArguments;
	}
	if (!g_engineOk || !g_storeOk) {
		ctx->WriteConsoleWarning("MultiSpec is off (see the multispec command)");
		return D2RL::ConsoleCommandResult::Failed;
	}
	bool expected = false;
	if (!g_req.busy.compare_exchange_strong(expected, true)) {
		// A queued game callback is dropped when the game session ends; don't stay busy for ever.
		if (GetTickCount64() - g_req.started < 10000) {
			ctx->WriteConsoleMessage("MultiSpec: busy, try again");
			return D2RL::ConsoleCommandResult::Failed;
		}
		plog::Warn("previous request never finished (game left?); starting a new one");
	}
	g_req.started = GetTickCount64();
	g_req.client = client;
	g_req.swapTo = to;
	g_req.lineCount = 0;
	g_req.active = 0;
	if (g_threads->runOnGameThread(ctx, GameWork, nullptr) != D2RL::Threads::Result::Success) {
		g_req.busy.store(false);
		ctx->WriteConsoleMessage("MultiSpec: only the game host can change specs (or no game is running)");
		return D2RL::ConsoleCommandResult::Failed;
	}
	return D2RL::ConsoleCommandResult::Handled;
}

void InstallCommand(const D2RL::PluginContext* context, char* report, size_t size) noexcept {
	if (context->QueryService(&g_threads) != D2RL::ServiceQueryResult::Success ||
	    !D2RL::HasThreadServiceField(g_threads, D2RL::ThreadServiceRequiredSize)) {
		g_threads = nullptr;
		std::snprintf(report, size, "off: D2RLoader has no thread service");
		return;
	}
	D2RL::ConsoleCommandRegistration reg = D2RL::MakeConsoleCommand("spec", SpecCommand,
	                                                                 "MultiSpec: show your specs, or load spec N (in town).");
	reg.usage = "spec [N]";
	reg.scope = D2RL::ConsoleCommandScope::LocalOnly;
	if (!context->RegisterConsoleCommand(reg)) {
		std::snprintf(report, size, "off: the spec command could not be registered (name taken?)");
		return;
	}
	std::snprintf(report, size, "on: spec, spec N (%d specs)", g_specCount);
}

void InstallParts(const D2RL::PluginContext* context) noexcept {
	for (Part& p : g_parts) {
		if (off_switch::Off(p.word)) {
			std::snprintf(p.report, sizeof(p.report), "off: left out by %s-off.txt", PluginName);
		} else {
			p.install(context, p.report, sizeof(p.report));
		}
		plog::Info("%s %s", p.word, p.report);
	}
}

// --- Status command -----------------------------------------------------------------------------------------------
auto StatusCommand(D2R::Game::Client*, const D2RL::ConsoleCommandContext* command, void*) noexcept
	-> D2RL::ConsoleCommandResult {
	if (command == nullptr || command->plugin == nullptr || g_context == nullptr) {
		return D2RL::ConsoleCommandResult::Failed;
	}
	const D2RL::PluginContext* ctx = command->plugin;
	char line[1100];
	char image[200];
	crash_guard::Describe(image, sizeof(image), D2RL::GetBuildVersion(g_context));
	std::snprintf(line, sizeof(line), "%s %s | %s | mod %s | file %s", PluginName, MULTISPEC_VERSION, image,
	              g_context->activeMod != nullptr ? g_context->activeMod : "?", g_selfPath);
	ctx->WriteConsoleMessage(line);
	if (off_switch::Words()[0] != '\0') {
		std::snprintf(line, sizeof(line), "off-file: %s", off_switch::Words());
		ctx->WriteConsoleMessage(line);
	}
	for (const Part& p : g_parts) {
		char state[700];
		if (std::strcmp(p.word, "envguard") == 0 && !off_switch::Off(p.word)) {
			EnvironmentStatus(state, sizeof(state));
		} else {
			std::snprintf(state, sizeof(state), "%s", p.report);
		}
		std::snprintf(line, sizeof(line), "%s %s", p.word, state);
		ctx->WriteConsoleMessage(line);
	}
	std::snprintf(line, sizeof(line), "spec_count %d | swaps: %u done, %u refused, %u rolled back, %u failed | last error: %s",
	              g_specCount, g_done.load(), g_refused.load(), g_rolledBack.load(), g_failed.load(), g_lastError);
	ctx->WriteConsoleMessage(line);
	std::snprintf(line, sizeof(line), "last character: %s", g_lastSummary);
	ctx->WriteConsoleMessage(line);
	OtherPlugins(line, sizeof(line));
	ctx->WriteConsoleMessage(line);
	plog::Info("%s", line);
	return D2RL::ConsoleCommandResult::Handled;
}

void ReadConfig(const D2RL::PluginContext* context) noexcept {
	static char toml[4096];
	uint32_t required = 0;
	if (context->ReadConfig(toml, sizeof(toml), &required)) {
		g_specCount = ms::ConfigSpecCount(toml, DefaultSpecCount);
		plog::Info("config: spec_count %d", g_specCount);
	} else {
		plog::Warn("config: multispec.toml not readable; spec_count %d", g_specCount);
	}
}

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
	crash_guard::Install(MULTISPEC_VERSION, context->exeBase);
	if (D2RL::GetPluginPath(context) != nullptr) {
		std::snprintf(g_selfPath, sizeof(g_selfPath), "%ls", D2RL::GetPluginPath(context));
	}
	char image[200];
	crash_guard::Describe(image, sizeof(image), D2RL::GetBuildVersion(context));
	plog::Info("%s loaded (mod %s, %s, file %s)", MULTISPEC_VERSION,
	           context->activeMod != nullptr ? context->activeMod : "?", image, g_selfPath);
	off_switch::Read();
	if (off_switch::Words()[0] != '\0') {
		plog::Info("%s-off.txt leaves out: %s", PluginName, off_switch::Words());
	}
	ReadConfig(context);
	InstallParts(context);
	return context->RegisterConsoleCommand(PluginName, StatusCommand, "MultiSpec status: version, image, parts, counters.");
}

D2RL_PLUGIN_EXPORT void D2RLoaderUnloadPlugin() noexcept {
	ms::RemoveSpecPanel();
	crash_guard::Remove();
	g_context = nullptr;
}
