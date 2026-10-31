#include "spec_model.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>

namespace ms {

namespace {

// Appends printf output to a buffer; remembers overflow.
struct Out {
	char* p;
	size_t size;
	size_t len;
	bool full;

	void Add(const char* fmt, ...) noexcept;
};

void Out::Add(const char* fmt, ...) noexcept {
	if (full) {
		return;
	}
	va_list args;
	va_start(args, fmt);
	const int n = std::vsnprintf(p + len, size - len, fmt, args);
	va_end(args);
	if (n < 0 || static_cast<size_t>(n) >= size - len) {
		full = true;
		return;
	}
	len += static_cast<size_t>(n);
}

// A tiny line reader over a NUL-free span.
struct Reader {
	const char* p;
	const char* end;

	// Next line into `line` (without the newline). False at the end or when the line is too long.
	auto Line(char* line, size_t size) noexcept -> bool {
		if (p >= end) {
			return false;
		}
		size_t n = 0;
		while (p < end && *p != '\n') {
			if (n + 1 >= size) {
				return false;
			}
			line[n++] = *p++;
		}
		if (n > 0 && line[n - 1] == '\r') {
			--n;
		}
		line[n] = '\0';
		if (p < end) {
			++p;  // the '\n'
		}
		return true;
	}
};

// Reads a decimal integer at *s (optional '-'), skipping leading spaces. False when none.
auto Int(const char** s, int64_t* value) noexcept -> bool {
	const char* c = *s;
	while (*c == ' ') {
		++c;
	}
	bool neg = false;
	if (*c == '-') {
		neg = true;
		++c;
	}
	if (*c < '0' || *c > '9') {
		return false;
	}
	int64_t v = 0;
	while (*c >= '0' && *c <= '9') {
		v = v * 10 + (*c - '0');
		if (v > 0x7fffffff) {
			return false;
		}
		++c;
	}
	*value = neg ? -v : v;
	*s = c;
	return true;
}

auto Word(const char** s, const char* word) noexcept -> bool {
	const char* c = *s;
	while (*c == ' ') {
		++c;
	}
	const size_t n = std::strlen(word);
	if (std::strncmp(c, word, n) != 0 || (c[n] != ' ' && c[n] != '\0')) {
		return false;
	}
	*s = c + n;
	return true;
}

auto AtEnd(const char* s) noexcept -> bool {
	while (*s == ' ') {
		++s;
	}
	return *s == '\0';
}

void Fail(char* error, size_t size, const char* what) noexcept {
	if (error != nullptr && size > 0) {
		std::snprintf(error, size, "%s", what);
	}
}

auto StateFromWord(const char** s, SpecState* state) noexcept -> bool {
	if (Word(s, "ok")) {
		*state = SpecState::Ok;
	} else if (Word(s, "adjusted")) {
		*state = SpecState::Adjusted;
	} else if (Word(s, "invalid")) {
		*state = SpecState::Invalid;
	} else {
		return false;
	}
	return true;
}

void CopyName(char* dst, const char* src) noexcept {
	std::snprintf(dst, MaxName, "%s", src);
}

}  // namespace

auto Crc32(const void* data, size_t size) noexcept -> uint32_t {
	const auto* b = static_cast<const uint8_t*>(data);
	uint32_t crc = 0xffffffffU;
	for (size_t i = 0; i < size; ++i) {
		crc ^= b[i];
		for (int k = 0; k < 8; ++k) {
			crc = (crc >> 1) ^ (0xedb88320U & (0U - (crc & 1U)));
		}
	}
	return ~crc;
}

auto SumLevels(const uint8_t* levels, int32_t count) noexcept -> int32_t {
	int32_t sum = 0;
	for (int32_t i = 0; i < count; ++i) {
		sum += levels[i];
	}
	return sum;
}

auto SameLevels(const uint8_t* a, const uint8_t* b, int32_t count) noexcept -> bool {
	return std::memcmp(a, b, static_cast<size_t>(count)) == 0;
}

auto StateName(SpecState state) noexcept -> const char* {
	switch (state) {
	case SpecState::Ok: return "ok";
	case SpecState::Adjusted: return "adjusted";
	case SpecState::Invalid: return "invalid";
	}
	return "?";
}

auto TotalPoints(const LiveChar& live) noexcept -> int32_t {
	return SumLevels(live.levels, live.skillCount) + live.unspent;
}

auto Serialize(const CharSpecs& s, char* out, size_t size) noexcept -> size_t {
	if (out == nullptr || size < 64 || s.skillCount < 0 || s.skillCount > MaxSkills || s.specCount < 1 ||
	    s.specCount > MaxSpecs) {
		return 0;
	}
	Out o {out, size, 0, false};
	o.Add("multispec %d\n", FileVersion);
	o.Add("char %s\n", s.name);
	o.Add("class %d\n", s.charClass);
	o.Add("skills %d", s.skillCount);
	for (int32_t i = 0; i < s.skillCount; ++i) {
		o.Add(" %d", s.skillIds[i]);
	}
	o.Add("\nactive %d\n", s.active);
	for (int32_t k = 0; k < s.specCount; ++k) {
		o.Add("spec %d %s", k + 1, StateName(s.specs[k].state));
		for (int32_t i = 0; i < s.skillCount; ++i) {
			o.Add(" %u", static_cast<unsigned>(s.specs[k].levels[i]));
		}
		o.Add("\n");
	}
	if (s.pendingFrom > 0) {
		o.Add("pending %d %d\n", s.pendingFrom, s.pendingTo);
	} else {
		o.Add("pending none\n");
	}
	if (o.full) {
		return 0;
	}
	const uint32_t crc = Crc32(out, o.len);
	o.Add("crc %08x\n", crc);
	return o.full ? 0 : o.len;
}

auto Parse(const char* text, size_t size, CharSpecs* out, char* error, size_t errorSize) noexcept -> bool {
	if (text == nullptr || out == nullptr) {
		Fail(error, errorSize, "no data");
		return false;
	}
	if (size > MaxFileBytes) {
		Fail(error, errorSize, "file too large");
		return false;
	}
	if (std::memchr(text, '\0', size) != nullptr) {
		Fail(error, errorSize, "binary data in file");
		return false;
	}
	// CRC: the last line is "crc <hex>" over every byte before it.
	const char* crcLine = nullptr;
	for (size_t i = size; i > 0; --i) {
		if ((i == 1 || text[i - 2] == '\n') && size - (i - 1) >= 4 && std::strncmp(text + i - 1, "crc ", 4) == 0) {
			crcLine = text + i - 1;
			break;
		}
	}
	if (crcLine == nullptr) {
		Fail(error, errorSize, "no crc line");
		return false;
	}
	unsigned stored = 0;
	char crcText[24] = "";
	const size_t crcLen = static_cast<size_t>(text + size - crcLine);
	if (crcLen >= sizeof(crcText)) {
		Fail(error, errorSize, "bad crc line");
		return false;
	}
	std::memcpy(crcText, crcLine, crcLen);
	crcText[crcLen] = '\0';
	// "crc " then exactly 8 hex digits, then the end of the line
	size_t digits = 0;
	for (const char* h = crcText + 4; *h != '\0' && *h != '\n' && *h != '\r'; ++h, ++digits) {
		const char ch = *h;
		const int v = (ch >= '0' && ch <= '9') ? ch - '0' : (ch >= 'a' && ch <= 'f') ? ch - 'a' + 10 : -1;
		if (v < 0 || digits >= 8) {
			digits = 0;
			break;
		}
		stored = (stored << 4) | static_cast<unsigned>(v);
	}
	if (digits != 8) {
		Fail(error, errorSize, "bad crc line");
		return false;
	}
	if (Crc32(text, static_cast<size_t>(crcLine - text)) != stored) {
		Fail(error, errorSize, "crc mismatch");
		return false;
	}

	CharSpecs s {};
	Reader r {text, crcLine};
	char line[2048];
	const char* c = nullptr;
	int64_t v = 0;

	if (!r.Line(line, sizeof(line)) || (c = line, !Word(&c, "multispec")) || !Int(&c, &v) || !AtEnd(c)) {
		Fail(error, errorSize, "not a multispec file");
		return false;
	}
	if (v != FileVersion) {
		Fail(error, errorSize, "unknown file version");
		return false;
	}
	if (!r.Line(line, sizeof(line)) || std::strncmp(line, "char ", 5) != 0 || !ValidName(line + 5)) {
		Fail(error, errorSize, "bad char line");
		return false;
	}
	CopyName(s.name, line + 5);
	if (!r.Line(line, sizeof(line)) || (c = line, !Word(&c, "class")) || !Int(&c, &v) || !AtEnd(c) || v < 0 || v > 255) {
		Fail(error, errorSize, "bad class line");
		return false;
	}
	s.charClass = static_cast<int32_t>(v);
	if (!r.Line(line, sizeof(line)) || (c = line, !Word(&c, "skills")) || !Int(&c, &v) || v < 1 || v > MaxSkills) {
		Fail(error, errorSize, "bad skills line");
		return false;
	}
	s.skillCount = static_cast<int32_t>(v);
	for (int32_t i = 0; i < s.skillCount; ++i) {
		if (!Int(&c, &v) || v < 0 || v > 32767) {
			Fail(error, errorSize, "bad skill id");
			return false;
		}
		s.skillIds[i] = static_cast<int16_t>(v);
	}
	if (!AtEnd(c)) {
		Fail(error, errorSize, "extra skill ids");
		return false;
	}
	if (!r.Line(line, sizeof(line)) || (c = line, !Word(&c, "active")) || !Int(&c, &v) || !AtEnd(c)) {
		Fail(error, errorSize, "bad active line");
		return false;
	}
	s.active = static_cast<int32_t>(v);
	for (;;) {
		if (!r.Line(line, sizeof(line))) {
			Fail(error, errorSize, "missing pending line");
			return false;
		}
		c = line;
		if (Word(&c, "pending")) {
			if (Word(&c, "none")) {
				s.pendingFrom = s.pendingTo = 0;
			} else {
				int64_t a = 0;
				int64_t b = 0;
				if (!Int(&c, &a) || !Int(&c, &b)) {
					Fail(error, errorSize, "bad pending line");
					return false;
				}
				s.pendingFrom = static_cast<int32_t>(a);
				s.pendingTo = static_cast<int32_t>(b);
			}
			if (!AtEnd(c)) {
				Fail(error, errorSize, "bad pending line");
				return false;
			}
			break;
		}
		if (!Word(&c, "spec") || !Int(&c, &v) || v != s.specCount + 1 || s.specCount >= MaxSpecs) {
			Fail(error, errorSize, "bad spec line");
			return false;
		}
		Spec& spec = s.specs[s.specCount];
		if (!StateFromWord(&c, &spec.state)) {
			Fail(error, errorSize, "bad spec state");
			return false;
		}
		for (int32_t i = 0; i < s.skillCount; ++i) {
			if (!Int(&c, &v) || v < 0 || v > 255) {
				Fail(error, errorSize, "bad spec level");
				return false;
			}
			spec.levels[i] = static_cast<uint8_t>(v);
		}
		if (!AtEnd(c)) {
			Fail(error, errorSize, "extra spec levels");
			return false;
		}
		++s.specCount;
	}
	if (r.Line(line, sizeof(line))) {
		Fail(error, errorSize, "lines after pending");
		return false;
	}
	if (s.specCount < 1 || s.active < 1 || s.active > s.specCount) {
		Fail(error, errorSize, "bad active spec");
		return false;
	}
	if (s.pendingFrom != 0 && (s.pendingFrom < 1 || s.pendingFrom > s.specCount || s.pendingTo < 1 ||
	                           s.pendingTo > s.specCount)) {
		Fail(error, errorSize, "bad pending spec");
		return false;
	}
	*out = s;
	return true;
}

auto ConfigSpecCount(const char* toml, int32_t fallback) noexcept -> int32_t {
	if (toml == nullptr) {
		return fallback;
	}
	// Only the [multispec] table counts.
	bool inTable = false;
	for (const char* line = toml; *line != '\0';) {
		const char* end = std::strchr(line, '\n');
		const size_t len = end != nullptr ? static_cast<size_t>(end - line) : std::strlen(line);
		char buf[256];
		const size_t n = len < sizeof(buf) - 1 ? len : sizeof(buf) - 1;
		std::memcpy(buf, line, n);
		buf[n] = '\0';
		char* hash = std::strchr(buf, '#');
		if (hash != nullptr) {
			*hash = '\0';
		}
		const char* c = buf;
		while (*c == ' ' || *c == '\t') {
			++c;
		}
		if (*c == '[') {
			inTable = std::strncmp(c, "[multispec]", 11) == 0;
		} else if (inTable && std::strncmp(c, "spec_count", 10) == 0) {
			c += 10;
			while (*c == ' ' || *c == '\t') {
				++c;
			}
			if (*c == '=') {
				++c;
				int64_t v = 0;
				while (*c == '\t') {
					++c;
				}
				if (Int(&c, &v) && v >= 1 && v <= MaxSpecs) {
					return static_cast<int32_t>(v);
				}
				return fallback;
			}
		}
		if (end == nullptr) {
			break;
		}
		line = end + 1;
	}
	return fallback;
}

auto ModinfoSavePath(const char* json, char* out, size_t size) noexcept -> bool {
	if (out == nullptr || size == 0) {
		return false;
	}
	out[0] = '\0';
	if (json == nullptr) {
		return true;
	}
	const char* key = std::strstr(json, "\"savepath\"");
	if (key == nullptr) {
		return true;
	}
	const char* c = key + 10;
	while (*c == ' ' || *c == '\t' || *c == '\r' || *c == '\n') {
		++c;
	}
	if (*c != ':') {
		return true;
	}
	++c;
	while (*c == ' ' || *c == '\t' || *c == '\r' || *c == '\n') {
		++c;
	}
	if (*c != '"') {
		return true;
	}
	++c;
	size_t n = 0;
	for (; *c != '\0' && *c != '"'; ++c) {
		char ch = *c;
		if (ch == '\\' && c[1] != '\0') {
			++c;
			ch = *c;
		}
		if (n + 1 >= size) {
			out[0] = '\0';
			return false;
		}
		out[n++] = ch;
	}
	out[n] = '\0';
	return true;
}

auto ValidName(const char* name) noexcept -> bool {
	if (name == nullptr) {
		return false;
	}
	const size_t n = std::strlen(name);
	if (n < 2 || n > 15) {
		return false;
	}
	for (size_t i = 0; i < n; ++i) {
		const char c = name[i];
		const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-' || c == '_';
		if (!ok) {
			return false;
		}
	}
	return true;
}

auto ReconciledName(Reconciled r) noexcept -> const char* {
	switch (r) {
	case Reconciled::Created: return "created";
	case Reconciled::Recreated: return "recreated (file was for another character)";
	case Reconciled::Unchanged: return "unchanged";
	case Reconciled::ActiveFromMatch: return "active spec taken from the saved character";
	case Reconciled::AdoptedLive: return "active spec updated from the character";
	case Reconciled::Recovery: return "RECOVERY: the last swap did not finish; load a spec with spec N";
	}
	return "?";
}

auto Reconcile(CharSpecs* file, bool haveFile, const LiveChar& live, int32_t configCount) noexcept -> ReconcileReport {
	ReconcileReport rep {Reconciled::Unchanged, false, 0, 0};
	if (configCount < 1) {
		configCount = 1;
	}
	if (configCount > MaxSpecs) {
		configCount = MaxSpecs;
	}
	const int32_t n = live.skillCount;

	const bool other = haveFile && (std::strcmp(file->name, live.name) != 0 || file->charClass != live.charClass);
	if (!haveFile || other) {
		CharSpecs fresh {};
		CopyName(fresh.name, live.name);
		fresh.charClass = live.charClass;
		fresh.skillCount = n;
		std::memcpy(fresh.skillIds, live.skillIds, sizeof(int16_t) * static_cast<size_t>(n));
		fresh.specCount = configCount;
		fresh.active = 1;
		std::memcpy(fresh.specs[0].levels, live.levels, static_cast<size_t>(n));  // spec 1 = live; others stay empty
		*file = fresh;
		rep.result = other ? Reconciled::Recreated : Reconciled::Created;
		rep.changed = true;
		return rep;
	}

	// Mod data changed the class skill list: remap every spec by skill id; points of skills that are gone are freed.
	bool sameIds = file->skillCount == n &&
	               std::memcmp(file->skillIds, live.skillIds, sizeof(int16_t) * static_cast<size_t>(n)) == 0;
	if (!sameIds) {
		for (int32_t k = 0; k < file->specCount; ++k) {
			uint8_t remapped[MaxSkills] = {};
			bool lost = false;
			for (int32_t j = 0; j < file->skillCount; ++j) {
				int32_t at = -1;
				for (int32_t i = 0; i < n; ++i) {
					if (live.skillIds[i] == file->skillIds[j]) {
						at = i;
						break;
					}
				}
				if (at >= 0) {
					remapped[at] = file->specs[k].levels[j];
				} else if (file->specs[k].levels[j] != 0) {
					lost = true;
				}
			}
			std::memcpy(file->specs[k].levels, remapped, sizeof(remapped));
			if (lost && file->specs[k].state == SpecState::Ok) {
				file->specs[k].state = SpecState::Adjusted;
			}
		}
		for (int32_t j = 0; j < file->skillCount || j < n; ++j) {
			if (j >= file->skillCount || j >= n || file->skillIds[j] != live.skillIds[j]) {
				++rep.remapped;
			}
		}
		file->skillCount = n;
		std::memcpy(file->skillIds, live.skillIds, sizeof(int16_t) * static_cast<size_t>(n));
		rep.changed = true;
	}

	// More specs configured than stored: add empty ones. Fewer: the extra ones are kept in the file.
	while (file->specCount < configCount) {
		Spec& add = file->specs[file->specCount++];
		std::memset(&add, 0, sizeof(add));
		rep.changed = true;
	}

	// Which stored spec is the live character? The active one first, then the swap's two ends, then any. "Any" reaches
	// above spec_count, so an unsaved swap back into range overwrites no spec. A character with no points never matches
	// "any": it was respecced, and an empty spec is not where it came from.
	int32_t match = 0;
	if (SameLevels(file->specs[file->active - 1].levels, live.levels, n)) {
		match = file->active;
	}
	if (match == 0 && file->pendingFrom > 0) {
		if (SameLevels(file->specs[file->pendingFrom - 1].levels, live.levels, n)) {
			match = file->pendingFrom;
		} else if (SameLevels(file->specs[file->pendingTo - 1].levels, live.levels, n)) {
			match = file->pendingTo;
		}
	}
	const bool anyPoints = SumLevels(live.levels, n) != 0;
	for (int32_t k = 1; match == 0 && anyPoints && k <= file->specCount; ++k) {
		if (SameLevels(file->specs[k - 1].levels, live.levels, n)) {
			match = k;
		}
	}
	if (match == 0 && file->pendingFrom > 0) {
		// A swap that did not finish (the character may be fully reset): never overwrite a spec with this state.
		rep.result = Reconciled::Recovery;
	} else if (match == file->active) {
		rep.result = Reconciled::Unchanged;
	} else if (match != 0) {
		file->active = match;
		rep.result = Reconciled::ActiveFromMatch;
		rep.changed = true;
	} else {
		std::memcpy(file->specs[file->active - 1].levels, live.levels, static_cast<size_t>(n));
		file->specs[file->active - 1].state = SpecState::Ok;
		rep.result = Reconciled::AdoptedLive;
		rep.changed = true;
	}
	if (file->pendingFrom != 0 && rep.result != Reconciled::Recovery) {
		file->pendingFrom = file->pendingTo = 0;
		rep.changed = true;
	}

	// Specs that need more points than the character has earned can't be loaded.
	const int32_t total = TotalPoints(live);
	for (int32_t k = 0; k < file->specCount; ++k) {
		Spec& spec = file->specs[k];
		const bool tooMany = SumLevels(spec.levels, n) > total;
		if (tooMany && spec.state != SpecState::Invalid) {
			spec.state = SpecState::Invalid;
			++rep.newlyInvalid;
			rep.changed = true;
		} else if (!tooMany && spec.state == SpecState::Invalid) {
			spec.state = SpecState::Adjusted;  // fits again (e.g. levels gained); keep the warning
			rep.changed = true;
		}
	}
	return rep;
}

}  // namespace ms
