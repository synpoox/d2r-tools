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

auto RowShows(const MenuRow& row, uint64_t handler) noexcept -> bool {
	if (handler == 0 || row.count < 2) {
		return false;
	}
	const uint32_t shown = row.count - 1 < MenuMaxHandlers ? row.count - 1 : MenuMaxHandlers;
	for (uint32_t i = 0; i < shown; ++i) {
		if (row.handlers[i] == handler) {
			return true;
		}
	}
	return false;
}

void ReadOffers(const MenuRow& row, const Handlers& handlers, MenuState* state) noexcept {
	for (int a = 1; a < ActionCount; ++a) {
		state->offers[a] = RowShows(row, handlers.of[a]);
	}
}

auto ChooseAction(const MenuState& menu, const Handlers& handlers) noexcept -> Action {
	for (int a = 1; a < ActionCount; ++a) {
		if (handlers.of[a] != 0 && menu.offers[a]) {
			return static_cast<Action>(a);
		}
	}
	return Action::None;
}

auto IsLeftPress(uint32_t flags) noexcept -> bool {
	return (flags & cmd::Left) != 0 && (flags & cmd::Press) != 0 && (flags & (cmd::Held | cmd::Release)) == 0;
}

auto ClassifyClick(const ClickInfo& click, bool hotkeyHeld, const MenuRow* rows, size_t rowCount) noexcept -> Click {
	if (click.unitType != UnitTypeMonster || !IsLeftPress(click.flags) || !click.hasTarget ||
	    click.targetType != UnitTypeMonster || click.targetId != click.unitId || !hotkeyHeld) {
		return Click::Vanilla;
	}
	return FindMenuRow(rows, rowCount, click.targetClass) >= 0 ? Click::Npc : Click::NoMenu;
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
		return false;  // idle, or the callback is already queued for this interaction
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
                            uint64_t nowMs) noexcept {
	if (!m_enabled) {
		return;
	}
	switch (ClassifyClick(click, hotkeyHeld, rows, rowCount)) {
	case Click::Npc:
		++m_counters.npcClicks;
		m_interaction.Arm(click.unitId, nowMs);
		break;
	case Click::NoMenu:
		++m_counters.noMenu;
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

auto Controller::OnQueuedCallback(const MenuState& menu, const Handlers& handlers) noexcept -> Action {
	if (!m_interaction.TakeQueued(menu.npcId)) {
		return Action::None;  // switched off, another click, or already done
	}
	const Action action = m_enabled && menu.shown ? ChooseAction(menu, handlers) : Action::None;
	if (action == Action::None) {
		++m_counters.dropped;  // a quest NPC, nothing to identify, an option switched off: D2R's menu stays
		return Action::None;
	}
	++m_counters.pressed[static_cast<int>(action)];
	return action;
}

auto DecidePart(bool offByFile) noexcept -> PartDecision {
	if (offByFile) {
		return PartDecision::OffByFile;
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
