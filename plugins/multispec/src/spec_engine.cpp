#include "spec_engine.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <cstdio>
#include <cstring>

namespace ms {

namespace {

// Copies `size` bytes at `p` without faulting. False when any of it is not readable.
auto SafeRead(const void* p, void* out, size_t size) noexcept -> bool {
	if (p == nullptr) {
		return false;
	}
	SIZE_T got = 0;
	return ReadProcessMemory(GetCurrentProcess(), p, out, size, &got) != 0 && got == size;
}

auto Readable(const void* p, size_t size) noexcept -> bool {
	uint8_t buf[0x500];
	return size <= sizeof(buf) && SafeRead(p, buf, size);
}

// A NUL-terminated name of at most 15 characters at `p`.
auto ReadName(const void* p, char* out) noexcept -> bool {
	char raw[16] = {};
	if (!SafeRead(p, raw, sizeof(raw))) {
		return false;
	}
	raw[15] = '\0';
	std::snprintf(out, MaxName, "%s", raw);
	return ValidName(out);
}

}  // namespace

auto ResolvePlayer(d2r::Client* client, Player* out, char* why, size_t size) noexcept -> bool {
	*out = {};
	uint8_t c[d2r::ClientSize];
	if (client == nullptr || !SafeRead(client, c, sizeof(c))) {
		std::snprintf(why, size, "the console gave no readable client");
		return false;
	}
	if ((d2r::At<uint32_t>(c, d2r::ClientFlags) & 1U) != 0 || d2r::At<uint64_t>(c, 0x278) == 0) {
		std::snprintf(why, size, "the client has no player yet");
		return false;
	}
	d2r::Unit* unit = d2r::api.controlUnit(client, 0);
	if (unit == nullptr || !Readable(unit, 0x108)) {
		std::snprintf(why, size, "no player unit for this client (not a server client?)");
		return false;
	}
	if (d2r::api.unitType(unit) != 0 || d2r::api.unitClient(unit) != client) {
		std::snprintf(why, size, "client and player do not match (not a server client?)");
		return false;
	}
	d2r::Game* game = d2r::api.unitGame(unit);
	if (game == nullptr || !Readable(game, d2r::GameDataFlag + 1)) {
		std::snprintf(why, size, "the player has no game");
		return false;
	}
	const uint8_t* data = d2r::api.playerData(unit);
	char fromData[MaxName] = "";
	char fromClient[MaxName] = "";
	if (data == nullptr || !ReadName(data + d2r::PlayerDataName, fromData) ||
	    !ReadName(c + d2r::ClientName, fromClient) || std::strcmp(fromData, fromClient) != 0) {
		std::snprintf(why, size, "could not read the character name (player data '%s', client '%s')", fromData, fromClient);
		return false;
	}
	const int32_t cls = d2r::api.unitClass(unit, __FILE__, __LINE__);
	if (cls < 0 || cls > 7) {
		std::snprintf(why, size, "unknown character class %d", cls);
		return false;
	}
	out->client = client;
	out->unit = unit;
	out->game = game;
	out->dataFlag = d2r::At<uint8_t>(game, d2r::GameDataFlag);
	out->charClass = cls;
	std::snprintf(out->name, sizeof(out->name), "%s", fromData);
	return true;
}

auto InTown(const Player& p) noexcept -> bool {
	d2r::Room* room = d2r::api.unitRoom(p.unit);
	return room != nullptr && d2r::api.roomInTown(room) != 0;
}

auto Alive(const Player& p) noexcept -> bool {
	const uint32_t mode = d2r::At<uint32_t>(p.unit, d2r::UnitMode);
	return mode != d2r::PlayerModeDeath && mode != d2r::PlayerModeDead;
}

auto CostsAreOnePoint(const Player& p, const LiveChar& live, char* why, size_t size) noexcept -> bool {
	for (int32_t i = 0; i < live.skillCount; ++i) {
		const uint8_t* rec = d2r::api.skillRecord(p.dataFlag, live.skillIds[i]);
		if (rec == nullptr) {
			std::snprintf(why, size, "skill %d has no skills.txt row", live.skillIds[i]);
			return false;
		}
		if (d2r::At<int32_t>(rec, d2r::SkPointsCalc) != -1) {
			std::snprintf(why, size, "skill %d has a skpoints formula; swapping would gain or lose points",
			              live.skillIds[i]);
			return false;
		}
	}
	return true;
}

auto D2REngine::SkillId(int32_t index) noexcept -> int32_t {
	return d2r::api.classSkillId(m_p.dataFlag, m_p.charClass, index);
}

auto D2REngine::HardLevel(int32_t skillId) noexcept -> int32_t {
	d2r::Skill* skill = d2r::api.getSkill(m_p.unit, skillId, -1);
	return skill != nullptr ? d2r::api.getSkillLevel(m_p.unit, skill, 0, 0) : 0;
}

auto D2REngine::Read(LiveChar* out) noexcept -> bool {
	*out = {};
	std::snprintf(out->name, sizeof(out->name), "%s", m_p.name);
	out->charClass = m_p.charClass;
	const int32_t n = d2r::api.classSkillCount(m_p.dataFlag, m_p.charClass);
	if (n < 1 || n > MaxSkills) {
		return false;
	}
	out->skillCount = n;
	for (int32_t i = 0; i < n; ++i) {
		const int32_t id = SkillId(i);
		if (id < 0 || id > 32767) {
			return false;
		}
		out->skillIds[i] = static_cast<int16_t>(id);
		const int32_t level = HardLevel(id);
		out->levels[i] = static_cast<uint8_t>(level < 0 ? 0 : (level > 255 ? 255 : level));
	}
	out->unspent = d2r::api.statBase(m_p.unit, d2r::StatNewSkills, 0);
	return true;
}

void D2REngine::Reset() noexcept {
	d2r::api.respecSkills(m_p.game, m_p.unit);
}

// The checks of D2R's add-skill packet handler (0x4b3ee0), for one point.
auto D2REngine::CanSpend(int32_t index) noexcept -> bool {
	const int32_t id = SkillId(index);
	const uint8_t* rec = d2r::api.skillRecord(m_p.dataFlag, id);
	if (rec == nullptr || d2r::At<int32_t>(rec, d2r::SkPointsCalc) != -1) {
		return false;
	}
	int32_t maxLevel = d2r::At<int16_t>(rec, d2r::SkMaxLevel);
	if (maxLevel <= 0) {
		maxLevel = d2r::DefaultMaxLevel;
	}
	return d2r::api.statBase(m_p.unit, d2r::StatNewSkills, 0) >= 1 && HardLevel(id) < maxLevel &&
	       d2r::api.canSkillClass(m_p.unit, id) != 0 && d2r::api.canSkillLevel(m_p.unit, id) != 0 &&
	       d2r::api.canSkillStats(m_p.unit, id) != 0;
}

auto D2REngine::Spend(int32_t index) noexcept -> bool {
	return d2r::api.spendSkillPoint(m_p.dataFlag, m_p.unit, SkillId(index)) != 0;
}

}  // namespace ms
