//
// This file defines the common module, which contains some variables
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
#include "tsc/strf.h"
#include "tsc/high_resolution_timer.h"
#include <memory>
#include <utility>
#include <random>

namespace tsc_bwai {

	struct unit;
	struct unit_stats;
	struct unit_type;

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


	struct common_module {
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


	// Estimates how long a unit with the specified stats would take to move
	// distance (including stopping).
	int frames_to_reach(double initial_speed, double acceleration, double max_speed, double stop_distance, double distance);
	int frames_to_reach(unit_stats* stats, double initial_speed, double distance);
	int frames_to_reach(unit*u, double distance);

	// Returns the distance from the origin, "moving" only along straight or
	// diagonal lines. Used as a fast approximation to euclidean distance.
	double diag_distance(xy pos);

	// Returns the Manhattan distance from the origin.
	// abs(x) + abs(y)
	double manhattan_distance(xy pos);

	// Returns the difference between the nearest points in two units bounding
	// boxes, with inputs their positions and dimensions (unit_type::dimensions).
	xy units_difference(xy a_pos, std::array<int, 4> a_dimensions, xy b_pos, std::array<int, 4> b_dimensions);

	// Returns the distance between two units, as used for instance by weapon range
	// calculations.
	double units_distance(unit* a, unit* b);
	double units_distance(xy a_pos, unit* a, xy b_pos, unit* b);
	double units_distance(xy a_pos, unit_type* at, xy b_pos, unit_type* bt);

	// Returns the position in the rectangle [top_left, bottom_right] that is
	// nearest to pos.
	xy nearest_spot_in_square(xy pos, xy top_left, xy bottom_right);


	template<typename T>
	struct refcounter {
		T&obj;
		refcounter(T&obj) : obj(obj) {
			obj.addref();
		}
		~refcounter() {
			obj.release();
		}
	};

	struct refcounted {
		int reference_count;
		refcounted() : reference_count(0) {}
		void addref() {
			++reference_count;
		}
		int release() {
			return --reference_count;
		}
	};

	template<typename T>
	struct refcounted_ptr {
		T*ptr = nullptr;
		refcounted_ptr() {}
		refcounted_ptr(T*ptr) : ptr(ptr) {
			if (ptr) ptr->addref();
		}
		refcounted_ptr(const refcounted_ptr&n) {
			*this = n;
		}
		refcounted_ptr(refcounted_ptr&&n) {
			*this = std::move(n);
		}
		~refcounted_ptr() {
			if (ptr) ptr->release();
		}
		operator T*() const {
			return ptr;
		}
		T*operator->() const {
			return ptr;
		}
		operator bool() const {
			return ptr ? true : false;
		}
		bool operator==(const refcounted_ptr&n) const {
			return ptr == n.ptr;
		}
		refcounted_ptr&operator=(const refcounted_ptr&n) {
			if (ptr) ptr->release();
			ptr = n.ptr;
			if (ptr) ptr->addref();
			return *this;
		}
		refcounted_ptr&operator=(refcounted_ptr&&n) {
			std::swap(ptr, n.ptr);
			return *this;
		}
	};

	template<int = 0>
	a_string short_type_name(unit_type*type) {
		if (type->game_unit_type == BWAPI::UnitTypes::Terran_Command_Center) return "CC";
		if (type->game_unit_type == BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode) return "Tank";
		a_string&s = type->name;
		if (s.find("Terran ") == 0) return s.substr(7);
		if (s.find("Protoss ") == 0) return s.substr(8);
		if (s.find("Zerg ") == 0) return s.substr(5);
		if (s.find("Terran_") == 0) return s.substr(7);
		if (s.find("Protoss_") == 0) return s.substr(8);
		if (s.find("Zerg_") == 0) return s.substr(5);

		return s;
	}

