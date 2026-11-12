// Checked reads and writes of the game's own memory (table rows), shared by the parts that edit the slot level's row.
#pragma once

#include <cstddef>

namespace carto {

// True when every page of [address, address + size) is committed and can be read.
auto Readable(const void* address, size_t size) noexcept -> bool;

// Copies into game memory, lifting the protection of every page the range touches that is not writable, for the copy only.
// False, and nothing written, if it could not.
auto WriteGame(void* dst, const void* src, size_t size) noexcept -> bool;

}  // namespace carto
