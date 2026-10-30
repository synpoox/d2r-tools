// better-auras.toml reader (config.h).
#include "config.h"

#include "log.h"

#define NOMINMAX
#include <windows.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace config {

namespace {

std::vector<SkillConfig> g_skills;

// skills.txt names of D2R 3.3's aura skills (aura=1 with a srvdofunc, or srvdofunc 65/66/81/82), for files that name
// skills when the mod has no loose skills.txt to read.
struct Named {
	const char* name;
	int32_t id;
};
constexpr Named VanillaAuras[] = {
	{"Might", 98}, {"Prayer", 99}, {"Resist Fire", 100}, {"Holy Fire", 102}, {"Thorns", 103}, {"Defiance", 104},
	{"Resist Cold", 105}, {"Blessed Aim", 108}, {"Cleansing", 109}, {"Resist Lightning", 110}, {"Concentration", 113},
	{"Holy Freeze", 114}, {"Vigor", 115}, {"Holy Shock", 118}, {"Sanctuary", 119}, {"Meditation", 120},
	{"Fanaticism", 122}, {"Conviction", 123}, {"Redemption", 124}, {"Salvation", 125}, {"Barbs Aura", 296},
	{"Wolverine Aura", 297}, {"Oak Sage Aura", 298}, {"MonHolyFreeze", 365}, {"MonHolyFire", 368},
	{"MonHolyShock", 369},
};

std::vector<std::pair<std::string, int32_t>> g_modNames;  // from the mod's loose skills.txt

auto Lower(std::string s) -> std::string {
	for (char& c : s) {
		c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
	}
	return s;
}

void ReadModSkillNames(const wchar_t* gameDir, const char* mod) noexcept {
	g_modNames.clear();
	if (gameDir == nullptr || mod == nullptr || mod[0] == '\0') {
		return;
	}
	wchar_t path[MAX_PATH];
	swprintf_s(path, L"%ls\\mods\\%hs\\%hs.mpq\\data\\global\\excel\\skills.txt", gameDir, mod, mod);
	FILE* f = nullptr;
	if (_wfopen_s(&f, path, L"rb") != 0 || f == nullptr) {
		return;
	}
	std::string text;
	char buf[65536];
	for (size_t n; (n = std::fread(buf, 1, sizeof(buf), f)) > 0;) {
		text.append(buf, n);
	}
	std::fclose(f);
	int nameCol = -1, idCol = -1;
	size_t pos = 0;
	for (int row = 0; pos < text.size(); ++row) {
		size_t end = text.find('\n', pos);
		if (end == std::string::npos) {
			end = text.size();
		}
		std::string line = text.substr(pos, end - pos);
		pos = end + 1;
		if (!line.empty() && line.back() == '\r') {
			line.pop_back();
		}
		std::vector<std::string> cols;
		for (size_t s = 0;;) {
			const size_t t = line.find('\t', s);
			cols.push_back(line.substr(s, t == std::string::npos ? std::string::npos : t - s));
			if (t == std::string::npos) {
				break;
			}
			s = t + 1;
		}
		if (row == 0) {
			for (int i = 0; i < static_cast<int>(cols.size()); ++i) {
				if (cols[i] == "skill") nameCol = i;
				if (cols[i] == "*Id" || cols[i] == "Id") idCol = i;
			}
			if (nameCol < 0 || idCol < 0) {
				return;
			}
			continue;
		}
		if (static_cast<int>(cols.size()) > std::max(nameCol, idCol) && !cols[idCol].empty()) {
			g_modNames.emplace_back(Lower(cols[nameCol]), std::atoi(cols[idCol].c_str()));
		}
	}
	plog::Info("config: %zu skill names from the mod's skills.txt", g_modNames.size());
}

auto ResolveSkill(const std::string& key) -> int32_t {
	if (!key.empty() && std::all_of(key.begin(), key.end(), [](char c) { return std::isdigit(static_cast<unsigned char>(c)) != 0; })) {
		return std::atoi(key.c_str());
	}
	const std::string k = Lower(key);
	for (const auto& [name, id] : g_modNames) {
		if (name == k) return id;
	}
	if (g_modNames.empty()) {
		for (const Named& n : VanillaAuras) {
			if (Lower(n.name) == k) return n.id;
		}
	}
	return -1;
}

// --- Minimal TOML ---------------------------------------------------------------------------------------------------
struct Value {
	enum Kind { Str, Num, Bool } kind = Str;
	std::string s;
	double n = 0;
	bool b = false;
};
struct Entry {
	std::vector<std::string> path;  // full dotted path: defaults.drain.resource
	Value value;
	int line;
};

struct Parser {
	const std::string& t;
	size_t p = 0;
	int line = 1;
	std::string error;

