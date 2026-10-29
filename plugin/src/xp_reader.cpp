// Reads XP, level and the experience table (xp_reader.h). NOT RUN IN GAME YET: needs live check throughout.
#include "xp_reader.h"

#include "d2r.h"
#include "log.h"

#define NOMINMAX
#include <windows.h>

#include <atomic>
#include <cstdio>
#include <vector>

namespace xp_reader {

namespace {

uintptr_t g_base = 0;
bool g_installed = false;
d2r::LocalSlotFn g_localSlot = nullptr;
d2r::PlayerUnitFn g_playerUnit = nullptr;
d2r::UnitClassFn g_unitClass = nullptr;
d2r::StatFn g_baseStat = nullptr;
d2r::StatFn g_totalStat = nullptr;
d2r::UnitDataFlagFn g_dataFlag = nullptr;
std::atomic<uint32_t> g_faults{0};

constexpr const char* ThisFile = "better-xp-bar/xp_reader.cpp";

// The game's own calls, as the stock XP bar makes them (findings (a), steps 2-6). Plain C++ (no SEH here).
void ReadPlayerRaw(Reading* r) noexcept {
	d2r::Unit* unit = g_playerUnit(g_localSlot());
	if (unit == nullptr) {
		return;
	}
	r->classId = g_unitClass(unit, ThisFile, __LINE__);
	const int32_t level = g_baseStat(unit, d2r::StatLevel, 0);
	r->xp = static_cast<uint32_t>(g_totalStat(unit, d2r::StatExperience, 0));
	r->dataFlag = g_dataFlag(unit);
	r->level = level > 0 ? static_cast<uint32_t>(level) : 0;
	r->ok = level > 0 && r->classId >= 0;
}

struct ExpVector {
	const uint32_t* rows;
	uint64_t count;
};

void ReadVectorRaw(uint32_t dataFlag, ExpVector* v) noexcept {
	const auto manager = *reinterpret_cast<const uintptr_t*>(g_base + d2r::RvaDataManagers + d2r::DataManagerStride * dataFlag);
	if (manager == 0) {
		return;
	}
	v->rows = *reinterpret_cast<const uint32_t* const*>(manager + d2r::ExpVectorOffset);
	v->count = *reinterpret_cast<const uint64_t*>(manager + d2r::ExpVectorOffset + 8);
}

void CopyColumnRaw(const uint32_t* rows, uint64_t count, int32_t classId, std::vector<uint64_t>* out) noexcept {
	for (uint64_t row = 1; row < count; ++row) {
		(*out)[row - 1] = rows[row * d2r::ExpRowWords + static_cast<uint32_t>(classId)];
	}
}

// SEH guards: a fault while reading (wrong state, stale pointer) turns this part's read into "no value" instead of a
// crash, and is counted. MSVC only; the CI build is MSVC.
#if defined(_MSC_VER)
#define BXB_TRY __try
#define BXB_EXCEPT __except (EXCEPTION_EXECUTE_HANDLER)
#else
#define BXB_TRY if (true)
#define BXB_EXCEPT else
#endif

auto GuardedReadPlayer(Reading* r) noexcept -> bool {
	BXB_TRY {
		ReadPlayerRaw(r);
		return true;
	}
	BXB_EXCEPT {
		return false;
	}
}

auto GuardedReadVector(uint32_t flag, ExpVector* v) noexcept -> bool {
	BXB_TRY {
		ReadVectorRaw(flag, v);
		return true;
	}
	BXB_EXCEPT {
		return false;
	}
}

auto GuardedCopyColumn(const uint32_t* rows, uint64_t count, int32_t classId, std::vector<uint64_t>* out) noexcept -> bool {
	BXB_TRY {
		CopyColumnRaw(rows, count, classId, out);
		return true;
	}
	BXB_EXCEPT {
		return false;
	}
}

auto GuardedMaxLevel(const uint32_t* rows, int32_t classId, uint32_t* out) noexcept -> bool {
	BXB_TRY {
		*out = rows[static_cast<uint32_t>(classId)];  // row 0
		return true;
	}
	BXB_EXCEPT {
		return false;
	}
}

void CountFault(const char* where) noexcept {
	static std::atomic<uint32_t> s_logged{0};
	++g_faults;
	if (plog::Sample(s_logged)) {
		plog::Error("xp: memory fault while reading %s (%u so far); the display shows nothing for this read", where, g_faults.load());
	}
}

}  // namespace

auto Install(const D2RL::PluginContext* context, char* report, size_t size) noexcept -> bool {
	for (const d2r::ExpectedBytes& c : d2r::Checks) {
		if (!context->CheckExpectedBytes(c.rva, c.bytes, c.size)) {
			std::snprintf(report, size, "off: game bytes differ at %s (another D2R/D2RLoader build, or patched)", c.what);
			return false;
		}
	}
	g_base = context->exeBase;
	g_localSlot = reinterpret_cast<d2r::LocalSlotFn>(g_base + d2r::RvaLocalSlot);
	g_playerUnit = reinterpret_cast<d2r::PlayerUnitFn>(g_base + d2r::RvaPlayerUnit);
	g_unitClass = reinterpret_cast<d2r::UnitClassFn>(g_base + d2r::RvaUnitClass);
	g_baseStat = reinterpret_cast<d2r::StatFn>(g_base + d2r::RvaBaseStat);
	g_totalStat = reinterpret_cast<d2r::StatFn>(g_base + d2r::RvaTotalStat);
	g_dataFlag = reinterpret_cast<d2r::UnitDataFlagFn>(g_base + d2r::RvaUnitDataFlag);
	g_installed = true;
	std::snprintf(report, size, "on: %zu byte checks passed", sizeof(d2r::Checks) / sizeof(d2r::Checks[0]));
	return true;
}

auto Installed() noexcept -> bool {
	return g_installed;
}

auto ReadPlayer() noexcept -> Reading {
	Reading r;
	if (!g_installed) {
		return r;
	}
	if (!GuardedReadPlayer(&r)) {
		CountFault("the player");
		return Reading {};
	}
	return r;
}

auto ReadTable(uint32_t dataFlag, int32_t classId, bxb::ExpTable* table, std::string* info) -> bool {
	char line[300];
	if (!g_installed || dataFlag >= d2r::DataBanks || classId < 0 || static_cast<uint32_t>(classId) >= d2r::ExpClasses) {
		std::snprintf(line, sizeof(line), "exp table refused: flag %u class %d (need flag < %u, class < %u)", dataFlag, classId,
		              d2r::DataBanks, d2r::ExpClasses);
		*info = line;
		return false;
	}
	ExpVector v {nullptr, 0};
	if (!GuardedReadVector(dataFlag, &v)) {
		CountFault("the experience table");
		*info = "exp table: memory fault";
		return false;
	}
	if (v.rows == nullptr || v.count < 3 || v.count > 1000) {
		std::snprintf(line, sizeof(line), "exp table refused: flag %u rows %p count %llu", dataFlag, static_cast<const void*>(v.rows),
		              static_cast<unsigned long long>(v.count));
		*info = line;
		return false;
	}
	std::vector<uint64_t> column(static_cast<size_t>(v.count - 1));
	uint32_t maxLevel = 0;
	if (!GuardedCopyColumn(v.rows, v.count, classId, &column) || !GuardedMaxLevel(v.rows, classId, &maxLevel)) {
		CountFault("the experience rows");
		*info = "exp table: memory fault";
		return false;
	}
	if (maxLevel < 2 || maxLevel + 1 > v.count) {
		std::snprintf(line, sizeof(line), "exp table refused: max level %u with %llu rows", maxLevel, static_cast<unsigned long long>(v.count));
		*info = line;
		return false;
	}
	column.resize(maxLevel);  // starts of levels 1..maxLevel = rows 1..maxLevel
	std::string error;
	if (!table->Assign(column, &error)) {
		*info = "exp table refused: " + error;
		return false;
	}
	std::snprintf(line, sizeof(line), "exp table: flag %u class %d rows %llu max level %u, start(1)=%llu start(2)=%llu start(%u)=%llu",
	              dataFlag, classId, static_cast<unsigned long long>(v.count), maxLevel,
	              static_cast<unsigned long long>(table->LevelStart(1)), static_cast<unsigned long long>(table->LevelStart(2)), maxLevel,
	              static_cast<unsigned long long>(table->LevelStart(maxLevel)));
	*info = line;
	return true;
}

}  // namespace xp_reader
