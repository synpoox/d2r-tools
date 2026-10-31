#include "swap.h"

#include <cstdio>
#include <cstring>

namespace ms {

auto SwapResultName(SwapResult r) noexcept -> const char* {
	switch (r) {
	case SwapResult::Done: return "done";
	case SwapResult::Refused: return "refused";
	case SwapResult::RolledBack: return "rolled back";
	case SwapResult::LeftReset: return "FAILED, character left reset";
	}
	return "?";
}

auto ApplyLevels(Engine& engine, int32_t total, const uint8_t* target, int32_t count, char* detail, size_t size) noexcept
	-> bool {
	engine.Reset();
	LiveChar live {};
	if (!engine.Read(&live) || live.skillCount != count) {
		std::snprintf(detail, size, "could not read the character after the respec");
		return false;
	}
	if (SumLevels(live.levels, count) != 0 || live.unspent != total) {
		std::snprintf(detail, size, "respec left %d points in skills and %d unspent (expected 0 and %d)",
		              SumLevels(live.levels, count), live.unspent, total);
		return false;
	}

	// Replay. A skill whose prerequisites are not in yet is skipped and picked up on a later pass.
	uint8_t have[MaxSkills] = {};
	for (;;) {
		bool progress = false;
		bool missing = false;
		for (int32_t i = 0; i < count; ++i) {
			while (have[i] < target[i]) {
				if (!engine.CanSpend(i)) {
					missing = true;
					break;
				}
				if (!engine.Spend(i)) {
					std::snprintf(detail, size, "D2R refused a point in class skill %d at level %d", i, have[i]);
					return false;
				}
				++have[i];
				progress = true;
			}
		}
		if (!missing) {
			break;
		}
		if (!progress) {
			for (int32_t i = 0; i < count; ++i) {
				if (have[i] < target[i]) {
					std::snprintf(detail, size, "class skill %d (id %d) cannot get level %d (requirement not met)", i,
					              live.skillIds[i], have[i] + 1);
					break;
				}
			}
			return false;
		}
	}

	if (!engine.Read(&live) || live.skillCount != count) {
		std::snprintf(detail, size, "could not read the character after the replay");
		return false;
	}
	const int32_t used = SumLevels(target, count);
	if (!SameLevels(live.levels, target, count) || live.unspent != total - used) {
		std::snprintf(detail, size, "after the replay: %d points in skills and %d unspent (expected %d and %d)",
		              SumLevels(live.levels, count), live.unspent, used, total - used);
		return false;
	}
	return true;
}

auto Swap(Engine& engine, Store& store, CharSpecs* file, const LiveChar& live, int32_t to, int32_t configCount,
          SwapReport* report) noexcept -> SwapResult {
	SwapReport rep {};
	rep.from = file->active;
	rep.to = to;
	const int32_t n = live.skillCount;
	const int32_t total = TotalPoints(live);
	rep.pointsUsed = SumLevels(live.levels, n);
	rep.unspent = live.unspent;

	auto finish = [&](SwapResult r) noexcept {
		rep.result = r;
		if (report != nullptr) {
			*report = rep;
		}
		return r;
	};

	const int32_t usable = configCount < file->specCount ? configCount : file->specCount;
	if (to < 1 || to > usable) {
		std::snprintf(rep.detail, sizeof(rep.detail), "there is no spec %d (specs 1-%d)", to, usable);
		return finish(SwapResult::Refused);
	}
	// Recovery: a swap did not finish, so the character matches no spec. Any spec may be loaded, the active one too.
	const bool recovery = file->pendingFrom > 0;
	if (to == file->active && !recovery) {
		std::snprintf(rep.detail, sizeof(rep.detail), "spec %d is already active", to);
		return finish(SwapResult::Refused);
	}
	if (file->skillCount != n || (!recovery && !SameLevels(file->specs[file->active - 1].levels, live.levels, n))) {
		std::snprintf(rep.detail, sizeof(rep.detail), "internal: the spec file is not reconciled with the character");
		return finish(SwapResult::Refused);
	}
	const Spec& target = file->specs[to - 1];
	if (target.state == SpecState::Invalid || SumLevels(target.levels, n) > total) {
		std::snprintf(rep.detail, sizeof(rep.detail), "spec %d needs %d points, the character has %d", to,
		              SumLevels(target.levels, n), total);
		return finish(SwapResult::Refused);
	}

	Spec& from = file->specs[file->active - 1];
	if (!recovery) {  // in recovery the character was reset, so its buttons say nothing about the spec
		from.left = live.left;
		from.right = live.right;
	}
	uint8_t old[MaxSkills];  // the rollback target: the stored active spec (== live outside recovery)
	std::memcpy(old, from.levels, static_cast<size_t>(n));
	file->pendingFrom = file->active;
	file->pendingTo = to;
	if (!store.Write(*file)) {
		file->pendingFrom = file->pendingTo = 0;
		std::snprintf(rep.detail, sizeof(rep.detail), "the spec file could not be written; nothing changed");
		return finish(SwapResult::Refused);
	}

	char why[200] = "";
	if (ApplyLevels(engine, total, target.levels, n, why, sizeof(why))) {
		engine.Select(true, target.left);
		engine.Select(false, target.right);
		file->active = to;
		file->pendingFrom = file->pendingTo = 0;
		(void)store.Write(*file);  // on failure the next reconciliation finds spec `to` by its levels
		rep.pointsUsed = SumLevels(target.levels, n);
		rep.unspent = total - rep.pointsUsed;
		return finish(SwapResult::Done);
	}

	char back[200] = "";
	const bool restored = ApplyLevels(engine, total, old, n, back, sizeof(back));
	if (restored) {
		engine.Select(true, from.left);
		engine.Select(false, from.right);
		file->pendingFrom = file->pendingTo = 0;
		(void)store.Write(*file);
		std::snprintf(rep.detail, sizeof(rep.detail), "%s", why);
		return finish(SwapResult::RolledBack);
	}
	// Keep `pending`: the next reconciliation sees a swap that did not finish and overwrites no spec.
	(void)store.Write(*file);
	LiveChar now {};
	if (engine.Read(&now)) {
		rep.pointsUsed = SumLevels(now.levels, n);
		rep.unspent = now.unspent;
	}
	std::snprintf(rep.detail, sizeof(rep.detail), "spec %d: %s; restoring spec %d: %s", to, why, rep.from, back);
	return finish(SwapResult::LeftReset);
}

}  // namespace ms
