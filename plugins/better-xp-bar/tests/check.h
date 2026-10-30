// Minimal test harness (no dependencies): TEST(name) { CHECK(cond); CHECK_EQ(a, b); CHECK_NEAR(a, b, eps); }
#pragma once

#include <cmath>
#include <cstdio>
#include <functional>
#include <sstream>
#include <string>
#include <vector>

namespace check {

struct Case {
	const char* name;
	std::function<void()> fn;
};
inline auto Cases() -> std::vector<Case>& {
	static std::vector<Case> cases;
	return cases;
}
inline int g_failures = 0;
inline const char* g_current = "";

struct Register {
	Register(const char* name, std::function<void()> fn) { Cases().push_back({name, std::move(fn)}); }
};

inline void Fail(const char* file, int line, const std::string& what) {
	++g_failures;
	std::printf("  FAIL %s:%d [%s] %s\n", file, line, g_current, what.c_str());
}

template <typename A, typename B>
void Eq(const A& a, const B& b, const char* ea, const char* eb, const char* file, int line) {
	if (!(a == b)) {
		std::ostringstream os;
		os << ea << " == " << eb << " (" << a << " vs " << b << ")";
		Fail(file, line, os.str());
	}
}

inline void Near(double a, double b, double eps, const char* ea, const char* eb, const char* file, int line) {
	if (!(std::fabs(a - b) <= eps)) {
		std::ostringstream os;
		os.precision(12);
		os << ea << " ~= " << eb << " (" << a << " vs " << b << ", eps " << eps << ")";
		Fail(file, line, os.str());
	}
}

inline auto RunAll() -> int {
	for (const Case& c : Cases()) {
		g_current = c.name;
		const int before = g_failures;
		c.fn();
		std::printf("%s %s\n", g_failures == before ? "ok  " : "FAIL", c.name);
	}
	std::printf("%zu tests, %d failed checks\n", Cases().size(), g_failures);
	return g_failures == 0 ? 0 : 1;
}

}  // namespace check

#define CHECK_CAT2(a, b) a##b
#define CHECK_CAT(a, b) CHECK_CAT2(a, b)
#define TEST(name)                                                                       \
	static void name();                                                                  \
	static ::check::Register CHECK_CAT(reg_, name)(#name, name);                         \
	static void name()
#define CHECK(cond) ((cond) ? (void)0 : ::check::Fail(__FILE__, __LINE__, #cond))
#define CHECK_EQ(a, b) ::check::Eq((a), (b), #a, #b, __FILE__, __LINE__)
#define CHECK_NEAR(a, b, eps) ::check::Near((a), (b), (eps), #a, #b, __FILE__, __LINE__)
