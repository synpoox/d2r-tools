// ASCII-only case helpers shared by the cores. Never use the C locale's tolower: the result must not depend on a locale set elsewhere.
#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace cartographer::layout {

inline char lowerAscii(char c) noexcept {
	return c >= 'A' && c <= 'Z' ? static_cast<char>(c - 'A' + 'a') : c;
}

inline std::string lowerAscii(std::string_view s) {
	std::string out(s);
	for (char &c : out) c = lowerAscii(c);
	return out;
}

inline bool equalsNoCase(std::string_view a, std::string_view b) noexcept {
	if (a.size() != b.size()) return false;
	for (size_t i = 0; i < a.size(); ++i)
		if (lowerAscii(a[i]) != lowerAscii(b[i])) return false;
	return true;
}

} // namespace cartographer::layout
