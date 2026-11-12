#include "mapname.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

namespace cartographer::mapname {

namespace {

auto IsContinuation(unsigned char c) -> bool {
	return (c & 0xC0) == 0x80;
}

void AppendJson(std::string& out, const std::string& text) {
	for (const char ch : text) {
		const auto c = static_cast<unsigned char>(ch);
		if (c == '"' || c == '\\') {
			out += '\\';
			out += ch;
		} else if (c < 0x20 || c == 0x7f) {
			char buf[8];
			std::snprintf(buf, sizeof(buf), "\\u%04x", c);
			out += buf;
		} else {
			out += ch;
		}
	}
}

void AppendRecord(std::string& out, const std::string& key, const std::string& text) {
	out += "{\"Key\":\"";
	AppendJson(out, key);
	out += '"';
	for (const char* language : Languages) {
		out += ",\"";
		out += language;
		out += "\":\"";
		AppendJson(out, text);
		out += '"';
	}
	out += '}';
}

// Length of the valid UTF-8 character at text[i], or 0 when the bytes there are not one (overlong, surrogate, past U+10FFFF or cut off).
auto ValidLength(const std::string& text, size_t i) -> size_t {
	const auto b = [&](size_t k) { return static_cast<unsigned char>(text[k]); };
	const unsigned char c = b(i);
	size_t length = 0;
	uint32_t code = 0;
	if (c < 0x80) {
		return 1;
	} else if (c >= 0xC2 && c <= 0xDF) {
		length = 2;
		code = c & 0x1F;
	} else if (c >= 0xE0 && c <= 0xEF) {
		length = 3;
		code = c & 0x0F;
	} else if (c >= 0xF0 && c <= 0xF4) {
		length = 4;
		code = c & 0x07;
	} else {
		return 0;
	}
	if (i + length > text.size()) {
		return 0;
	}
	for (size_t k = 1; k < length; ++k) {
		if (!IsContinuation(b(i + k))) {
			return 0;
		}
		code = (code << 6) | (b(i + k) & 0x3F);
	}
	const uint32_t minimum = length == 2 ? 0x80 : length == 3 ? 0x800 : 0x10000;
	if (code < minimum || code > 0x10FFFF || (code >= 0xD800 && code <= 0xDFFF)) {
		return 0;
	}
	return length;
}

auto ReplaceInvalid(const std::string& text) -> std::string {
	std::string out;
	for (size_t i = 0; i < text.size();) {
		const size_t length = ValidLength(text, i);
		if (length == 0) {
			out += "\xEF\xBF\xBD";
			++i;
		} else {
			out.append(text, i, length);
			i += length;
		}
	}
	return out;
}

}  // namespace

auto DisplayName(const std::string& name) -> std::string {
	std::string text;
	for (const char ch : name) {
		const auto c = static_cast<unsigned char>(ch);
		text += (c < 0x20 || c == 0x7f) ? ' ' : ch;
	}
	text = ReplaceInvalid(text);
	const size_t first = text.find_first_not_of(' ');
	if (first == std::string::npos) {
		return "Map";
	}
	text = text.substr(first, text.find_last_not_of(' ') - first + 1);
	if (text.size() > MaxNameBytes) {
		size_t cut = MaxNameBytes;
		while (cut > 0 && IsContinuation(static_cast<unsigned char>(text[cut]))) {
			--cut;  // do not leave half a character
		}
		text.resize(cut);
		const size_t last = text.find_last_not_of(' ');
		text = last == std::string::npos ? std::string() : text.substr(0, last + 1);
	}
	return text.empty() ? std::string("Map") : text;
}

auto EntryText(const std::string& displayName) -> std::string {
	return "Entering " + displayName;
}

auto NameLocalKey(size_t index) -> std::string {
	return "N" + std::to_string(index);
}

auto EntryLocalKey(size_t index) -> std::string {
	return "E" + std::to_string(index);
}

auto RowKey(const std::string& pluginId, const std::string& localKey) -> std::string {
	return pluginId + ":" + localKey;
}

auto KeyFits(const std::string& key) -> bool {
	if (key.empty() || key.size() >= FieldSize) {
		return false;
	}
	return std::all_of(key.begin(), key.end(), [](char ch) {
		const auto c = static_cast<unsigned char>(ch);
		return c > 0x20 && c < 0x7f && ch != '"' && ch != '\\';
	});
}

auto StringsJson(const std::vector<std::string>& names) -> std::string {
	std::string out = "[";
	const size_t count = std::min(names.size(), MaxMaps);
	for (size_t i = 0; i < count; ++i) {
		if (i > 0) {
			out += ',';
		}
		AppendRecord(out, NameLocalKey(i), names[i]);
		out += ',';
		AppendRecord(out, EntryLocalKey(i), EntryText(names[i]));
	}
	out += "]";
	return out;
}

auto BossLocalKey(const std::string& name) -> std::string {
	return "boss_" + name;
}

auto BossRowKey(const std::string& pluginId, const std::string& name) -> std::string {
	return RowKey(pluginId, BossLocalKey(name));
}

auto BossStringsJson(const std::vector<std::string>& names) -> std::string {
	std::string out = "[";
	std::vector<std::string> seen;
	for (const std::string& name : names) {
		if (std::find(seen.begin(), seen.end(), name) != seen.end()) {
			continue;
		}
		if (!seen.empty()) {
			out += ',';
		}
		seen.push_back(name);
		AppendRecord(out, BossLocalKey(name), DisplayName(name));
	}
	out += "]";
	return out;
}

auto WriteKey(uint8_t* field, const std::string& key) -> bool {
	if (field == nullptr || !KeyFits(key)) {
		return false;
	}
	std::memset(field, 0, FieldSize);
	std::memcpy(field, key.data(), key.size());
	return true;
}

auto LooksLikeKey(const uint8_t* field) -> bool {
	if (field == nullptr) {
		return false;
	}
	for (size_t i = 0; i < FieldSize; ++i) {
		if (field[i] == 0) {
			return true;
		}
		if (field[i] < 0x20 || field[i] >= 0x7f) {
			return false;
		}
	}
	return false;  // no NUL inside the field
}

auto RowSaver::Remember(int level, const uint8_t* name, const uint8_t* entry) -> bool {
	if (Has(level) || name == nullptr || entry == nullptr) {
		return false;
	}
	SavedRow row;
	row.level = level;
	std::memcpy(row.name, name, FieldSize);
	std::memcpy(row.entry, entry, FieldSize);
	rows_.push_back(row);
	return true;
}

auto RowSaver::Has(int level) const -> bool {
	return std::any_of(rows_.begin(), rows_.end(), [level](const SavedRow& r) { return r.level == level; });
}

auto IsOwnKey(const uint8_t* field, const std::string& pluginId) -> bool {
	if (field == nullptr || pluginId.empty() || pluginId.size() + 1 >= FieldSize) {
		return false;
	}
	return std::memcmp(field, pluginId.data(), pluginId.size()) == 0 && field[pluginId.size()] == ':';
}

auto RowSaver::Take(int level, SavedRow* out) -> bool {
	for (auto it = rows_.begin(); it != rows_.end(); ++it) {
		if (it->level == level) {
			if (out != nullptr) {
				*out = *it;
			}
			rows_.erase(it);
			return true;
		}
	}
	return false;
}

void RegistrationPlan::Want(bool wanted, std::vector<std::string> names) {
	wantRegistered_ = wanted && !names.empty();
	want_ = wantRegistered_ ? std::move(names) : std::vector<std::string>();
}

auto RegistrationPlan::Next(bool rowsHeld) const -> Step {
	const bool same = wantRegistered_ ? (heldRegistered_ && held_ == want_) : !heldRegistered_;
	if (same) {
		return Step::Nothing;
	}
	return rowsHeld ? Step::Deferred : Step::Apply;
}

void RegistrationPlan::Dropped() {
	heldRegistered_ = false;
	held_.clear();
}

void RegistrationPlan::Applied(std::vector<std::string> names) {
	heldRegistered_ = !names.empty();
	held_ = std::move(names);
}

auto RowSaver::SetKeys(int level, const uint8_t* name, const uint8_t* entry) -> bool {
	if (name == nullptr || entry == nullptr) {
		return false;
	}
	for (SavedRow& row : rows_) {
		if (row.level == level) {
			std::memcpy(row.keyName, name, FieldSize);
			std::memcpy(row.keyEntry, entry, FieldSize);
			return true;
		}
	}
	return false;
}

auto RowSaver::TakeAll() -> std::vector<SavedRow> {
	std::vector<SavedRow> out;
	out.swap(rows_);
	return out;
}

}  // namespace cartographer::mapname
