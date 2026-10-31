// Unit tests for the portable core (spec_model, swap, spec_panel_layout). No game needed:
//   tests/run_tests.sh            (g++ or clang++ on any system)
//   cmake -DMULTISPEC_TESTS=ON    (builds multispec_tests next to the plugin)
#include "../src/spec_model.h"
#include "../src/spec_panel_layout.h"
#include "../src/swap.h"

#include <cstdio>
#include <cstring>
#include <initializer_list>

namespace {

int g_failed = 0;
int g_checks = 0;

#define CHECK(cond)                                                              \
	do {                                                                         \
		++g_checks;                                                              \
		if (!(cond)) {                                                           \
			++g_failed;                                                          \
			std::printf("  FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);        \
		}                                                                        \
	} while (0)

constexpr int N = 6;  // a small class: skills 0..5; 3 needs 0, 4 needs 3, 5 needs 1 and 2

// Fake D2R: hard levels, unspent points, prerequisites, max level 20.
class FakeEngine : public ms::Engine {
public:
	uint8_t level[N] = {};
	int32_t unspent = 0;
	int32_t refuseSpendIndex = -1;  // Spend() fails for this skill
	bool breakReset = false;        // Reset() loses a point
	int resets = 0;

	auto Read(ms::LiveChar* out) noexcept -> bool override {
		*out = {};
		std::snprintf(out->name, sizeof(out->name), "Tester");
		out->charClass = 1;
		out->skillCount = N;
		for (int i = 0; i < N; ++i) {
			out->skillIds[i] = static_cast<int16_t>(36 + i);
			out->levels[i] = level[i];
		}
		out->unspent = unspent;
		return true;
	}
	void Reset() noexcept override {
		++resets;
		for (int i = 0; i < N; ++i) {
			unspent += level[i];
			level[i] = 0;
		}
		if (breakReset && unspent > 0) {
			--unspent;
		}
	}
	auto CanSpend(int32_t i) noexcept -> bool override {
		if (unspent < 1 || level[i] >= 20) {
			return false;
		}
		switch (i) {
		case 3: return level[0] > 0;
		case 4: return level[3] > 0;
		case 5: return level[1] > 0 && level[2] > 0;
		default: return true;
		}
	}
	auto Spend(int32_t i) noexcept -> bool override {
		if (i == refuseSpendIndex) {
			return false;
		}
		--unspent;
		++level[i];
		return true;
	}
};

class FakeStore : public ms::Store {
public:
	int writes = 0;
	bool fail = false;
	ms::CharSpecs last {};
	auto Write(const ms::CharSpecs& s) noexcept -> bool override {
		if (fail) {
			return false;
		}
		++writes;
		last = s;
		return true;
	}
};

auto Live(FakeEngine& e) -> ms::LiveChar {
	ms::LiveChar l {};
	e.Read(&l);
	return l;
}

void SetLevels(uint8_t* dst, std::initializer_list<int> v) {
	int i = 0;
	for (int x : v) {
		dst[i++] = static_cast<uint8_t>(x);
	}
}

void TestCrcAndRoundTrip() {
	std::printf("file round trip\n");
	CHECK(ms::Crc32("123456789", 9) == 0xcbf43926U);  // the standard CRC-32 check value

	FakeEngine e;
	SetLevels(e.level, {1, 2, 0, 1, 1, 0});
	e.unspent = 4;
	ms::CharSpecs s {};
	auto rep = ms::Reconcile(&s, false, Live(e), 2);
	CHECK(rep.result == ms::Reconciled::Created);
	CHECK(s.specCount == 2 && s.active == 1);
	CHECK(ms::SumLevels(s.specs[1].levels, N) == 0);  // new spec starts empty

	char text[ms::MaxFileBytes];
	const size_t len = ms::Serialize(s, text, sizeof(text));
	CHECK(len > 0);
	ms::CharSpecs back {};
	char err[64] = "";
	CHECK(ms::Parse(text, len, &back, err, sizeof(err)));
	CHECK(std::strcmp(back.name, "Tester") == 0 && back.charClass == 1 && back.skillCount == N);
	CHECK(ms::SameLevels(back.specs[0].levels, s.specs[0].levels, N));
	CHECK(back.active == 1 && back.pendingFrom == 0);

	// The largest file: every spec, every skill, three-digit levels.
	{
		static ms::CharSpecs big {};
		std::snprintf(big.name, sizeof(big.name), "%s", "Largest");
		big.charClass = 1;
		big.skillCount = ms::MaxSkills;
		big.specCount = ms::MaxSpecs;
		big.active = ms::MaxSpecs;
		for (int i = 0; i < ms::MaxSkills; ++i) {
			big.skillIds[i] = static_cast<int16_t>(1000 + i);
		}
		for (int k = 0; k < ms::MaxSpecs; ++k) {
			std::memset(big.specs[k].levels, 255, sizeof(big.specs[k].levels));
		}
		static char bigText[ms::MaxFileBytes];
		const size_t bigLen = ms::Serialize(big, bigText, sizeof(bigText));
		CHECK(bigLen > 0);
		static ms::CharSpecs bigBack {};
		CHECK(ms::Parse(bigText, bigLen, &bigBack, err, sizeof(err)));
		CHECK(bigBack.specCount == ms::MaxSpecs && bigBack.active == ms::MaxSpecs);
		CHECK(bigBack.specs[ms::MaxSpecs - 1].levels[ms::MaxSkills - 1] == 255);
	}

	// one flipped byte: refused
	text[20] ^= 1;
	CHECK(!ms::Parse(text, len, &back, err, sizeof(err)));
	CHECK(std::strcmp(err, "crc mismatch") == 0 || std::strncmp(err, "bad", 3) == 0);
	text[20] ^= 1;
	// truncated: refused
	CHECK(!ms::Parse(text, len - 5, &back, err, sizeof(err)));
	// malformed crc lines: refused
	{
		char bad[ms::MaxFileBytes];
		const char* crc = std::strstr(text, "crc ");
		const size_t body = static_cast<size_t>(crc - text);
		std::memcpy(bad, text, body);
		const int n1 = std::snprintf(bad + body, sizeof(bad) - body, "crc 1234567\n");
		CHECK(!ms::Parse(bad, body + static_cast<size_t>(n1), &back, err, sizeof(err)));
		const int n2 = std::snprintf(bad + body, sizeof(bad) - body, "crc 12345678x\n");
		CHECK(!ms::Parse(bad, body + static_cast<size_t>(n2), &back, err, sizeof(err)));
	}
	// unknown version: refused even with a good CRC
	s.pendingFrom = 1;
	s.pendingTo = 2;
	const size_t len2 = ms::Serialize(s, text, sizeof(text));
	CHECK(ms::Parse(text, len2, &back, err, sizeof(err)) && back.pendingFrom == 1 && back.pendingTo == 2);
}

void TestConfigAndModinfo() {
	std::printf("config and modinfo\n");
	CHECK(ms::ConfigSpecCount("[multispec]\nspec_count = 3\n", 2) == 3);
	CHECK(ms::ConfigSpecCount("[multispec]\nspec_count = 3 # three\n", 2) == 3);
	CHECK(ms::ConfigSpecCount("[other]\nspec_count = 5\n[multispec]\n", 2) == 2);
	CHECK(ms::ConfigSpecCount("[multispec]\nspec_count = 0\n", 2) == 2);
	CHECK(ms::ConfigSpecCount("[multispec]\nspec_count = 9\n", 2) == 9);
	CHECK(ms::ConfigSpecCount("[multispec]\nspec_count = 99\n", 2) == 99);
	CHECK(ms::ConfigSpecCount("[multispec]\nspec_count = 100\n", 2) == 2);
	CHECK(ms::ConfigSpecCount(nullptr, 2) == 2);
	char sp[64];
	CHECK(ms::ModinfoSavePath("{\"name\": \"MyMod\", \"savepath\": \"MyMod-s2/\"}", sp, sizeof(sp)) &&
	      std::strcmp(sp, "MyMod-s2/") == 0);
	CHECK(ms::ModinfoSavePath("{\"name\": \"MyMod\"}", sp, sizeof(sp)) && sp[0] == '\0');
	CHECK(ms::ValidName("Lyra") && ms::ValidName("A_b-c") && !ms::ValidName("a") && !ms::ValidName("..\\x") &&
	      !ms::ValidName("ThisNameIsTooLong"));
}

void TestReconcile() {
	std::printf("reconcile\n");
	FakeEngine e;
	SetLevels(e.level, {1, 2, 0, 1, 1, 0});
	e.unspent = 4;
	ms::CharSpecs s {};
	ms::Reconcile(&s, false, Live(e), 2);
	SetLevels(s.specs[1].levels, {0, 1, 1, 0, 0, 1});

	// unchanged
	auto rep = ms::Reconcile(&s, true, Live(e), 2);
	CHECK(rep.result == ms::Reconciled::Unchanged && !rep.changed);

	// the player spent a point in the active spec
	e.level[2] = 1;
	e.unspent = 3;
	rep = ms::Reconcile(&s, true, Live(e), 2);
	CHECK(rep.result == ms::Reconciled::AdoptedLive && s.specs[0].levels[2] == 1 && s.active == 1);

	// crash after a swap 1 -> 2 that was never saved: file says 2 / pending, live is still spec 1
	s.active = 2;
	s.pendingFrom = 1;
	s.pendingTo = 2;
	rep = ms::Reconcile(&s, true, Live(e), 2);
	CHECK(rep.result == ms::Reconciled::ActiveFromMatch && s.active == 1 && s.pendingFrom == 0);

	// more specs configured
	rep = ms::Reconcile(&s, true, Live(e), 4);
	CHECK(s.specCount == 4 && ms::SumLevels(s.specs[3].levels, N) == 0 && rep.changed);
	// fewer configured: kept in the file
	rep = ms::Reconcile(&s, true, Live(e), 2);
	CHECK(s.specCount == 4);

	// another character with the same name and another class
	ms::LiveChar other = Live(e);
	other.charClass = 4;
	rep = ms::Reconcile(&s, true, other, 2);
	CHECK(rep.result == ms::Reconciled::Recreated && s.charClass == 4 && s.specCount == 2);

	// mod data changed the skill list: remap by id, lost points freed
	ms::Reconcile(&s, false, Live(e), 2);
	SetLevels(s.specs[1].levels, {0, 0, 0, 0, 0, 3});
	ms::LiveChar changed = Live(e);
	changed.skillIds[5] = 99;  // skill 41 is gone
	changed.levels[5] = 0;
	ms::CharSpecs copy = s;
	rep = ms::Reconcile(&copy, true, changed, 2);
	CHECK(rep.remapped == 1 && copy.specs[1].state == ms::SpecState::Adjusted && copy.specs[1].levels[5] == 0);

	// a spec needing more points than the character has is invalid
	ms::Reconcile(&s, false, Live(e), 2);
	SetLevels(s.specs[1].levels, {20, 20, 0, 0, 0, 0});
	rep = ms::Reconcile(&s, true, Live(e), 2);
	CHECK(rep.newlyInvalid == 1 && s.specs[1].state == ms::SpecState::Invalid);

	// a respec token in spec 1 stays in spec 1: an empty spec in or above range does not claim the reset character
	SetLevels(e.level, {1, 2, 0, 1, 1, 0});
	e.unspent = 4;
	ms::Reconcile(&s, false, Live(e), 2);
	(void)ms::Reconcile(&s, true, Live(e), 3);
	SetLevels(e.level, {0, 0, 0, 0, 0, 0});
	e.unspent = 9;
	rep = ms::Reconcile(&s, true, Live(e), 2);
	CHECK(rep.result == ms::Reconciled::AdoptedLive && s.active == 1 && ms::SumLevels(s.specs[0].levels, N) == 0);

	// a reset character after an unfinished swap is still recovery, even with an empty spec stored
	ms::Reconcile(&s, false, Live(e), 3);
	SetLevels(s.specs[0].levels, {1, 2, 0, 1, 1, 0});
	SetLevels(s.specs[1].levels, {0, 1, 1, 0, 0, 1});
	s.pendingFrom = 1;
	s.pendingTo = 2;
	rep = ms::Reconcile(&s, true, Live(e), 3);
	CHECK(rep.result == ms::Reconciled::Recovery && s.active == 1 && ms::SumLevels(s.specs[0].levels, N) == 5);

	// a match above spec_count is not taken; the active spec adopts the live skills
	ms::Reconcile(&s, false, Live(e), 3);
	SetLevels(s.specs[2].levels, {0, 1, 1, 0, 0, 1});
	SetLevels(e.level, {0, 1, 1, 0, 0, 1});
	e.unspent = 6;
	rep = ms::Reconcile(&s, true, Live(e), 2);
	CHECK(rep.result == ms::Reconciled::AdoptedLive && s.active == 1 && s.specCount == 3);
}

void TestSwap() {
	std::printf("swap\n");
	FakeEngine e;
	SetLevels(e.level, {1, 2, 0, 1, 1, 0});  // spec 1: 5 points
	e.unspent = 4;                          // T = 9
	FakeStore st;
	ms::CharSpecs s {};
	ms::Reconcile(&s, false, Live(e), 2);
	SetLevels(s.specs[1].levels, {0, 1, 1, 0, 0, 3});  // spec 2: 5 needs 1 and 2 (listed before them? no, after)

	ms::SwapReport r {};
	ms::ReconcileReport rep {};
	CHECK(ms::Swap(e, st, &s, Live(e), 2, 2, &r) == ms::SwapResult::Done);
	CHECK(s.active == 2 && s.pendingFrom == 0);
	uint8_t want2[N];
	SetLevels(want2, {0, 1, 1, 0, 0, 3});
	CHECK(ms::SameLevels(e.level, want2, N) && e.unspent == 4);
	CHECK(r.pointsUsed == 5 && r.unspent == 4);
	CHECK(st.writes == 2);  // pending, then done

	// swap back restores spec 1 exactly, unspent preserved
	CHECK(ms::Swap(e, st, &s, Live(e), 1, 2, &r) == ms::SwapResult::Done);
	uint8_t want1[N];
	SetLevels(want1, {1, 2, 0, 1, 1, 0});
	CHECK(ms::SameLevels(e.level, want1, N) && e.unspent == 4);

	// a point earned in spec 1 reaches spec 2 as unspent
	e.unspent += 1;  // level-up
	ms::Reconcile(&s, true, Live(e), 2);
	CHECK(ms::Swap(e, st, &s, Live(e), 2, 2, &r) == ms::SwapResult::Done && e.unspent == 5);
	ms::Reconcile(&s, true, Live(e), 2);
	CHECK(ms::Swap(e, st, &s, Live(e), 1, 2, &r) == ms::SwapResult::Done && e.unspent == 5);

	// prerequisites listed after the skill that needs them: replay takes extra passes
	ms::Reconcile(&s, true, Live(e), 2);
	SetLevels(s.specs[1].levels, {1, 0, 0, 1, 2, 0});  // 4 needs 3 needs 0
	CHECK(ms::Swap(e, st, &s, Live(e), 2, 2, &r) == ms::SwapResult::Done && e.level[4] == 2);
	ms::Reconcile(&s, true, Live(e), 2);
	CHECK(ms::Swap(e, st, &s, Live(e), 1, 2, &r) == ms::SwapResult::Done);

	// refusals change nothing
	ms::Reconcile(&s, true, Live(e), 2);
	const int resets = e.resets;
	CHECK(ms::Swap(e, st, &s, Live(e), 1, 2, &r) == ms::SwapResult::Refused);  // already active
	CHECK(ms::Swap(e, st, &s, Live(e), 3, 2, &r) == ms::SwapResult::Refused);  // no spec 3
	CHECK(ms::Swap(e, st, &s, Live(e), 0, 2, &r) == ms::SwapResult::Refused);
	st.fail = true;
	CHECK(ms::Swap(e, st, &s, Live(e), 2, 2, &r) == ms::SwapResult::Refused);  // file not writable
	st.fail = false;
	CHECK(e.resets == resets && ms::SameLevels(e.level, want1, N));

	// unreachable spec (prerequisite never met): rolled back to spec 1
	SetLevels(s.specs[1].levels, {0, 0, 0, 0, 1, 0});  // 4 without 3
	CHECK(ms::Swap(e, st, &s, Live(e), 2, 2, &r) == ms::SwapResult::RolledBack);
	CHECK(s.active == 1 && s.pendingFrom == 0 && ms::SameLevels(e.level, want1, N) && e.unspent == 5);
	std::printf("  rolled back: %s\n", r.detail);

	// D2R refuses a point: rolled back
	SetLevels(s.specs[1].levels, {0, 1, 1, 0, 0, 3});
	e.refuseSpendIndex = 5;
	CHECK(ms::Swap(e, st, &s, Live(e), 2, 2, &r) == ms::SwapResult::RolledBack);
	CHECK(ms::SameLevels(e.level, want1, N));
	e.refuseSpendIndex = -1;

	// the respec loses a point: neither spec can be checked, character left reset, no spec lost
	e.breakReset = true;
	CHECK(ms::Swap(e, st, &s, Live(e), 2, 2, &r) == ms::SwapResult::LeftReset);
	CHECK(ms::SumLevels(e.level, N) == 0);
	CHECK(ms::SameLevels(s.specs[0].levels, want1, N) && ms::SameLevels(s.specs[1].levels, want2, N));
	CHECK(s.pendingFrom == 1 && s.pendingTo == 2);
	std::printf("  left reset: %s\n", r.detail);

	// recovery: the next reconciliation overwrites no spec, even though the character is all zeros and no spec is
	SetLevels(s.specs[1].levels, {0, 1, 1, 0, 0, 3});
	rep = ms::Reconcile(&s, true, Live(e), 2);
	CHECK(rep.result == ms::Reconciled::Recovery && s.pendingFrom == 1);
	CHECK(ms::SameLevels(s.specs[0].levels, want1, N) && ms::SameLevels(s.specs[1].levels, want2, N));
	// ...and spec N loads any spec from there, the active one too
	e.breakReset = false;
	e.unspent = 10;  // the fake's broken respecs lost points; restore T = 9 + 1 earned
	CHECK(ms::Swap(e, st, &s, Live(e), 1, 2, &r) == ms::SwapResult::Done);
	CHECK(ms::SameLevels(e.level, want1, N) && s.pendingFrom == 0 && s.active == 1);

	// spec_count lowered below the active spec: the swap back into range works and keeps every spec.
	(void)ms::Reconcile(&s, true, Live(e), 3);
	CHECK(s.specCount == 3);
	CHECK(ms::Swap(e, st, &s, Live(e), 3, 3, &r) == ms::SwapResult::Done && s.active == 3);
	const ms::ReconcileReport low = ms::Reconcile(&s, true, Live(e), 2);
	CHECK(low.result != ms::Reconciled::Recovery && s.active == 3 && s.specCount == 3);
	CHECK(ms::Swap(e, st, &s, Live(e), 1, 2, &r) == ms::SwapResult::Done);
	CHECK(ms::SameLevels(e.level, want1, N) && s.active == 1 && s.specCount == 3);
}

void TestSpecPanelLayout() {
	namespace panel = ms::panel;
	// Geometry: the column fits the wing below the heading, one belt slot ends where the tree starts.
	CHECK(panel::UpY >= panel::HeadingTop + panel::HeadingHeight);
	CHECK(panel::SlotY == panel::UpY + panel::ArrowSize && panel::DownY == panel::SlotY + panel::SlotHeight);  // touching
	CHECK(panel::DownY + panel::ArrowSize <= panel::WingHeight - panel::WingBorder);
	CHECK(panel::SlotX >= panel::WingBorder && panel::SlotX + panel::SlotWidth == panel::VisibleWidth);
	CHECK(panel::ArrowX >= panel::SlotX && panel::ArrowX + panel::ArrowSize <= panel::VisibleWidth);
	static char json[65536];
	for (int32_t specs = 1; specs <= ms::MaxSpecs; ++specs) {
		const size_t n = panel::BuildLayout(specs, json, sizeof(json));
		CHECK(n > 0 && n < sizeof(json) && json[n] == '\0');
		char want[40];
		std::snprintf(want, sizeof(want), "\"Number%d\"", specs);
		CHECK(std::strstr(json, want) != nullptr);
		std::snprintf(want, sizeof(want), "\"Number%d\"", specs + 1);
		CHECK(std::strstr(json, want) == nullptr);
		// Balanced braces and brackets: the game refuses a broken layout.
		int braces = 0;
		int brackets = 0;
		for (size_t i = 0; i < n; ++i) {
			braces += json[i] == '{' ? 1 : (json[i] == '}' ? -1 : 0);
			brackets += json[i] == '[' ? 1 : (json[i] == ']' ? -1 : 0);
		}
		CHECK(braces == 0 && brackets == 0);
	}
	CHECK(std::strstr(json, "multispec/SpecUp") != nullptr && std::strstr(json, "multispec/SpecDown") != nullptr);
	// Only the game's own sprites: nothing of ours to ship.
	CHECK(std::strstr(json, "d2rloader\\\\multispec") == nullptr);
	CHECK(panel::BuildLayout(0, json, sizeof(json)) == 0);
	CHECK(panel::BuildLayout(2, json, 64) == 0);  // refuses a short buffer
	CHECK(panel::ParseStep("multispec/SpecUp") == 1);
	CHECK(panel::ParseStep("multispec/SpecDown") == -1);
	CHECK(panel::ParseStep("multispec/SpecUp2") == 0);
	CHECK(panel::ParseStep("SkillsTreePanel") == 0);
	CHECK(panel::ParseStep(nullptr) == 0);
	CHECK(panel::NextSpec(1, 3, 1) == 2);
	CHECK(panel::NextSpec(2, 3, -1) == 1);
	CHECK(panel::NextSpec(3, 3, 1) == 1);   // wraps at the top
	CHECK(panel::NextSpec(1, 3, -1) == 3);  // and at the bottom
	CHECK(panel::NextSpec(1, 2, 1) == 2 && panel::NextSpec(1, 2, -1) == 2);
	CHECK(panel::NextSpec(1, 1, 1) == 0);   // one spec: nowhere to go
	CHECK(panel::NextSpec(0, 3, 1) == 0);   // active spec not known yet
	CHECK(panel::NextSpec(80, 3, 1) == 1);  // spec_count lowered below the active spec: back into range
	CHECK(panel::NextSpec(80, 3, -1) == 3);
	CHECK(panel::NextSpec(80, 1, 1) == 1 && panel::NextSpec(80, 1, -1) == 1);
	CHECK(panel::NextSpec(1, 3, 0) == 0);
}

}  // namespace

int main() {
	TestCrcAndRoundTrip();
	TestConfigAndModinfo();
	TestReconcile();
	TestSwap();
	TestSpecPanelLayout();
	std::printf("%d checks, %d failed\n", g_checks, g_failed);
	return g_failed == 0 ? 0 : 1;
}
