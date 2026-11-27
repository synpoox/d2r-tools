#include "history.h"
#include <fstream>
#include <sstream>
#include <limits>
#include <type_traits>
#include <charconv>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif
namespace recap {
namespace {
// A versioned numeric wire format, never a raw C++ structure or engine pointer.
struct Writer {
	std::ostringstream out;
	template<class T> void operator()(T& value) {
		if constexpr (std::is_signed_v<T>) out << static_cast<int64_t>(value);
		else out << static_cast<uint64_t>(value);
		out << ' ';
	}
};
struct Reader {
	std::istringstream in;
	bool ok = true;
	template<class T> void operator()(T& value) {
		std::string text;
		if (!(in >> text) || text.size() > 21) { ok = false; return; }
		if constexpr (std::is_signed_v<T>) {
			int64_t n = 0; const auto result = std::from_chars(text.data(), text.data() + text.size(), n);
			if (result.ec != std::errc{} || result.ptr != text.data() + text.size() || n < std::numeric_limits<T>::min() || n > std::numeric_limits<T>::max()) { ok = false; return; }
			value = static_cast<T>(n);
		} else {
			uint64_t n = 0; const auto result = std::from_chars(text.data(), text.data() + text.size(), n);
			uint64_t limit;
			if constexpr (std::is_enum_v<T>) limit = std::is_same_v<T, Evidence> ? 2 : 8;
			else limit = std::numeric_limits<T>::max();
			if (result.ec != std::errc{} || result.ptr != text.data() + text.size() || n > limit) { ok = false; return; }
			value = static_cast<T>(n);
		}
	}
};
template<class A, class T> void FieldWire(A& a, Field<T>& f) { a(f.value); a(f.evidence); }
template<class A> void UnitWire(A& a, Field<Unit>& f) { a(f.value.type); a(f.value.id); a(f.value.classId); a(f.evidence); }
template<class A> void ResourceWire(A& a, Resource& r) { FieldWire(a, r.before); FieldWire(a, r.after); FieldWire(a, r.lost); FieldWire(a, r.restored); }
template <class A> void EventWire(A &a, Event &e, bool extended = true) {
	a(e.session); a(e.sequence); a(e.ms); a(e.kind); a(e.cause);
	UnitWire(a, e.attacker); UnitWire(a, e.origin); UnitWire(a, e.missile); FieldWire(a, e.skill); FieldWire(a, e.fatalSequence); FieldWire(a, e.dataBank);
	FieldWire(a, e.maxLife); FieldWire(a, e.maxMana);
	for (auto& f : e.modifiers) FieldWire(a, f);
	ResourceWire(a, e.life); ResourceWire(a, e.mana);
	FieldWire(a, e.damage.pre); FieldWire(a, e.damage.post); FieldWire(a, e.damage.overkill);
	for (auto& f : e.damage.preByElement) FieldWire(a, f);
	for (auto& f : e.damage.postByElement) FieldWire(a, f);
	a(e.defense.statCount); a(e.defense.statsTruncated); a(e.defense.stateCount); a(e.defense.stateEvidence); a(e.defense.statesTruncated);
	for (auto& s : e.defense.stats) { a(s.id); a(s.layer); FieldWire(a, s.amount); }
	for (auto& s : e.defense.states) a(s);
	if (extended) {
		FieldWire(a, e.characterLevel);
		FieldWire(a, e.areaId);
	}
}
template<class A> void ReportWire(A& a, Report& r) {
	a(r.session); a(r.deathMs); a(r.windowStartMs); a(r.windowTruncated); a(r.rejected);
	a(r.sequenceGap); a(r.boundaryUnconfirmed); a(r.deathResourcesUnconfirmed); a(r.capturePartial); a(r.replicated); a(r.feedDropped);
}
void AmountJson(std::ostream& o, const Amount& a) {
	o << "{\"raw\":";
	if (a.evidence == Evidence::Unavailable) o << "null"; else o << a.value;
	o << ",\"evidence\":" << int(a.evidence) << '}';
}
void ResourceJson(std::ostream& o, const Resource& r) {
	o << "{\"before\":"; AmountJson(o, r.before); o << ",\"after\":"; AmountJson(o, r.after);
	o << ",\"lost\":"; AmountJson(o, r.lost); o << ",\"restored\":"; AmountJson(o, r.restored); o << '}';
}
void UnitJson(std::ostream& o, const Field<Unit>& u) {
	if (u.evidence == Evidence::Unavailable) { o << "null"; return; }
	o << "{\"type\":" << u.value.type << ",\"id\":" << u.value.id << ",\"class_id\":" << u.value.classId << ",\"evidence\":" << int(u.evidence) << '}';
}
void StringJson(std::ostream& o, const std::string& text) {
	o << '"';
	constexpr char hex[] = "0123456789abcdef";
	for (unsigned char c : text) {
		if (c == '"' || c == '\\') o << '\\' << static_cast<char>(c);
		else if (c < 32) o << "\\u00" << hex[c >> 4] << hex[c & 15];
		else o << static_cast<char>(c);
	}
	o << '"';
}
}
std::string Encode(const History& history) {
	Writer a;
	a.out << "D2RRECAP2\n" << history.size() << '\n';
	for (auto r : history) {
		ReportWire(a, r);
		a.out << r.areaName.size() << ' ';
		for (uint8_t c : r.areaName)
			a(c);
		a.out << r.names.size() << '\n';
		for (auto& name : r.names) {
			a(name.unit.type); a(name.unit.id); a(name.unit.classId); a(name.bank); a.out << name.text.size() << ' ';
			for (uint8_t c : name.text) a(c);
			a.out << '\n';
		}
		a.out << r.timeline.size() << '\n';
		for (auto& e : r.timeline) { EventWire(a, e); a.out << '\n'; }
	}
	return a.out.str();
}
bool Decode(const std::string& bytes, History& history) {
	if (bytes.size() > 32 * 1024 * 1024) return false;
	Reader a {std::istringstream(bytes)};
	std::string magic; size_t count = 0; a.in >> magic >> count;
	if (!a.in || (magic != "D2RRECAP1" && magic != "D2RRECAP2") || count > 100) return false;
	History loaded;
	for (size_t i = 0; i < count; ++i) {
		Report r;
		ReportWire(a, r);
		if (magic == "D2RRECAP2") {
			size_t length = 0;
			a(length);
			if (!a.ok || length > 1024) return false;
			for (size_t c = 0; c < length; ++c) {
				uint8_t byte = 0;
				a(byte);
				r.areaName += static_cast<char>(byte);
			}
		}
		size_t nameCount = 0;
		a(nameCount);
		if (!a.ok || nameCount > 2048) return false;
		for (size_t j = 0; j < nameCount; ++j) {
			NamedUnit name; a(name.unit.type); a(name.unit.id); a(name.unit.classId); a(name.bank); size_t size = 0; a(size);
			if (!a.ok || size > 255 || name.bank < 1 || name.bank > 3) return false;
			for (size_t k = 0; k < size; ++k) { uint8_t c = 0; a(c); name.text += static_cast<char>(c); }
			if (!a.ok) return false;
			r.names.push_back(std::move(name));
		}
		size_t events = 0; a.in >> events;
		if (!a.ok || !a.in || events == 0 || events > 4096) return false;
		uint64_t sequence = 0, ms = 0;
		for (size_t j = 0; j < events; ++j) {
			Event e;
			EventWire(a, e, magic == "D2RRECAP2");
			if (!a.ok || e.session != r.session || e.sequence <= sequence || e.ms < ms ||
				e.defense.statCount > 16 || e.defense.stateCount > 16 ||
				(j + 1 != events && e.kind == Kind::Death)) return false;
			sequence = e.sequence; ms = e.ms;
			Account(r.life, e.life, e.kind); Account(r.mana, e.mana, e.kind);
			r.timeline.push_back(e);
		}
		if (r.timeline.back().kind != Kind::Death || r.timeline.back().cause != Evidence::Confirmed || ms != r.deathMs || r.windowStartMs > r.deathMs) return false;
		loaded.push_back(std::move(r));
	}
	a.in >> std::ws;
	if (!a.ok || !a.in.eof()) return false;
	history = std::move(loaded); return true;
}
bool WriteAtomic(const std::filesystem::path& path, const std::string& bytes) {
	const auto temporary = std::filesystem::path(path.wstring() + L".tmp");
	std::error_code ec;
	std::filesystem::create_directories(path.parent_path(), ec);
	if (ec) return false;
	{
		std::ofstream out(temporary, std::ios::binary | std::ios::trunc);
		out.write(bytes.data(), static_cast<std::streamsize>(bytes.size())); out.flush();
		if (!out) return false;
		out.close(); if (!out) return false;
	}
#ifdef _WIN32
	return MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
#else
	std::filesystem::rename(temporary, path, ec); return !ec;
#endif
}
History LoadHistory(const std::filesystem::path& path, size_t retention) {
	std::error_code ec;
	const auto size = std::filesystem::file_size(path, ec);
	if (ec || size > 32 * 1024 * 1024) return {};
	std::ifstream in(path, std::ios::binary);
	std::string bytes(static_cast<size_t>(size), '\0');
	if (!in.read(bytes.data(), static_cast<std::streamsize>(size))) return {};
	History history; if (!Decode(bytes, history)) return {};
	retention = std::clamp<size_t>(retention, 1, 100);
	if (history.size() > retention) history.erase(history.begin(), history.end() - static_cast<ptrdiff_t>(retention));
	return history;
}
std::string Json(const Report& r) {
	std::ostringstream o;
	o << "{\"schema\":2,\"resource_units\":\"1/256\",\"defense_units\":\"native stat units\",\"session\":" << r.session << ",\"death_ms\":" << r.deathMs
	  << ",\"window_start_ms\":" << r.windowStartMs << ",\"capture_partial\":" << (r.capturePartial ? "true" : "false")
	  << ",\"replicated\":" << (r.replicated ? "true" : "false") << ",\"feed_dropped\":" << r.feedDropped << ",\"window_truncated\":" << r.windowTruncated
	  << ",\"sequence_gap\":" << r.sequenceGap << ",\"boundary_unconfirmed\":" << r.boundaryUnconfirmed
	  << ",\"death_resources_unconfirmed\":" << r.deathResourcesUnconfirmed << ",\"rejected\":" << r.rejected << ",\"area_name\":";
	StringJson(o, r.areaName);
	o << ",\"names\":[";
	for (size_t i = 0; i < r.names.size(); ++i) {
		if (i) o << ',';
		const auto& n = r.names[i]; o << "{\"type\":" << n.unit.type << ",\"id\":" << n.unit.id << ",\"class_id\":" << n.unit.classId << ",\"bank\":" << n.bank << ",\"text\":"; StringJson(o, n.text); o << '}';
	}
	o << "],\"timeline\":[";
	bool comma = false;
	for (const auto& e : r.timeline) {
		if (comma) o << ','; comma = true;
		o << "{\"sequence\":" << e.sequence << ",\"ms\":" << e.ms << ",\"kind\":" << int(e.kind) << ",\"cause\":" << int(e.cause) << ",\"life\":";
		ResourceJson(o, e.life); o << ",\"mana\":"; ResourceJson(o, e.mana);
		o << ",\"attacker\":"; UnitJson(o, e.attacker); o << ",\"origin\":"; UnitJson(o, e.origin); o << ",\"missile\":"; UnitJson(o, e.missile);
		o << ",\"skill\":{\"id\":" << e.skill.value << ",\"evidence\":" << int(e.skill.evidence) << "},\"fatal_sequence\":{\"id\":" << e.fatalSequence.value << ",\"evidence\":" << int(e.fatalSequence.evidence) << '}';
		o << ",\"data_bank\":{\"id\":" << e.dataBank.value << ",\"evidence\":" << int(e.dataBank.evidence) << '}';
		o << ",\"character_level\":{\"id\":" << e.characterLevel.value << ",\"evidence\":" << int(e.characterLevel.evidence)
		  << "},\"area_id\":{\"id\":" << e.areaId.value << ",\"evidence\":" << int(e.areaId.evidence) << '}';
		o << ",\"max_life\":";
		AmountJson(o, e.maxLife);
		o << ",\"max_mana\":";
		AmountJson(o, e.maxMana);
		o << ",\"pre\":"; AmountJson(o, e.damage.pre); o << ",\"post\":"; AmountJson(o, e.damage.post); o << ",\"overkill\":"; AmountJson(o, e.damage.overkill);
		for (bool pre : {true, false}) {
			o << (pre ? ",\"pre_elements\":[" : ",\"post_elements\":["); bool sep = false;
			for (const auto& f : pre ? e.damage.preByElement : e.damage.postByElement) { if (sep) o << ','; sep = true; AmountJson(o, f); } o << ']';
		}
		o << ",\"defense\":[";
		for (size_t i = 0; i < e.defense.statCount; ++i) { if (i) o << ','; const auto& s = e.defense.stats[i]; o << "{\"id\":" << s.id << ",\"layer\":" << s.layer << ",\"amount\":"; AmountJson(o, s.amount); o << '}'; }
		o << "],\"states_evidence\":" << int(e.defense.stateEvidence) << ",\"states\":[";
		for (size_t i = 0; i < e.defense.stateCount; ++i) { if (i) o << ','; o << e.defense.states[i]; }
		o << "],\"states_truncated\":" << e.defense.statesTruncated << ",\"stats_truncated\":" << e.defense.statsTruncated << ",\"modifiers\":[";
		for (size_t i = 0; i < e.modifiers.size(); ++i) { if (i) o << ','; o << "{\"id\":" << e.modifiers[i].value << ",\"evidence\":" << int(e.modifiers[i].evidence) << '}'; }
		o << "]}";
	}
	o << "]}\n"; return o.str();
}
}
