//
// This file contains some common declarations, like container types,
// string formatting and utility functions.
// Also contains some globals which don't really belong anywhere.
//

#ifndef TSC_BWAI_COMMON_H
#define TSC_BWAI_COMMON_H

#ifdef _WIN32
#include <windows.h>

#undef min
#undef max
#undef near
#undef far
#endif

#include "containers.h"
#include "global.h"
#include "log.h"
#include "tsc/intrusive_list.h"
#include "tsc/strf.h"
#include "tsc/high_resolution_timer.h"
#include <memory>
#include <utility>
#include <random>

namespace tsc_bwai {

	extern tsc_bwai::global<int> goo;
	extern global<bool> test_mode;
	extern global<int> current_frame;


	extern global<a_string> format_string;
	// Type-safe printf-like formatting function.
	// Returns a pointer to a null terminated string, which is only valid until
	// the next time format is called, so you might want to assign it to an a_string.
	template<typename...T>
	const char* format(const char* fmt, T&&...args) {
		return tsc::strf::format(format_string, fmt, std::forward<T>(args)...);
	}

	extern global<std::default_random_engine> rng_engine;

	// Returns a random integer in the range [0, v)
	template<typename T, typename std::enable_if<std::is_integral<T>::value>::type* = nullptr>
	static T rng(T v) {
		std::uniform_int_distribution<T> dis(0, v - 1);
		return dis(rng_engine);
	}
	// Returns a random floating point in the range [0, v)
	template<typename T, typename std::enable_if<std::is_floating_point<T>::value>::type* = nullptr>
	static T rng(T v) {
		std::uniform_real_distribution<T> dis(0, v);
		return dis(rng_engine);
	}

	struct xcept_t : std::exception {
		const char*str;
		xcept_t(const char*str) : str(str) {}
		virtual const char*what() const throw() {
			return str;
		}
	};

	struct xcept_wrapper_t {
		a_string str1, str2;
		int n;
		xcept_wrapper_t() {
			str1.reserve(0x100);
			str2.reserve(0x100);
			n = 0;
		}
		template<typename...T>
		void operator()(const char*fmt, T&&...args) {
			try {
				auto&str = ++n % 2 ? str1 : str2;
				tsc::strf::format(str, fmt, std::forward<T>(args)...);
				log(log_level_info, "about to throw exception %s\n", str);
				throw xcept_t(str.c_str());
			} catch (const std::bad_alloc&) {
				throw xcept_t(fmt);
			}
		}
	};
	static xcept_wrapper_t xcept_wrapper;
	template<typename...args>
	void xcept(args&&...v) {
		xcept_wrapper(std::forward<args>(v)...);
	}

}

#endif
