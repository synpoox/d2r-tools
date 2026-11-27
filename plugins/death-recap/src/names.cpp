#include "names.h"
#include "native.h"
#include "scan.h"
#include "signatures.h"
#include <mutex>

namespace recap {
namespace {
const D2RL::DataTableService* tables;
const D2RL::LocalizationService* localization;
std::mutex nameMutex;
std::array<std::vector<std::string>, 4> names;
void Loaded(const D2RL::PluginContext* ctx, const D2RL::Lifecycle::DataTablesLoadedEvent*, void*) noexcept {
	try {
		std::array<std::vector<std::string>, 4> copy;
		for (uint32_t bank = 1; bank <= 3; ++bank) {
			D2RL::DataTables::TableView view {}; view.structSize = sizeof(view);
			if (tables->getTable(ctx, static_cast<D2RL::DataTables::Bank>(bank), D2RL::DataTables::TableId::States, &view) == D2RL::DataTables::Result::Success)
				native::StateCount(bank, std::min(view.rowCount, 1024U));
			else native::StateCount(bank, 0);
			view = {}; view.structSize = sizeof(view);
			if (tables->getTable(ctx, static_cast<D2RL::DataTables::Bank>(bank), D2RL::DataTables::TableId::MonStats, &view) != D2RL::DataTables::Result::Success || view.rowSize != 0x1fc || view.rowCount > 32768 || !view.rows) continue;
			copy[bank].resize(view.rowCount);
			for (uint32_t i = 0; i < view.rowCount; ++i) {
				const auto* row = static_cast<const uint8_t*>(view.rows) + size_t(i) * view.rowSize;
				uint16_t index, stringId; std::memcpy(&index, row, 2); std::memcpy(&stringId, row + 0x36, 2);
				if (index != i) continue;
				char text[256] {};
				if (localization && localization->getStringById(ctx, stringId, text, sizeof(text), nullptr) == D2RL::Localization::Result::Success && text[0]) copy[bank][i] = text;
				else {
					const auto* key = reinterpret_cast<const char*>(row + 2);
					const auto* end = static_cast<const char*>(std::memchr(key, 0, 47));
					if (end) copy[bank][i] = "monstats:" + std::string(key, end);
				}
			}
		}
		std::lock_guard lock(nameMutex); names = std::move(copy);
	} catch (...) { }
}
}
bool InstallNames(const D2RL::PluginContext* ctx) noexcept {
	const D2RL::LifecycleService* lifecycle = nullptr;
	if (!native::Find(ctx->exeBase, native::NameLayoutSig) ||
		ctx->QueryService(&tables) != D2RL::ServiceQueryResult::Success || !D2RL::HasDataTableServiceField(tables, D2RL::DataTableServiceRequiredSize) ||
		ctx->QueryService(&lifecycle) != D2RL::ServiceQueryResult::Success || !D2RL::HasLifecycleServiceField(lifecycle, D2RL::LifecycleServiceRequiredSize)) return false;
	if (ctx->QueryService(&localization) != D2RL::ServiceQueryResult::Success || !D2RL::HasLocalizationServiceField(localization, D2RL::LocalizationServiceRequiredSize)) localization = nullptr;
	D2RL::Lifecycle::DataTablesLoadedListener listener {}; listener.structSize = sizeof(listener); listener.callback = Loaded;
	D2RL::Lifecycle::ListenerHandle h = 0;
	return lifecycle->registerDataTablesLoadedListener(ctx, &listener, &h) == D2RL::Lifecycle::Result::Success;
}
void CopyNames(Report& report) {
	std::lock_guard lock(nameMutex);
	for (const auto& e : report.timeline) {
		if (!e.attacker.Confirmed() || !e.dataBank.Confirmed() || e.attacker.value.type != 1 || e.dataBank.value >= names.size()) continue;
		const auto unit = e.attacker.value; const auto bank = e.dataBank.value;
		if (unit.classId >= names[bank].size() || names[bank][unit.classId].empty()) continue;
		const auto found = std::find_if(report.names.begin(), report.names.end(), [&](const NamedUnit& n) { return n.bank == bank && n.unit.type == unit.type && n.unit.id == unit.id && n.unit.classId == unit.classId; });
		if (found == report.names.end()) report.names.push_back({unit, bank, names[bank][unit.classId]});
	}
}
std::string SourceName(const Report& report, const Event& e) {
	if (!e.attacker.Confirmed()) return "unknown";
	const auto u = e.attacker.value;
	if (e.dataBank.Confirmed()) for (const auto& n : report.names) if (n.bank == e.dataBank.value && n.unit.type == u.type && n.unit.id == u.id && n.unit.classId == u.classId) return n.text;
	return "type " + std::to_string(u.type) + " row " + std::to_string(u.classId);
}
}
