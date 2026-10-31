// The real engine for the swap: D2R's own functions on the server player (d2r.h). Game thread only.
#pragma once

#include "d2r.h"
#include "swap.h"

#include <cstddef>
#include <cstdint>

namespace ms {

struct Player {
	d2r::Client* client;
	d2r::Unit* unit;
	d2r::Game* game;
	uint32_t dataFlag;
	int32_t charClass;
	char name[MaxName];
};

// Finds the server player behind a console command's client. Every pointer is first read with ReadProcessMemory, so a
// pointer that is not a server client (a wrong guess about what D2RLoader passes) gives a reason, not a crash. Checks:
// readable client, ClientGetControlUnit gives a player whose client is this client, a game, and the two copies of the
// name (player data +0 and client +0x2c) agree.
auto ResolvePlayer(d2r::Client* client, Player* out, char* why, size_t size) noexcept -> bool;

auto InTown(const Player& p) noexcept -> bool;
auto Alive(const Player& p) noexcept -> bool;

// D2R's respec refunds one point per level; spending charges the skpoints formula. A class skill with a formula
// (record +0x210 != -1) would mint or burn points on every swap: refuse. Names the first one.
auto CostsAreOnePoint(const Player& p, const LiveChar& live, char* why, size_t size) noexcept -> bool;

class D2REngine final : public Engine {
public:
	explicit D2REngine(const Player& p) noexcept : m_p(p) {}
	auto Read(LiveChar* out) noexcept -> bool override;
	void Reset() noexcept override;
	auto CanSpend(int32_t index) noexcept -> bool override;
	auto Spend(int32_t index) noexcept -> bool override;
	void Select(bool left, const SkillSlot& slot) noexcept override;

private:
	auto SkillId(int32_t index) noexcept -> int32_t;
	auto HardLevel(int32_t skillId) noexcept -> int32_t;
	Player m_p;
};

}  // namespace ms
