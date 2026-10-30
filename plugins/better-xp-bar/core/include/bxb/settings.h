// better-xp-bar.toml (D2RLoader plugin config): parsing and defaults. Pure C++ (the plugin reads the file, this parses the text).
#pragma once

#include "bxb/format.h"
#include "bxb/xp_session.h"

#include <array>
#include <string>
#include <vector>

namespace bxb {

enum class Anchor { TopLeft, TopCenter, TopRight, BottomLeft, BottomCenter, BottomRight };
enum class TextAlign { Left, Center, Right };

struct Settings {
	bool visible = true;                    // [display] visible
	Anchor anchor = Anchor::BottomCenter;   // [display] anchor
	TextAlign textAlign = TextAlign::Left;  // [display] text_align: the lines' alignment and the box edge placed at offset_x
	float offsetX = -764.0f;                // [display] offset_x, layout units (+ right): left of the skill bar at bottom-center
	float offsetY = -345.0f;                // [display] offset_y, layout units (+ down): just above the skill bar
	float textSize = 0.0f;                  // [display] text_size, 0 = 30 layout units
	float scale = 1.0f;                     // [display] scale: the whole box (text, line height, padding), 0.25..4
	std::string xpFormat = DefaultXpFormat; // [display] xp_format: the XP line (tokens: format.h FormatXp)
	float background = 0.7f;                // [display] background, opacity 0..1 (0 = none)
	std::array<float, 3> textColor{1.0f, 0.86f, 0.55f};  // [display] text_color = r, g, b (0..1)
	std::vector<LineId> lines{LineId::Level, LineId::Xp, LineId::Session, LineId::Rate, LineId::Eta};
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
auto TextAlignName(TextAlign a) -> const char*;
auto LineName(LineId id) -> const char*;

}  // namespace bxb
