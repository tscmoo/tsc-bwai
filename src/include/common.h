//
// This file defines the common_t class, which contains some variables
// and functions which are frequently used but don't really belong anywhere
// special.
// It also contains some container types, string formatting and utility 
// functions.
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
#include "log.h"
#include "tsc/intrusive_list.h"
#include "tsc/strf.h"
#include "tsc/high_resolution_timer.h"
#include <memory>
#include <utility>
#include <random>

namespace tsc_bwai {

	struct xcept_t : std::exception {
		a_string str;
		xcept_t() {}
		xcept_t(const char* str) : str(str) {}
		virtual const char* what() const throw() {
			return str.c_str();
		}
	};

	template<typename...T>
	void xcept(const char*fmt, T&& ...args) {
		xcept_t e;
		tsc::strf::format(e.str, fmt, std::forward<T>(args)...);
		throw std::move(e);
	}

	struct common_t {
		std::default_random_engine rng_engine;

		// Returns a random integer in the range [0, v)
		template<typename T, typename std::enable_if<std::is_integral<T>::value>::type* = nullptr>
		T rng(T v) {
			std::uniform_int_distribution<T> dis(0, v - 1);
			return dis(rng_engine);
		}
		// Returns a random floating point in the range [0, v)
		template<typename T, typename std::enable_if<std::is_floating_point<T>::value>::type* = nullptr>
		T rng(T v) {
			std::uniform_real_distribution<T> dis(0, v);
			return dis(rng_engine);
		}
	};

	// Type-safe printf-like formatting function.
	template<typename...T>
	a_string format(const char* fmt, T&&...args) {
		a_string r;
		tsc::strf::format(r, fmt, std::forward<T>(args)...);
		return r;
	}

	enum { race_unknown = -1, race_terran = 0, race_protoss = 1, race_zerg = 2 };

	const double PI = 3.1415926535897932384626433;


	template<typename utype>
	struct xy_typed {
		utype x, y;
		typedef xy_typed<utype> xy;
		xy() : x(0), y(0) {}
		xy(utype x, utype y) : x(x), y(y) {}
		bool operator<(const xy&n) const {
			if (y == n.y) return x < n.x;
			return y < n.y;
		}
		bool operator>(const xy& n) const {
			if (y == n.y) return x > n.x;
			return y > n.y;
		}
		bool operator<=(const xy& n) const {
			if (y == n.y) return x <= n.x;
			return y <= n.y;
		}
		bool operator>=(const xy& n) const {
			if (y == n.y) return x >= n.x;
			return y >= n.y;
		}
		bool operator==(const xy& n) const {
			return x == n.x && y == n.y;
		}
		bool operator!=(const xy& n) const {
			return x != n.x || y != n.y;
		}
		xy operator-(const xy& n) const {
			xy r(*this);
			return r -= n;
		}
		xy&operator-=(const xy& n) {
			x -= n.x;
			y -= n.y;
			return *this;
		}
		xy operator+(const xy& n) const {
			xy r(*this);
			return r += n;
		}
		xy&operator+=(const xy& n) {
			x += n.x;
			y += n.y;
			return *this;
		}
		double length() const {
			return sqrt(x*x + y*y);
		}
		xy operator -() const {
			return xy(-x, -y);
		}
		double angle() const {
			return atan2(y, x);
		}
		xy operator/(const xy& n) const {
			xy r(*this);
			return r /= n;
		}
		xy&operator/=(const xy& n) {
			x /= n.x;
			y /= n.y;
			return *this;
		}
		xy operator/(utype d) const {
			xy r(*this);
			return r /= d;
		}
		xy&operator/=(utype d) {
			x /= d;
			y /= d;
			return *this;
		}
		xy operator*(const xy& n) const {
			xy r(*this);
			return r *= n;
		}
		xy&operator*=(const xy& n) {
			x *= n.x;
			y *= n.y;
			return *this;
		}
		xy operator*(utype d) const {
			xy r(*this);
			return r *= d;
		}
		xy&operator*=(utype d) {
			x *= d;
			y *= d;
			return *this;
		}
	};

	typedef xy_typed<int> xy;

}

#endif
