#include "autocheck.h"

#include <algorithm>
#include <cctype>

namespace cartographer::autocheck {

namespace {

auto lower(std::string text) -> std::string {
	std::transform(text.begin(), text.end(), text.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	return text;
}

auto trimmed(const std::string &text) -> std::string {
	size_t a = 0, b = text.size();
	while (a < b && std::isspace(static_cast<unsigned char>(text[a]))) {
		++a;
	}
	while (b > a && std::isspace(static_cast<unsigned char>(text[b - 1]))) {
		--b;
	}
	return text.substr(a, b - a);
}

}  // namespace

auto itemName(Item item) -> const char * {
	switch (item) {
	case Item::Open: return "open";
	case Item::Arrival: return "arrival";
	case Item::Boss: return "boss";
	case Item::MapSettings: return "monster-settings";
	case Item::StatChanges: return "stat-changes";
	}
	return "unknown";
}

auto Checklist::next() const -> std::optional<Item> {
	for (size_t i = 0; i < ItemCount; ++i) {
		if (!done_[i]) {
			return static_cast<Item>(i);
		}
	}
	return std::nullopt;
}

auto Checklist::settled(Item item) const -> bool {
	return done_[static_cast<size_t>(item)];
}

void Checklist::record(Item item, bool pass, const std::string &detail) {
	const size_t i = static_cast<size_t>(item);
	if (done_[i]) {
		return;
	}
	done_[i] = true;
	result_[i] = Result{item, pass, detail, false};
}

void Checklist::recordNotApplicable(Item item, const std::string &detail) {
	const size_t i = static_cast<size_t>(item);
	if (done_[i]) {
		return;
	}
	done_[i] = true;
	result_[i] = Result{item, true, detail, true};
}

void Checklist::failRest(const std::string &reason) {
	for (size_t i = 0; i < ItemCount; ++i) {
		record(static_cast<Item>(i), false, reason);
	}
}

auto Checklist::complete() const -> bool {
	return !next().has_value();
}

auto Checklist::passCount() const -> size_t {
	size_t n = 0;
	for (size_t i = 0; i < ItemCount; ++i) {
		n += done_[i] && result_[i].pass && !result_[i].notApplicable ? 1 : 0;
	}
	return n;
}

auto Checklist::failCount() const -> size_t {
	size_t n = 0;
	for (size_t i = 0; i < ItemCount; ++i) {
		n += done_[i] && !result_[i].pass ? 1 : 0;
	}
	return n;
}

auto Checklist::notApplicableCount() const -> size_t {
	size_t n = 0;
	for (size_t i = 0; i < ItemCount; ++i) {
		n += done_[i] && result_[i].notApplicable ? 1 : 0;
	}
	return n;
}

auto Checklist::results() const -> std::vector<Result> {
	std::vector<Result> out;
	for (size_t i = 0; i < ItemCount; ++i) {
		if (done_[i]) {
			out.push_back(result_[i]);
		}
	}
	return out;
}

auto resultLine(const Result &result) -> std::string {
	std::string line = std::string("AUTOCHECK ") + (result.notApplicable ? "N/A " : result.pass ? "PASS " : "FAIL ") + itemName(result.item);
	const std::string detail = trimmed(result.detail);
	if (!detail.empty()) {
		line += ": " + detail;
	}
	return line;
}

auto doneLine(const Checklist &list) -> std::string {
	return "AUTOCHECK DONE pass=" + std::to_string(list.passCount()) + " fail=" + std::to_string(list.failCount()) +
	       (list.notApplicableCount() > 0 ? " n/a=" + std::to_string(list.notApplicableCount()) : std::string());
}

auto finalLines(Checklist list) -> std::vector<std::string> {
	list.failRest("not reached");
	std::vector<std::string> lines;
	for (const Result &result : list.results()) {
		lines.push_back(resultLine(result));
	}
	lines.push_back(doneLine(list));
	return lines;
}

auto resolveConfig(int autoCheck, const std::string &autoCheckMap, const std::vector<std::string> &mapNames) -> Config {
	Config config;
	if (autoCheck == 0) {
		return config;
	}
	config.enabled = true;
	const std::string wanted = trimmed(autoCheckMap);
	if (wanted.empty()) {
		config.problem = "AutoCheck is 1 but AutoCheckMap is empty; name a map from maps.txt";
		return config;
	}
	for (const std::string &name : mapNames) {
		if (lower(name) == lower(wanted)) {
			config.map = name;
			return config;
		}
	}
	config.problem = "AutoCheckMap '" + wanted + "' is not a map in maps.txt";
	return config;
}

auto bossStandPoint(int tileX, int tileY, int tileW, int tileH) -> Point {
	const int offsetX = std::max(1, tileW / 4);
	const int offsetY = std::max(1, tileH / 2);
	return Point{(tileX + offsetX) * SubtilesPerTile, (tileY + offsetY) * SubtilesPerTile};
}

auto timedOut(unsigned long long startMs, unsigned long long nowMs, unsigned long long limitMs) -> bool {
	return nowMs >= startMs && nowMs - startMs > limitMs;
}

}  // namespace cartographer::autocheck
