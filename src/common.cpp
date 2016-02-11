//
// This file implements the common globals, and maybe some other common things.
//


#include "common.h"
#include "global.h"
#include <chrono>
#include <thread>
using namespace tsc_bwai;

global<bool> tsc_bwai::test_mode = true;
global<int> tsc_bwai::current_frame;

global<std::default_random_engine> rng_engine([] {
	tsc::high_resolution_timer ht;
	std::array<unsigned int, 4> arr;
	arr[0] = std::random_device()();
	arr[1] = (unsigned int)std::chrono::high_resolution_clock::now().time_since_epoch().count();
	arr[2] = (unsigned int)std::hash<std::thread::id>()(std::this_thread::get_id());
	arr[3] = (unsigned int)ht.count();
	std::seed_seq seq(arr.begin(), arr.end());
	std::default_random_engine e(seq);
	return e;
}());
