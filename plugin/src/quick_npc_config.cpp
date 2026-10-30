// Quick NPC settings (quick_npc_config.h).
#include "quick_npc_config.h"

#include <cstdio>
#include <cstring>

namespace quick_npc {

namespace {

auto Lower(char c) noexcept -> char {
	return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
}

auto IsSpace(char c) noexcept -> bool {
	return c == ' ' || c == '\t' || c == '\r';
}

// "Left Ctrl" -> "leftctrl"
void Normalize(const char* in, char* out, size_t size) noexcept {
	size_t n = 0;
	for (; *in != '\0' && n + 1 < size; ++in) {
		if (*in != ' ' && *in != '-' && *in != '_') {
			out[n++] = Lower(*in);
		}
	}
	out[n] = '\0';
}

struct Named {
	const char* name;
	uint32_t vk;
};

constexpr Named Names[] = {
	{"leftctrl", 0xA2},   {"lctrl", 0xA2},     {"leftcontrol", 0xA2}, {"lcontrol", 0xA2},
	{"rightctrl", 0xA3},  {"rctrl", 0xA3},     {"rightcontrol", 0xA3}, {"rcontrol", 0xA3},
	{"ctrl", 0x11},       {"control", 0x11},
	{"leftshift", 0xA0},  {"lshift", 0xA0},    {"rightshift", 0xA1},  {"rshift", 0xA1},  {"shift", 0x10},
	{"leftalt", 0xA4},    {"lalt", 0xA4},      {"rightalt", 0xA5},    {"ralt", 0xA5},    {"altgr", 0xA5},
	{"alt", 0x12},
	{"space", 0x20},      {"tab", 0x09},       {"capslock", 0x14},
	{"backquote", 0xC0},  {"grave", 0xC0},     {"tilde", 0xC0},
	{"mouse3", 0x04},     {"middlemouse", 0x04}, {"mbutton", 0x04},
	{"mouse4", 0x05},     {"xbutton1", 0x05},  {"mouse5", 0x06},      {"xbutton2", 0x06},
};

auto ParseUnsigned(const char* s, uint32_t base, uint32_t* out) noexcept -> bool {
	if (*s == '\0') {
		return false;
	}
	uint64_t v = 0;
	for (; *s != '\0'; ++s) {
		uint32_t d = 0;
		if (*s >= '0' && *s <= '9') {
			d = static_cast<uint32_t>(*s - '0');
		} else if (base == 16 && Lower(*s) >= 'a' && Lower(*s) <= 'f') {
			d = static_cast<uint32_t>(Lower(*s) - 'a' + 10);
		} else {
			return false;
		}
		v = v * base + d;
		if (v > 0xFFFFFFFFu) {
			return false;
		}
	}
	*out = static_cast<uint32_t>(v);
	return true;
}

class Issues {
public:
	Issues(ConfigIssue* list, int max) noexcept : m_list(list), m_max(max) {}

	void Add(int line, const char* fmt, const char* a = "", const char* b = "") noexcept {
		if (m_list != nullptr && m_count < m_max) {
			m_list[m_count].line = line;
			std::snprintf(m_list[m_count].text, sizeof(m_list[m_count].text), fmt, a, b);
		}
		++m_count;
	}

