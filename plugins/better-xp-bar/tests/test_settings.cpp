#include "bxb/settings.h"
#include "check.h"

#include <algorithm>
#include <cstdio>

using namespace bxb;

TEST(settings_defaults_match_default_toml) {
	std::vector<std::string> w;
	const Settings a = ParseSettings(DefaultToml(), &w);
	const Settings b;
	CHECK(w.empty());
	CHECK_EQ(a.visible, b.visible);
	CHECK(a.anchor == b.anchor);
	CHECK(a.textAlign == b.textAlign);
	CHECK_NEAR(a.scale, b.scale, 1e-6);
	CHECK_EQ(a.xpFormat, b.xpFormat);
	CHECK_NEAR(a.offsetX, b.offsetX, 1e-6);
	CHECK_NEAR(a.offsetY, b.offsetY, 1e-6);
	CHECK_NEAR(a.background, b.background, 1e-6);
	CHECK_EQ(a.lines.size(), b.lines.size());
	CHECK_NEAR(a.session.windowSeconds, 600.0, 1e-9);
	CHECK(a.session.scope == SessionScope::Game);
	CHECK(!a.session.etaFromSessionRate);
}

TEST(settings_parse_values) {
	std::vector<std::string> w;
	const Settings s = ParseSettings(
		"\xEF\xBB\xBF[Display]\n"
		"Visible = off\n"
		"anchor = \"TOP-LEFT\" # comment\n"
		"text_align = \"Right\"\n"
		"offset_x = 12.5\n"
		"offset_y=-3\n"
		"text_size = 18\n"
		"scale = 0.75\n"
		"xp_format = \"{current}/{needed} #{percent}\"\n"
		"background = 0\n"
		"text_color = [0.1, 0.2,0.3]\n"
		"lines = [\"rate\", 'eta']\n"
		"[session]\n"
		"scope = \"character\"\n"
		"window_seconds = 300\n"
		"eta_from = \"session\" # a '#' in \"quotes\" is kept\n"
		"[log]\n"
		"xp_changes = yes\n",
		&w);
	CHECK(w.empty());
	CHECK(!s.visible);
	CHECK(s.anchor == Anchor::TopLeft);
	CHECK(s.textAlign == TextAlign::Right);
	CHECK_NEAR(s.offsetX, 12.5, 1e-6);
	CHECK_NEAR(s.offsetY, -3.0, 1e-6);
	CHECK_NEAR(s.textSize, 18.0, 1e-6);
	CHECK_NEAR(s.scale, 0.75, 1e-6);
	CHECK_EQ(s.xpFormat, std::string("{current}/{needed} #{percent}"));
	CHECK_NEAR(s.background, 0.0, 1e-6);
	CHECK_NEAR(s.textColor[2], 0.3, 1e-6);
	CHECK_EQ(s.lines.size(), 2u);
	CHECK(s.lines[0] == LineId::Rate);
	CHECK(s.lines[1] == LineId::Eta);
	CHECK(s.session.scope == SessionScope::Character);
	CHECK_NEAR(s.session.windowSeconds, 300.0, 1e-9);
	CHECK(s.session.etaFromSessionRate);
	CHECK(s.logXpChanges);
}

TEST(settings_bad_values_keep_defaults_and_warn) {
	std::vector<std::string> w;
	const Settings s = ParseSettings(
		"[display]\nanchor = \"middle\"\ntext_align = \"justify\"\nscale = 9\nbackground = 2\ntext_color = [1,2]\nlines = [\"level\", \"bogus\"]\nnokey\n"
		"[session]\nwindow_seconds = 10\nscope = \"forever\"\n[other]\nx = 1\n",
		&w);
	const Settings d;
	CHECK(s.anchor == d.anchor);
	CHECK(s.textAlign == d.textAlign);
	CHECK_NEAR(s.background, d.background, 1e-6);
	CHECK_NEAR(s.textColor[0], d.textColor[0], 1e-6);
	CHECK_EQ(s.lines.size(), 1u);  // the known line stays
	CHECK_NEAR(s.session.windowSeconds, 600.0, 1e-9);
	CHECK(s.session.scope == SessionScope::Game);
	CHECK_EQ(w.size(), 10u);
	CHECK(w[0].find("line 2") == 0);
}

TEST(settings_empty_lines_allowed) {
	const Settings s = ParseSettings("[display]\nlines = []\n");
	CHECK(s.lines.empty());
}

TEST(settings_lines_must_be_an_array) {
	std::vector<std::string> w;
	const Settings s = ParseSettings("[display]\nlines = \"rate\"\n", &w);
	CHECK_EQ(s.lines.size(), Settings{}.lines.size());
	CHECK_EQ(w.size(), 1u);
}

TEST(settings_embedded_file_equals_default_text) {
	// plugin/better-xp-bar.toml is what the DLL embeds; it must stay the same as DefaultToml().
	FILE* f = std::fopen(BXB_SOURCE_DIR "/plugin/better-xp-bar.toml", "rb");
	CHECK(f != nullptr);
	if (f == nullptr) {
		return;
	}
	std::string text;
	char buf[4096];
	size_t n;
	while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) {
		text.append(buf, n);
	}
	std::fclose(f);
	text.erase(std::remove(text.begin(), text.end(), '\r'), text.end());
	CHECK(text == DefaultToml());
}
