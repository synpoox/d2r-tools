#pragma once
#include <charconv>
#include <string_view>
#include <algorithm>
namespace recap {
struct Settings {
	bool enabled = true, autoOpen = true;
	uint32_t windowSeconds = 10, retention = 20;
};
inline std::string_view Trim(std::string_view s) noexcept {
	const auto a = s.find_first_not_of(" \t\r");
	if (a == s.npos) return {};
	return s.substr(a, s.find_last_not_of(" \t\r") - a + 1);
}
inline Settings ParseSettings(std::string_view text) noexcept {
	Settings s;
	bool section = false;
	while (!text.empty()) {
		const auto end = text.find('\n');
		auto line = text.substr(0, end);
		text = end == text.npos ? std::string_view{} : text.substr(end + 1);
		line = Trim(line.substr(0, line.find('#')));
		if (line.starts_with('[')) { section = line == "[death-recap]"; continue; }
		if (!section) continue;
		const auto eq = line.find('=');
		if (eq == line.npos) continue;
		const auto key = Trim(line.substr(0, eq)), value = Trim(line.substr(eq + 1));
		if (key == "enabled" || key == "auto_open") {
			if (value == "true" || value == "false") (key == "enabled" ? s.enabled : s.autoOpen) = value == "true";
		} else {
			uint32_t n = 0;
			const auto result = std::from_chars(value.data(), value.data() + value.size(), n);
			if (result.ec != std::errc{} || result.ptr != value.data() + value.size()) continue;
			if (key == "window_seconds") s.windowSeconds = std::clamp(n, 1U, 60U);
			if (key == "history_limit") s.retention = std::clamp(n, 1U, 100U);
		}
	}
	return s;
}
}