	auto count() const noexcept -> int { return m_count; }

private:
	ConfigIssue* m_list;
	int m_max;
	int m_count = 0;
};

enum class ValueKind { Bool, Integer, String, Invalid };

struct Value {
	ValueKind kind = ValueKind::Invalid;
	bool b = false;
	int64_t i = 0;
	char s[64] = "";
};

// Parses the value part of a line (after '='), up to an unquoted '#'.
auto ParseValue(const char* p, const char* end, Value* v) noexcept -> bool {
	while (p < end && IsSpace(*p)) {
		++p;
	}
	if (p >= end) {
		return false;
	}
	const char* rest = nullptr;
	if (*p == '"' || *p == '\'') {
		const char quote = *p++;
		size_t n = 0;
		for (; p < end && *p != quote; ++p) {
			char c = *p;
			if (quote == '"' && c == '\\' && p + 1 < end) {
				++p;
				c = *p == 't' ? '\t' : *p;  // \" \\ \t; others keep the character
			}
			if (n + 1 >= sizeof(v->s)) {
				return false;  // longer than any setting needs
			}
			v->s[n++] = c;
		}
		if (p >= end) {
			return false;  // unterminated
		}
		v->s[n] = '\0';
		v->kind = ValueKind::String;
		rest = p + 1;
	} else {
		char word[32];
		size_t n = 0;
		for (; p < end && !IsSpace(*p) && *p != '#'; ++p) {
			if (n + 1 >= sizeof(word)) {
				return false;
			}
			word[n++] = *p;
		}
		word[n] = '\0';
		rest = p;
		if (std::strcmp(word, "true") == 0 || std::strcmp(word, "false") == 0) {
			v->kind = ValueKind::Bool;
			v->b = word[0] == 't';
		} else {
			const bool negative = word[0] == '-';
			const char* digits = (word[0] == '-' || word[0] == '+') ? word + 1 : word;
			char clean[32];
			size_t m = 0;
			for (const char* d = digits; *d != '\0'; ++d) {
				if (*d != '_') {
					clean[m++] = *d;
				}
			}
			clean[m] = '\0';
			uint32_t u = 0;
			if (!ParseUnsigned(clean, 10, &u)) {
				return false;
			}
			v->kind = ValueKind::Integer;
			v->i = negative ? -static_cast<int64_t>(u) : static_cast<int64_t>(u);
		}
	}
	while (rest < end && IsSpace(*rest)) {
		++rest;
	}
	return rest >= end || *rest == '#';  // only a comment may follow
}

auto TableName(const char* p, const char* end, char* out, size_t size) noexcept -> bool {
	++p;  // '['
	while (p < end && IsSpace(*p)) {
		++p;
	}
	const char* close = p;
	while (close < end && *close != ']') {
		++close;
	}
	if (close >= end) {
		return false;
	}
	const char* e = close;
	while (e > p && IsSpace(e[-1])) {
		--e;
	}
	if (e - p >= 2 && (*p == '"' || *p == '\'') && e[-1] == *p) {
		++p;
		--e;
	}
	const size_t n = static_cast<size_t>(e - p);
	if (n + 1 > size) {
		return false;
	}
	std::memcpy(out, p, n);
	out[n] = '\0';
	return true;
}

}  // namespace

auto KeyFromName(const char* name, uint32_t* vk) noexcept -> bool {
	if (name == nullptr || vk == nullptr) {
		return false;
	}
	char k[40];
	Normalize(name, k, sizeof(k));
	uint32_t v = 0;
	bool ok = false;
	if (k[0] == '0' && k[1] == 'x') {
		ok = ParseUnsigned(k + 2, 16, &v) && v >= 0x03 && v <= 0xFE;
	} else if (k[0] != '\0' && k[1] == '\0' && ((k[0] >= 'a' && k[0] <= 'z') || (k[0] >= '0' && k[0] <= '9'))) {
		v = (k[0] >= 'a' && k[0] <= 'z') ? static_cast<uint32_t>(k[0] - 'a' + 'A') : static_cast<uint32_t>(k[0]);
		ok = true;
	} else if (k[0] == 'f' && ParseUnsigned(k + 1, 10, &v) && v >= 1 && v <= 24) {
		v = 0x70 + v - 1;
		ok = true;
	} else if (std::strncmp(k, "numpad", 6) == 0 && k[6] >= '0' && k[6] <= '9' && k[7] == '\0') {
		v = 0x60 + static_cast<uint32_t>(k[6] - '0');
		ok = true;
	} else {
		for (const Named& n : Names) {
			if (std::strcmp(k, n.name) == 0) {
				v = n.vk;
				ok = true;
				break;
			}
		}
	}
	if (ok) {
		*vk = v;
	}
	return ok;
}

auto HotkeyBlocksInteraction(uint32_t vk) noexcept -> bool {
	return vk == 0x10 || vk == 0xA0 || vk == 0xA1;
}

auto HotkeyNote(uint32_t vk) noexcept -> const char* {
	if (HotkeyBlocksInteraction(vk)) {
		return "Shift is D2R's Stand Still key by default, and Stand Still stops NPC interaction: rebind Stand Still or "
		       "choose another hotkey";
	}
	if (vk == 0x11 || vk == 0xA2 || vk == 0xA3) {
		return "Ctrl is D2R's Run key by default: you run to the vendor";
	}
	if (vk == 0x12 || vk == 0xA4 || vk == 0xA5) {
		return "Alt is D2R's Show Items key by default: a click on an item label picks the item up instead";
	}
	return nullptr;
}

auto ParseConfig(const char* toml, size_t length, Config* out, ConfigIssue* issues, int maxIssues) noexcept -> int {
	Issues found(issues, maxIssues);
	if (out == nullptr) {
		return 0;
	}
	if (toml == nullptr || length == 0) {
		found.Add(0, "the config file is empty: defaults used");
		return found.count();
	}
	bool inTable = false;
	bool sawTable = false;
	int line = 0;
	const char* p = toml;
	const char* const fileEnd = toml + length;
	while (p < fileEnd) {
		++line;
		const char* eol = p;
		while (eol < fileEnd && *eol != '\n' && *eol != '\0') {
			++eol;
		}
		const char* s = p;
		const char* e = eol;
		p = eol < fileEnd ? eol + 1 : fileEnd;
		if (eol < fileEnd && *eol == '\0') {
			p = fileEnd;
		}
		if (line == 1 && e - s >= 3 && static_cast<unsigned char>(s[0]) == 0xEF && static_cast<unsigned char>(s[1]) == 0xBB &&
		    static_cast<unsigned char>(s[2]) == 0xBF) {
			s += 3;  // UTF-8 byte order mark
		}
		while (s < e && IsSpace(*s)) {
			++s;
		}
		if (s >= e || *s == '#') {
			continue;
		}
		if (*s == '[') {
			char name[64];
			if (s + 1 < e && s[1] == '[') {
				inTable = false;  // array of tables: not ours
				continue;
			}
			if (!TableName(s, e, name, sizeof(name))) {
				found.Add(line, "unreadable table header");
				inTable = false;
				continue;
			}
			inTable = std::strcmp(name, ConfigTable) == 0;
			sawTable = sawTable || inTable;
			continue;
		}
		const char* eq = s;
		while (eq < e && *eq != '=') {
			++eq;
		}
		if (eq >= e) {
			found.Add(line, "expected key = value");
			continue;
		}
		char key[40];
		const char* ke = eq;
		while (ke > s && IsSpace(ke[-1])) {
			--ke;
		}
		const size_t keyLen = static_cast<size_t>(ke - s);
		if (keyLen == 0 || keyLen + 1 > sizeof(key)) {
			found.Add(line, "unreadable key");
			continue;
		}
		std::memcpy(key, s, keyLen);
		key[keyLen] = '\0';
		if (!inTable) {
			if (!sawTable) {
				found.Add(line, "%s is outside [quick-npc] and is ignored", key);
			}
			continue;  // other tables (for example D2RLoader's [d2rl]) are not ours
		}
		Value v;
		if (!ParseValue(eq + 1, e, &v)) {
			found.Add(line, "%s: unreadable value", key);
			continue;
		}
		if (std::strcmp(key, "enabled") == 0) {
			if (v.kind != ValueKind::Bool) {
				found.Add(line, "enabled must be true or false");
			} else {
				out->enabled = v.b;
			}
		} else if (std::strcmp(key, "hotkey") == 0) {
			uint32_t vk = 0;
			if (v.kind != ValueKind::String) {
				found.Add(line, "hotkey must be a quoted key name, for example \"LeftCtrl\"");
			} else if (!KeyFromName(v.s, &vk)) {
				found.Add(line, "hotkey \"%s\" is not a key this plugin knows: kept \"%s\"", v.s, out->hotkeyName);
			} else {
				out->hotkeyVk = vk;
				std::snprintf(out->hotkeyName, sizeof(out->hotkeyName), "%s", v.s);
			}
		} else if (bool* flag = std::strcmp(key, "trade") == 0           ? &out->trade
		                        : std::strcmp(key, "cain_identify") == 0 ? &out->cainIdentify
		                        : std::strcmp(key, "mercenary") == 0     ? &out->mercenary
		                        : std::strcmp(key, "travel") == 0        ? &out->travel
		                                                                 : nullptr) {
			if (v.kind != ValueKind::Bool) {
				found.Add(line, "%s must be true or false", key);
			} else {
				*flag = v.b;
			}
		} else if (std::strcmp(key, "timeout_seconds") == 0) {
			if (v.kind != ValueKind::Integer || v.i < MinTimeoutSeconds || v.i > MaxTimeoutSeconds) {
				found.Add(line, "timeout_seconds must be a whole number from 5 to 600");
			} else {
				out->timeoutMs = static_cast<uint32_t>(v.i) * 1000;
			}
		} else {
			found.Add(line, "unknown setting %s", key);
		}
	}
	if (!sawTable) {
		found.Add(0, "no [quick-npc] table: defaults used");
	}
	return found.count();
}

}  // namespace quick_npc
