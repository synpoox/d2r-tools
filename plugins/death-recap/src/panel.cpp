#include "panel.h"
#include "scan.h"
#include "signatures.h"
#include "summary.h"
#include "names.h"
#include "log.h"
#include <sstream>
#include <iomanip>

namespace recap::panel {
namespace {
const D2RL::PluginContext* context;
const D2RL::PanelService* panels;
D2RL::Panels::RegistrationHandle handle = 0;
using FindPanelFn = void* (*)(const char*) noexcept;
using FindChildFn = void* (*)(void*, const char*) noexcept;
struct TextView { const char* text; uint64_t size; };
using SetTextFn = void (*)(void*, const TextView*) noexcept;
FindPanelFn findPanel;
FindChildFn findChild;
SetTextFn setText;
std::string Number(int64_t n) { std::ostringstream o; o << std::fixed << std::setprecision(2) << double(n) / 256.; return o.str(); }
std::string Value(const Amount& n) { return n.Confirmed() ? Number(n.value) : "unavailable"; }
std::string Measured(const Measure& m) { return m.known ? Number(m.total) + (m.unknown ? " (partial)" : "") : "unavailable"; }
const char* KindName(Kind k) {
	constexpr const char* names[] {"damage", "healing", "potion", "regen/net", "cost", "state", "sample", "death", "other"};
	return names[std::min<size_t>(static_cast<size_t>(k), 8)];
}
bool Text(void* root, const char* name, const char* text) noexcept {
	__try {
		void* box = findChild(root, name); if (!box) return false;
		const TextView view {text, std::strlen(text)}; setText(box, &view); return true;
	} __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
bool Draw(const History& history, size_t selected, size_t page) {
	void* root = findPanel("death-recap/Recap");
	if (!root) return false;
	std::array<std::string, 24> lines;
	lines[0] = "DEATH RECAP";
	lines[22] = "F8: close/reopen  |  [ / ]: history  |  PgUp/PgDn: timeline";
	lines[23] = "Pad: D-pad history/timeline, B dismiss  |  Escape dismiss  |  Export/clear via console";
	const Report* report = history.empty() ? nullptr : &history[std::min(selected, history.size() - 1)];
	if (!report) lines[2] = "No recorded deaths.";
	else {
		const auto& r = *report;
		const auto summary = Summarize(r);
		lines[1] = "Report " + std::to_string(selected + 1) + " / " + std::to_string(history.size()) + "  |  " + (r.replicated ? "REPLICATED SNAPSHOTS" : "LOCAL CAPTURE") + " - PARTIAL COVERAGE";
		const bool lifeGross = std::any_of(r.timeline.begin(), r.timeline.end(), [](const Event& e) { return GrossKnown(e.life); });
		const bool manaGross = std::any_of(r.timeline.begin(), r.timeline.end(), [](const Event& e) { return GrossKnown(e.mana); });
		lines[2] = "HP lost " + (lifeGross ? Number(r.life.lost) : "unavailable") + " / restored " + (lifeGross ? Number(r.life.restored) : "unavailable") + " / unresolved net " + Number(r.life.unresolvedNet);
		lines[3] = "Mana lost " + (manaGross ? Number(r.mana.lost) : "unavailable") + " / restored " + (manaGross ? Number(r.mana.restored) : "unavailable") + " / unresolved net " + Number(r.mana.unresolvedNet);
		lines[4] = "Measured subtotals; poison, OW, shield/cost attribution and other writers may be missing.";
		lines[5] = "Attempted direct damage " + Measured(summary.post) + " | overkill " + Measured(summary.overkill);
		lines[6] = "Killing blow: ";
		if (const auto* fatal = KillingBlow(r)) lines[6] += "confirmed event " + std::to_string(fatal->sequence);
		else lines[6] += "unavailable (the last hit alone is not proof)";
		lines[7] = "Source/skill: unknown when capture context is unavailable; projectile ability is never guessed.";
		lines[8] = std::string("Accounting: ") + (r.life.Reconciled() && r.mana.Reconciled() ? "reconciled" : "incomplete") + " | dropped " + std::to_string(r.feedDropped) + (r.windowTruncated ? " | truncated window" : "") + (r.sequenceGap ? " | missing events" : "");
		lines[9] = "HP samples across reporting window (gaps are not interpolated)";
		lines[10] = "HP by source: ";
		size_t sourceCount = 0;
		for (const auto& [key, measure] : summary.lossByAttacker) {
			if (sourceCount++ == 2) { lines[10] += "... (all sources in JSON)"; break; }
			std::string name = "unknown";
			if (key) for (const auto& e : r.timeline) if (Key(e.attacker) == key) { name = SourceName(r, e); break; }
			lines[10] += name + " " + Measured(measure) + "; ";
		}
		lines[11] = "HP by ability: unknown " + (summary.lossBySkill.contains(std::nullopt) ? Measured(summary.lossBySkill.at(std::nullopt)) : "unavailable");
		lines[12] = "Before effects (physical/fire/lightning/cold/magic): ";
		lines[13] = "After effects (physical/fire/lightning/cold/magic): ";
		for (size_t i : {0U, 1U, 2U, 3U, 5U}) {
			lines[12] += Measured(summary.preByElement[i]) + " / ";
			lines[13] += Measured(summary.postByElement[i]) + " / ";
		}
		std::vector<const Event*> mutations;
		for (const auto& e : r.timeline) if (e.kind != Kind::Sample) mutations.push_back(&e);
		const size_t pages = std::max<size_t>(1, (mutations.size() + 5) / 6);
		page = std::min(page, pages - 1);
		lines[14] = "Timeline " + std::to_string(page + 1) + " / " + std::to_string(pages) + " (seconds before death)";
		for (size_t i = 0; i < 6 && page * 6 + i < mutations.size(); ++i) {
			const auto& e = *mutations[page * 6 + i];
			std::ostringstream o; o << std::fixed << std::setprecision(3) << '-' << double(r.deathMs - e.ms) / 1000 << "s " << KindName(e.kind)
				<< " | HP " << Value(e.life.before) << " -> " << Value(e.life.after) << " | mana " << Value(e.mana.before) << " -> " << Value(e.mana.after);
			o << " | " << SourceName(r, e);
			lines[15 + i] = o.str();
		}
		const auto& d = r.timeline.back().defense;
		lines[21] = "Defense snapshot: " + std::to_string(d.statCount) + " stats, states " + (d.stateEvidence == Evidence::Confirmed ? std::to_string(d.stateCount) + (d.statesTruncated ? "+" : "") : "unavailable") + "; modifiers unavailable. Details in JSON export.";
	}
	for (size_t i = 0; i < lines.size(); ++i) {
		char name[20]; std::snprintf(name, sizeof(name), "Line%zu", i);
		if (!Text(root, name, lines[i].c_str())) return false;
	}
	return true;
}
void Graph(const Report& r) noexcept {
	__try {
		void* root = findPanel("death-recap/Recap"); if (!root) return;
		for (size_t i = 0; i < 64; ++i) {
			const Event* point = nullptr;
			const uint64_t duration = std::max<uint64_t>(1, r.deathMs - r.windowStartMs + 1);
			for (const auto& e : r.timeline) {
				if (e.ms < r.windowStartMs || !e.life.after.Confirmed() || !e.maxLife.Confirmed() || e.maxLife.value <= 0) continue;
				if (std::min<uint64_t>(63, (e.ms - r.windowStartMs) * 64 / duration) == i) point = &e;
			}
			char name[16]; std::snprintf(name, sizeof(name), "HP%zu", i);
			void* widget = findChild(root, name); if (!widget) continue;
			auto* rect = reinterpret_cast<int32_t*>(static_cast<uint8_t*>(widget) + 0x70);
			if (rect[2] != 12) continue;
			const int32_t height = point ? static_cast<int32_t>(std::clamp<int64_t>(int64_t(point->life.after.value) * 120 / point->maxLife.value, 1, 120)) : 0;
			rect[1] = 590 - height; rect[3] = height;
		}
	} __except (EXCEPTION_EXECUTE_HANDLER) { }
}
}
bool Install(const D2RL::PluginContext* ctx) noexcept {
	context = ctx;
	const D2RL::ResourceService* resources = nullptr;
	if (ctx->QueryService(&panels) != D2RL::ServiceQueryResult::Success || !D2RL::HasPanelServiceField(panels, D2RL::PanelServiceRequiredSize) ||
		ctx->QueryService(&resources) != D2RL::ServiceQueryResult::Success || !D2RL::HasResourceServiceField(resources, D2RL::ResourceServiceRequiredSize)) return false;
	findPanel = reinterpret_cast<FindPanelFn>(native::Find(ctx->exeBase, native::FindPanelSig));
	findChild = reinterpret_cast<FindChildFn>(native::Find(ctx->exeBase, native::FindChildSig));
	const auto* text = native::Find(ctx->exeBase, native::SetTextSig);
	setText = text ? reinterpret_cast<SetTextFn>(const_cast<uint8_t*>(text - 16)) : nullptr;
	if (!findPanel || !findChild || !setText) return false;
	try {
		std::ostringstream o;
		o << R"({"type":"Panel","name":"death-recap/Recap","fields":{"rect":{"x":-690,"y":-750,"width":1380,"height":1500},"anchor":{"x":0.5,"y":0.5}},"children":[
{"type":"RectangleWidget","name":"Background","fields":{"rect":{"x":0,"y":0,"width":1380,"height":1500},"color":[0.035,0.02,0.015,0.96]}})";
		for (size_t i = 0; i < 24; ++i) {
			const size_t y = i < 10 ? 20 + i * 43 : i < 14 ? 620 + (i - 10) * 45 : 830 + (i - 14) * 51;
			o << ",{\"type\":\"TextBoxWidget\",\"name\":\"Line" << i << "\",\"fields\":{\"text\":\"\",\"rect\":{\"x\":24,\"y\":" << y << ",\"width\":1330,\"height\":46},\"style\":{\"pointSize\":24,\"fontColor\":{\"r\":224,\"g\":211,\"b\":177,\"a\":255},\"alignment\":{\"h\":\"left\",\"v\":\"center\"}}}}";
		}
		for (size_t i = 0; i < 64; ++i) o << ",{\"type\":\"RectangleWidget\",\"name\":\"HP" << i << "\",\"fields\":{\"rect\":{\"x\":" << 24 + i * 20 << ",\"y\":590,\"width\":12,\"height\":0},\"color\":[0.62,0.12,0.1,1]}}";
		o << "]}";
		const auto json = o.str();
		D2RL::Resources::ResourceRegistration resource {}; resource.structSize = sizeof(resource);
		resource.path = "data/global/ui/layouts/death-recap/Recaphd.json"; resource.bytes = json.data(); resource.byteCount = json.size();
		D2RL::Resources::RegistrationHandle resourceHandle = 0;
		if (resources->registerResource(ctx, &resource, &resourceHandle) != D2RL::Resources::Result::Success) return false;
		D2RL::Panels::PanelRegistration registration {}; registration.structSize = sizeof(registration);
		registration.localId = "Recap"; registration.flags = D2RL::Panels::PanelFlags::CloseOnEscape;
		if (panels->registerPanel(ctx, &registration, &handle) != D2RL::Panels::Result::Success) return false;
		// One route is available globally. A conflict leaves only this optional
		// controller reopen route off; other plugins keep their route.
		constexpr const char* title = R"([{"Key":"Title","enUS":"Death Recap","deDE":"Death Recap","esES":"Death Recap","esMX":"Death Recap","frFR":"Death Recap","itIT":"Death Recap","jaJP":"Death Recap","koKR":"Death Recap","plPL":"Death Recap","ptBR":"Death Recap","ruRU":"Death Recap","zhCN":"Death Recap","zhTW":"Death Recap"}])";
		resource.path = "data/local/lng/strings/d2rloader/death-recap/strings.json"; resource.bytes = title; resource.byteCount = std::strlen(title);
		const bool strings = resources->registerResource(ctx, &resource, &resourceHandle) == D2RL::Resources::Result::Success;
		if (strings && D2RL::HasPanelServiceField(panels, static_cast<uint32_t>(offsetof(D2RL::PanelService, registerControllerRoute) + sizeof(panels->registerControllerRoute))) && panels->registerControllerRoute) {
			D2RL::Panels::ControllerRouteRegistration route {}; route.structSize = sizeof(route);
			route.panel = handle; route.flags = D2RL::Panels::ControllerRouteFlags::PairWithPlayerInventory;
			route.placement = D2RL::Panels::ControllerRoutePlacement::AfterPlayerInventory; route.localizedLabelKey = "@death-recap:Title";
			D2RL::Panels::ControllerRouteHandle h = 0;
			const auto result = panels->registerControllerRoute(ctx, &route, &h);
			plog::Info("controller reopen route result %u", static_cast<unsigned>(result));
		}
		return true;
	} catch (...) { return false; }
}
bool Open() noexcept {
	if (!handle) return false;
	D2RL::Panels::PanelInfo info {}; info.structSize = sizeof(info);
	return panels->getPanelInfo(context, handle, &info) == D2RL::Panels::Result::Success && info.presentationState == D2RL::Panels::PresentationState::Open;
}
void Show(bool toggle) noexcept {
	if (!handle) return;
	if (toggle) (void)panels->togglePanel(context, handle); else (void)panels->openPanel(context, handle);
}
void Close() noexcept { if (handle) (void)panels->closePanel(context, handle); }
void Update(const History& history, size_t selected, size_t page) noexcept {
	static bool rendered = false;
	static size_t oldSelected = 0, oldPage = 0, oldSize = 0;
	static uint64_t oldDeath = 0;
	if (!Open()) { rendered = false; return; }
	const uint64_t death = history.empty() ? 0 : history[std::min(selected, history.size() - 1)].deathMs;
	if (rendered && selected == oldSelected && page == oldPage && history.size() == oldSize && death == oldDeath) return;
	try {
		if (Draw(history, selected, page)) {
			if (!history.empty()) Graph(history[std::min(selected, history.size() - 1)]);
			else { const Report empty; Graph(empty); }
			rendered = true; oldSelected = selected; oldPage = page; oldSize = history.size(); oldDeath = death;
		}
	}
	catch (...) { plog::Warn("recap panel update failed"); }
}
}
