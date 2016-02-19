#ifndef TSC_HIGH_RESOLUTION_TIMER_H
#define TSC_HIGH_RESOLUTION_TIMER_H

#ifndef _WIN32
#include <chrono>
#endif

namespace tsc {
;

namespace win_detail {
	using LARGE_INTEGER = int64_t;
	extern "C" int __stdcall QueryPerformanceCounter(LARGE_INTEGER * lpPerformanceCount);
	extern "C" int __stdcall QueryPerformanceFrequency(LARGE_INTEGER * lpFrequency);
};

#ifdef _WIN32
struct high_resolution_timer {
	int64_t last_count, freq;
	high_resolution_timer() {
		reset();
	}
	void reset() {
		win_detail::QueryPerformanceFrequency(&(win_detail::LARGE_INTEGER&)freq);
		win_detail::QueryPerformanceCounter(&(win_detail::LARGE_INTEGER&)last_count);
	}
	int64_t count() {
		int64_t count;
		win_detail::QueryPerformanceCounter(&(win_detail::LARGE_INTEGER&)count);
		return count;
	}
	double elapsed() {
		int64_t count;
		win_detail::QueryPerformanceCounter(&(win_detail::LARGE_INTEGER&)count);
		return (double)(count-last_count) / freq;
	}
	double elapsed_and_reset() {
		int64_t count;
		win_detail::QueryPerformanceCounter(&(win_detail::LARGE_INTEGER&)count);
		win_detail::QueryPerformanceFrequency(&(win_detail::LARGE_INTEGER&)freq);
		double r = (double)(count-last_count) / freq;
		last_count = count;
		return r;
	}
};
#else
struct high_resolution_timer {
	typedef std::chrono::high_resolution_clock clock_t;
	clock_t::time_point last_count;
	high_resolution_timer() {
		reset();
	}
	void reset() {
		last_count = clock_t::now();
	}
	double elapsed() {
		auto now = clock_t::now();
		return std::chrono::duration_cast<std::chrono::duration<double>>(now - last_count).count();
	}
	double elapsed_and_reset() {
		auto now = clock_t::now();
		double r = std::chrono::duration_cast<std::chrono::duration<double>>(now - last_count).count();
		last_count = now;
		return r;
	}
	int64_t count() {
		return std::chrono::duration_cast<std::chrono::duration<int64_t,std::nano>>(clock_t::now().time_since_epoch()).count();
	}
};
#endif

}

#endif

