#include "bxb/format.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

namespace bxb {

auto FormatCount(uint64_t value) -> std::string {
	std::string digits = std::to_string(value);
	std::string out;
	const size_t n = digits.size();
	for (size_t i = 0; i < n; ++i) {
		out += digits[i];
		if ((n - i - 1) % 3 == 0 && i + 1 < n) {
			out += ',';
		}
	}
	return out;
}

auto FormatSigned(int64_t value) -> std::string {
	if (value == 0) {
		return "0";
	}
	const uint64_t mag = value < 0 ? 0 - static_cast<uint64_t>(value) : static_cast<uint64_t>(value);
	return (value < 0 ? "-" : "+") + FormatCount(mag);
}

auto FormatCompact(double value) -> std::string {
	if (!std::isfinite(value)) {
		return NotAvailable;
	}
	const double a = std::fabs(value);
	char buf[32];
	if (a >= 1e9) {
		std::snprintf(buf, sizeof(buf), "%.2fB", value / 1e9);
	} else if (a >= 1e6) {
		std::snprintf(buf, sizeof(buf), "%.2fM", value / 1e6);
	} else if (a >= 1e4) {
		std::snprintf(buf, sizeof(buf), "%.1fK", value / 1e3);
	} else {
		std::snprintf(buf, sizeof(buf), "%.0f", value);
	}
	return buf;
}

auto FormatPercent(double fraction, int decimals) -> std::string {
	char buf[32];
	std::snprintf(buf, sizeof(buf), "%.*f%%", decimals, fraction * 100.0);
	return buf;
}

auto FormatSignedPercent(double fraction, int decimals) -> std::string {
	char buf[32];
	std::snprintf(buf, sizeof(buf), "%+.*f%%", decimals, fraction * 100.0);
	return buf;
}

auto FormatClock(double seconds) -> std::string {
	const auto total = static_cast<uint64_t>(std::max(0.0, std::floor(seconds)));
	char buf[32];
	std::snprintf(buf, sizeof(buf), "%02llu:%02llu:%02llu", static_cast<unsigned long long>(total / 3600),
	              static_cast<unsigned long long>((total / 60) % 60), static_cast<unsigned long long>(total % 60));
	return buf;
}

auto FormatEta(const std::optional<double>& seconds) -> std::string {
	if (!seconds.has_value() || !std::isfinite(*seconds) || *seconds < 0.0) {
		return NotAvailable;
	}
	const auto t = static_cast<uint64_t>(std::ceil(*seconds));
	char buf[32];
	if (t < 60) {
		std::snprintf(buf, sizeof(buf), "%llus", static_cast<unsigned long long>(t));
	} else if (t < 3600) {
		std::snprintf(buf, sizeof(buf), "%llum %02llus", static_cast<unsigned long long>(t / 60),
		              static_cast<unsigned long long>(t % 60));
	} else if (t < 86400) {
		std::snprintf(buf, sizeof(buf), "%lluh %02llum", static_cast<unsigned long long>(t / 3600),
		              static_cast<unsigned long long>((t / 60) % 60));
	} else {
		std::snprintf(buf, sizeof(buf), "%llud %02lluh", static_cast<unsigned long long>(t / 86400),
		              static_cast<unsigned long long>((t / 3600) % 24));
	}
	return buf;
}

auto FormatLine(const Snapshot& s, LineId which) -> std::string {
	if (!s.valid) {
		return {};
	}
	switch (which) {
	case LineId::Level:
		return "Level " + std::to_string(s.level) + (s.atMaxLevel ? " (max)" : "");
	case LineId::Xp:
		return "XP " + FormatCount(s.xp) + "  " + (s.atMaxLevel ? std::string(NotAvailable) : FormatPercent(s.levelProgress));
	case LineId::Session:
		return "Session " + FormatSigned(s.sessionXp) + "  " + FormatSignedPercent(s.sessionLevels);
	case LineId::Rate: {
		std::string line = "XP/h ";
		line += s.live ? FormatCompact(s.live->xpPerHour) + "  " + FormatPercent(s.live->levelsPerHour) : std::string(NotAvailable);
		line += "  (avg ";
		line += s.session ? FormatCompact(s.session->xpPerHour) : std::string(NotAvailable);
		line += ")";
		return line;
	}
	case LineId::Time:
		return "Time " + FormatClock(s.activeSeconds);
	case LineId::Eta:
		return "Next level " + (s.atMaxLevel ? std::string(NotAvailable) : FormatEta(s.etaSeconds));
	case LineId::Count:
		break;
	}
	return {};
}

}  // namespace bxb
