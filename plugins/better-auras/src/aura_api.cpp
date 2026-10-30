// Binds aura_api.h to the running image (build 93847 only; plugin.cpp checks the image first).
#include "aura_api.h"

namespace aura {

Api api {};
StartRightAuraFn startRightAuraOriginal = nullptr;

namespace {
template <typename Fn>
void BindOne(Fn& slot, uint64_t exeBase, uint64_t rva) noexcept {
	slot = reinterpret_cast<Fn>(exeBase + rva);
}
}  // namespace

void Bind(uint64_t exeBase) noexcept {
	BindOne(api.scheduleAuraTick, exeBase, 0x43b2b0);
	BindOne(api.removeEvents, exeBase, 0x48b890);
	BindOne(api.tickArg, exeBase, 0x3e2b64c);
	BindOne(api.unitGame, exeBase, 0x48ff00);
	BindOne(api.rightSkill, exeBase, 0x34b400);
	BindOne(api.skillEntryId, exeBase, 0x33e080);
	BindOne(api.unitSkill, exeBase, 0x33dd40);
	BindOne(api.skillLevel, exeBase, 0x33d1e0);
	BindOne(api.stateList, exeBase, 0x2f5940);
	BindOne(api.detachList, exeBase, 0x2f7920);
	BindOne(api.freeList, exeBase, 0x2f4180);
	BindOne(api.setState, exeBase, 0x3354c0);
	BindOne(api.hasState, exeBase, 0x3351b0);
	BindOne(api.setListStat, exeBase, 0x2f7c00);
	BindOne(api.listStat, exeBase, 0x2f5df0);
	BindOne(api.stat, exeBase, 0x2f5020);
	BindOne(api.setStat, exeBase, 0x2f7d10);
	BindOne(api.addStat, exeBase, 0x2f34f0);
	BindOne(api.unitRoom, exeBase, 0x34b440);
	BindOne(api.roomIsTown, exeBase, 0x2f0750);
	BindOne(api.unitDead, exeBase, 0x34c2c0);
}

}  // namespace aura
