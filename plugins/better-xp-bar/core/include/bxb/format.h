// Text for the display lines. Pure C++; "--" stands for "not available yet".
#pragma once

#include "bxb/xp_session.h"

#include <cstdint>
#include <optional>
#include <string>

namespace bxb {

inline constexpr const char* NotAvailable = "--";

auto FormatCount(uint64_t value) -> std::string;           // 1234567 -> "1,234,567"
auto FormatSigned(int64_t value) -> std::string;           // "+1,234" / "-1,234" / "0"
auto FormatCompact(double value) -> std::string;           // 1234567 -> "1.23M", 98765 -> "98.8K", 512 -> "512"
auto FormatPercent(double fraction, int decimals = 2) -> std::string;  // 0.5 -> "50.00%"
auto FormatSignedPercent(double fraction, int decimals = 2) -> std::string;
auto FormatClock(double seconds) -> std::string;           // hh:mm:ss (hours not capped: "123:04:05")
auto FormatEta(const std::optional<double>& seconds) -> std::string;  // "--", "<1m", "13m" (rounded up), "3h 07m", "2d 04h"

// The display lines, in order: level, XP, session XP, XP/hour, session time, time to level.
// `which` is the 0-based line number (see LineId). Empty string when the snapshot is not valid.
enum class LineId { Level = 0, Xp, Session, Rate, Time, Eta, Count };

// The XP line's text: `format` with tokens replaced. {current} / {needed} / {remaining}: XP inside the level, the
// level's size and what is left, compact ("1.35M"); add _full for "1,350,000". {total}: total XP, full. {percent}:
// progress through the level. Per-level tokens show "--" at the maximum level. Unknown {tokens} are kept as written.
inline constexpr const char* DefaultXpFormat = "XP {current}/{needed}  {percent}";
auto FormatXp(const Snapshot& s, const std::string& format) -> std::string;

auto FormatLine(const Snapshot& s, LineId which, const std::string& xpFormat = DefaultXpFormat) -> std::string;

}  // namespace bxb
