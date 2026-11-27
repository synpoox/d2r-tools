#include "panel.h"
#include "scan.h"
#include "signatures.h"
#include "view.h"
#include "names.h"
#include "log.h"
#include <atomic>
#include <map>
#include <commctrl.h>

namespace recap::panel {
namespace {
const D2RL::PluginContext *context;
const D2RL::PanelService *panels;
const D2RL::WidgetService *widgets;
D2RL::Panels::RegistrationHandle handle = 0;
using FindPanelFn = void *(*)(const char *) noexcept;
using FindChildFn = void *(*)(void *, const char *) noexcept;
struct TextView {
	const char *text;
	uint64_t size;
};
using SetTextFn = void (*)(void *, const TextView *) noexcept;
FindPanelFn findPanel;
FindChildFn findChild;
SetTextFn setText;
using ScreenPosFn = void *(*)(void *, int32_t *) noexcept;
ScreenPosFn screenPos;
int32_t clipOffset = 0;
float uiScale = .5f;
view::Navigation nav;
std::array<size_t, 3> detailPages{1, 1, 1};
bool dirty = true, wasOpen = false, resetPage = false;
bool keyboardFocus = false;
uint64_t reportSession = 0, reportDeath = 0;
size_t reportCount = 0;
std::atomic<int> pending{0}, wheel{0};
std::atomic<bool> shown{false};
HWND window = nullptr;
constexpr UINT_PTR WheelSubclass = 1;
constexpr int W = 2520, H = 1940, Top = 350, Bottom = 1640;
struct Rect {
	int x, y, w, h;
};
struct Node {
	std::string name, type, fields, text, sentText;
	Rect initial{}, rect{}, sent{};
	bool content = false, visible = true, sentVisible = true;
};
std::vector<Node> nodes;
std::map<std::string, size_t> ids;
using Color = std::array<int, 3>;
constexpr Color Gold{202, 173, 118}, White{218, 215, 202}, Red{202, 55, 53}, Muted{160, 146, 117};
constexpr const char *ElementNames[]{"Physical", "Fire", "Lightning", "Cold", "Poison", "Magic"};
constexpr Color ElementColors[]{{163, 153, 139}, {193, 85, 16}, {45, 81, 217}, {68, 156, 195}, {76, 139, 25}, {170, 100, 190}};
std::string Rgb(Color c, double alpha = 1) {
	std::ostringstream o;
	o << '[' << c[0] / 255. << ',' << c[1] / 255. << ',' << c[2] / 255. << ',' << alpha << ']';
	return o.str();
}
void Add(std::string name, const char *type, Rect rect, std::string fields, bool content = true) {
	ids[name] = nodes.size();
	nodes.push_back({std::move(name), type, std::move(fields), {}, {}, rect, rect, rect, content});
}
void Box(const std::string &name, Rect r, bool content = true, double alpha = .40) {
	Add(name, "NineTileImageWidget", r,
		"\"filename\":\"PANEL/TooltipBorder/TooltipBorder\",\"tileScale\":0.45,\"backgroundColor\":[0.025,0.022,0.019," + std::to_string(alpha) + "]", content);
}
void Rectangle(const std::string &name, Rect r, Color c, double alpha = 1, bool content = true) {
	Add(name, "RectangleWidget", r, "\"color\":" + Rgb(c, alpha), content);
}
void Label(const std::string &name, Rect r, int size, Color c = White, const char *align = "left", bool content = true) {
	std::ostringstream o;
	o << "\"text\":\"\",\"truncateWithEllipsis\":true,\"style\":{\"options\":{\"hideOverflow\":true,\"lineWrap\":false},\"pointSize\":" << size
	  << ",\"fontColor\":{\"r\":" << c[0] << ",\"g\":" << c[1] << ",\"b\":" << c[2]
	  << ",\"a\":255},\"dropShadow\":\"$DefaultDropShadow\",\"alignment\":{\"h\":\"" << align << "\",\"v\":\"center\"}}";
	Add(name, "TextBoxWidget", r, o.str(), content);
}
void Button(const std::string &name, Rect r, const char *action, bool content = true) {
	Add(name, "ButtonWidget", r, std::string("\"onClickMessage\":\"PanelManager:OpenPanel:death-recap/") + action + "\"", content);
}
Node &Get(const std::string &name) { return nodes[ids.at(name)]; }
void Text(const std::string &name, std::string value) { Get(name).text = std::move(value); }
void Move(const std::string &name, int y) { Get(name).rect.y = y; }
void Visible(const std::string &name, bool value) { Get(name).visible = value; }
std::string Layout() {
	Add("Catcher", "ClickCatcherWidget", {0, 0, W, H}, "\"visible\":true", false);
	for (int i = 0; i < 3; ++i)
		Add("Texture" + std::to_string(i), "ImageWidget",
			{0,
			 i == 0	  ? 0
			 : i == 1 ? 517
					  : 1023,
			 1245, 453},
			"\"filename\":\"PANEL/Modals/Modal_No_Title_BG\"", false);
	Add("ScaleMarker", "Widget", {W, 0, 0, 0}, "\"visible\":false", false);
	Box("TitlePlate", {800, -10, 920, 138}, false);
	Label("Title", {815, 5, 890, 100}, 64, Gold, "center", false);
	Label("Died", {170, 145, 2180, 110}, 82, Red, "center", false);
	Rectangle("HeaderRule", {500, 255, 1520, 2}, {100, 24, 21}, .65, false);
	Label("Context", {160, 266, 2200, 58}, 39, White, "center", false);
	Box("KillingFrame", {150, 0, 2220, 310});
	Label("KillingTitle", {245, 28, 1300, 52}, 39);
	Label("Attacker", {245, 90, 1320, 90}, 66, Gold);
	Label("Ability", {245, 200, 1300, 68}, 41);
	Rectangle("KillingDivider", {1600, 42, 2, 225}, Gold, .4);
	Label("FinalLabel", {1670, 55, 360, 62}, 32);
	Label("Final", {1980, 50, 300, 76}, 48, Red, "right");
	Label("FinalUnavailable", {1980, 50, 300, 76}, 30, Red, "right");
	Label("OverkillUnavailable", {1980, 175, 300, 76}, 30, White, "right");
	Rectangle("FinalRule", {1660, 150, 630, 2}, Muted, .28);
	Label("OverkillLabel", {1670, 179, 360, 62}, 32);
	Label("Overkill", {1980, 175, 300, 76}, 44, White, "right");
	Box("DamageFrame", {150, 360, 1110, 450});
	Box("LifeFrame", {1300, 360, 1070, 450});
	Label("DamageTitle", {188, 374, 1030, 62}, 43, Gold);
	Label("LifeTitle", {1350, 374, 970, 62}, 43, Gold);
	Rectangle("DamageRule", {195, 450, 1015, 2}, Muted, .25);
	for (size_t i = 0; i < 6; ++i) {
		const std::string n = std::to_string(i);
		const int y = 467 + int(i) * 44;
		Label("Element" + n, {220, y, 380, 44}, 34);
		Label("Damage" + n, {595, y, 170, 44}, 34, White, "right");
		Label("Percent" + n, {785, y, 130, 44}, 32, Gold, "right");
		Rectangle("Track" + n, {945, y + 14, 270, 23}, {44, 41, 36});
		for (size_t e = 0; e < 6; ++e)
			Rectangle("Bar" + n + "_" + std::to_string(e), {945, y + 14, 0, 23}, ElementColors[e]);
	}
	Label("DamageEmpty", {220, 480, 980, 100}, 36, Muted);
	Label("DamageNote", {210, 747, 1020, 47}, 28, Muted);
	constexpr int gx = 1490, gy = 472, gw = 840, gh = 280;
	for (int i = 0; i <= 4; ++i) {
		Rectangle("GridH" + std::to_string(i), {gx, gy + i * 70, gw, 4}, {104, 96, 80}, .28);
		Rectangle("GridV" + std::to_string(i), {gx + i * 210, gy, 4, gh}, {104, 96, 80}, .23);
		Label("Axis" + std::to_string(i), {1320, gy + i * 70 - 22, 145, 45}, 30, Gold, "right");
	}
	for (const char *prefix : {"Fill", "Curve", "Heal"})
		for (size_t i = 0; i < view::Graph::Columns; ++i) {
			const Color color = std::string_view(prefix) == "Fill"	  ? Color{125, 16, 16}
								: std::string_view(prefix) == "Curve" ? Color{204, 34, 29}
																	  : Color{77, 165, 30};
			Rectangle(std::string(prefix) + std::to_string(i), {gx + int(i) * 2, gy + gh, 4, 0}, color, std::string_view(prefix) == "Fill" ? .28 : 1.);
		}
	Rectangle("LastPoint", {1490, 749, 6, 6}, {220, 44, 37});
	Label("StartTime", {1490, 756, 310, 45}, 28, Muted);
	Label("EndTime", {2090, 756, 240, 45}, 28, Muted, "right");
	Label("GraphEmpty", {1510, 540, 800, 90}, 36, Muted, "center");
	for (int i = 0; i < 3; ++i) {
		const std::string n = std::to_string(i);
		const int y = 870 + i * 130;
		Box("Section" + n, {150, y, 2220, 110});
		Rectangle("Focus" + n, {168, y + 20, 3, 70}, Gold, .85);
		Label("SectionTitle" + n, {215, y + 15, 1940, 80}, 44, Gold);
		Label("Chevron" + n, {2220, y + 12, 90, 85}, 46, Gold, "center");
		Button("Toggle" + n, {155, y + 3, 2210, 104}, ("Toggle" + n).c_str());
		Box("Details" + n, {150, y + 112, 2220, 385});
		for (int row = 0; row < 7; ++row)
			Label("Detail" + n + "_" + std::to_string(row), {220, y + 130 + row * 48, 2060, 48}, 33);
	}
	// The stock modal button supplies native texture, hover and pressed states.
	Add("CloseButton", "ButtonWidget", {985, 1715, 550, 112},
		R"("filename":"PANEL/Modals/ModalButton","onClickMessage":"PanelManager:OpenPanel:death-recap/Close","hoveredFrame":3,"pressedFrame":1,"disabledFrame":2,"textString":"Close","pointSize":48,"textColor":"$FontColorWhite")",
		false);
	std::ostringstream o;
	o << R"({"type":"Panel","name":"death-recap/Recap","fields":{"rect":{"x":-1260,"y":-970,"width":2520,"height":1940},"anchor":{"x":0.5,"y":0.5},"priority":100},"children":[)";
	bool first = true;
	for (const auto &n : nodes) {
		if (!first) o << ',';
		first = false;
		const auto &r = n.initial;
		o << "{\"type\":\"" << n.type << "\",\"name\":\"" << n.name << "\",\"fields\":{\"rect\":{\"x\":" << r.x << ",\"y\":" << r.y + (n.content ? Top : 0)
		  << ",\"width\":" << r.w << ",\"height\":" << r.h << (n.name.starts_with("Texture") ? ",\"scale\":2.024096" : "") << "}," << n.fields << "}}";
	}
	o << "]}";
	return o.str();
}
const char *KindName(Kind k) {
	constexpr const char *names[]{"Damage", "Healing", "Potion", "Regeneration / net", "Resource cost", "State", "Sample", "Death", "Other"};
	return names[std::min<size_t>(size_t(k), 8)];
}
std::string Skill(const Event *e) {
	return e && e->skill.Confirmed() ? "Ability unavailable (#" + std::to_string(e->skill.value) + ")" : "Ability unavailable";
}
std::string MeasureText(const Measure &m) { return m.known ? view::Number(m.total) + (m.unknown ? " (partial)" : "") : "Unavailable"; }
std::string StatName(uint32_t id) {
	switch (id) {
	case 34:
		return "Flat physical damage reduction";
	case 35:
		return "Flat magic damage reduction";
	case 36:
		return "Physical damage reduction";
	case 37:
		return "Magic resistance";
	case 142:
		return "Fire absorption";
	case 143:
		return "Flat fire absorption";
	case 144:
		return "Lightning absorption";
	case 145:
		return "Flat lightning absorption";
	case 148:
		return "Cold absorption";
	case 39:
		return "Fire resistance";
	case 40:
		return "Maximum fire resistance";
	case 41:
		return "Lightning resistance";
	case 42:
		return "Maximum lightning resistance";
	case 43:
		return "Cold resistance";
	case 44:
		return "Maximum cold resistance";
	case 45:
		return "Poison resistance";
	default:
		return "Defense stat #" + std::to_string(id);
	}
}
void Present(const Report *report, size_t page, bool preview) {
	for (auto &n : nodes) {
		n.rect = n.initial;
		n.visible = true;
		n.text.clear();
	}
	Text("Title", "Death Recap");
	Text("Died", "You Have Died");
	Text("Context", preview ? "Synthetic preview - no character death" : "Character level and area unavailable");
	Text("KillingTitle", "Killing Blow");
	Text("FinalLabel", "Final Damage");
	Text("OverkillLabel", "Overkill");
	Text("Attacker", "Attribution unavailable");
	Text("Ability", "Ability unavailable");
	Text("Final", "");
	Text("Overkill", "");
	Text("FinalUnavailable", "Unavailable");
	Text("OverkillUnavailable", "Unavailable");
	Text("DamageEmpty", report ? "Element attribution unavailable" : "No recorded deaths");
	Text("DamageNote", "Measured post-mitigation; life loss unavailable");
	for (size_t i = 0; i < 6; ++i) {
		for (const char *prefix : {"Element", "Damage", "Percent", "Track"})
			Visible(std::string(prefix) + std::to_string(i), false);
		for (size_t e = 0; e < 6; ++e)
			Visible("Bar" + std::to_string(i) + "_" + std::to_string(e), false);
	}
	for (size_t i = 0; i < view::Graph::Columns; ++i) {
		for (const char *prefix : {"Fill", "Curve", "Heal"})
			Visible(std::string(prefix) + std::to_string(i), false);
	}
	Text("GraphEmpty", "Life telemetry unavailable");
	Visible("LastPoint", false);
	const uint64_t duration = report ? report->deathMs - report->windowStartMs : 10000;
	Text("DamageTitle", "Damage Taken (" + view::Seconds(duration) + "s)");
	Text("LifeTitle", "Life (Last " + view::Seconds(duration) + "s)");
	Text("StartTime", "-" + view::Seconds(duration) + "s");
	Text("EndTime", "0.0s");
	if (report) {
		const auto &r = *report;
		const auto s = Summarize(r);
		if (preview && !r.areaName.empty()) Text("Context", "Synthetic preview | " + r.areaName);
		if (!preview && !r.timeline.empty()) {
			const auto &level = r.timeline.back().characterLevel;
			Text("Context", "Killed at " + (level.Confirmed() ? "Level " + std::to_string(level.value) : "level unavailable") + " in " +
								(r.areaName.empty() ? "area unavailable" : r.areaName));
		}
		const auto *fatal = KillingBlow(r);
		Text("Attacker", fatal && fatal->attacker.Confirmed() ? view::Shorten(SourceName(r, *fatal), 20) : "Attribution unavailable");
		Text("Ability", Skill(fatal));
		if (fatal) {
			if (fatal->damage.post.Confirmed()) {
				Text("Final", view::Value(fatal->damage.post));
				Visible("FinalUnavailable", false);
			}
			if (fatal->damage.overkill.Confirmed()) {
				Text("Overkill", view::Value(fatal->damage.overkill));
				Visible("OverkillUnavailable", false);
			}
		}
		const auto rows = view::Elements(s);
		Visible("DamageEmpty", rows.empty());
		const int rowHeight = rows.size() > 4 ? 44 : 72;
		for (size_t i = 0; i < rows.size(); ++i) {
			const auto &row = rows[i];
			const std::string n = std::to_string(i);
			const int y = 467 + int(i) * rowHeight;
			for (const char *prefix : {"Element", "Damage", "Percent", "Track"}) {
				Visible(std::string(prefix) + n, true);
				Move(std::string(prefix) + n, y + (std::string(prefix) == "Track" ? 14 : 0));
			}
			Text("Element" + n, ElementNames[row.element]);
			Text("Damage" + n, view::Number(row.amount) + (row.partial ? "*" : ""));
			Text("Percent" + n, std::to_string(int(std::lround(row.fraction * 100))) + "%");
			auto &bar = Get("Bar" + n + "_" + std::to_string(row.element));
			bar.visible = true;
			bar.rect.y = y + 14;
			bar.rect.w = int(row.fraction * 270);
		}
		Text("DamageNote", "Measured subtotal | Life lost: " + MeasureText(s.actualLifeLost));
		const int pixelStep = std::max(1, int(std::ceil(840 * uiScale / view::Graph::Columns)));
		const size_t columns = std::clamp<size_t>(size_t(840 * uiScale / pixelStep), 1, view::Graph::Columns);
		const int stroke = std::max(1, int(std::ceil(pixelStep / uiScale)));
		const auto graph = view::Life(r, 280, columns);
		bool any = false;
		for (int i = 0; i <= 4; ++i)
			Text("Axis" + std::to_string(i), view::Number(graph.maximum * (4 - i) / 4));
		for (size_t i = 0; i < columns; ++i) {
			const auto &c = graph.columns[i];
			if (!c.known) continue;
			any = true;
			const std::string n = std::to_string(i);
			auto &fill = Get("Fill" + n);
			fill.visible = i == 0 || !graph.columns[i - 1].known || graph.columns[i - 1].life != c.life;
			size_t end = i + 1;
			if (fill.visible)
				while (end < columns && graph.columns[end].known && graph.columns[end].life == c.life)
					++end;
			fill.rect.x = 1490 + int(std::ceil(i * pixelStep / uiScale));
			fill.rect.w = int(std::ceil(end * pixelStep / uiScale)) - int(std::ceil(i * pixelStep / uiScale));
			fill.rect.y = 752 - c.life;
			fill.rect.h = c.life;
			auto &curve = Get("Curve" + n);
			const bool flat = c.low == c.high && !c.healing;
			curve.visible = !flat || i == 0 || !graph.columns[i - 1].known || graph.columns[i - 1].low != c.low || graph.columns[i - 1].high != c.high ||
							graph.columns[i - 1].healing;
			curve.rect.x = fill.rect.x;
			curve.rect.w = stroke;
			if (flat && curve.visible) {
				size_t endCurve = i + 1;
				while (endCurve < columns && graph.columns[endCurve].known && graph.columns[endCurve].low == c.low && graph.columns[endCurve].high == c.high &&
					   !graph.columns[endCurve].healing)
					++endCurve;
				curve.rect.w = int(std::ceil(endCurve * pixelStep / uiScale)) - int(std::ceil(i * pixelStep / uiScale));
			}
			curve.rect.y = 752 - c.high;
			curve.rect.h = std::max(stroke, c.high - c.low);
			auto &heal = Get("Heal" + n);
			heal.visible = c.healing;
			heal.rect.x = fill.rect.x;
			heal.rect.w = stroke;
			heal.rect.y = 752 - c.high;
			heal.rect.h = std::max(stroke, c.high - c.low);
		}
		Visible("GraphEmpty", !any);
		for (size_t i = columns; i > 0; --i)
			if (graph.columns[i - 1].known) {
				auto &point = Get("LastPoint");
				point.visible = true;
				point.rect.x = 1488 + int(std::ceil((i - 1) * pixelStep / uiScale));
				point.rect.y = 750 - graph.columns[i - 1].life;
				break;
			}
		std::array<std::vector<std::string>, 3> details;
		for (const auto &e : r.timeline) {
			if (e.kind == Kind::Sample) continue;
			std::string line = "-" + view::Seconds(r.deathMs - e.ms) + "s  " + KindName(e.kind);
			if (e.life.before.Confirmed() || e.life.after.Confirmed()) line += "  | Life " + view::Value(e.life.before) + " > " + view::Value(e.life.after);
			if (e.mana.before.Confirmed() || e.mana.after.Confirmed()) line += "  | Mana " + view::Value(e.mana.before) + " > " + view::Value(e.mana.after);
			if (e.kind == Kind::Damage) line += "  | " + SourceName(r, e);
			details[0].push_back(std::move(line));
		}
		details[0].push_back(std::string(r.replicated ? "Replicated snapshots" : "Local capture") +
							 "; partial coverage. Missing events: " + std::to_string(r.feedDropped) + (r.sequenceGap ? "; sequence gap" : ""));
		details[0].push_back("Life restored: " + view::Number(r.life.restored) + "; mana lost: " + view::Number(r.mana.lost) +
							 "; unresolved life net: " + view::Number(r.life.unresolvedNet));
		const Defense *defense = nullptr;
		for (auto it = r.timeline.rbegin(); it != r.timeline.rend(); ++it)
			if (it->kind == Kind::Death) {
				defense = &it->defense;
				break;
			}
		if (defense) {
			for (size_t i = 0; i < std::min<size_t>(defense->statCount, defense->stats.size()); ++i) {
				const auto &stat = defense->stats[i];
				const bool percent = (stat.id >= 36 && stat.id <= 45) || stat.id == 142 || stat.id == 144 || stat.id == 148;
				details[1].push_back(StatName(stat.id) + (percent ? " (raw)" : "") + ": " +
									 (stat.amount.Confirmed() ? std::to_string(stat.amount.value) + (percent ? "%" : "") : "Unavailable"));
			}
			if (defense->stateEvidence == Evidence::Confirmed) {
				std::string states = "Active states";
				for (size_t i = 0; i < std::min<size_t>(defense->stateCount, defense->states.size()); ++i)
					states += " #" + std::to_string(defense->states[i]);
				details[1].push_back(states + (defense->statesTruncated ? " (truncated)" : ""));
			} else
				details[1].push_back("Curses, auras and active states: unavailable");
			if (defense->statsTruncated) details[1].push_back("Defense snapshot truncated");
		}
		details[1].push_back("Stat totals before difficulty penalties and caps; effective resistances unavailable.");
		details[1].push_back("Unlisted absorption, block and avoidance outcomes: unavailable");
		details[1].push_back("Partial evidence; no cause of death inferred from resistances alone.");
		for (const auto &[key, measure] : s.lossByAttacker) {
			std::string source = "Attribution unavailable";
			for (const auto &e : r.timeline)
				if (Key(e.attacker) == key) {
					source = SourceName(r, e);
					break;
				}
			details[2].push_back(source + " | actual life lost: " + MeasureText(measure));
		}
		for (const auto &[skill, measure] : s.lossBySkill)
			details[2].push_back((skill ? "Ability #" + std::to_string(*skill) : "Ability unavailable") + " | actual life lost: " + MeasureText(measure));
		bool modifiers = false;
		for (const auto &e : r.timeline) {
			std::string line;
			for (const auto &m : e.modifiers)
				if (m.Confirmed()) {
					line += " #" + std::to_string(m.value);
					modifiers = true;
				}
			if (!line.empty()) details[2].push_back(SourceName(r, e) + " | modifier IDs:" + line);
			if (e.origin.Confirmed())
				details[2].push_back("Origin: type " + std::to_string(e.origin.value.type) + ", id " + std::to_string(e.origin.value.id) + ", class " +
									 std::to_string(e.origin.value.classId));
			if (e.missile.Confirmed())
				details[2].push_back("Missile: id " + std::to_string(e.missile.value.id) + ", class " + std::to_string(e.missile.value.classId));
		}
		if (!modifiers) details[2].push_back("Attacker modifiers: unavailable");
		details[2].push_back("Post-mitigation measured: " + MeasureText(s.post) + "; overkill: " + MeasureText(s.overkill));
		for (size_t section = 0; section < 3; ++section) {
			const auto lines = view::Wrap(details[section]);
			const size_t pages = std::max<size_t>(1, (lines.size() + 5) / 6);
			detailPages[section] = pages;
			const size_t first = std::min(page, pages - 1) * 6;
			for (size_t i = 0; i < 6; ++i)
				Text("Detail" + std::to_string(section) + "_" + std::to_string(i), first + i < lines.size() ? lines[first + i] : "");
			Text("Detail" + std::to_string(section) + "_6",
				 pages > 1 ? "Page " + std::to_string(std::min(page, pages - 1) + 1) + " / " + std::to_string(pages) + "  -  Page Up / Page Down" : "");
		}
	}
	const char *titles[]{"Combat Timeline", "Your Defenses at Death", "Attacker Details"};
	int y = 870;
	for (int i = 0; i < 3; ++i) {
		const std::string n = std::to_string(i);
		for (const char *prefix : {"Section", "Focus", "SectionTitle", "Chevron", "Toggle", "Details"})
			Get(std::string(prefix) + n).rect.y += y - (870 + i * 130);
		Text("SectionTitle" + n, titles[i]);
		if (i == 2 && report)
			if (const auto *fatal = KillingBlow(*report)) Text("SectionTitle" + n, "Attacker Details (" + SourceName(*report, *fatal) + ")");
		Text("Chevron" + n, nav.expanded[size_t(i)] ? "^" : "v");
		Visible("Focus" + n, nav.focus == i && (keyboardFocus || nav.expanded[size_t(i)]));
		Visible("Details" + n, nav.expanded[size_t(i)]);
		for (int j = 0; j < 7; ++j) {
			auto &line = Get("Detail" + n + "_" + std::to_string(j));
			line.rect.y += y - (870 + i * 130);
			line.visible = nav.expanded[size_t(i)];
		}
		y += 130 + (nav.expanded[size_t(i)] ? 400 : 0);
	}
}
// Check the complete last-written rect before touching an owned widget. A
// rebuilt panel starts from its declared rect; foreign layouts are left alone.
bool ApplyRaw(void *root, Node *n, Rect r, bool visible) noexcept {
	__try {
		void *child = findChild(root, n->name.c_str());
		if (!child) return false;
		if (n->name.starts_with("Texture")) return true;
		auto *rect = reinterpret_cast<Rect *>(static_cast<uint8_t *>(child) + 0x70);
		Rect initial = n->initial;
		if (n->content) initial.y += Top;
		if (std::memcmp(rect, &n->sent, sizeof(Rect)) != 0 && std::memcmp(rect, &initial, sizeof(Rect)) != 0) {
			plog::Warn("owned rect mismatch %s: %d,%d,%d,%d expected %d,%d,%d,%d", n->name.c_str(), rect->x, rect->y, rect->w, rect->h, initial.x, initial.y,
					   initial.w, initial.h);
			return false;
		}
		*rect = r;
		n->sent = r;
		if (n->type == "RectangleWidget") {
			if (!visible) {
				rect->w = 0;
				rect->h = 0;
				n->sent = *rect;
			}
			return true;
		}
		D2RL::Widgets::WidgetHandle panel = 0, widget = 0;
		if (widgets->findPanel(context, "death-recap/Recap", &panel) != D2RL::Widgets::Result::Success ||
			widgets->findWidget(context, panel, n->name.c_str(), &widget) != D2RL::Widgets::Result::Success ||
			widgets->setWidgetVisible(context, widget, visible) != D2RL::Widgets::Result::Success)
			return false;
		if (n->type == "TextBoxWidget") {
			const TextView text{n->text.c_str(), n->text.size()};
			setText(child, &text);
		}
		return true;
	} __except (EXCEPTION_EXECUTE_HANDLER) {
		return false;
	}
}
bool ClipTextures(void *root) noexcept {
	__try {
		if (!screenPos || clipOffset != 0xac) return false;
		int32_t origin[2]{}, end[2]{};
		screenPos(root, origin);
		void *marker = findChild(root, "ScaleMarker");
		if (!marker) return false;
		screenPos(marker, end);
		static std::array<int32_t, 4> oldPosition{};
		const std::array<int32_t, 4> position{origin[0], origin[1], end[0], end[1]};
		if (position != oldPosition) {
			oldPosition = position;
			dirty = true;
		}
		const float scale = float(end[0] - origin[0]) / W;
		if (scale <= 0 || scale > 4) return false;
		uiScale = scale;
		constexpr int tops[]{0, 700, 1240}, bottoms[]{700, 1240, H};
		for (int i = 0; i < 3; ++i) {
			char name[20];
			std::snprintf(name, sizeof(name), "Texture%d", i);
			void *image = findChild(root, name);
			if (!image) return false;
			auto *clip = reinterpret_cast<Rect *>(static_cast<uint8_t *>(image) + clipOffset);
			*clip = {origin[0], origin[1] + int(tops[i] * scale), int(W * scale), int((bottoms[i] - tops[i]) * scale)};
		}
		return true;
	} __except (EXCEPTION_EXECUTE_HANDLER) {
		return false;
	}
}
bool Apply() {
	void *root = findPanel("death-recap/Recap");
	if (!root) return false;
	if (!ClipTextures(root)) return false;
	for (auto &n : nodes) {
		auto r = n.rect;
		bool visible = n.visible;
		if (n.content) {
			if (n.initial.y < 350 && nav.scroll > 0) visible = false;
			if (n.initial.y >= 350 && n.initial.y < 810 && nav.scroll > 360) visible = false;
			r.y += Top - nav.scroll;
			// Keep text whole and reflow section borders around the visible body.
			if (n.type == "NineTileImageWidget") {
				const int bottom = std::min(Bottom, r.y + r.h);
				r.y = std::max(Top, r.y);
				r.h = std::max(0, bottom - r.y);
				visible = visible && r.h >= 24;
			} else if (r.y < Top || r.y + r.h > Bottom)
				visible = false;
		}
		if (!ApplyRaw(root, &n, r, visible)) {
			static bool logged = false;
			if (!logged) {
				plog::Warn("UI apply failed: %s", n.name.c_str());
				logged = true;
			}
			return false;
		}
	}
	return true;
}
LRESULT CALLBACK WheelProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp, UINT_PTR, DWORD_PTR) {
	if (shown.load() && msg == WM_MOUSEWHEEL) {
		wheel.fetch_add(-GET_WHEEL_DELTA_WPARAM(wp) / WHEEL_DELTA);
		return 0;
	}
	return DefSubclassProc(hwnd, msg, wp, lp);
}
BOOL CALLBACK FindWindow(HWND hwnd, LPARAM result) {
	DWORD pid = 0;
	GetWindowThreadProcessId(hwnd, &pid);
	char name[32]{};
	if (pid == GetCurrentProcessId() && GetClassNameA(hwnd, name, sizeof(name)) && std::strcmp(name, "OsWindow") == 0) {
		*reinterpret_cast<HWND *>(result) = hwnd;
		return FALSE;
	}
	return TRUE;
}
void WatchWheel() {
	if (window) return;
	HWND hwnd = nullptr;
	EnumWindows(FindWindow, reinterpret_cast<LPARAM>(&hwnd));
	if (!hwnd) return;
	if (SetWindowSubclass(hwnd, WheelProc, WheelSubclass, 0)) window = hwnd;
}
D2RL::SharedEvents::UiMessageAction Message(const D2RL::PluginContext *, const D2RL::SharedEvents::UiMessageEvent *e, void *) noexcept {
	if (!e || !e->target || !e->command || !e->text || std::strcmp(e->target, "PanelManager") || std::strcmp(e->command, "OpenPanel"))
		return D2RL::SharedEvents::UiMessageAction::Continue;
	constexpr const char *actions[]{"death-recap/Toggle0", "death-recap/Toggle1", "death-recap/Toggle2", "death-recap/Close"};
	for (int i = 0; i < 4; ++i)
		if (!std::strcmp(e->text, actions[i])) {
			pending = i + 1;
			return D2RL::SharedEvents::UiMessageAction::Consume;
		}
	return D2RL::SharedEvents::UiMessageAction::Continue;
}
} // namespace
bool Install(const D2RL::PluginContext *ctx) noexcept {
	context = ctx;
	const D2RL::ResourceService *resources = nullptr;
	const D2RL::SharedEventService *events = nullptr;
	if (ctx->QueryService(&panels) != D2RL::ServiceQueryResult::Success || !D2RL::HasPanelServiceField(panels, D2RL::PanelServiceRequiredSize) ||
		ctx->QueryService(&widgets) != D2RL::ServiceQueryResult::Success || !D2RL::HasWidgetServiceField(widgets, D2RL::WidgetServiceRequiredSize) ||
		ctx->QueryService(&events) != D2RL::ServiceQueryResult::Success || !D2RL::HasSharedEventServiceField(events, D2RL::SharedEventServiceRequiredSize) ||
		ctx->QueryService(&resources) != D2RL::ServiceQueryResult::Success || !D2RL::HasResourceServiceField(resources, D2RL::ResourceServiceRequiredSize))
		return false;
	findPanel = reinterpret_cast<FindPanelFn>(native::Find(ctx->exeBase, native::FindPanelSig));
	findChild = reinterpret_cast<FindChildFn>(native::Find(ctx->exeBase, native::FindChildSig));
	const auto *text = native::Find(ctx->exeBase, native::SetTextSig);
	setText = text ? reinterpret_cast<SetTextFn>(const_cast<uint8_t *>(text - 16)) : nullptr;
	screenPos = reinterpret_cast<ScreenPosFn>(native::Find(ctx->exeBase, native::WidgetScreenPosSig));
	const auto *clip = native::Find(ctx->exeBase, native::ImageClipSig);
	if (clip) std::memcpy(&clipOffset, clip + 3, sizeof(clipOffset));
	if (!findPanel || !findChild || !setText || !screenPos || clipOffset != 0xac) {
		plog::Warn("recap UI off: native text/geometry/clip witness unavailable");
		return false;
	}
	try {
		const auto json = Layout();
		D2RL::Resources::ResourceRegistration resource{};
		resource.structSize = sizeof(resource);
		resource.path = "data/global/ui/layouts/death-recap/Recaphd.json";
		resource.bytes = json.data();
		resource.byteCount = json.size();
		D2RL::Resources::RegistrationHandle resourceHandle = 0;
		if (resources->registerResource(ctx, &resource, &resourceHandle) != D2RL::Resources::Result::Success) return false;
		D2RL::Panels::PanelRegistration registration{};
		registration.structSize = sizeof(registration);
		registration.localId = "Recap";
		registration.flags = D2RL::Panels::PanelFlags::CloseOnEscape;
		if (panels->registerPanel(ctx, &registration, &handle) != D2RL::Panels::Result::Success) return false;
		D2RL::SharedEvents::UiMessageListener listener{};
		listener.structSize = sizeof(listener);
		listener.callback = Message;
		D2RL::SharedEvents::ListenerHandle listenerHandle = 0;
		if (events->registerUiMessageListener(ctx, &listener, &listenerHandle) != D2RL::SharedEvents::Result::Success) return false;
		// One route is available globally. A conflict leaves only this optional
		// controller reopen route off; other plugins keep their route.
		constexpr const char *title =
			R"([{"Key":"Title","enUS":"Death Recap","deDE":"Death Recap","esES":"Death Recap","esMX":"Death Recap","frFR":"Death Recap","itIT":"Death Recap","jaJP":"Death Recap","koKR":"Death Recap","plPL":"Death Recap","ptBR":"Death Recap","ruRU":"Death Recap","zhCN":"Death Recap","zhTW":"Death Recap"}])";
		resource.path = "data/local/lng/strings/d2rloader/death-recap/strings.json";
		resource.bytes = title;
		resource.byteCount = std::strlen(title);
		const bool strings = resources->registerResource(ctx, &resource, &resourceHandle) == D2RL::Resources::Result::Success;
		if (strings &&
			D2RL::HasPanelServiceField(
				panels, static_cast<uint32_t>(offsetof(D2RL::PanelService, registerControllerRoute) + sizeof(panels->registerControllerRoute))) &&
			panels->registerControllerRoute) {
			D2RL::Panels::ControllerRouteRegistration route{};
			route.structSize = sizeof(route);
			route.panel = handle;
			route.flags = D2RL::Panels::ControllerRouteFlags::None;
			route.placement = D2RL::Panels::ControllerRoutePlacement::AfterPlayerInventory;
			route.localizedLabelKey = "@death-recap:Title";
			D2RL::Panels::ControllerRouteHandle h = 0;
			const auto result = panels->registerControllerRoute(ctx, &route, &h);
			plog::Info("controller reopen route result %u", static_cast<unsigned>(result));
		}

		return true;
	} catch (...) {
		return false;
	}
}
bool Open() noexcept {
	if (!handle) return false;
	D2RL::Panels::PanelInfo info{};
	info.structSize = sizeof(info);
	return panels->getPanelInfo(context, handle, &info) == D2RL::Panels::Result::Success && info.presentationState == D2RL::Panels::PresentationState::Open;
}
void Show(bool toggle) noexcept {
	if (!handle) return;
	nav.Reset();
	keyboardFocus = false;
	dirty = true;
	if (toggle)
		(void)panels->togglePanel(context, handle);
	else
		(void)panels->openPanel(context, handle);
}
void Close() noexcept {
	shown = false;
	if (handle) (void)panels->closePanel(context, handle);
}
void Input(uint32_t command) noexcept {
	if (!Open()) return;
	if (command == 2) nav.focus = (nav.focus + 2) % 3;
	if (command == 3) nav.focus = (nav.focus + 1) % 3;
	if (command == 2 || command == 3) {
		keyboardFocus = true;
		int y = 870 + nav.focus * 130;
		for (int i = 0; i < nav.focus; ++i)
			if (nav.expanded[size_t(i)]) y += 400;
		if (y < nav.scroll) nav.scroll = y;
		if (y + 110 > nav.scroll + 1290) nav.scroll = y + 110 - 1290;
		nav.Scroll(0);
	}
	if (command == 11) {
		nav.Toggle(nav.focus);
		resetPage = true;
	}
	if (command == 12) nav.Scroll(-144);
	if (command == 13) nav.Scroll(144);
	dirty = true;
}
size_t Pages() noexcept { return detailPages[size_t(nav.focus)]; }
void Stop() noexcept {
	shown = false;
	if (window) RemoveWindowSubclass(window, WheelProc, WheelSubclass);
	window = nullptr;
}
void Update(const History &history, size_t selected, size_t &page, bool preview) noexcept {
	static size_t oldPage = 0;
	const bool open = Open();
	shown = open;
	if (!open) {
		wasOpen = false;
		return;
	}
	if (!wasOpen) {
		nav.Reset();
		dirty = true;
		wasOpen = true;
	}
	WatchWheel();
	if (void *root = findPanel("death-recap/Recap")) (void)ClipTextures(root);
	if (reportCount != history.size()) {
		reportCount = history.size();
		dirty = true;
	}
	const auto *report = history.empty() ? nullptr : &history[std::min(selected, history.size() - 1)];
	if (report && (reportSession != report->session || reportDeath != report->deathMs)) {
		nav.Reset();
		dirty = true;
		reportSession = report->session;
		reportDeath = report->deathMs;
	}
	const int command = pending.exchange(0), scroll = wheel.exchange(0);
	if (command == 4) {
		Close();
		return;
	}
	if (command) {
		nav.Toggle(command - 1);
		resetPage = true;
		dirty = true;
	}
	if (scroll) {
		nav.Scroll(scroll * 144);
		dirty = true;
	}
	if (resetPage) {
		page = 0;
		resetPage = false;
	}
	if (oldPage != page) {
		dirty = true;
		oldPage = page;
	}
	if (!dirty) {
		if (void *root = findPanel("death-recap/Recap")) (void)ClipTextures(root);
		return;
	}
	try {
		Present(report, page, preview);
		if (Apply()) dirty = false;
	} catch (...) {
		plog::Warn("recap presentation failed; gameplay is unaffected");
	}
}
} // namespace recap::panel
