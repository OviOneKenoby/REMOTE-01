// Build: g++ -std=c++17 -Icomponents/user_app tests/control/home_clock_tests.cpp
// The summer-time checks need a C library that reads POSIX time-zone rules;
// the Windows library does not, so there only format and placeholder run (UTC).
#include <cassert>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#ifdef _WIN32
static inline std::tm *localtime_r(const std::time_t *t, std::tm *out) {
	return localtime_s(out, t) ? nullptr : out;
}
#endif
#include "home_clock.hpp"

static bool shows(std::time_t t, const char *expected) {
	char out[6];
	home_clock::format(t, out);
	if (std::strcmp(out, expected) == 0) return true;
	std::printf("%lld: got %s, expected %s\n", static_cast<long long>(t), out, expected);
	return false;
}

int main() {
#ifdef _WIN32
	_putenv_s("TZ", "UTC0");
	_tzset();
	assert(shows(0, "--:--"));
	assert(shows(home_clock::first_valid_time - 1, "--:--"));
	assert(shows(1768471500, "10:05"));
	assert(shows(1768471559, "10:05"));
	assert(shows(1768471560, "10:06"));
	// 24 h format around midnight.
	assert(shows(1768521599, "23:59") && shows(1768521600, "00:00"));
	assert(shows(1768482300, "13:05"));
	std::puts("HOME clock format: PASS (summer-time rules NOT tested here)");
#else
	setenv("TZ", home_clock::timezone_rule, 1);
	tzset();
	assert(shows(0, "--:--"));
	// 2026-01-15 10:05:00 UTC: winter time, UTC+2.
	assert(shows(1768471500, "12:05") && shows(1768471560, "12:06"));
	// 2026-07-15 10:05:00 UTC: summer time, UTC+3.
	assert(shows(1784109900, "13:05"));
	// Summer time starts 2026-03-29 01:00 UTC and ends 2026-10-25 01:00 UTC.
	assert(shows(1774745999, "02:59") && shows(1774746000, "04:00"));
	assert(shows(1792889999, "03:59") && shows(1792890000, "03:00"));
	std::puts("HOME clock: PASS");
#endif
}