	void SkipSpace() {
		while (p < t.size() && (t[p] == ' ' || t[p] == '\t')) ++p;
	}
	void SkipSpaceLines() {
		for (;;) {
			SkipSpace();
			if (p < t.size() && t[p] == '#') {
				while (p < t.size() && t[p] != '\n') ++p;
			}
			if (p < t.size() && (t[p] == '\n' || t[p] == '\r')) {
				if (t[p] == '\n') ++line;
				++p;
				continue;
			}
			return;
		}
	}
	auto Key(std::string& out) -> bool {
		SkipSpace();
		out.clear();
		if (p < t.size() && (t[p] == '"' || t[p] == '\'')) {
			const char q = t[p++];
			while (p < t.size() && t[p] != q && t[p] != '\n') out += t[p++];
			if (p >= t.size() || t[p] != q) return Fail("unterminated quoted key");
			++p;
			return true;
		}
		while (p < t.size() && (std::isalnum(static_cast<unsigned char>(t[p])) || t[p] == '_' || t[p] == '-')) out += t[p++];
		return !out.empty() || Fail("expected a key");
	}
	auto DottedKey(std::vector<std::string>& out) -> bool {
		out.clear();
		for (;;) {
			std::string k;
			if (!Key(k)) return false;
			out.push_back(k);
			SkipSpace();
			if (p < t.size() && t[p] == '.') {
				++p;
				continue;
			}
			return true;
		}
	}
	auto Fail(const char* what) -> bool {
		if (error.empty()) error = "line " + std::to_string(line) + ": " + what;
		return false;
	}
	auto ParseValue(std::vector<std::string>& path, std::vector<Entry>& out) -> bool {
		SkipSpace();
		if (p >= t.size()) return Fail("missing value");
		const char c = t[p];
		if (c == '{') {
			++p;
			for (;;) {
				SkipSpace();
				if (p < t.size() && t[p] == '}') {
					++p;
					return true;
				}
				std::vector<std::string> sub;
				if (!DottedKey(sub)) return false;
				SkipSpace();
				if (p >= t.size() || t[p] != '=') return Fail("expected '=' in an inline table");
				++p;
				std::vector<std::string> full = path;
				full.insert(full.end(), sub.begin(), sub.end());
				if (!ParseValue(full, out)) return false;
				SkipSpace();
				if (p < t.size() && t[p] == ',') ++p;
			}
		}
		Value v;
		if (c == '"' || c == '\'') {
			++p;
			while (p < t.size() && t[p] != c && t[p] != '\n') v.s += t[p++];
			if (p >= t.size() || t[p] != c) return Fail("unterminated string");
			++p;
		} else if (t.compare(p, 4, "true") == 0) {
			v.kind = Value::Bool, v.b = true, p += 4;
		} else if (t.compare(p, 5, "false") == 0) {
			v.kind = Value::Bool, v.b = false, p += 5;
		} else {
			char* end = nullptr;
			v.kind = Value::Num;
			v.n = std::strtod(t.c_str() + p, &end);
			if (end == t.c_str() + p) return Fail("expected a string, number or true/false");
			p = static_cast<size_t>(end - t.c_str());
		}
		out.push_back({path, v, line});
		return true;
	}
	auto Parse(std::vector<Entry>& out) -> bool {
		std::vector<std::string> table;
		for (;;) {
			SkipSpaceLines();
			if (p >= t.size()) return true;
			if (t[p] == '[') {
				++p;
				if (!DottedKey(table)) return false;
				SkipSpace();
				if (p >= t.size() || t[p] != ']') return Fail("expected ']'");
				++p;
				continue;
			}
			std::vector<std::string> key;
			if (!DottedKey(key)) return false;
			SkipSpace();
			if (p >= t.size() || t[p] != '=') return Fail("expected '='");
			++p;
			std::vector<std::string> full = table;
			full.insert(full.end(), key.begin(), key.end());
			if (!ParseValue(full, out)) return false;
			SkipSpace();
			if (p < t.size() && t[p] == '#') continue;
			if (p < t.size() && t[p] != '\n' && t[p] != '\r') return Fail("unexpected text after the value");
		}
	}
};

auto ParseResource(const std::string& s, Resource& r) -> bool {
	const std::string l = Lower(s);
	if (l == "mana") r = Resource::Mana;
	else if (l == "life") r = Resource::Life;
	else if (l == "none" || l.empty()) r = Resource::None;
	else return false;
	return true;
}

// Applies one setting (path after the table name, e.g. {"drain","perSecond"}) to a skill config.
auto Apply(SkillConfig& c, const std::vector<std::string>& field, const Value& v, std::string& error) -> bool {
	auto want = [&](Value::Kind k, const char* what) {
		if (v.kind != k) error = std::string("expects ") + what;
		return v.kind == k;
	};
	const std::string f0 = field.empty() ? "" : field[0];
	const std::string f1 = field.size() > 1 ? field[1] : "";
	if (field.size() == 1 && f0 == "enabled") return want(Value::Bool, "true/false") && ((c.enabled = v.b), true);
	if (field.size() == 1 && f0 == "toggle") return want(Value::Bool, "true/false") && ((c.toggle = v.b), true);
	if (field.size() == 1 && f0 == "trigger") {
		if (!want(Value::Str, "\"select\" or \"cast\"")) return false;
		const std::string l = Lower(v.s);
		if (l == "select") c.trigger = Trigger::Select;
		else if (l == "cast") c.trigger = Trigger::Cast;
		else return (error = "expects \"select\" or \"cast\""), false;
		return true;
	}
	if (field.size() == 2 && f0 == "drain") {
		if (f1 == "resource") return want(Value::Str, "\"mana\" or \"life\"") && (ParseResource(v.s, c.drainResource) || ((error = "expects \"mana\" or \"life\""), false));
		if (f1 == "perSecond") return want(Value::Num, "a number") && (v.n >= 0 || ((error = "must be >= 0"), false)) && ((c.drainPerSecond = v.n), true);
	}
	if (field.size() == 2 && f0 == "reserve") {
		if (f1 == "resource") return want(Value::Str, "\"mana\" or \"life\"") && (ParseResource(v.s, c.reserveResource) || ((error = "expects \"mana\" or \"life\""), false));
		if (f1 == "mode") {
			if (!want(Value::Str, "\"percent\" or \"flat\"")) return false;
			const std::string l = Lower(v.s);
			if (l == "percent") c.reserveMode = ReserveMode::Percent;
			else if (l == "flat") c.reserveMode = ReserveMode::Flat;
			else return (error = "expects \"percent\" or \"flat\""), false;
			return true;
		}
		if (f1 == "value") return want(Value::Num, "a number") && (v.n >= 0 || ((error = "must be >= 0"), false)) && ((c.reserveValue = v.n), true);
	}
	error = "unknown setting";
	return false;
}

auto Join(const std::vector<std::string>& v, size_t from) -> std::string {
	std::string s;
	for (size_t i = from; i < v.size(); ++i) s += (i > from ? "." : "") + v[i];
	return s;
}

}  // namespace

