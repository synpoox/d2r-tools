// XP display (display.h). NOT RUN IN GAME YET: needs live check throughout (TESTING.md).
//
// Threads:
//  - D2R UI thread: lifecycle gameplay events, Tick() (queued with ThreadService::runOnUiThread), the reset / reload
//    requests. All game reads and all XpSession use happen here.
//  - Overlay frame callback (loader's draw thread, possibly another one): only copies the cached lines and draws them.
//  - Input action callbacks and console commands: only queue UI work or flip an atomic.
// g_lock guards everything the overlay callback and the status command read.
#include "display.h"

#include "bxb/format.h"
#include "bxb/settings.h"
#include "bxb/xp_session.h"
#include "log.h"
#include "xp_reader.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <mutex>
#include <string>
#include <vector>

namespace display {

namespace {

const D2RL::PluginContext* g_context = nullptr;
const D2RL::ThreadService* g_threads = nullptr;
const D2RL::OverlayService* g_overlay = nullptr;
const D2RL::LifecycleService* g_lifecycle = nullptr;
const D2RL::InputService* g_input = nullptr;

std::mutex g_lock;
bxb::Settings g_settings;           // g_lock
bxb::XpSession g_session;           // UI thread (and g_lock for the status command)
std::vector<std::string> g_lines;   // g_lock: what the overlay draws
std::string g_lastRead = "no read yet";  // g_lock
std::string g_tableInfo = "no table yet";  // g_lock

std::atomic<bool> g_tracking{false};  // part "xp" installed
std::atomic<bool> g_visible{true};
std::atomic<bool> g_ready{false};     // local player ready in a game
std::atomic<bool> g_tickQueued{false};
std::atomic<int64_t> g_lastTickMs{0};

// UI-thread state
bool g_firstRead = true;
bool g_tableOk = false;
uint32_t g_tableFlag = 0xffffffffu;
int32_t g_tableClass = -1;
int32_t g_lastClass = -1;
uint64_t g_lastXp = 0;
bool g_haveLast = false;
uint32_t g_characters = 0;
uint32_t g_lastLosses = 0;
uint32_t g_lastLevelUps = 0;
uint64_t g_reads = 0;
uint64_t g_badReads = 0;

constexpr int64_t TickIntervalMs = 250;
constexpr float FallbackTextSize = 18.0f;  // when neither the config nor the loader gives a usable size

std::atomic<uint64_t> g_frames{0};
std::atomic<uint64_t> g_drawn{0};       // frames where lines were drawn
std::atomic<uint32_t> g_drawErrors{0};
std::atomic<uint32_t> g_lastDrawResult{0};
float g_lastScreenW = 0, g_lastScreenH = 0, g_lastDefaultSize = 0;  // draw thread only; read approximately by Status

void NoteDrawResult(D2RL::Overlay::Result r, const char* what) noexcept {
	if (r == D2RL::Overlay::Result::Success) {
		return;
	}
	g_lastDrawResult = static_cast<uint32_t>(r);
	static std::atomic<uint32_t> s_logged{0};
	++g_drawErrors;
	if (plog::Sample(s_logged)) {
		plog::Error("overlay: %s failed with result %u (%u errors so far)", what, static_cast<uint32_t>(r), g_drawErrors.load());
	}
}

auto NowSeconds() noexcept -> double {
	using namespace std::chrono;
	static const steady_clock::time_point start = steady_clock::now();
	return duration<double>(steady_clock::now() - start).count();
}

auto NowMs() noexcept -> int64_t {
	return static_cast<int64_t>(NowSeconds() * 1000.0);
}

void RebuildLines(const bxb::Snapshot& s) {
	std::vector<std::string> lines;
	if (s.valid) {
		for (const bxb::LineId id : g_settings.lines) {
			lines.push_back(bxb::FormatLine(s, id));
		}
	}
	g_lines = std::move(lines);
}

void LogSummary(const char* why) {
	const bxb::Snapshot s = g_session.Get(NowSeconds());
	if (!s.valid) {
		return;
	}
	plog::Info("session (%s): level %u, %s XP (%s), time %s, avg XP/h %s, gained %s, lost %s in %u drop(s), %u level-up(s)", why, s.level,
	           bxb::FormatSigned(s.sessionXp).c_str(), bxb::FormatSignedPercent(s.sessionLevels).c_str(), bxb::FormatClock(s.activeSeconds).c_str(),
	           s.session ? bxb::FormatCompact(s.session->xpPerHour).c_str() : bxb::NotAvailable, bxb::FormatCount(s.grossXp).c_str(),
	           bxb::FormatCount(s.lostXp).c_str(), s.losses, s.levelUps);
}

// --- UI thread ----------------------------------------------------------------------------------------------------
void __cdecl Tick(const D2RL::PluginContext*, void*) noexcept {
	g_tickQueued = false;
	if (!g_ready || !g_tracking) {
		return;
	}
	const double now = NowSeconds();
	const xp_reader::Reading r = xp_reader::ReadPlayer();  // needs live check
	std::lock_guard<std::mutex> lock(g_lock);
	++g_reads;
	if (!r.ok) {
		++g_badReads;
		g_lastRead = "no player unit / not ready";
		return;
	}
	char buf[200];
	std::snprintf(buf, sizeof(buf), "level %u xp %llu class %d flag %u", r.level, static_cast<unsigned long long>(r.xp), r.classId, r.dataFlag);
	g_lastRead = buf;

	if (!g_tableOk || r.dataFlag != g_tableFlag || r.classId != g_tableClass) {
		bxb::ExpTable table;
		std::string info;
		g_tableOk = xp_reader::ReadTable(r.dataFlag, r.classId, &table, &info);  // needs live check
		g_tableFlag = r.dataFlag;
		g_tableClass = r.classId;
		g_tableInfo = info;
		if (g_tableOk) {
			plog::Info("%s", info.c_str());
			g_session.SetTable(std::move(table));
		} else {
			plog::Error("%s: the display stays empty", info.c_str());
		}
	}
	if (!g_tableOk) {
		return;
	}
	const uint32_t tableLevel = g_session.Table().LevelForXp(r.xp);
	if (tableLevel != r.level) {
		static std::atomic<uint32_t> s_mismatch{0};
		if (plog::Sample(s_mismatch)) {
			plog::Warn("level from stat 12 is %u, from the experience table %u (xp %llu); showing the table's", r.level, tableLevel,
			           static_cast<unsigned long long>(r.xp));
		}
	}

	if (g_firstRead) {
		// character key (evidence only: class + XP continuity, findings (a)); matters for scope = character
		if (!g_haveLast || r.classId != g_lastClass || r.xp != g_lastXp) {
			++g_characters;
			char key[32];
			std::snprintf(key, sizeof(key), "character-%u", g_characters);
			if (g_session.BeginCharacter(key, now) && g_haveLast) {
				plog::Info("another character (class %d -> %d, xp %llu -> %llu): new session", g_lastClass, r.classId,
				           static_cast<unsigned long long>(g_lastXp), static_cast<unsigned long long>(r.xp));
			}
		}
		g_firstRead = false;
		plog::Info("first read in this game: %s", buf);
	}
	g_session.Sample(now, r.xp);
	g_lastClass = r.classId;
	g_lastXp = r.xp;
	g_haveLast = true;

	const bxb::Snapshot s = g_session.Get(now);
	if (s.losses != g_lastLosses) {
		plog::Info("XP lost: %s in total this session (%u drop(s)); level %u", bxb::FormatCount(s.lostXp).c_str(), s.losses, s.level);
	}
	if (s.levelUps != g_lastLevelUps) {
		plog::Info("level up: now %u (%u this session)", s.level, s.levelUps);
	}
	if (g_settings.logXpChanges) {
		static std::atomic<uint32_t> s_changes{0};
		static uint64_t s_prev = 0;
		if (r.xp != s_prev && plog::Sample(s_changes)) {
			plog::Info("xp change #%u: %llu", s_changes.load(), static_cast<unsigned long long>(r.xp));
		}
		s_prev = r.xp;
	}
	g_lastLosses = s.losses;
	g_lastLevelUps = s.levelUps;
	RebuildLines(s);
}

void QueueTick() noexcept {
	if (g_threads == nullptr || !g_ready) {
		return;
	}
	const int64_t now = NowMs();
	if (now - g_lastTickMs < TickIntervalMs || g_tickQueued.exchange(true)) {
		return;
	}
	g_lastTickMs = now;
	if (g_threads->runOnUiThread(g_context, Tick, nullptr) != D2RL::Threads::Result::Success) {
		g_tickQueued = false;
	}
}

void __cdecl DoReset(const D2RL::PluginContext*, void*) noexcept {
	std::lock_guard<std::mutex> lock(g_lock);
	LogSummary("reset by the player");
	g_session.Reset(NowSeconds());
	g_lastLosses = g_lastLevelUps = 0;
	g_lines.clear();
	plog::Info("session reset");
}

void __cdecl DoReload(const D2RL::PluginContext* context, void*) noexcept {
	LoadConfig(context);
}

void OnGameplay(const D2RL::PluginContext*, const D2RL::Lifecycle::GameplayEvent* e, void*) noexcept {
	if (!D2RL::Lifecycle::HasGameplayEventField(e, D2RL::Lifecycle::GameplayEventRequiredSize)) {
		return;
	}
	const double now = NowSeconds();
	std::lock_guard<std::mutex> lock(g_lock);
	switch (e->kind) {
	case D2RL::Lifecycle::GameplayEventKind::GameJoined:
		plog::Info("game joined (session generation %llu)", static_cast<unsigned long long>(e->sessionGeneration));
		break;
	case D2RL::Lifecycle::GameplayEventKind::LocalPlayerReady:
		plog::Info("local player ready (player id %u, difficulty %u)", e->playerId, e->difficulty);
		g_session.EnterGame(now);
		g_firstRead = true;
		g_lastLosses = g_lastLevelUps = 0;
		g_ready = true;
		break;
	case D2RL::Lifecycle::GameplayEventKind::GameLeft:
		plog::Info("game left");
		LogSummary("game left");
		g_ready = false;
		g_session.LeaveGame(now);
		g_lines.clear();
		break;
	case D2RL::Lifecycle::GameplayEventKind::PlayerLevelChanged:
		plog::Info("level changed event: %d -> %d", e->previousValue, e->currentValue);
		break;
	default:
		break;
	}
}

void __cdecl OnGameplayCdecl(const D2RL::PluginContext* c, const D2RL::Lifecycle::GameplayEvent* e, void* u) noexcept {
	OnGameplay(c, e, u);
}

// --- Overlay (draw thread) ----------------------------------------------------------------------------------------
void __cdecl Draw(const D2RL::PluginContext* context, const D2RL::Overlay::Frame* frame, void*) noexcept {
	QueueTick();  // the frame callback is the clock that drives the UI-thread reads
	if (frame == nullptr) {
		return;
	}
	const uint64_t n = ++g_frames;
	g_lastScreenW = frame->screenWidth;
	g_lastScreenH = frame->screenHeight;
	g_lastDefaultSize = frame->defaultTextSize;
	if (n == 1) {
		plog::Info("overlay: first frame: screen %.0f x %.0f, default text size %.2f, struct size %u", frame->screenWidth,
		           frame->screenHeight, frame->defaultTextSize, frame->structSize);
	}
	if (!g_visible || !g_ready) {
		return;
	}
	std::vector<std::string> lines;
	bxb::Settings s;
	{
		std::lock_guard<std::mutex> lock(g_lock);
		lines = g_lines;
		s = g_settings;
	}
	if (lines.empty()) {
		return;
	}
	float size = s.textSize > 0.0f ? s.textSize : frame->defaultTextSize;
	if (!(size >= 6.0f)) {
		size = FallbackTextSize;
	}
	const float screenW = frame->screenWidth > 0.0f ? frame->screenWidth : 1920.0f;
	const float screenH = frame->screenHeight > 0.0f ? frame->screenHeight : 1080.0f;
	constexpr float Pad = 6.0f;
	float width = 0.0f;
	std::vector<float> heights;
	for (const std::string& line : lines) {
		D2RL::Overlay::MeasureTextRequest m {};
		m.structSize = D2RL::Overlay::MeasureTextRequestSize;
		m.canvas = frame->canvas;
		m.textSize = size;
		m.text = line.c_str();
		m.textLength = static_cast<uint32_t>(std::min<size_t>(line.size(), D2RL::Overlay::MaxTextBytes));
		D2RL::Overlay::TextMetrics tm {};
		tm.structSize = D2RL::Overlay::TextMetricsSize;
		const auto mr = g_overlay->measureText(context, &m, &tm);
		NoteDrawResult(mr, "measureText");
		if (mr != D2RL::Overlay::Result::Success || !(tm.height > 0.0f)) {
			tm.width = size * 0.55f * static_cast<float>(line.size());
			tm.height = size;
		}
		width = std::max(width, tm.width);
		heights.push_back(std::max(tm.height, size));
	}
	float height = 0.0f;
	for (const float h : heights) {
		height += h;
	}
	const float boxW = width + 2 * Pad;
	const float boxH = height + 2 * Pad;
	float x = 0.0f;
	float y = 0.0f;
	switch (s.anchor) {
	case bxb::Anchor::TopLeft: x = 0; y = 0; break;
	case bxb::Anchor::TopCenter: x = (screenW - boxW) / 2; y = 0; break;
	case bxb::Anchor::TopRight: x = screenW - boxW; y = 0; break;
	case bxb::Anchor::BottomLeft: x = 0; y = screenH - boxH; break;
	case bxb::Anchor::BottomCenter: x = (screenW - boxW) / 2; y = screenH - boxH; break;
	case bxb::Anchor::BottomRight: x = screenW - boxW; y = screenH - boxH; break;
	}
	x = std::clamp(x + s.offsetX, 0.0f, std::max(0.0f, screenW - boxW));
	y = std::clamp(y + s.offsetY, 0.0f, std::max(0.0f, screenH - boxH));
	if (s.background > 0.0f) {
		D2RL::Overlay::FilledRectangleRequest bg {};
		bg.structSize = D2RL::Overlay::FilledRectangleRequestSize;
		bg.canvas = frame->canvas;
		bg.rect = {x, y, x + boxW, y + boxH};
		bg.color = {0.0f, 0.0f, 0.0f, s.background};
		bg.rounding = 4.0f;
		NoteDrawResult(g_overlay->drawFilledRectangle(context, &bg), "drawFilledRectangle");
	}
	float ty = y + Pad;
	for (size_t i = 0; i < lines.size(); ++i) {
		D2RL::Overlay::TextRequest t {};
		t.structSize = D2RL::Overlay::TextRequestSize;
		t.canvas = frame->canvas;
		t.position = {x + Pad, ty};
		t.color = {s.textColor[0], s.textColor[1], s.textColor[2], 1.0f};
		t.textSize = size;
		t.text = lines[i].c_str();
		t.textLength = static_cast<uint32_t>(std::min<size_t>(lines[i].size(), D2RL::Overlay::MaxTextBytes));
		NoteDrawResult(g_overlay->drawText(context, &t), "drawText");
		ty += heights[i];
	}
	if (++g_drawn == 1) {
		plog::Info("overlay: first draw: %zu lines, text size %.1f, box %.0f,%.0f %.0f x %.0f", lines.size(), size, x, y, boxW, boxH);
	}
}

// --- Input (input processing) -------------------------------------------------------------------------------------
auto __cdecl OnToggle(const D2RL::PluginContext*, const D2RL::Input::ActionEvent* e, void*) noexcept -> D2RL::Input::ActionResult {
	if (e == nullptr || e->kind != D2RL::Input::ActionEventKind::Pressed) {
		return D2RL::Input::ActionResult::Ignored;
	}
	RequestToggle();
	return D2RL::Input::ActionResult::Handled;
}

auto __cdecl OnReset(const D2RL::PluginContext*, const D2RL::Input::ActionEvent* e, void*) noexcept -> D2RL::Input::ActionResult {
	if (e == nullptr || e->kind != D2RL::Input::ActionEventKind::Pressed) {
		return D2RL::Input::ActionResult::Ignored;
	}
	RequestReset();
	return D2RL::Input::ActionResult::Handled;
}

template <typename Service>
auto Query(const D2RL::PluginContext* context, const Service** out, uint32_t required, const char* name, char* report, size_t size) noexcept
	-> bool {
	if (context->QueryService(out) != D2RL::ServiceQueryResult::Success || *out == nullptr || (*out)->serviceSize < required) {
		std::snprintf(report, size, "off: D2RLoader has no %s service of the expected version", name);
		*out = nullptr;
		return false;
	}
	return true;
}

}  // namespace

void LoadConfig(const D2RL::PluginContext* context) noexcept {
	std::vector<char> buf(16384);
	uint32_t required = 0;
	bxb::Settings s;
	std::vector<std::string> warnings;
	bool read = context->ReadConfig(buf.data(), static_cast<uint32_t>(buf.size()), &required);
	if (!read && required > buf.size() && required < (1u << 20)) {
		buf.resize(required);
		read = context->ReadConfig(buf.data(), static_cast<uint32_t>(buf.size()), &required);
	}
	if (read) {
		s = bxb::ParseSettings(std::string(buf.data()), &warnings);
	} else {
		plog::Warn("config: could not read better-xp-bar.toml; using the built-in defaults");
	}
	for (const std::string& w : warnings) {
		plog::Warn("config: %s", w.c_str());
	}
	std::string lines;
	for (const bxb::LineId id : s.lines) {
		lines += std::string(lines.empty() ? "" : ",") + bxb::LineName(id);
	}
	plog::Info("config: visible %d, anchor %s, offset %.0f,%.0f, text size %.1f, background %.2f, lines [%s], scope %s, window %.0f s, eta from %s",
	           s.visible ? 1 : 0, bxb::AnchorName(s.anchor), s.offsetX, s.offsetY, s.textSize, s.background, lines.c_str(),
	           s.session.scope == bxb::SessionScope::Game ? "game" : "character", s.session.windowSeconds,
	           s.session.etaFromSessionRate ? "session" : "window");
	std::lock_guard<std::mutex> lock(g_lock);
	g_settings = s;
	g_session.SetConfig(s.session);
	g_visible = s.visible;
}

void InstallTracking(const D2RL::PluginContext* context, char* report, size_t size) noexcept {
	g_context = context;
	char inner[256];
	if (!xp_reader::Install(context, inner, sizeof(inner))) {
		std::snprintf(report, size, "%s", inner);
		return;
	}
	if (!Query(context, &g_threads, D2RL::ThreadServiceRequiredSize, "Thread", report, size) ||
	    !Query(context, &g_lifecycle, D2RL::LifecycleServiceRequiredSize, "Lifecycle", report, size)) {
		return;
	}
	using K = D2RL::Lifecycle::GameplayEventKind;
	for (const K kind : {K::GameJoined, K::LocalPlayerReady, K::GameLeft, K::PlayerLevelChanged}) {
		D2RL::Lifecycle::GameplayEventListener l {};
		l.structSize = D2RL::Lifecycle::GameplayEventListenerSize;
		l.kind = kind;
		l.callback = OnGameplayCdecl;
		D2RL::Lifecycle::ListenerHandle h = D2RL::Lifecycle::InvalidHandle;
		if (g_lifecycle->registerGameplayEventListener(context, &l, &h) != D2RL::Lifecycle::Result::Success) {
			std::snprintf(report, size, "off: gameplay event %u listener refused", static_cast<uint32_t>(kind));
			return;
		}
	}
	g_tracking = true;
	std::snprintf(report, size, "%s; game events registered", inner);
}

void InstallOverlay(const D2RL::PluginContext* context, char* report, size_t size) noexcept {
	g_context = context;
	if (!g_tracking) {
		std::snprintf(report, size, "off: needs part xp");
		return;
	}
	if (!Query(context, &g_overlay, D2RL::OverlayServiceRequiredSize, "Overlay", report, size)) {
		return;
	}
	D2RL::Overlay::CallbackRegistration r {};
	r.structSize = D2RL::Overlay::CallbackRegistrationSize;
	r.phase = D2RL::Overlay::Phase::AfterGameUi;
	r.priority = 0;
	r.callback = Draw;
	D2RL::Overlay::CallbackHandle h = D2RL::Overlay::InvalidCallbackHandle;
	const auto result = g_overlay->registerFrameCallback(context, &r, &h);
	if (result != D2RL::Overlay::Result::Success) {
		std::snprintf(report, size, "off: overlay callback refused (result %u)", static_cast<uint32_t>(result));
		g_overlay = nullptr;
		return;
	}
	std::snprintf(report, size, "on: overlay frame callback registered");
}

void InstallHotkeys(const D2RL::PluginContext* context, char* report, size_t size) noexcept {
	if (!Query(context, &g_input, D2RL::InputServiceRequiredSize, "Input", report, size)) {
		return;
	}
	struct Action {
		const char* id;
		const char* name;
		D2RL::Input::Key key;
		D2RL::Input::ActionCallback cb;
	};
	// Defaults: Ctrl+X toggles, Ctrl+Shift is not supported (one modifier at most), so Ctrl+R resets.
	const Action actions[] = {
		{"toggle", "Toggle XP display", D2RL::Input::Key::X, OnToggle},
		{"reset", "Reset XP session", D2RL::Input::Key::R, OnReset},
	};
	int ok = 0;
	for (const Action& a : actions) {
		D2RL::Input::ActionRegistration r {};
		r.structSize = D2RL::Input::ActionRegistrationSize;
		r.logicalId = a.id;
		r.displayName = a.name;
		r.category = "Better XP Bar";
		r.defaultPrimary = {a.key, D2RL::Input::Modifier::Control};
		r.defaultSecondary = {D2RL::Input::Key::None, D2RL::Input::Modifier::None};
		r.callback = a.cb;
		D2RL::Input::ActionHandle h = D2RL::Input::InvalidHandle;
		const auto result = g_input->registerAction(context, &r, &h);
		if (result == D2RL::Input::Result::Success) {
			++ok;
		} else {
			plog::Warn("hotkey '%s' refused (result %u)", a.name, static_cast<uint32_t>(result));
		}
	}
	std::snprintf(report, size, "%s: %d/2 actions (Ctrl+X toggle, Ctrl+R reset by default; rebind in Options > Controls)",
	              ok == 2 ? "on" : "partial", ok);
}

void RequestReset() noexcept {
	if (g_threads != nullptr) {
		(void)g_threads->runOnUiThread(g_context, DoReset, nullptr);
	}
}

void RequestToggle() noexcept {
	const bool now = !g_visible.load();
	g_visible = now;
	plog::Info("display %s", now ? "shown" : "hidden");
}

void RequestReload() noexcept {
	if (g_threads != nullptr) {
		(void)g_threads->runOnUiThread(g_context, DoReload, nullptr);
	}
}

void Status(char* out, size_t size) noexcept {
	std::lock_guard<std::mutex> lock(g_lock);
	const bxb::Snapshot s = g_session.Get(NowSeconds());
	std::snprintf(out, size,
	              "%s, display %s | last read: %s | %s | reads %llu (%llu without a player) | session %s XP, gained %s, lost %s (%u), "
	              "%u level-up(s), time %s, resets %u | overlay frames %llu, drawn %llu, draw errors %u (last %u), screen %.0fx%.0f, "
	              "default text %.1f",
	              g_ready ? "in game" : "not in game", g_visible ? "shown" : "hidden", g_lastRead.c_str(), g_tableInfo.c_str(),
	              static_cast<unsigned long long>(g_reads), static_cast<unsigned long long>(g_badReads), bxb::FormatSigned(s.sessionXp).c_str(),
	              bxb::FormatCount(s.grossXp).c_str(), bxb::FormatCount(s.lostXp).c_str(), s.losses, s.levelUps,
	              bxb::FormatClock(s.activeSeconds).c_str(), g_session.Resets(), static_cast<unsigned long long>(g_frames.load()),
	              static_cast<unsigned long long>(g_drawn.load()), g_drawErrors.load(), g_lastDrawResult.load(), g_lastScreenW, g_lastScreenH,
	              g_lastDefaultSize);
}

void Lines(char* out, size_t size) noexcept {
	std::lock_guard<std::mutex> lock(g_lock);
	std::string joined;
	for (const std::string& l : g_lines) {
		joined += (joined.empty() ? "" : " | ") + l;
	}
	std::snprintf(out, size, "%s", joined.empty() ? "(nothing to show yet)" : joined.c_str());
}

}  // namespace display
