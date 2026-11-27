// Contracts and ownership: docs/reverse-engineering/death-recap-static.md.
#include "native.h"
#include "scan.h"
#include "signatures.h"
#include "log.h"
#include <D2RLPlugin/mutations.h>

namespace recap::native {
namespace {
const D2RL::PluginContext* context;
const D2RL::MutationService* mutations;
Feed<2048> feed;
std::atomic<uint64_t> identity {UINT32_MAX}, dropped {0};
std::atomic<DWORD> producer {0};
std::atomic<uint64_t> boundIdentity {UINT32_MAX};
std::atomic<bool> stopped {false};
uint32_t parts = 0;
bool unsafeInstall = false;
using StatFn = int32_t (*)(void*, int32_t, int32_t) noexcept;
using WriteFn = uint64_t (*)(void*, int32_t, int32_t, int32_t) noexcept;
using HitFn = uint64_t (*)(void*, void*, void*, int32_t, void*) noexcept;
using EffectsFn = uint64_t (*)(void*, void*, void*, void*) noexcept;
using HealFn = int32_t (*)(void*, int32_t) noexcept;
using ModeFn = uint64_t (*)(void*, void*, void*, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t) noexcept;
using SlotFn = uint32_t (*)() noexcept;
using PlayerFn = void* (*)(uint32_t) noexcept;
StatFn stat;
D2RL::Reimplementations::GetLevelIdFromUnitFn levelFromUnit;
using HasStateFn = int32_t (*)(void*, int32_t) noexcept;
HasStateFn hasState;
std::array<std::atomic<uint32_t>, 4> stateCounts {};
WriteFn lifeWrite, manaWrite, regenLifeWrite, regenManaWrite;
HitFn hitOriginal;
EffectsFn effectsOriginal;
HealFn healOriginal;
ModeFn modeOriginal;
SlotFn slot;
PlayerFn player;
uint64_t token = 0, sequence = 0, mutationsSeen = 0;
bool dead = false, deathPending = false;
Event pendingDeath;
DeathLatch latch;
struct Transaction { void* block = nullptr; void* victim = nullptr; Event metadata; };
thread_local std::array<Transaction, 8> transactions;
thread_local unsigned depth = 0;

template<class T> T Read(const void* p, size_t offset) noexcept {
	T value; std::memcpy(&value, static_cast<const uint8_t*>(p) + offset, sizeof(value)); return value;
}
bool Local(void* u) noexcept {
	return u && !stopped.load(std::memory_order_relaxed) && boundIdentity.load(std::memory_order_acquire) == identity.load(std::memory_order_acquire) && GetCurrentThreadId() == producer.load(std::memory_order_acquire) &&
		Read<uint32_t>(u, 0) == 0 && Read<uint32_t>(u, 8) == static_cast<uint32_t>(identity.load(std::memory_order_acquire));
}
Endpoints Get(void* u) noexcept { return {stat(u, 6, 0), stat(u, 8, 0), stat(u, 7, 0), stat(u, 9, 0)}; }
void Emit(Event e) noexcept {
	if (dead && e.kind != Kind::Death) return;
	e.session = token; e.sequence = ++sequence; e.ms = GetTickCount64();
	if (!feed.Push(e)) {
		++dropped;
		if (e.kind == Kind::Death) { pendingDeath = e; deathPending = true; }
	}
}
bool Begin(void* u) noexcept {
	const uint64_t now = boundIdentity.load(std::memory_order_acquire);
	if (now != identity.load(std::memory_order_acquire)) return false;
	if (now != token) { token = now; sequence = mutationsSeen = 0; dead = deathPending = false; latch.Reset(); }
	if (!dead) Emit(Sample(Get(u)));
	return !dead;
}
Transaction* Current(void* u) noexcept {
	if (depth == 0 || depth > transactions.size()) return nullptr;
	auto& t = transactions[depth - 1];
	return t.victim == u ? &t : nullptr;
}
bool Before(void* u, Endpoints& out) noexcept {
	__try { if (!Local(u) || !Begin(u)) return false; out = Get(u); return true; }
	__except (EXCEPTION_EXECUTE_HANDLER) { stopped = true; return false; }
}
Amount Attempted(void* u) noexcept {
	__try { if (auto* t = Current(u)) return Confirm(Read<int32_t>(t->block, 0x134)); }
	__except (EXCEPTION_EXECUTE_HANDLER) { stopped = true; }
	return {};
}
void After(void* u, Endpoints before, bool isolated, Kind kind, uint64_t nested, int32_t resource, Amount attempted = {}) noexcept {
	__try {
		const auto after = Get(u);
		Event e;
		if (auto* t = Current(u)) {
			e = t->metadata;
			if (kind == Kind::Damage && resource == 6) {
				e.damage.post = attempted;
				if (attempted.Confirmed() && attempted.value >= 0 && before.life >= 0)
					e.damage.overkill = Confirm(static_cast<int32_t>(std::max<int64_t>(0, int64_t(attempted.value) - before.life)));
			}
		}
		if (resource != 6 || kind != Kind::Damage) e.damage = {};
		e.kind = kind;
		if (kind == Kind::Damage) e.cause = Evidence::Confirmed;
		// Nested writes are already in the feed. Only retain a sample for the
		// enclosing writer; otherwise it would record those same losses twice.
		if (nested != mutationsSeen) { Emit(Sample(after)); return; }
		e.life = Transition(before.life, after.life, isolated && resource == 6);
		e.mana = Transition(before.mana, after.mana, isolated && resource == 8);
		if (resource != 6 && before.life == after.life) e.life = Transition(before.life, after.life, true);
		if (resource != 8 && before.mana == after.mana) e.mana = Transition(before.mana, after.mana, true);
		e.maxLife = Confirm(after.maxLife); e.maxMana = Confirm(after.maxMana);
		++mutationsSeen; Emit(e);
		if (deathPending && !dead) { pendingDeath.life.after = Confirm(after.life); pendingDeath.mana.after = Confirm(after.mana); }
	} __except (EXCEPTION_EXECUTE_HANDLER) { stopped = true; }
}
template<WriteFn* Original, bool Isolated, int32_t Resource> uint64_t Write(void* u, int32_t id, int32_t value, int32_t layer) noexcept {
	Endpoints before;
	const bool observe = Before(u, before);
	const uint64_t nested = observe ? mutationsSeen : 0;
	const Amount attempted = observe && Isolated && Resource == 6 ? Attempted(u) : Amount{};
	const uint64_t result = (*Original)(u, id, value, layer);
	if (observe) After(u, before, Isolated, Isolated ? Kind::Damage : Kind::Regen, nested, Resource, attempted);
	return result;
}
int32_t HealHook(void* u, int32_t offered) noexcept {
	Endpoints before;
	const bool observe = Before(u, before);
	const auto nested = observe ? mutationsSeen : 0;
	const int32_t result = healOriginal(u, offered);
	if (observe) After(u, before, true, Kind::Healing, nested, 6);
	return result;
}
void Elements(Event& e, void* block, bool pre) noexcept {
	constexpr size_t offsets[] {0x18, 0x20, 0x2c, 0x34, 0, 0x30};
	auto& amounts = pre ? e.damage.preByElement : e.damage.postByElement;
	for (size_t i = 0; i < amounts.size(); ++i) if (offsets[i]) amounts[i] = Confirm(Read<int32_t>(block, offsets[i]));
}
void Enter(void* game, void* attacker, void* victim, void* block) noexcept {
	__try {
		if (depth <= transactions.size()) {
			auto& t = transactions[depth - 1]; t = {}; t.block = block; t.victim = victim;
			if (game) { const uint32_t bank = Read<uint8_t>(game, 0x106); if (bank >= 1 && bank <= 3) t.metadata.dataBank = {bank, Evidence::Confirmed}; }
			if (attacker) t.metadata.attacker = {{Read<uint32_t>(attacker, 0), Read<uint32_t>(attacker, 8), Read<uint32_t>(attacker, 4)}, Evidence::Confirmed};
		}
	} __except (EXCEPTION_EXECUTE_HANDLER) { stopped = true; }
}
void PublishDeath() noexcept {
	if (GetCurrentThreadId() != producer.load(std::memory_order_acquire) || depth != 0 || !deathPending) return;
	if (!dead) {
		pendingDeath.session = token; pendingDeath.sequence = ++sequence; pendingDeath.ms = GetTickCount64(); dead = true;
	}
	if (feed.Push(pendingDeath)) deathPending = false;
}
uint64_t HitHook(void* game, void* attacker, void* victim, int32_t area, void* block) noexcept {
	// Context is bounded and thread-local, including non-local hits nested in a
	// local transaction. Overflow deliberately makes attribution unavailable.
	++depth; Enter(game, attacker, victim, block);
	const auto result = hitOriginal(game, attacker, victim, area, block);
	--depth; PublishDeath();
	return result;
}
uint64_t EffectsHook(void* game, void* attacker, void* victim, void* block) noexcept {
	Event stages;
	__try { if (block) Elements(stages, block, true); } __except (EXCEPTION_EXECUTE_HANDLER) { stopped = true; }
	const auto result = effectsOriginal(game, attacker, victim, block);
	__try {
		if (auto* t = Current(victim); t && t->block == block) {
			Elements(stages, block, false);
			t->metadata.damage = stages.damage;
		}
	} __except (EXCEPTION_EXECUTE_HANDLER) { stopped = true; }
	return result;
}
void Snapshot(void* game, void* u, Event& e) noexcept {
	const auto p = Get(u); e = Sample(p); e.kind = Kind::Death; e.cause = Evidence::Confirmed;
	const int level = stat(u, 12, 0);
	if (level > 0) e.characterLevel = {static_cast<uint32_t>(level), Evidence::Confirmed};
	if (levelFromUnit) {
		const int area = levelFromUnit(u);
		if (area > 0) e.areaId = {static_cast<uint32_t>(area), Evidence::Confirmed};
	}
	constexpr uint32_t ids[]{36, 37, 39, 40, 41, 42, 43, 44, 45, 34, 35, 142, 143, 144, 145, 148};
	for (uint32_t id : ids) e.defense.stats[e.defense.statCount++] = {id, 0, Confirm(stat(u, static_cast<int32_t>(id), 0))};
	if (game) {
		const uint32_t bank = Read<uint8_t>(game, 0x106);
		if (bank >= 1 && bank <= 3) {
			e.dataBank = {bank, Evidence::Confirmed};
			const auto count = stateCounts[bank].load();
			if (hasState && count) {
				e.defense.stateEvidence = Evidence::Confirmed;
				for (uint32_t i = 0; i < count; ++i) if (hasState(u, static_cast<int32_t>(i))) {
					if (e.defense.stateCount < e.defense.states.size()) e.defense.states[e.defense.stateCount++] = i;
					else e.defense.statesTruncated = true;
				}
			}
		}
	}
}
uint64_t ModeHook(void* game, void* u, void* skill, uint32_t mode, uint32_t type, uint32_t id, uint32_t a, uint32_t b) noexcept {
	Event death; uint32_t before = 0; bool observe = false;
	__try { if (mode == 0 && Local(u) && Begin(u)) { before = Read<uint32_t>(u, 0xc); Snapshot(game, u, death); observe = true; } }
	__except (EXCEPTION_EXECUTE_HANDLER) { stopped = true; }
	const auto result = modeOriginal(game, u, skill, mode, type, id, a, b);
	__try {
		if (observe && latch.ConfirmTransition(before, Read<uint32_t>(u, 0xc))) {
			const auto p = Get(u); death.life.after = Confirm(p.life); death.mana.after = Confirm(p.mana);
			pendingDeath = death; deathPending = true; PublishDeath();
		}
	} __except (EXCEPTION_EXECUTE_HANDLER) { stopped = true; }
	return result;
}

bool Hook(const Signature& s, void* replacement, void** original) noexcept {
	const auto* address = Find(context->exeBase, s.bytes);
	if (!address) { plog::Warn("%s off: signature missing, ambiguous or owned", s.name); return false; }
	D2RL::Mutations::TransactionHandle tx = 0; D2RL::Mutations::OperationHandle op = 0;
	if (mutations->beginTransaction(context, &tx) != D2RL::Mutations::Result::Success) return false;
	D2RL::Mutations::InlineHookRequest request {};
	request.structSize = sizeof(request); request.rva = reinterpret_cast<uintptr_t>(address) - context->exeBase;
	request.expected = address; request.expectedSize = static_cast<uint32_t>(s.entrySize); request.target = replacement;
	D2RL::Mutations::CommitResult result {}; result.structSize = sizeof(result);
	const bool staged = mutations->stageInlineHook(context, tx, &request, &op) == D2RL::Mutations::Result::Success;
	bool ok = false;
	if (staged) {
		ok = mutations->commit(context, tx, &result) == D2RL::Mutations::Result::Success && result.result == D2RL::Mutations::Result::Success;
		if (!ok && !D2RL::Mutations::HasFlag(result.flags, D2RL::Mutations::CommitFlags::OriginalStateRestored)) unsafeInstall = true;
	}
	if (ok) {
		ok = mutations->getInlineHookOriginal(context, tx, op, original) == D2RL::Mutations::Result::Success && *original;
		if (!ok) unsafeInstall = true;  // reject plugin load so the SDK restores all committed mutations
	}
	else (void)mutations->cancelTransaction(context, tx);
	plog::Info("%s %s", s.name, ok ? "on" : "off: mutation refused"); return ok;
}
uint8_t* relays = nullptr;
size_t relayCount = 0;
bool Call(const Signature& s, void* replacement, WriteFn& original, const uint8_t* expectedTarget) noexcept {
	const auto* start = Find(context->exeBase, s.bytes);
	const auto* call = start ? start + s.callOffset : nullptr;
	const auto* target = Target(call);
	if (!target || !Executable(context->exeBase, target) || target != expectedTarget || target[0] != 0xff || target[1] != 0x25 || !relays) {
		plog::Warn("%s off: signature/target guard failed", s.name); return false;
	}
	auto* relay = relays + relayCount++ * 16;
	relay[0] = 0xff; relay[1] = 0x25; std::memset(relay + 2, 0, 4); std::memcpy(relay + 6, &replacement, 8);
	FlushInstructionCache(GetCurrentProcess(), relay, 16);
	const int64_t distance = reinterpret_cast<intptr_t>(relay) - reinterpret_cast<intptr_t>(call + 5);
	if (distance < INT32_MIN || distance > INT32_MAX) return false;
	uint8_t bytes[5] {0xe8}; const int32_t rel = static_cast<int32_t>(distance); std::memcpy(bytes + 1, &rel, 4);
	D2RL::Mutations::TransactionHandle tx = 0; D2RL::Mutations::OperationHandle op = 0;
	if (mutations->beginTransaction(context, &tx) != D2RL::Mutations::Result::Success) return false;
	D2RL::Mutations::BytePatchRequest request {};
	request.structSize = sizeof(request); request.rva = reinterpret_cast<uintptr_t>(call) - context->exeBase;
	request.expected = call; request.expectedSize = 5; request.bytes = bytes; request.size = 5;
	D2RL::Mutations::CommitResult result {}; result.structSize = sizeof(result);
	original = reinterpret_cast<WriteFn>(target);
	const bool staged = mutations->stageBytePatch(context, tx, &request, &op) == D2RL::Mutations::Result::Success;
	bool ok = false;
	if (staged) {
		ok = mutations->commit(context, tx, &result) == D2RL::Mutations::Result::Success && result.result == D2RL::Mutations::Result::Success;
		if (!ok && !D2RL::Mutations::HasFlag(result.flags, D2RL::Mutations::CommitFlags::OriginalStateRestored)) unsafeInstall = true;
	}
	if (!ok) (void)mutations->cancelTransaction(context, tx);
	plog::Info("%s %s", s.name, ok ? "on" : "off: ownership/expected bytes refused"); return ok;
}
}

bool Install(const D2RL::PluginContext* ctx) noexcept {
	if (const auto core = GetModuleHandleW(L"D2RCore.dll"))
		levelFromUnit = reinterpret_cast<D2RL::Reimplementations::GetLevelIdFromUnitFn>(
			GetProcAddress(core, MAKEINTRESOURCEA(D2RL::Reimplementations::GetLevelIdFromUnitInfo.ordinal)));
	context = ctx;
	// Reuse proven client getters by signature; no fixed image-version gate.
	if (const auto* p = Find(ctx->exeBase, ClientSig)) {
		slot = reinterpret_cast<SlotFn>(Target(p + 0x1b)); player = reinterpret_cast<PlayerFn>(Target(p + 0x22));
		if (!Executable(ctx->exeBase, reinterpret_cast<const uint8_t*>(slot)) || !Executable(ctx->exeBase, reinterpret_cast<const uint8_t*>(player))) slot = nullptr;
	}
	stat = reinterpret_cast<StatFn>(Find(ctx->exeBase, GetterSig));
	hasState = reinterpret_cast<HasStateFn>(Find(ctx->exeBase, HasStateSig));
	if (!stat || !Executable(ctx->exeBase, reinterpret_cast<const uint8_t*>(stat))) { plog::Warn("capture off: stat getter unavailable"); return true; }
	if (ctx->QueryService(&mutations) != D2RL::ServiceQueryResult::Success ||
		!D2RL::HasMutationServiceField(mutations, D2RL::MutationServiceRequiredSize)) { plog::Warn("capture off: mutation service unavailable"); return true; }
	// One page of call relays lives until process exit: SDK restores mutations
	// after Unload returns, so freeing executable relays inside Unload is unsafe.
	for (uintptr_t delta = 0x10000; delta < 0x70000000 && !relays; delta += 0x10000)
		relays = static_cast<uint8_t*>(VirtualAlloc(reinterpret_cast<void*>((ctx->exeBase + delta) & ~uintptr_t(0xffff)), 4096, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE));
	const auto* setTarget = Find(ctx->exeBase, SetterSig);
	const auto* addTarget = Find(ctx->exeBase, AdderSig);
	if (Call(LifeDebit, reinterpret_cast<void*>(Write<&lifeWrite, true, 6>), lifeWrite, setTarget)) parts |= 1;
	if (Call(ManaDebit, reinterpret_cast<void*>(Write<&manaWrite, true, 8>), manaWrite, setTarget)) parts |= 2;
	if (Call(LifeRegen, reinterpret_cast<void*>(Write<&regenLifeWrite, false, 6>), regenLifeWrite, setTarget)) parts |= 4;
	if (Call(ManaRegen, reinterpret_cast<void*>(Write<&regenManaWrite, false, 8>), regenManaWrite, addTarget)) parts |= 8;
	if (Hook(Mode, reinterpret_cast<void*>(ModeHook), reinterpret_cast<void**>(&modeOriginal))) parts |= 16;
	if (Hook(Hit, reinterpret_cast<void*>(HitHook), reinterpret_cast<void**>(&hitOriginal))) parts |= 32;
	if (Hook(Effects, reinterpret_cast<void*>(EffectsHook), reinterpret_cast<void**>(&effectsOriginal))) parts |= 64;
	if (Hook(Heal, reinterpret_cast<void*>(HealHook), reinterpret_cast<void**>(&healOriginal))) parts |= 128;
	return !unsafeInstall;
}
void Identity(uint64_t value) noexcept { identity.store(value, std::memory_order_release); }
void GameTick(const D2RL::PluginContext*, void*) noexcept {
	const uint64_t currentIdentity = identity.load(std::memory_order_acquire);
	if (currentIdentity != boundIdentity.load(std::memory_order_relaxed)) {
		producer.store(GetCurrentThreadId(), std::memory_order_relaxed);
		boundIdentity.store(currentIdentity, std::memory_order_release);
	}
	DWORD expected = 0; const DWORD current = GetCurrentThreadId();
	if (!producer.compare_exchange_strong(expected, current) && expected != current) { stopped = true; return; }
	PublishDeath();
}
bool Pop(Event& e) noexcept { return feed.Pop(e); }
bool ClientSample(Endpoints& p, uint32_t& mode) noexcept {
	__try {
		if (!slot || !player || !stat) return false;
		void* u = player(slot());
		if (!u || Read<uint32_t>(u, 0) != 0 || Read<uint32_t>(u, 8) != static_cast<uint32_t>(identity.load())) return false;
		p = Get(u); mode = Read<uint32_t>(u, 0xc); return true;
	} __except (EXCEPTION_EXECUTE_HANDLER) { return false; }
}
uint64_t Dropped() noexcept { return dropped.load(); }
uint32_t Parts() noexcept { return parts; }
void Stop() noexcept { stopped = true; }
bool Faulted() noexcept { return stopped.load(); }
void StateCount(uint32_t bank, uint32_t count) noexcept { if (bank < stateCounts.size()) stateCounts[bank] = std::min(count, 1024U); }
}
