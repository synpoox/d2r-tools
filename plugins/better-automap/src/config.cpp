// TOML subset reader (config.h).
#include "config.h"

#define NOMINMAX
#include <windows.h>

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace config {

namespace {

Settings g_settings;
char g_source[600] = "defaults (no config read)";
char g_problems[600] = "";

constexpr size_t MaxFile = 64 * 1024;
char g_text[MaxFile + 1];

void Problem(const char* fmt, const char* a, const char* b = "") noexcept {
	char line[200];
	std::snprintf(line, sizeof(line), fmt, a, b);
	const size_t used = std::strlen(g_problems);
	std::snprintf(g_problems + used, sizeof(g_problems) - used, "%s%s", used != 0 ? "; " : "", line);
}

// <dir of this DLL>\better-automap.toml
auto ReadBesideDll(char* path, size_t pathSize) noexcept -> bool {
	HMODULE self = nullptr;
	if (!GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
	                        reinterpret_cast<LPCSTR>(&ReadBesideDll), &self) ||
	    GetModuleFileNameA(self, path, static_cast<DWORD>(pathSize)) == 0) {
		return false;
	}
	char* slash = std::strrchr(path, '\\');
	if (slash == nullptr || static_cast<size_t>(slash - path) + 20 >= pathSize) {
		return false;
	}
	strcpy_s(slash + 1, pathSize - static_cast<size_t>(slash + 1 - path), "better-automap.toml");
	FILE* f = nullptr;
	if (fopen_s(&f, path, "rb") != 0 || f == nullptr) {
		return false;
	}
	const size_t n = std::fread(g_text, 1, MaxFile, f);
	g_text[n] = '\0';
	std::fclose(f);
	return true;
}

auto Trim(char* s) noexcept -> char* {
	while (*s == ' ' || *s == '\t') {
		++s;
	}
	char* end = s + std::strlen(s);
	while (end > s && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r')) {
		*--end = '\0';
	}
	return s;
}

// Cuts a # comment that is not inside a "string".
void StripComment(char* s) noexcept {
	bool quoted = false;
	for (; *s != '\0'; ++s) {
		if (*s == '"') {
			quoted = !quoted;
		} else if (*s == '#' && !quoted) {
			*s = '\0';
			return;
		}
	}
}

auto ParseBool(const char* v, bool* out) noexcept -> bool {
	if (std::strcmp(v, "true") == 0) {
		*out = true;
		return true;
	}
	if (std::strcmp(v, "false") == 0) {
		*out = false;
		return true;
	}
	return false;
}

auto ParseFloat(const char* v, float* out) noexcept -> bool {
	char* end = nullptr;
	const double d = std::strtod(v, &end);
	if (end == v || *end != '\0') {
		return false;
	}
	*out = static_cast<float>(d);
	return true;
}

// "text" -> text (lower case)
auto ParseString(char* v, char** out) noexcept -> bool {
	const size_t len = std::strlen(v);
	if (len < 2 || v[0] != '"' || v[len - 1] != '"') {
		return false;
	}
	v[len - 1] = '\0';
	for (char* c = v + 1; *c != '\0'; ++c) {
		*c = static_cast<char>(std::tolower(static_cast<unsigned char>(*c)));
	}
	*out = v + 1;
	return true;
}

void Apply(const char* table, const char* key, char* value) noexcept {
	Settings& s = g_settings;
	char full[96];
	std::snprintf(full, sizeof(full), "%s.%s", table, key);
	char shown[96];  // the value as written, for the problem text (parsing a string edits it)
	std::snprintf(shown, sizeof(shown), "%s", value);
	bool b = false;
	float f = 0.0f;
	char* str = nullptr;
	if (std::strcmp(full, "minimap.enabled") == 0) {
		if (ParseBool(value, &b)) {
			s.minimapEnabled = b;
			return;
		}
	} else if (std::strcmp(full, "minimap.position") == 0) {
		if (ParseString(value, &str)) {
			if (std::strcmp(str, "right") == 0 || std::strcmp(str, "left") == 0) {
				s.minimapLeft = std::strcmp(str, "left") == 0;
				return;
			}
		}
	} else if (std::strcmp(full, "overlay.enabled") == 0) {
		if (ParseBool(value, &b)) {
			s.overlayEnabled = b;
			return;
		}
	} else if (std::strcmp(full, "zoom.enabled") == 0) {
		if (ParseBool(value, &b)) {
			s.zoomEnabled = b;
			return;
		}
	} else if (std::strcmp(full, "zoom.min") == 0 || std::strcmp(full, "zoom.max") == 0 ||
	           std::strcmp(full, "zoom.step") == 0) {
		if (ParseFloat(value, &f)) {
			(key[1] == 'i' ? s.zoomMin : key[1] == 'a' ? s.zoomMax : s.zoomStep) = f;
			return;
		}
	} else {
		return;  // unknown key: ignored (also the reserved [d2rl] table)
	}
	Problem("bad value for %s: %s (default kept)", full, shown);
}

void Parse() noexcept {
	char table[64] = "";
	char* line = g_text;
	while (line != nullptr && *line != '\0') {
		char* next = std::strchr(line, '\n');
		if (next != nullptr) {
			*next++ = '\0';
		}
		StripComment(line);
		char* t = Trim(line);
		if (*t == '[') {
			char* close = std::strchr(t, ']');
			if (close != nullptr) {
				*close = '\0';
				strncpy_s(table, sizeof(table), Trim(t + 1), _TRUNCATE);
			}
		} else if (*t != '\0') {
			char* eq = std::strchr(t, '=');
			if (eq != nullptr) {
				*eq = '\0';
				Apply(table, Trim(t), Trim(eq + 1));
			}
		}
		line = next;
	}
}

// Keeps the zoom range usable whatever the file says.
void Validate() noexcept {
	Settings& s = g_settings;
	const Settings d;
	if (!(s.zoomMin > 0.0f) || !(s.zoomMin <= 1.0f)) {
		Problem("zoom.min must be above 0 and at most 1 (%s%s used)", "default ", "0.5");
		s.zoomMin = d.zoomMin;
	}
	if (!(s.zoomMax >= 1.0f) || !(s.zoomMax <= 8.0f)) {
		Problem("zoom.max must be 1 to 8 (%s%s used)", "default ", "2.5");
		s.zoomMax = d.zoomMax;
	}
	if (!(s.zoomStep > 0.0f) || !(s.zoomStep <= s.zoomMax - s.zoomMin)) {
		Problem("zoom.step must be above 0 and fit between min and max (%s%s used)", "default ", "0.25");
		s.zoomStep = d.zoomStep;
	}
}

}  // namespace

void Load(const D2RL::PluginContext* context) noexcept {
	g_settings = Settings {};
	g_problems[0] = '\0';
	char path[MAX_PATH] = "";
	bool read = ReadBesideDll(path, sizeof(path));
	if (read) {
		std::snprintf(g_source, sizeof(g_source), "%s", path);
	} else if (context != nullptr) {
		uint32_t required = 0;
		std::memset(g_text, 0, sizeof(g_text));
		read = context->ReadConfig(g_text, static_cast<uint32_t>(MaxFile), &required);
		if (read) {
			g_text[required < MaxFile ? required : MaxFile] = '\0';
			std::snprintf(g_source, sizeof(g_source), "%ls", context->pluginConfigPath != nullptr ? context->pluginConfigPath
			                                                                                      : L"D2RLoader config");
		}
	}
	if (!read) {
		std::snprintf(g_source, sizeof(g_source), "defaults (no better-automap.toml found)");
		return;
	}
	Parse();
	Validate();
}

auto Get() noexcept -> const Settings& {
	return g_settings;
}

auto Source() noexcept -> const char* {
	return g_source;
}

auto Problems() noexcept -> const char* {
	return g_problems;
}

}  // namespace config
