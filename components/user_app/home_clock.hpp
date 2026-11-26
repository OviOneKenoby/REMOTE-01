#pragma once
// HOME clock text. No ESP-IDF dependency, so it can be tested on the host.
#include <cstddef>
#include <ctime>

namespace home_clock {

// Europe/Bucharest with its summer-time rules, as the TUNER's default.
constexpr char timezone_rule[] = "EET-2EEST,M3.5.0/3,M10.5.0/4";
// Anything earlier means the clock was never set from the network.
constexpr std::time_t first_valid_time = 1700000000;

// Writes "HH:MM" (24 h, local time), or "--:--" while the time is unknown.
inline void format(std::time_t now, char (&out)[6]) {
	std::tm local{};
	if (now < first_valid_time || !localtime_r(&now, &local) ||
	    std::strftime(out, sizeof out, "%H:%M", &local) != 5) {
		const char unknown[] = "--:--";
		for (std::size_t i = 0; i < sizeof out; ++i) out[i] = unknown[i];
	}
}

} // namespace home_clock
