#include <D2RLPlugin/api.h>
#include "crash_guard.h"
#include "log.h"
#include "native.h"
#include "panel.h"
#include "settings.h"
#include "names.h"
#include "controller.h"
#include "preview.h"
#include <condition_variable>
#include <mutex>
#include <thread>
#include <optional>

namespace {
constexpr const char* Version = "0.1.0-alpha.2";
constexpr D2RL::PluginInfo Info {
	.infoSize = D2RL::PluginInfoSize, .abiVersion = D2RL_PLUGIN_ABI_VERSION,
	.id = "death-recap", .name = "Death Recap", .version = Version, .author = "Synpoo",
	.description = "Reports the combat events leading to your character's death.",
	.flags = D2RL::PluginFlags::Client | D2RL::PluginFlags::NativeHooks,
};
const D2RL::PluginContext* context;
const D2RL::ThreadService* threads;
const D2RL::OverlayService* overlay;
D2RL::Overlay::CallbackHandle clockHandle = 0;
recap::Settings settings;
recap::History history;
recap::History previewHistory;
bool previewActive = false;
recap::Rolling<2048> rolling;
uint64_t lifeToken = UINT32_MAX, consumerToken = 0, clientSequence = 0;
uint64_t reportedToken = 0;
uint32_t epoch = 0, previousMode = 0;
bool clientBaseline = false, clientDead = false, replicated = false;
bool consumerSampled = false;
size_t selected = 0, page = 0;
std::atomic<bool> uiQueued {false}, gameQueued {false}, stopping {false};
std::atomic<uint32_t> action {0};
std::atomic<int> storageResult {0};
bool panelReady = false;
recap::Controller controller;
std::filesystem::path historyPath;
std::thread writer;
std::mutex storageMutex;
std::condition_variable storageWake;
bool writerStop = false;
std::optional<recap::History> pendingHistory;
std::optional<recap::Report> pendingExport;

void SaveWorker() noexcept {
	for (;;) {
		std::optional<recap::History> save;
		std::optional<recap::Report> exportReport;
		{
			std::unique_lock lock(storageMutex);
			storageWake.wait(lock, [] { return writerStop || pendingHistory || pendingExport; });
			save = std::move(pendingHistory); pendingHistory.reset();
			exportReport = std::move(pendingExport); pendingExport.reset();
			if (!save && !exportReport && writerStop) return;
		}
		try {
			bool ok = true;
			if (save) {
				auto bytes = recap::Encode(*save);
				while (bytes.size() > 32 * 1024 * 1024 && !save->empty()) { save->erase(save->begin()); bytes = recap::Encode(*save); }
				ok = recap::WriteAtomic(historyPath, bytes);
			}
			if (exportReport) ok = recap::WriteAtomic(historyPath.parent_path() / "latest-export.json", recap::Json(*exportReport)) && ok;
			storageResult = ok ? 1 : -1;
		} catch (...) { storageResult = -1; }
	}
}
void ScheduleSave(bool exportOnly = false) {
	if (!writer.joinable()) return;
	std::lock_guard lock(storageMutex);
	if (exportOnly) { if (!history.empty()) pendingExport = history[selected]; }
	else pendingHistory = history;
	storageWake.notify_one();
}
void Accept(recap::Report report, bool sampled) {
	if (reportedToken == report.session) return;
	previewActive = false;
	recap::CopyNames(report);
	report.capturePartial = true; report.replicated = sampled;
	report.feedDropped = sampled ? 0 : recap::native::Dropped();
	history.push_back(std::move(report));
	reportedToken = history.back().session;
	if (history.size() > settings.retention) history.erase(history.begin());
	selected = history.size() - 1; page = 0;
	ScheduleSave();
	if (settings.autoOpen) recap::panel::Show();
}
void Consume(const recap::Event& e, bool sampled = false) {
	if (consumerToken != e.session || consumerSampled != sampled) { consumerToken = e.session; consumerSampled = sampled; rolling.Reset(e.session, e.ms); }
	if (rolling.Push(e) && e.kind == recap::Kind::Death) {
		if (auto report = rolling.Freeze(uint64_t(settings.windowSeconds) * 1000)) Accept(std::move(*report), sampled);
	}
}
void ClientSample() {
	recap::Endpoints endpoints; uint32_t mode = 0;
	if (!recap::native::ClientSample(endpoints, mode) || clientDead) return;
	auto e = recap::Sample(endpoints);
	e.session = lifeToken; e.sequence = ++clientSequence; e.ms = GetTickCount64();
	if (clientBaseline && previousMode != 0 && previousMode != 17 && (mode == 0 || mode == 17)) {
		e.kind = recap::Kind::Death; e.cause = recap::Evidence::Confirmed; clientDead = true;
	}
	previousMode = mode; clientBaseline = true;
	Consume(e, true);
}
void GameTask(const D2RL::PluginContext* ctx, void*) noexcept {
	gameQueued = false;
	if (!stopping) recap::native::GameTick(ctx, nullptr);
}
void UiTask(const D2RL::PluginContext* ctx, void*) noexcept {
	uiQueued = false;
	if (stopping) return;
	try {
		if (static_cast<uint32_t>(lifeToken) != UINT32_MAX) {
			if (!gameQueued.exchange(true)) {
				const auto result = threads->runOnGameThread(ctx, GameTask, nullptr);
				if (result != D2RL::Threads::Result::Success) gameQueued = false;
				replicated = result == D2RL::Threads::Result::Unavailable;
			}
			if (replicated || !(recap::native::Parts() & 16) || recap::native::Faulted()) ClientSample();
		}
		recap::Event e;
		while (recap::native::Pop(e)) if (!replicated && !recap::native::Faulted() && (recap::native::Parts() & 16) && e.session == lifeToken) Consume(e);
		auto command = action.exchange(0);
		const auto controllerCommand = controller.Poll(recap::panel::Open());
		if (!command) command = controllerCommand;
		if (command == 10) {
			previewHistory = {recap::PreviewReport()}; previewActive = true; selected = page = 0; recap::panel::Show();
			recap::Endpoints real; uint32_t mode = 0;
			if (recap::native::ClientSample(real, mode)) plog::Info("synthetic preview; real character unchanged: life %d/%d, mana %d/%d raw, mode %u; no history write", real.life, real.maxLife, real.mana, real.maxMana, mode);
		}
		if (command == 1) { previewActive = false; if (!history.empty()) selected = history.size() - 1; page = 0; recap::panel::Show(true); }
		auto& shown = previewActive ? previewHistory : history;
		if (command == 2 && !shown.empty()) { selected = selected ? selected - 1 : shown.size() - 1; page = 0; }
		if (command == 3 && !shown.empty()) { selected = (selected + 1) % shown.size(); page = 0; }
		if (command == 4 && page) --page;
		if (command == 5 && !shown.empty()) {
			const auto& timeline = shown[selected].timeline;
			const auto count = static_cast<size_t>(std::count_if(timeline.begin(), timeline.end(), [](const recap::Event& e) { return e.kind != recap::Kind::Sample; }));
			page = std::min(page + 1, count ? (count - 1) / 6 : 0);
		}
		if (command == 6 && !previewActive) ScheduleSave(true);
		if (command == 7) { history.clear(); selected = page = 0; ScheduleSave(); }
		if (command == 8) recap::panel::Close();
		if (command == 9) { previewActive = false; if (!history.empty()) selected = history.size() - 1; page = 0; recap::panel::Show(); }
		recap::panel::Update(previewActive ? previewHistory : history, selected, page, previewActive);
		if (storageResult.exchange(0) == -1) plog::Warn("history/export write failed; report remains available in memory");
	} catch (...) { plog::Warn("recap UI update failed; gameplay is unaffected"); }
}
void OnFrame(const D2RL::PluginContext* ctx, const D2RL::Overlay::Frame*, void*) noexcept {
	if (!stopping && !uiQueued.exchange(true) && threads->runOnUiThread(ctx, UiTask, nullptr) != D2RL::Threads::Result::Success) uiQueued = false;
}
void OnLife(const D2RL::PluginContext*, const D2RL::Lifecycle::GameplayEvent* e, void*) noexcept {
	if (!D2RL::Lifecycle::HasGameplayEventField(e, D2RL::Lifecycle::GameplayEventRequiredSize)) return;
	if (e->kind == D2RL::Lifecycle::GameplayEventKind::PlayerResurrected && e->playerId != static_cast<uint32_t>(lifeToken)) return;
	if (e->kind == D2RL::Lifecycle::GameplayEventKind::GameLeft) {
		lifeToken = (uint64_t(++epoch) << 32) | UINT32_MAX;
		recap::panel::Close(); gameQueued = false;
	} else {
		lifeToken = (uint64_t(++epoch) << 32) | e->playerId;
		if (e->kind == D2RL::Lifecycle::GameplayEventKind::PlayerResurrected) recap::panel::Close();
	}
	clientBaseline = clientDead = false; clientSequence = 0; consumerToken = 0;
	recap::native::Identity(lifeToken);
}
D2RL::Input::ActionResult OnAction(const D2RL::PluginContext*, const D2RL::Input::ActionEvent* e, void* data) noexcept {
	if (!D2RL::Input::HasActionEventField(e, D2RL::Input::ActionEventRequiredSize) || e->kind != D2RL::Input::ActionEventKind::Pressed) return D2RL::Input::ActionResult::Ignored;
	const auto command = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(data));
	if (command != 1 && !recap::panel::Open()) return D2RL::Input::ActionResult::Ignored;
	action = command; return D2RL::Input::ActionResult::Handled;
}
auto Command(D2R::Game::Client*, const D2RL::ConsoleCommandContext* cmd, void*) noexcept -> D2RL::ConsoleCommandResult {
	if (!cmd || !cmd->plugin) return D2RL::ConsoleCommandResult::Failed;
	const std::string_view arg = cmd->args ? recap::Trim(std::string_view(cmd->args, cmd->argsLength)) : std::string_view{};
	if (arg == "open") action = 9;
	else if (arg == "preview") action = 10;
	else if (arg == "previous") action = 2;
	else if (arg == "next") action = 3;
	else if (arg == "export") action = 6;
	else if (arg == "clear") action = 7;
	else if (arg == "close") action = 8;
	else if (arg.empty() || arg == "status") {
		char line[320]; std::snprintf(line, sizeof(line), "Death Recap %s | parts 0x%x | capture %s | panel %s | dropped %llu | partial coverage. open / previous / next / export / clear / close",
			Version, recap::native::Parts(), recap::native::Faulted() ? "off" : "available", panelReady ? "ready" : "off", static_cast<unsigned long long>(recap::native::Dropped()));
		cmd->plugin->WriteConsoleMessage(line);
		plog::Info("%s", line);
	} else return D2RL::ConsoleCommandResult::InvalidArguments;
	return D2RL::ConsoleCommandResult::Handled;
}
bool Register() noexcept {
	const D2RL::LifecycleService* lifecycle = nullptr;
	if (context->QueryService(&threads) != D2RL::ServiceQueryResult::Success || !D2RL::HasThreadServiceField(threads, D2RL::ThreadServiceRequiredSize) ||
		context->QueryService(&overlay) != D2RL::ServiceQueryResult::Success || !D2RL::HasOverlayServiceField(overlay, D2RL::OverlayServiceRequiredSize) ||
		context->QueryService(&lifecycle) != D2RL::ServiceQueryResult::Success || !D2RL::HasLifecycleServiceField(lifecycle, D2RL::LifecycleServiceRequiredSize)) return false;
	for (auto kind : {D2RL::Lifecycle::GameplayEventKind::LocalPlayerReady, D2RL::Lifecycle::GameplayEventKind::GameLeft, D2RL::Lifecycle::GameplayEventKind::PlayerResurrected}) {
		D2RL::Lifecycle::GameplayEventListener listener {}; listener.structSize = sizeof(listener); listener.kind = kind; listener.callback = OnLife;
		D2RL::Lifecycle::ListenerHandle h = 0;
		if (lifecycle->registerGameplayEventListener(context, &listener, &h) != D2RL::Lifecycle::Result::Success) return false;
	}
	D2RL::Overlay::CallbackRegistration clock {}; clock.structSize = sizeof(clock); clock.phase = D2RL::Overlay::Phase::AfterGameUi; clock.callback = OnFrame;
	if (overlay->registerFrameCallback(context, &clock, &clockHandle) != D2RL::Overlay::Result::Success) return false;
	const D2RL::InputService* input = nullptr;
	if (context->QueryService(&input) == D2RL::ServiceQueryResult::Success && D2RL::HasInputServiceField(input, D2RL::InputServiceRequiredSize)) {
		constexpr const char* ids[] {"reopen", "previous", "next", "timeline-previous", "timeline-next"};
		constexpr const char* labels[] {"Toggle latest death recap", "Previous death recap", "Next death recap", "Previous recap timeline page", "Next recap timeline page"};
		constexpr D2RL::Input::Key keys[] {D2RL::Input::Key::F8, D2RL::Input::Key::LeftBracket, D2RL::Input::Key::RightBracket, D2RL::Input::Key::PageUp, D2RL::Input::Key::PageDown};
		for (uintptr_t i = 0; i < 5; ++i) {
			D2RL::Input::ActionRegistration r {}; r.structSize = sizeof(r); r.logicalId = ids[i]; r.displayName = labels[i]; r.category = "Death Recap";
			r.defaultPrimary.key = keys[i]; r.callback = OnAction; r.userData = reinterpret_cast<void*>(i + 1);
			D2RL::Input::ActionHandle h = 0;
			if (input->registerAction(context, &r, &h) != D2RL::Input::Result::Success) plog::Warn("input action %s unavailable", ids[i]);
		}
	}
	return true;
}
void Cleanup() noexcept {
	stopping = true; recap::native::Stop();
	controller.Stop();
	if (overlay && clockHandle) (void)overlay->unregisterFrameCallback(context, clockHandle);
	{ std::lock_guard lock(storageMutex); writerStop = true; storageWake.notify_one(); }
	if (writer.joinable()) writer.join();
	crash_guard::Remove();
}
}
D2RL_PLUGIN_EXPORT auto D2RLoaderGetPluginInfo() noexcept -> const D2RL::PluginInfo* { return &Info; }
D2RL_PLUGIN_EXPORT auto D2RLoaderLoadPlugin(const D2RL::PluginContext* ctx) noexcept -> bool {
	if (!ctx) return false;
	context = ctx; plog::Init(ctx, "death-recap"); crash_guard::Install(Version, ctx->exeBase);
	try {
		char toml[8192] {}; if (ctx->ReadConfig(toml, sizeof(toml) - 1)) settings = recap::ParseSettings(toml);
		if (!ctx->RegisterConsoleCommand("death-recap", Command, "Death recap status, history and export.")) { Cleanup(); return false; }
		if (!settings.enabled) { plog::Info("disabled by configuration"); return true; }
		wchar_t local[32768] {}; const DWORD n = GetEnvironmentVariableW(L"LOCALAPPDATA", local, static_cast<DWORD>(std::size(local)));
		if (n && n < std::size(local)) {
			historyPath = std::filesystem::path(local) / "D2RLoader" / "death-recap" / "history.v1";
			history = recap::LoadHistory(historyPath, settings.retention); selected = history.empty() ? 0 : history.size() - 1;
			writer = std::thread(SaveWorker);
		} else plog::Warn("local application-data directory unavailable; history is in memory only");
		if (!Register()) { Cleanup(); return false; }
		panelReady = recap::panel::Install(ctx);
		controller.Start();
		if (!recap::InstallNames(ctx)) plog::Warn("localized source names and state table bounds unavailable");
		plog::Info("native panel %s", panelReady ? "ready" : "off: missing service/signature");
		if (!recap::native::Install(ctx)) { plog::Error("capture original unavailable; rejecting load to restore mutations"); Cleanup(); return false; }
		plog::Info("%s loaded; capture partial, no live validation", Version);
		return true;
	} catch (...) { Cleanup(); return false; }
}
D2RL_PLUGIN_EXPORT void D2RLoaderUnloadPlugin() noexcept { Cleanup(); }
