// better-xp-bar.toml (D2RLoader plugin config): parsing and defaults. Pure C++ (the plugin reads the file, this parses the text).
#pragma once

#include "bxb/format.h"
#include "bxb/xp_session.h"

#include <array>
#include <string>
#include <vector>

namespace bxb {

enum class Anchor { TopLeft, TopCenter, TopRight, BottomLeft, BottomCenter, BottomRight };

struct Settings {
	bool visible = true;                    // [display] visible
	Anchor anchor = Anchor::BottomCenter;   // [display] anchor
	float offsetX = 0.0f;                   // [display] offset_x, pixels (+ right)
	float offsetY = -160.0f;                // [display] offset_y, pixels (+ down)
	float textSize = 0.0f;                  // [display] text_size, 0 = the loader's default
	float background = 0.55f;               // [display] background, opacity 0..1 (0 = none)
	std::array<float, 3> textColor{1.0f, 0.86f, 0.55f};  // [display] text_color = r, g, b (0..1)
	std::vector<LineId> lines{LineId::Level, LineId::Xp, LineId::Session, LineId::Rate, LineId::Time, LineId::Eta};
	SessionConfig session;                  // [session] window_seconds, scope, eta_from
	bool logXpChanges = false;              // [log] xp_changes: one log line per XP change (sampled)
};

// Parses the TOML subset the file uses ("[section]", key = value with numbers, booleans, "strings" and [arrays],
// '#' comments). Unknown keys and bad values keep the default
// and add a warning. Never throws.
auto ParseSettings(const std::string& text, std::vector<std::string>* warnings = nullptr) -> Settings;

// The commented default file (the same text is embedded in the DLL as plugin/better-xp-bar.toml).
auto DefaultToml() -> std::string;

auto AnchorName(Anchor a) -> const char*;
auto LineName(LineId id) -> const char*;

}  // namespace bxb
