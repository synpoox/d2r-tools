#include "capture.h"
#include "history.h"
#include "settings.h"
#include "preview.h"
#include <cstdlib>
#include <iostream>
#include <thread>
#include <sstream>
using namespace recap;
#define CHECK(x) do { if (!(x)) { std::cerr << "line " << __LINE__ << ": " << #x << '\n'; std::exit(1); } } while (false)
int main() {
	const auto preview = PreviewReport();
	CHECK(preview.life.Reconciled() && preview.mana.Reconciled());
	CHECK(preview.life.lost == 1200 * 256 && preview.life.restored == 200 * 256);
	CHECK(preview.mana.lost == 250 * 256 && preview.life.last == 0 && KillingBlow(preview));
	const auto clipped = Transition(300, 0, true);
	CHECK(clipped.lost.Confirmed() && clipped.lost.value == 300 && GrossKnown(clipped));
	const auto regen = Transition(300, 350, false);
	CHECK(!regen.lost.Confirmed() && !regen.restored.Confirmed());
	CHECK(!GrossKnown(Transition(INT32_MIN, INT32_MAX, true)));
	DeathLatch death;
	CHECK(!death.ConfirmTransition(1, 1)); CHECK(death.ConfirmTransition(1, 0));
	CHECK(!death.ConfirmTransition(0, 17)); CHECK(!death.ConfirmTransition(1, 0));
	death.Reset(); CHECK(!death.ConfirmTransition(17, 0)); CHECK(death.ConfirmTransition(1, 17));
	Feed<4> feed; Event e; e.sequence = 1;
	CHECK(feed.Push(e)); e.sequence = 2; CHECK(feed.Push(e)); e.sequence = 3; CHECK(feed.Push(e));
	e.sequence = 4; CHECK(!feed.Push(e)); CHECK(feed.Pop(e) && e.sequence == 1);
	CHECK(feed.Pop(e) && e.sequence == 2); CHECK(feed.Pop(e) && e.sequence == 3); CHECK(!feed.Pop(e));
	Feed<64> threaded;
	std::thread producer([&] { for (uint64_t i = 1; i <= 20000; ++i) { Event item; item.sequence = i; while (!threaded.Push(item)) std::this_thread::yield(); } });
	for (uint64_t i = 1; i <= 20000; ++i) { while (!threaded.Pop(e)) std::this_thread::yield(); CHECK(e.sequence == i); }
	producer.join();
	const auto settings = ParseSettings("[other]\nenabled=false\n[death-recap]\nwindow_seconds=900\nhistory_limit=0\nenabled=trueish\nauto_open=false # comment\n");
	CHECK(settings.enabled && !settings.autoOpen && settings.windowSeconds == 60 && settings.retention == 1);
	CHECK(ParseSettings("[death-recap]\nwindow_seconds=2junk\nhistory_limit=-1\n").windowSeconds == 10);
	Rolling<8> rolling; rolling.Reset(7, 0);
	e = Sample({1000, 500, 1000, 500}); e.session = 7; e.sequence = 1; CHECK(rolling.Push(e));
	e.sequence = 2; e.ms = 100; e.kind = Kind::Damage; e.life = Transition(1000, 0, true); e.mana = Transition(500, 500, true);
	e.attacker = {{1, 9, 12}, Evidence::Confirmed}; e.damage.post = Confirm(1200); e.damage.overkill = Confirm(200); CHECK(rolling.Push(e));
	e.sequence = 3; e.kind = Kind::Death; e.cause = Evidence::Confirmed; e.fatalSequence = {2, Evidence::Confirmed};
	e.life = {{}, Confirm(0), {}, {}}; e.mana = {{}, Confirm(500), {}, {}}; CHECK(rolling.Push(e));
	History history {*rolling.Freeze()}; history[0].replicated = true; history[0].feedDropped = 3;
	history[0].names.push_back({{1, 9, 12}, 3, "Source \"name\"\n"});
	history[0].areaName = "The Chaos Sanctuary";
	history[0].timeline.back().characterLevel = {92, Evidence::Confirmed};
	history[0].timeline.back().areaId = {108, Evidence::Confirmed};
	const std::string bytes = Encode(history);
	History loaded;
	CHECK(Decode(bytes, loaded)); CHECK(Encode(loaded) == bytes);
	CHECK(loaded[0].names[0].text == history[0].names[0].text);
	CHECK(loaded[0].areaName == history[0].areaName && loaded[0].timeline.back().characterLevel.value == 92 && loaded[0].timeline.back().areaId.value == 108);
	CHECK(loaded[0].life.lost == 1000 && loaded[0].mana.last == 500 && loaded[0].capturePartial && loaded[0].replicated && loaded[0].feedDropped == 3);
	CHECK(!Decode(bytes.substr(0, bytes.size() - 30), loaded)); CHECK(loaded.size() == 1);
	CHECK(!Decode(bytes + "trailing", loaded));
	CHECK(!Decode("D2RRECAP3\n0\n", loaded));
	CHECK(!Decode("D2RRECAP1\n999999999\n", loaded));
	// Reconstruct the previous wire format to prove existing reports still load.
	std::istringstream current(bytes);
	std::string line, legacy = "D2RRECAP1\n";
	std::getline(current, line);
	std::getline(current, line);
	legacy += line + "\n";
	std::getline(current, line);
	std::istringstream reportFields(line);
	std::vector<std::string> fields;
	for (std::string token; reportFields >> token;)
		fields.push_back(token);
	for (size_t i = 0; i < 11; ++i)
		legacy += fields[i] + " ";
	legacy += fields.back() + "\n";
	for (size_t i = 0; i < history[0].names.size(); ++i) {
		std::getline(current, line);
		legacy += line + "\n";
	}
	std::getline(current, line);
	legacy += line + "\n";
	while (std::getline(current, line)) {
		std::istringstream eventFields(line);
		fields.clear();
		for (std::string token; eventFields >> token;)
			fields.push_back(token);
		CHECK(fields.size() > 4);
		for (size_t i = 0; i + 4 < fields.size(); ++i)
			legacy += fields[i] + " ";
		legacy += "\n";
	}
	History old;
	CHECK(Decode(legacy, old));
	CHECK(old.size() == 1 && old[0].areaName.empty() && !old[0].timeline.back().characterLevel.Confirmed());
	CHECK(old[0].life.lost == history[0].life.lost && old[0].names[0].text == history[0].names[0].text);

	const auto json = Json(history[0]); CHECK(json.find("\"capture_partial\":true") != json.npos); CHECK(json.find("\"raw\":null") != json.npos);
	CHECK(json.find("Source \\\"name\\\"\\u000a") != json.npos);
	const auto folder = std::filesystem::temp_directory_path() / ("death-recap-unit-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
	const auto path = folder / "history.v1";
	CHECK(WriteAtomic(path, bytes)); CHECK(LoadHistory(path, 20).size() == 1);
	CHECK(WriteAtomic(path, Encode({}))); CHECK(LoadHistory(path, 20).empty());
	CHECK(WriteAtomic(path, "corrupt")); CHECK(LoadHistory(path, 20).empty());
	std::filesystem::remove(path); std::filesystem::remove(folder);
	std::cout << "adapter, SPSC, settings, persistence tests passed\n";
}
