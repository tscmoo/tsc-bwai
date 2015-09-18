
#ifndef TSC_RNG_H
#define TSC_RNG_H

#include "high_resolution_timer.h"

namespace tsc {
;

std::default_random_engine rng_engine([]{
	high_resolution_timer ht;
	std::array<unsigned int,4> arr;
	arr[0] = std::random_device()();
	//arr[1] = (unsigned int)std::chrono::high_resolution_clock::now().time_since_epoch().count();
	arr[1] = (unsigned int)0;
	arr[1] = (unsigned int)0;
	arr[2] = (unsigned int)std::hash<std::thread::id>()(std::this_thread::get_id());
	arr[3] = (unsigned int)ht.count();
	std::seed_seq seq(arr.begin(),arr.end());
	std::default_random_engine e(seq);
	return e;
}());


template<typename T, typename std::enable_if<std::is_integral<T>::value>::type* = nullptr>
T rng(T v) {
	std::uniform_int_distribution<T> dis(0,v-1);
	return dis(rng_engine);
}
template<typename T, typename std::enable_if<std::is_floating_point<T>::value>::type* = nullptr>
T rng(T v) {
	std::uniform_real_distribution<T> dis(0,v);
	return dis(rng_engine);
}

}


#endif
