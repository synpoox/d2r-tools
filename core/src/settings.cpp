#include "bxb/settings.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <sstream>

namespace bxb {

namespace {

auto Trim(std::string s) -> std::string {
	const auto notSpace = [](unsigned char c) { return !std::isspace(c); };
	s.erase(s.begin(), std::find_if(s.begin(), s.end(), notSpace));
	s.erase(std::find_if(s.rbegin(), s.rend(), notSpace).base(), s.end());
	return s;
}

auto Lower(std::string s) -> std::string {
	for (char& c : s) {
		c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
	}
	return s;
}

auto Split(const std::string& s, char sep) -> std::vector<std::string> {
	std::vector<std::string> out;
	std::string item;
	std::istringstream in(s);
	while (std::getline(in, item, sep)) {
		item = Trim(item);
		if (!item.empty()) {
			out.push_back(item);
		}
	}
	return out;
}

// TOML scalars: "text" or 'text' -> text; anything else as written.
auto Unquote(const std::string& v) -> std::string {
	if (v.size() >= 2 && (v.front() == '"' || v.front() == '\'') && v.back() == v.front()) {
		return v.substr(1, v.size() - 2);
	}
	return v;
}

// TOML array [a, "b", c] -> items (unquoted). Not an array -> false.
auto ParseArray(const std::string& v, std::vector<std::string>* out) -> bool {
	if (v.size() < 2 || v.front() != '[' || v.back() != ']') {
		return false;
	}
	out->clear();
	for (const std::string& item : Split(v.substr(1, v.size() - 2), ',')) {
		out->push_back(Unquote(item));
	}
	return true;
}

// Cut a '#' comment that is not inside a quoted string.
auto StripComment(const std::string& line) -> std::string {
	char quote = 0;
	for (size_t i = 0; i < line.size(); ++i) {
		const char c = line[i];
		if (quote != 0) {
			if (c == quote) {
				quote = 0;
			}
		} else if (c == '"' || c == '\'') {
			quote = c;
		} else if (c == '#') {
			return line.substr(0, i);
		}
	}
	return line;
}

auto ParseFloat(const std::string& v, float* out) -> bool {
	char* end = nullptr;
	const double d = std::strtod(v.c_str(), &end);
	if (end == v.c_str() || *end != '\0') {
		return false;
	}
	*out = static_cast<float>(d);
	return true;
}

auto ParseBool(const std::string& v, bool* out) -> bool {
	const std::string l = Lower(v);
	if (l == "1" || l == "true" || l == "yes" || l == "on") {
		*out = true;
		return true;
	}
	if (l == "0" || l == "false" || l == "no" || l == "off") {
		*out = false;
		return true;
	}
	return false;
}

constexpr std::array<const char*, 6> AnchorNames{"top-left", "top-center", "top-right", "bottom-left", "bottom-center", "bottom-right"};
constexpr std::array<const char*, 6> LineNames{"level", "xp", "session", "rate", "time", "eta"};

}  // namespace

auto AnchorName(Anchor a) -> const char* {
	return AnchorNames[static_cast<size_t>(a)];
}

auto LineName(LineId id) -> const char* {
	const auto i = static_cast<size_t>(id);
	return i < LineNames.size() ? LineNames[i] : "?";
}

auto ParseSettings(const std::string& text, std::vector<std::string>* warnings) -> Settings {
	Settings s;
	const auto warn = [&](int line, const std::string& what) {
		if (warnings != nullptr) {
			warnings->push_back("line " + std::to_string(line) + ": " + what);
		}
	};
	std::string section;
	std::istringstream in(text);
	std::string raw;
	int n = 0;
	while (std::getline(in, raw)) {
		++n;
		std::string line = raw;
		if (n == 1 && line.size() >= 3 && static_cast<unsigned char>(line[0]) == 0xEF) {
			line = line.substr(3);  // UTF-8 BOM
		}
		line = Trim(StripComment(line));
		if (line.empty()) {
			continue;
		}
		if (line.front() == '[' && line.back() == ']') {
			section = Lower(Trim(line.substr(1, line.size() - 2)));
			continue;
		}
		const size_t eq = line.find('=');
		if (eq == std::string::npos) {
			warn(n, "expected key = value");
			continue;
		}
		const std::string key = section + "." + Lower(Trim(line.substr(0, eq)));
		const std::string rawValue = Trim(line.substr(eq + 1));
		const std::string value = Unquote(rawValue);
		const std::string lvalue = Lower(value);
		std::vector<std::string> items;
		const bool isArray = ParseArray(rawValue, &items);
		float f = 0.0f;
		bool ok = true;
		if (key == "display.visible") {
			ok = ParseBool(value, &s.visible);
		} else if (key == "display.anchor") {
			const auto it = std::find(AnchorNames.begin(), AnchorNames.end(), lvalue);
			ok = it != AnchorNames.end();
			if (ok) {
				s.anchor = static_cast<Anchor>(it - AnchorNames.begin());
			}
		} else if (key == "display.offset_x") {
			ok = ParseFloat(value, &s.offsetX);
		} else if (key == "display.offset_y") {
			ok = ParseFloat(value, &s.offsetY);
		} else if (key == "display.text_size") {
			ok = ParseFloat(value, &f) && f >= 0.0f && f <= 200.0f;
			if (ok) {
				s.textSize = f;
			}
		} else if (key == "display.background") {
			ok = ParseFloat(value, &f) && f >= 0.0f && f <= 1.0f;
			if (ok) {
				s.background = f;
			}
		} else if (key == "display.text_color") {
			std::array<float, 3> c{};
			ok = isArray && items.size() == 3;
			for (size_t i = 0; ok && i < 3; ++i) {
				ok = ParseFloat(items[i], &c[i]) && c[i] >= 0.0f && c[i] <= 1.0f;
			}
			if (ok) {
				s.textColor = c;
			}
		} else if (key == "display.lines") {
			if (!isArray) {
				warn(n, "'display.lines' must be an array, e.g. [\"level\", \"xp\"] (default kept)");
				continue;
			}
			std::vector<LineId> lines;
			for (const std::string& item : items) {
				const std::string name = Lower(item);
				const auto it = std::find(LineNames.begin(), LineNames.end(), name);
				if (it == LineNames.end()) {
					warn(n, "unknown line '" + name + "'");
					continue;
				}
				lines.push_back(static_cast<LineId>(it - LineNames.begin()));
			}
			s.lines = lines;  // an empty list is allowed: nothing is drawn
		} else if (key == "session.window_seconds") {
			ok = ParseFloat(value, &f) && f >= 60.0f && f <= 86400.0f;
			if (ok) {
				s.session.windowSeconds = f;
			}
		} else if (key == "session.scope") {
			ok = lvalue == "game" || lvalue == "character";
			if (ok) {
				s.session.scope = lvalue == "game" ? SessionScope::Game : SessionScope::Character;
			}
		} else if (key == "session.eta_from") {
			ok = lvalue == "window" || lvalue == "session";
			if (ok) {
				s.session.etaFromSessionRate = lvalue == "session";
			}
		} else if (key == "log.xp_changes") {
			ok = ParseBool(value, &s.logXpChanges);
		} else {
			warn(n, "unknown key '" + key + "'");
			continue;
		}
		if (!ok) {
			warn(n, "bad value for '" + key + "': '" + value + "' (default kept)");
		}
	}
	return s;
}

auto DefaultToml() -> std::string {
	return R"(# Better XP Bar settings (D2RLoader writes this file from the plugin's built-in defaults when it is missing).
# Restart the game after a change, or type "better-xp-bar reload" in the D2RLoader console.

[display]
visible = true              # shown when a game starts (toggle with the hotkey)
anchor = "bottom-center"    # top-left, top-center, top-right, bottom-left, bottom-center, bottom-right
offset_x = 0                # pixels from the anchor, + = right
offset_y = -160             # pixels from the anchor, + = down
text_size = 0               # 0 = D2RLoader's default text size
background = 0.55           # background opacity 0..1 (0 = no background)
text_color = [1.0, 0.86, 0.55]
lines = ["level", "xp", "session", "rate", "time", "eta"]   # which lines, in this order

[session]
scope = "game"              # game: a new session every game. character: one session per character across games
window_seconds = 600        # rolling window for the live XP/hour (60..86400)
eta_from = "window"         # window: time to level from the live rate. session: from the session average

[log]
xp_changes = false          # true: log XP changes (sampled 1st, 100th, 10,000th ...)
)";
}

}  // namespace bxb