auto Load(const wchar_t* gameDir, const char* mod, char* report, size_t reportSize) noexcept -> bool {
	g_skills.clear();
	HMODULE self = nullptr;
	wchar_t path[MAX_PATH] = L"";
	if (!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
	                        reinterpret_cast<LPCWSTR>(&Load), &self) ||
	    GetModuleFileNameW(self, path, MAX_PATH) == 0) {
		std::snprintf(report, reportSize, "off: cannot find this DLL's folder");
		return false;
	}
	wchar_t* dot = wcsrchr(path, L'.');
	if (dot == nullptr) {
		std::snprintf(report, reportSize, "off: odd DLL path");
		return false;
	}
	wcscpy_s(dot, MAX_PATH - static_cast<size_t>(dot - path), L".toml");
	FILE* f = nullptr;
	if (_wfopen_s(&f, path, L"rb") != 0 || f == nullptr) {
		std::snprintf(report, reportSize, "off: no %ls (no skill is managed)", path);
		return false;
	}
	std::string text;
	char buf[16384];
	for (size_t n; (n = std::fread(buf, 1, sizeof(buf), f)) > 0;) {
		text.append(buf, n);
	}
	std::fclose(f);
	if (text.size() >= 3 && static_cast<unsigned char>(text[0]) == 0xef) {
		text.erase(0, 3);  // UTF-8 BOM
	}

	std::vector<Entry> entries;
	Parser parser {text};
	if (!parser.Parse(entries)) {
		std::snprintf(report, reportSize, "off: %ls %s", path, parser.error.c_str());
		return false;
	}
	ReadModSkillNames(gameDir, mod);

	SkillConfig defaults;
	int problems = 0;
	for (const Entry& e : entries) {
		if (e.path.size() >= 2 && e.path[0] == "defaults") {
			std::string err;
			if (!Apply(defaults, std::vector<std::string>(e.path.begin() + 1, e.path.end()), e.value, err)) {
				plog::Warn("config line %d: defaults.%s %s", e.line, Join(e.path, 1).c_str(), err.c_str());
				++problems;
			}
		}
	}
	for (const Entry& e : entries) {
		if (e.path.size() < 3 || e.path[0] != "skills") {
			if (e.path.empty() || e.path[0] != "defaults") {
				plog::Warn("config line %d: %s ignored (settings go under [defaults] or [skills.<name>])", e.line, Join(e.path, 0).c_str());
				++problems;
			}
			continue;
		}
		SkillConfig* c = nullptr;
		for (SkillConfig& s : g_skills) {
			if (e.path[1] == s.key) c = &s;
		}
		if (c == nullptr) {
			const int32_t id = ResolveSkill(e.path[1]);
			if (id < 0) {
				plog::Warn("config line %d: unknown skill \"%s\" (use its skills.txt name or id)", e.line, e.path[1].c_str());
				++problems;
				continue;
			}
			g_skills.push_back(defaults);
			c = &g_skills.back();
			c->skill = id;
			strncpy_s(c->key, e.path[1].c_str(), _TRUNCATE);
		}
		std::string err;
		if (!Apply(*c, std::vector<std::string>(e.path.begin() + 2, e.path.end()), e.value, err)) {
			plog::Warn("config line %d: skills.%s %s", e.line, Join(e.path, 1).c_str(), err.c_str());
			++problems;
		}
	}
	// a skill listed only as a table header with nothing under it gets no entry: say so
	for (const SkillConfig& c : g_skills) {
		plog::Info("config: %s (id %d) %s%s, drain %s %.2f/s, reserve %s %.2f%s", c.key, c.skill, c.enabled ? "on" : "OFF",
		           c.toggle ? (c.trigger == Trigger::Cast ? " toggle-on-cast" : " toggle-on-select") : " vanilla-select",
		           ResourceName(c.drainResource), c.drainPerSecond, ResourceName(c.reserveResource), c.reserveValue,
		           c.reserveMode == ReserveMode::Percent ? "%" : " flat");
	}
	std::snprintf(report, reportSize, "on: %zu skill(s) from %ls%s", g_skills.size(), path,
	              problems > 0 ? " (with warnings: see the log)" : "");
	return true;
}

auto Find(int32_t skill) noexcept -> const SkillConfig* {
	for (const SkillConfig& c : g_skills) {
		if (c.skill == skill) return c.enabled ? &c : nullptr;
	}
	return nullptr;
}

auto Count() noexcept -> int {
	return static_cast<int>(g_skills.size());
}

auto At(int i) noexcept -> const SkillConfig& {
	return g_skills[static_cast<size_t>(i)];
}

auto ResourceName(Resource r) noexcept -> const char* {
	return r == Resource::Mana ? "mana" : r == Resource::Life ? "life" : "none";
}

}  // namespace config
