#include "bxb/format.h"
#include "check.h"

using namespace bxb;

TEST(format_numbers) {
	CHECK_EQ(FormatCount(0), std::string("0"));
	CHECK_EQ(FormatCount(999), std::string("999"));
	CHECK_EQ(FormatCount(1000), std::string("1,000"));
	CHECK_EQ(FormatCount(1234567890ULL), std::string("1,234,567,890"));
	CHECK_EQ(FormatSigned(-1234), std::string("-1,234"));
	CHECK_EQ(FormatSigned(1234), std::string("+1,234"));
	CHECK_EQ(FormatSigned(0), std::string("0"));
	CHECK_EQ(FormatCompact(512), std::string("512"));
	CHECK_EQ(FormatCompact(98765), std::string("98.8K"));
	CHECK_EQ(FormatCompact(1234567), std::string("1.23M"));
	CHECK_EQ(FormatCompact(2.5e9), std::string("2.50B"));
	CHECK_EQ(FormatCompact(-1234567), std::string("-1.23M"));
	CHECK_EQ(FormatCompact(-512), std::string("-512"));
	CHECK_EQ(FormatPercent(0.5), std::string("50.00%"));
	CHECK_EQ(FormatSignedPercent(0.75), std::string("+75.00%"));
	CHECK_EQ(FormatSignedPercent(-0.125), std::string("-12.50%"));
}

TEST(format_clock_and_eta) {
	CHECK_EQ(FormatClock(0), std::string("00:00:00"));
	CHECK_EQ(FormatClock(3661.9), std::string("01:01:01"));
	CHECK_EQ(FormatClock(123 * 3600 + 4 * 60 + 5), std::string("123:04:05"));
	CHECK_EQ(FormatClock(-5), std::string("00:00:00"));
	CHECK_EQ(FormatEta(std::nullopt), std::string("--"));
	CHECK_EQ(FormatEta(-1.0), std::string("--"));
	CHECK_EQ(FormatEta(44.2), std::string("45s"));
	CHECK_EQ(FormatEta(723.0), std::string("12m 03s"));
	CHECK_EQ(FormatEta(3 * 3600 + 7 * 60), std::string("3h 07m"));
	CHECK_EQ(FormatEta(2 * 86400 + 4 * 3600 + 5), std::string("2d 04h"));
}

TEST(format_lines) {
	Snapshot s;
	CHECK_EQ(FormatLine(s, LineId::Level), std::string());  // not valid -> nothing
	s.valid = true;
	s.level = 80;
	s.maxLevel = 99;
	s.xp = 1234567;
	s.levelProgress = 0.5;
	s.sessionXp = -100;
	s.sessionLevels = -0.001;
	s.activeSeconds = 3725;
	CHECK_EQ(FormatLine(s, LineId::Level), std::string("Level 80"));
	CHECK_EQ(FormatLine(s, LineId::Xp), std::string("XP 1,234,567  50.00%"));
	CHECK_EQ(FormatLine(s, LineId::Session), std::string("Session -100  -0.10%"));
	CHECK_EQ(FormatLine(s, LineId::Rate), std::string("XP/h --  (avg --)"));
	CHECK_EQ(FormatLine(s, LineId::Time), std::string("Time 01:02:05"));
	CHECK_EQ(FormatLine(s, LineId::Eta), std::string("Next level --"));
	s.live = Rate{1.5e6, 0.123};
	s.session = Rate{1.2e6, 0.1};
	s.etaSeconds = 600;
	CHECK_EQ(FormatLine(s, LineId::Rate), std::string("XP/h 1.50M  12.30%  (avg 1.20M)"));
	CHECK_EQ(FormatLine(s, LineId::Eta), std::string("Next level 10m 00s"));
	s.live = Rate{-45000.0, -0.0123};          // a death outweighed the gains in the window
	s.etaSeconds.reset();
	CHECK_EQ(FormatLine(s, LineId::Rate), std::string("XP/h -45.0K  -1.23%  (avg 1.20M)"));
	CHECK_EQ(FormatLine(s, LineId::Eta), std::string("Next level --"));
	s.live = Rate{1.5e6, 0.123};
	s.atMaxLevel = true;
	s.level = 99;
	CHECK_EQ(FormatLine(s, LineId::Level), std::string("Level 99 (max)"));
	CHECK_EQ(FormatLine(s, LineId::Xp), std::string("XP 1,234,567  --"));
	CHECK_EQ(FormatLine(s, LineId::Eta), std::string("Next level --"));
}
