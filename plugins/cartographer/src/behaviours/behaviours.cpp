#include "behaviours.h"

#include <algorithm>
#include <cstdlib>

namespace cartographer::behaviours {

int clampExtraGroups(int n) {
	return std::clamp(n, 0, kMaxExtraGroups);
}

int lifestealBlock(int current, int dealt, int pct) {
	if (dealt <= 0 || pct <= 0) {
		return current;
	}
	const int64_t gain = static_cast<int64_t>(dealt) * std::min(pct, kMaxLifestealPct) / 100;
	return static_cast<int>(std::max<int64_t>(current, std::min<int64_t>(static_cast<int64_t>(current) + gain, kMaxLifestealBlock)));
}

bool elementFromName(const std::string& name, Element* out) {
	static const std::pair<const char*, Element> names[] = {
	    {"fire", Element::Fire}, {"ltng", Element::Lightning}, {"cold", Element::Cold}, {"mag", Element::Magic}, {"pois", Element::Poison}};
	for (const auto& n : names) {
		if (name == n.first) {
			*out = n.second;
			return true;
		}
	}
	return false;
}

std::vector<StatValue> physAsStats(Element element, int pct, int minDmg, int maxDmg) {
	if (pct <= 0 || maxDmg <= 0 || minDmg < 0) {
		return {};
	}
	pct = std::min(pct, kMaxPhysAsPct);
	const int lo = static_cast<int>(static_cast<int64_t>(minDmg) * pct / 100);
	const int hi = std::max(1, static_cast<int>(static_cast<int64_t>(maxDmg) * pct / 100));
	switch (element) {
	case Element::Fire: return {{48, lo}, {49, hi}};
	case Element::Lightning: return {{50, lo}, {51, hi}};
	case Element::Magic: return {{52, lo}, {53, hi}};
	case Element::Cold: return {{54, lo}, {55, hi}, {56, kColdLength}};
	case Element::Poison: {
		const auto perFrame = [](int v) { return static_cast<int>(static_cast<int64_t>(v) * 256 / kPoisonLength); };
		return {{57, perFrame(lo)}, {58, std::max(1, perFrame(hi))}, {59, kPoisonLength}};
	}
	}
	return {};
}

bool any(const MapBehaviours& b) {
	return b.dropBonusPct > 0 || b.etherealPct > 0 || b.socketedPct > 0 || b.extraBoss > 0 || !b.dropTcs.empty();
}

Rng::Rng(uint64_t seed) : s_(seed != 0 ? seed : 0x9E3779B97F4A7C15ull) {}

uint32_t Rng::next() {
	s_ ^= s_ >> 12;
	s_ ^= s_ << 25;
	s_ ^= s_ >> 27;
	return static_cast<uint32_t>((s_ * 2685821657736338717ull) >> 32);
}

uint32_t Rng::below(uint32_t n) {
	return n == 0 ? 0 : next() % n;
}

bool chance(int pct, Rng& rng) {
	if (pct <= 0) {
		return false;
	}
	return pct >= 100 || rng.below(100) < static_cast<uint32_t>(pct);
}

int extraDrops(int bonusPct, Rng& rng) {
	if (bonusPct <= 0) {
		return 0;
	}
	return bonusPct / 100 + (chance(bonusPct % 100, rng) ? 1 : 0);
}

std::vector<size_t> hitTcs(const std::vector<std::pair<std::string, int>>& tcs, Rng& rng) {
	std::vector<size_t> hit;
	for (size_t i = 0; i < tcs.size(); ++i) {
		if (chance(tcs[i].second, rng)) {
			hit.push_back(i);
		}
	}
	return hit;
}

int socketCount(Rng& rng) {
	return 1 + static_cast<int>(rng.below(2));
}

namespace {

bool parseInt(const std::string& text, int lo, int hi, int* out) {
	if (text.empty()) {
		return false;
	}
	char* end = nullptr;
	const long v = std::strtol(text.c_str(), &end, 10);
	if (*end != '\0' || v < lo || v > hi) {
		return false;
	}
	*out = static_cast<int>(v);
	return true;
}

}  // namespace

bool parseTestRouting(const std::string& spec, std::vector<TestRoute>* out, std::string* why) {
	std::vector<TestRoute> routes;
	size_t pos = 0;
	while (pos <= spec.size()) {
		size_t end = spec.find(';', pos);
		if (end == std::string::npos) {
			end = spec.size();
		}
		const std::string item = spec.substr(pos, end - pos);
		pos = end + 1;
		if (item.empty()) {
			continue;
		}
		const size_t eq = item.find('=');
		TestRoute r;
		bool ok = eq != std::string::npos && eq > 0 && parseInt(item.substr(eq + 1), 1, 1000000, &r.value);
		if (ok) {
			const std::string name = item.substr(0, eq);
			const size_t colon = name.find(':');
			r.key = colon == std::string::npos ? name : name.substr(0, colon);
			r.param = colon == std::string::npos ? std::string() : name.substr(colon + 1);
			ok = !r.key.empty() && (colon == std::string::npos || !r.param.empty());
		}
		if (!ok) {
			*why = "bad item '" + item + "' (<key>[:<param>]=<whole number above 0>)";
			return false;
		}
		routes.push_back(std::move(r));
	}
	*out = std::move(routes);
	return true;
}

bool parseSpec(const std::string& spec, MapBehaviours* out, std::string* why) {
	MapBehaviours b;
	size_t pos = 0;
	while (pos <= spec.size()) {
		size_t end = spec.find(';', pos);
		if (end == std::string::npos) {
			end = spec.size();
		}
		const std::string item = spec.substr(pos, end - pos);
		pos = end + 1;
		if (item.empty()) {
			continue;
		}
		const size_t eq = item.find('=');
		const std::string key = eq == std::string::npos ? item : item.substr(0, eq);
		const std::string value = eq == std::string::npos ? std::string() : item.substr(eq + 1);
		bool ok = false;
		if (key == "tc") {
			const size_t colon = value.rfind(':');
			int pct = 0;
			ok = colon != std::string::npos && colon > 0 && parseInt(value.substr(colon + 1), 0, 100000, &pct);
			if (ok) {
				b.dropTcs.emplace_back(value.substr(0, colon), pct);
			}
		} else if (key == "bonus") {
			ok = parseInt(value, 0, 100000, &b.dropBonusPct);
		} else if (key == "eth") {
			ok = parseInt(value, 0, 100000, &b.etherealPct);
		} else if (key == "sock") {
			ok = parseInt(value, 0, 100000, &b.socketedPct);
		} else if (key == "boss") {
			ok = parseInt(value, 0, kMaxExtraBoss, &b.extraBoss);
		}
		if (!ok) {
			*why = "bad item '" + item + "' (tc=<name>:<pct>, bonus=, eth=, sock=, boss=0.." + std::to_string(kMaxExtraBoss) + ")";
			return false;
		}
	}
	*out = std::move(b);
	return true;
}

}  // namespace cartographer::behaviours
