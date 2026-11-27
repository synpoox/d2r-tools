// Execute the production adapters against fake units and original functions.
// No engine image, live process, game install, or code mutation is involved.
#include "../src/native.cpp"
#include <iostream>
#include <cstdlib>
#include <thread>
using namespace recap;
using namespace recap::native;
#define CHECK(x) do { if (!(x)) { std::cerr << "line " << __LINE__ << ": " << #x << '\n'; std::exit(1); } } while (false)
struct FakeUnit {
	uint32_t type = 0, classId = 2, id = 5, mode = 1;
	std::array<int32_t, 200> stats{};
};
FakeUnit victim, attacker;
std::array<uint8_t, 0x180> block {};
unsigned writes = 0, heals = 0, modes = 0, hits = 0, effects = 0;
bool nestedHeal = false;
int32_t FakeStat(void* u, int32_t id, int32_t) noexcept { return static_cast<FakeUnit*>(u)->stats[static_cast<size_t>(id)]; }
int32_t FakeHeal(void* u, int32_t value) noexcept {
	++heals; auto& v = static_cast<FakeUnit*>(u)->stats[6]; const int32_t added = std::min(value, 1000 - v); v += added; return added;
}
uint64_t FakeWrite(void* u, int32_t id, int32_t value, int32_t) noexcept {
	++writes;
	if (nestedHeal) { nestedHeal = false; (void)HealHook(u, 10); }
	static_cast<FakeUnit*>(u)->stats[static_cast<size_t>(id)] = value;
	return 0x123456789abcdeffULL;
}
uint64_t FakeMode(void*, void* u, void*, uint32_t mode, uint32_t type, uint32_t id, uint32_t a, uint32_t b) noexcept {
	++modes; CHECK(type == 1 && id == 9 && a == 123 && b == 456);
	static_cast<FakeUnit*>(u)->mode = mode; static_cast<FakeUnit*>(u)->stats[6] = 0; return 73;
}
uint64_t FakeEffects(void*, void*, void*, void*) noexcept { ++effects; return 91; }
uint64_t FakeHit(void* game, void* source, void* u, int32_t area, void* damage) noexcept {
	++hits; CHECK(area == 23); CHECK(EffectsHook(game, source, u, damage) == 91);
	CHECK((Write<&lifeWrite, true, 6>(u, 6, 0, 0) == 0x123456789abcdeffULL));
	CHECK(ModeHook(game, u, nullptr, 0, 1, 9, 123, 456) == 73);
	// A mutation after a nested death dispatch must precede the one death marker.
	CHECK((Write<&manaWrite, true, 8>(u, 8, 100, 0) == 0x123456789abcdeffULL));
	return 79;
}
std::vector<Event> Drain() { std::vector<Event> events; Event e; while (Pop(e)) events.push_back(e); return events; }
void Reset() {
	Identity((uint64_t(1) << 32) | victim.id); GameTick(nullptr, nullptr); token = 0; stopped = false;
	victim.mode = 1; victim.stats[6] = 300; victim.stats[8] = 200; victim.stats[7] = 1000; victim.stats[9] = 500;
	writes = heals = modes = hits = effects = 0; (void)Begin(&victim); Drain();
}
using MutationResult = D2RL::Mutations::Result;
int failure = 0, commits = 0, cancels = 0;
MutationResult BeginMutation(const D2RL::PluginContext*, D2RL::Mutations::TransactionHandle* tx) noexcept { *tx = 1; return MutationResult::Success; }
MutationResult StageHook(const D2RL::PluginContext*, D2RL::Mutations::TransactionHandle, const D2RL::Mutations::InlineHookRequest* request, D2RL::Mutations::OperationHandle* op) noexcept {
	CHECK(request->expectedSize == 17 && request->rva == 1024); *op = 1;
	return failure == 1 ? MutationResult::Conflict : MutationResult::Success;
}
MutationResult CommitMutation(const D2RL::PluginContext*, D2RL::Mutations::TransactionHandle, D2RL::Mutations::CommitResult* result) noexcept {
	++commits;
	result->result = failure == 2 || failure == 4 ? MutationResult::CommitFailed : MutationResult::Success;
	result->flags = failure == 2 ? D2RL::Mutations::CommitFlags::OriginalStateRestored : D2RL::Mutations::CommitFlags::None;
	return result->result;
}
MutationResult GetOriginal(const D2RL::PluginContext*, D2RL::Mutations::TransactionHandle, D2RL::Mutations::OperationHandle, void** original) noexcept {
	*original = failure == 3 ? nullptr : reinterpret_cast<void*>(FakeMode); return failure == 3 ? MutationResult::Unavailable : MutationResult::Success;
}
MutationResult CancelMutation(const D2RL::PluginContext*, D2RL::Mutations::TransactionHandle) noexcept { ++cancels; return MutationResult::Success; }
void MutationTests() {
	std::array<uint8_t, 4096> image {};
	auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(image.data()); dos->e_lfanew = 128;
	auto* nt = reinterpret_cast<IMAGE_NT_HEADERS64*>(image.data() + 128);
	nt->FileHeader.NumberOfSections = 1; nt->FileHeader.SizeOfOptionalHeader = sizeof(nt->OptionalHeader);
	auto* section = IMAGE_FIRST_SECTION(nt); section->VirtualAddress = 1024; section->Misc.VirtualSize = 2048; section->Characteristics = IMAGE_SCN_MEM_EXECUTE;
	const Pattern pattern(Mode.bytes);
	for (size_t i = 0; i < pattern.size; ++i) image[1024 + i] = static_cast<uint8_t>(std::max(0, pattern.bytes[i]));
	D2RL::PluginContext ctx {}; ctx.exeBase = reinterpret_cast<uintptr_t>(image.data()); context = &ctx;
	D2RL::MutationService service {}; service.beginTransaction = BeginMutation; service.stageInlineHook = StageHook;
	service.commit = CommitMutation; service.getInlineHookOriginal = GetOriginal; service.cancelTransaction = CancelMutation; mutations = &service;
	void* original = nullptr;
	CHECK(Hook(Mode, reinterpret_cast<void*>(ModeHook), &original)); CHECK(original == reinterpret_cast<void*>(FakeMode));
	failure = 1; CHECK(!Hook(Mode, reinterpret_cast<void*>(ModeHook), &original)); CHECK(commits == 1 && cancels == 1 && !unsafeInstall);
	failure = 2; CHECK(!Hook(Mode, reinterpret_cast<void*>(ModeHook), &original)); CHECK(!unsafeInstall);
	failure = 3; CHECK(!Hook(Mode, reinterpret_cast<void*>(ModeHook), &original)); CHECK(unsafeInstall);
	unsafeInstall = false; failure = 4; CHECK(!Hook(Mode, reinterpret_cast<void*>(ModeHook), &original)); CHECK(unsafeInstall);
	const int before = commits; image[1024] ^= 1;
	CHECK(!Hook(Mode, reinterpret_cast<void*>(ModeHook), &original)); CHECK(commits == before);
	context = nullptr; mutations = nullptr;
}
int main() {
	recap::native::stat = FakeStat; lifeWrite = manaWrite = regenLifeWrite = regenManaWrite = FakeWrite;
	healOriginal = FakeHeal; modeOriginal = FakeMode; effectsOriginal = FakeEffects; hitOriginal = FakeHit;
	Reset();
	CHECK((Write<&lifeWrite, true, 6>(&victim, 6, 0, 0) == 0x123456789abcdeffULL));
	auto events = Drain(); CHECK(writes == 1 && events.size() == 2);
	CHECK(events[1].life.lost.Confirmed() && events[1].life.lost.value == 300 && !events[1].attacker.Confirmed());
	Reset(); nestedHeal = true;
	(void)Write<&lifeWrite, true, 6>(&victim, 6, 200, 0);
	events = Drain(); CHECK(writes == 1 && heals == 1 && events.size() == 4);
	CHECK(events[2].kind == Kind::Healing && events[2].life.restored.value == 10 && events[3].kind == Kind::Sample);
	Reset(); (void)Write<&regenLifeWrite, false, 6>(&victim, 6, 350, 0);
	events = Drain(); CHECK(events.size() == 2 && !events[1].life.lost.Confirmed() && !events[1].life.restored.Confirmed());
	Reset(); attacker.type = 1; attacker.id = 9; int32_t attempted = 100; std::memcpy(block.data() + 0x134, &attempted, 4);
	CHECK(HitHook(nullptr, &attacker, &victim, 23, block.data()) == 79);
	events = Drain(); CHECK(writes == 2 && modes == 1 && hits == 1 && effects == 1);
	unsigned deathCount = 0; uint64_t seq = 0;
	for (const auto& e : events) { CHECK(e.sequence > seq); seq = e.sequence; if (e.kind == Kind::Death) ++deathCount; }
	CHECK(deathCount == 1 && events.back().kind == Kind::Death && events.back().mana.after.value == 100);
	CHECK(events[1].life.lost.value == 300 && events[1].damage.post.value == 100 && events[1].damage.overkill.value == 0);
	CHECK(events[1].attacker.value.id == 9 && events[1].damage.postByElement[4].evidence == Evidence::Unavailable);
	CHECK(events[events.size() - 2].damage.post.evidence == Evidence::Unavailable);
	CHECK(ModeHook(nullptr, &victim, nullptr, 17, 1, 9, 123, 456) == 73); CHECK(Drain().empty());
	Reset(); attempted = 500; std::memcpy(block.data() + 0x134, &attempted, 4);
	CHECK(HitHook(nullptr, &attacker, &victim, 23, block.data()) == 79);
	events = Drain(); CHECK(events[1].damage.overkill.value == 200 && events[1].life.lost.value == 300);
	Reset(); std::array<uint8_t, 0x108> game {}; game[0x106] = 3;
	hasState = [](void*, int32_t state) noexcept -> int32_t { return state < 20; }; StateCount(3, 32);
	victim.stats[12] = 99;
	levelFromUnit = [](const void *) noexcept -> int32_t { return 108; };
	CHECK(ModeHook(game.data(), &victim, nullptr, 0, 1, 9, 123, 456) == 73);
	events = Drain();
	CHECK(events.back().characterLevel.Confirmed() && events.back().characterLevel.value == 99 && events.back().areaId.value == 108);
	CHECK(events.back().defense.statCount == 16);
	levelFromUnit = nullptr;
	CHECK(events.back().defense.stateCount == 16 && events.back().defense.statesTruncated && events.back().defense.stateEvidence == Evidence::Confirmed);
	Reset();
	for (unsigned i = 0; i < 2047; ++i) Emit(Sample(Get(&victim)));
	CHECK(ModeHook(nullptr, &victim, nullptr, 0, 1, 9, 123, 456) == 73);
	CHECK(deathPending); CHECK(Drain().size() == 2047);
	GameTick(nullptr, nullptr); events = Drain(); CHECK(events.size() == 1 && events[0].kind == Kind::Death && !deathPending);
	Reset();
	std::thread foreign([] { (void)Write<&lifeWrite, true, 6>(&victim, 6, 200, 0); }); foreign.join();
	CHECK(writes == 1 && Drain().empty());
	Reset(); victim.id = 6;
	(void)Write<&lifeWrite, true, 6>(&victim, 6, 200, 0); CHECK(writes == 1 && Drain().empty());
	victim.id = 5; Stop(); (void)Write<&lifeWrite, true, 6>(&victim, 6, 100, 0); CHECK(writes == 2 && Drain().empty());
	MutationTests();
	std::cout << "native adapter originals, nested mutations, fractional flush, death ordering and identity tests passed\n";
}
