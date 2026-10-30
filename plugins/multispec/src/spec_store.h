// <name>.multispec next to the character's save (docs/design.md section 3): where it is, reading it, writing it
// atomically. No game calls.
#pragma once

#include "swap.h"

#include <D2RLPlugin/api.h>

#include <cstddef>

namespace ms {

// Remembers the mod's save folder (from modinfo.json "savepath" under Saved Games\Diablo II Resurrected\) and a
// fallback folder (<mod support dir or plugin dir>\multispec). Call once at load. `report` says which.
void InitStorePaths(const D2RL::PluginContext* context, char* report, size_t size) noexcept;

enum class LoadResult { Loaded, Missing, Quarantined, Failed };

class FileStore final : public Store {
public:
	// Picks the file for `name`: the save folder when <name>.d2s is there, else the fallback folder.
	auto Open(const char* name, char* note, size_t size) noexcept -> bool;
	auto Load(CharSpecs* out, char* why, size_t size) noexcept -> LoadResult;
	auto Write(const CharSpecs& specs) noexcept -> bool override;
	auto PathUtf8(char* out, size_t size) const noexcept -> void;

private:
	wchar_t m_path[520] = L"";
};

}  // namespace ms
