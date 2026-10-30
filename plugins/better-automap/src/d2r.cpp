// Image base for d2r.h (bound by plugin.cpp after the image check).
#include "d2r.h"

namespace d2r {

namespace {
uintptr_t g_base = 0;
}  // namespace

void Bind(uintptr_t exeBase) noexcept {
	g_base = exeBase;
}

auto Base() noexcept -> uintptr_t {
	return g_base;
}

}  // namespace d2r
