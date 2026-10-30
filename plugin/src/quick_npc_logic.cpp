// Quick NPC decision logic (quick_npc_logic.h).
#include "quick_npc_logic.h"

#include <cstdio>
#include <initializer_list>

namespace quick_npc {

auto FindMenuRow(const MenuRow* rows, size_t rowCount, uint32_t classId) noexcept -> int {
	if (rows == nullptr) {
		return -1;
	}
	for (size_t i = 0; i < rowCount; ++i) {
		if (static_cast<uint16_t>(rows[i].classId) == static_cast<uint16_t>(classId)) {
			return static_cast<int>(i);
		}
	}
	return -1;
}

auto RowOffersTrade(const MenuRow& row, uint64_t tradeHandler) noexcept -> bool {
	if (tradeHandler == 0 || row.count < 2) {
		return false;
	}
	const uint32_t shown = row.count - 1 < MenuMaxHandlers ? row.count - 1 : MenuMaxHandlers;
	for (uint32_t i = 0; i < shown; ++i) {
		if (row.handlers[i] == tradeHandler) {
			return true;
		}
	}
	return false;
}

auto IsVendorClass(const MenuRow* rows, size_t rowCount, uint32_t classId, uint64_t tradeHandler) noexcept -> bool {
	const int index = FindMenuRow(rows, rowCount, classId);
	return index >= 0 && RowOffersTrade(rows[index], tradeHandler);
}

auto IsLeftPress(uint32_t flags) noexcept -> bool {
	return (flags & cmd::Left) != 0 && (flags & cmd::Press) != 0 && (flags & (cmd::Held | cmd::Release)) == 0;
}

auto ClassifyClick(const ClickInfo& click, bool hotkeyHeld, const MenuRow* rows, size_t rowCount,
                   uint64_t tradeHandler) noexcept -> Click {
	if (click.unitType != UnitTypeMonster || !IsLeftPress(click.flags) || !click.hasTarget ||
	    click.targetType != UnitTypeMonster || click.targetId != click.unitId || !hotkeyHeld) {
		return Click::Vanilla;
	}
	return IsVendorClass(rows, rowCount, click.targetClass, tradeHandler) ? Click::Vendor : Click::NotVendor;
}

void Interaction::Arm(uint32_t unitId, uint64_t nowMs) noexcept {
	m_state = State::Armed;
	m_unit = unitId;
	m_armedAt = nowMs;
}

void Interaction::Clear() noexcept {
	m_state = State::Idle;
	m_unit = 0;
	m_armedAt = 0;
}

auto Interaction::MenuOpened(uint32_t unitId, uint64_t nowMs) noexcept -> bool {
	if (m_state != State::Armed) {
		return false;  // idle, or Trade is already queued for this interaction
	}
	if (unitId != m_unit || nowMs < m_armedAt || nowMs - m_armedAt > m_timeoutMs) {
		Clear();  // another NPC's dialog, or a stale click: this interaction is over
		return false;
	}
	m_state = State::Queued;
	return true;
}

auto Interaction::TakeQueued(uint32_t currentNpc) noexcept -> bool {
	const bool go = m_state == State::Queued && currentNpc == m_unit;
	if (m_state == State::Queued) {
		Clear();
	}
	return go;
}

void Controller::SetEnabled(bool enabled) noexcept {
	m_enabled = enabled;
	m_interaction.Clear();
}

void Controller::OnInteract(const ClickInfo& click, bool hotkeyHeld, const MenuRow* rows, size_t rowCount,
                            uint64_t tradeHandler, uint64_t nowMs) noexcept {
	if (!m_enabled) {
		return;
	}
	switch (ClassifyClick(click, hotkeyHeld, rows, rowCount, tradeHandler)) {
	case Click::Vendor:
		++m_counters.npcClicks;
		++m_counters.armed;
		m_interaction.Arm(click.unitId, nowMs);
		break;
	case Click::NotVendor:
		++m_counters.npcClicks;
		++m_counters.notVendor;
		m_interaction.Clear();
		break;
	case Click::Vanilla:
		m_interaction.Clear();  // a vanilla interaction replaces any pending one
		break;
	}
}

auto Controller::OnMenuOpened(const MenuState& menu, uint64_t nowMs) noexcept -> bool {
	if (!m_enabled || !menu.shown || !m_interaction.MenuOpened(menu.npcId, nowMs)) {
		return false;
	}
	++m_counters.queued;
	return true;
}

void Controller::OnQueueFailed() noexcept {
	++m_counters.queueFailed;
	m_interaction.Clear();
}

auto Controller::OnQueuedCallback(const MenuState& menu) noexcept -> bool {
	const bool taken = m_interaction.TakeQueued(menu.npcId);
	if (!taken) {
		return false;  // switched off, another click, or already done
	}
	if (!m_enabled || !menu.shown || !menu.offersTrade) {
		++m_counters.dropped;  // the menu changed while the callback waited
		return false;
	}
	++m_counters.traded;
	return true;
}

auto DecidePart(bool offByFile, bool needsImage, bool imageOk) noexcept -> PartDecision {
	if (offByFile) {
		return PartDecision::OffByFile;
	}
	if (needsImage && !imageOk) {
		return PartDecision::OffImage;
	}
	return PartDecision::Install;
}

auto RunInstall(const InstallSteps& steps, char* report, size_t reportSize) noexcept -> InstallResult {
	char why[160] = "";
	if (steps.servicesReady == nullptr || steps.alreadyHooked == nullptr || steps.installHooks == nullptr ||
	    !steps.servicesReady(steps.user, why, sizeof(why))) {
		std::snprintf(report, reportSize, "off: %s", why[0] != '\0' ? why : "D2RLoader services missing");
		return InstallResult::NoServices;
	}
	for (const uint64_t rva : {RvaInteract, RvaOpenNpcMenu}) {
		if (steps.alreadyHooked(steps.user, rva)) {
			std::snprintf(report, reportSize, "off: 0x%llx is already hooked (jmp [rip+x] at its entry)",
			              static_cast<unsigned long long>(rva));
			return InstallResult::AlreadyHooked;
		}
	}
	if (!steps.installHooks(steps.user, why, sizeof(why))) {
		std::snprintf(report, reportSize, "off: hooks on 0x%llx + 0x%llx refused, nothing patched (%s)",
		              static_cast<unsigned long long>(RvaInteract), static_cast<unsigned long long>(RvaOpenNpcMenu),
		              why[0] != '\0' ? why : "entry bytes differ");
		return InstallResult::HooksRefused;
	}
	std::snprintf(report, reportSize, "on: hooks 0x%llx + 0x%llx", static_cast<unsigned long long>(RvaInteract),
	              static_cast<unsigned long long>(RvaOpenNpcMenu));
	return InstallResult::On;
}

}  // namespace quick_npc