	struct no_value_t {};
	static const no_value_t no_value;
	template<typename A, typename B>
	bool is_same_but_not_no_value(A&&a, B&&b) {
		return a == b;
	}
	template<typename A>
	bool is_same_but_not_no_value(A&&a, no_value_t) {
		return false;
	}
	template<typename B>
	bool is_same_but_not_no_value(no_value_t, B&&b) {
		return false;
	}
	template<typename T>
	struct identity {
		const T&operator()(const T&v) const {
			return v;
		}
	};
	struct identity_any {
		template<typename T>
		const T&operator()(const T&v) const {
			return v;
		}
	};

	// For each v in cont, where r = score(v), return the accompanying v for the lowest r.
	// If score(v) evaluates equal to invalid_score, then it is skipped.
	// If score(v) evaluates equal to best_possible_score, then v is returned immediately.
	// If cont is empty, cont::value_type{} is returned.
	template<typename cont_T, typename score_F = identity<cont_T::value_type>, typename invalid_score_T = no_value_t, typename best_possible_score_T = no_value_t>
	typename cont_T::value_type get_best_score(cont_T&cont, score_F&&score = score_F(), const invalid_score_T invalid_score = invalid_score_T(), const best_possible_score_T best_possible_score = best_possible_score_T()) {
		std::remove_const<std::remove_reference<std::result_of<score_F(cont_T::value_type)>::type>::type>::type best_score{};
		cont_T::value_type best{};
		int n = 0;
		for (auto&&v : cont) {
			auto s = score(v);
			if (is_same_but_not_no_value(s, invalid_score)) continue;
			if (n == 0 || s < best_score) {
				++n;
				best = v;
				best_score = s;
				if (is_same_but_not_no_value(s, best_possible_score)) break;
			}
		}
		return best;
	}
	// For each v in cont, where r = score(v), return the accompanying &v for the lowest r.
	// If score(&v) evaluates equal to invalid_score, then it is skipped.
	// If score(&v) evaluates equal to best_possible_score, then &v is returned immediately.
	// If cont is empty, nullptr is returned.
	template<typename cont_T, typename score_F, typename invalid_score_T = no_value_t, typename best_possible_score_T = no_value_t>
	typename cont_T::pointer get_best_score_p(cont_T&cont, score_F&&score, const invalid_score_T invalid_score = invalid_score_T(), const best_possible_score_T best_possible_score = best_possible_score_T()) {
		return get_best_score(make_transform_filter(cont, [&](cont_T::reference v) {
			return &v;
		}), std::forward<score_F>(score), invalid_score, best_possible_score);
	}
	// For each v in cont, where r = score(v), return the lowest r.
	// If score(v) evaluates equal to invalid_score, then it is skipped.
	// If score(v) evaluates equal to best_possible_score, then it is returned immediately.
	// If cont is empty, a directly initialized object of the type of the return
	// value of score(v) is returned.
	template<typename cont_T, typename score_F, typename invalid_score_T = no_value_t, typename best_possible_score_T = no_value_t>
	typename std::result_of<score_F(typename cont_T::value_type)>::type get_best_score_value(cont_T& cont, score_F&& score, const invalid_score_T invalid_score = invalid_score_T(), const best_possible_score_T best_possible_score = best_possible_score_T()) {
		auto t_cont = make_transform_filter(cont, std::forward<score_F>(score));
		return get_best_score(t_cont, identity_any(), invalid_score, best_possible_score);
	}

	// Convenience function for finding and erasing a value in a vector (relative
	// order of elements preserved).
	template<typename cont_T, typename val_T>
	typename cont_T::iterator find_and_erase(cont_T& cont, val_T&& val) {
		return cont.erase(std::find(cont.begin(), cont.end(), std::forward<val_T>(val)));
	}

	// Returns true if pred(v) evaluates to true for any v in cont.
	template<typename cont_T, typename pred_T>
	bool test_pred(cont_T& cont, pred_T&& pred) {
		for (auto& v : cont) {
			if (pred(v)) return true;
		}
		return false;
	}

}

#endif
