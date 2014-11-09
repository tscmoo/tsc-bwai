#define _CRT_SECURE_NO_WARNINGS
#define _SCL_SECURE_NO_WARNINGS

#pragma warning(disable: 4456) // warning C4456: declaration of 'x' hides previous local declaration
#pragma warning(disable: 4458) // warning C4458: declaration of 'x' hides class member
#pragma warning(disable: 4459) // warning C4459: declaration of 'x' hides global declaration
#pragma warning(disable: 4457) // warning C4457: declaration of 'x' hides function parameter

#include <cstdlib>
#include <cstdio>

#include <BWApi.h>
#include <BWAPI/Client.h>

#undef min
#undef max
#undef near
#undef far

#ifndef _DEBUG
#pragma comment(lib,"BWAPI.lib")
#pragma comment(lib,"BWAPIClient.lib")
#else
#pragma comment(lib,"BWAPId.lib")
#pragma comment(lib,"BWAPIClientd.lib")
#endif

#include <cmath>

#include <array>
#include <vector>
#include <set>
#include <map>
#include <unordered_set>
#include <unordered_map>
#include <queue>
#include <functional>
#include <numeric>
#include <memory>
#include <random>
#include <chrono>
#include <thread>
#include <initializer_list>
#include <mutex>
#include <algorithm>
#include <utility>

#include <tsc/containers.h>
#include <tsc/alloc_unique.h>
#include <tsc/alloc.h>
#include <tsc/alloc_containers.h>

#include <tsc/strf.h>

int current_frame;

struct simple_logger {
	std::mutex mut;
	tsc::a_string str, str2;
	bool newline;
	FILE*f;
	simple_logger() : newline(true) {
		f = fopen("log.txt","w");
	}
	template<typename...T>
	void operator()(const char*fmt,T&&...args) {
		std::lock_guard<std::mutex> lock(mut);
		try {
			tsc::strf::format(str, fmt, std::forward<T>(args)...);
		} catch (const std::exception&) {
			str = fmt;
		}
		if (newline) tsc::strf::format(str2,"%5d: %s",current_frame,str);
		const char*out_str= newline ? str2.c_str() : str.c_str();
		newline = strchr(out_str,'\n') ? true : false;
		if (f) {
			fputs(out_str,f);
			fflush(f);
		}
		fputs(out_str,stdout);
	}
};
simple_logger logger;

template<typename...T>
void log(const char*fmt,T&&...args) {
	logger(fmt,std::forward<T>(args)...);
}

struct xcept_t {
	tsc::a_string str1, str2;
	int n;
	xcept_t() {
		str1.reserve(0x100);
		str2.reserve(0x100);
		n = 0;
	}
	template<typename...T>
	void operator()(const char*fmt,T&&...args) {
		try {
			auto&str = ++n%2 ? str1 : str2;
			tsc::strf::format(str,fmt,std::forward<T>(args)...);
			log("about to throw exception %s\n",str);
#ifdef _DEBUG
			DebugBreak();
#endif
			throw (const char*)str.c_str();
		} catch (const std::bad_alloc&) {
			throw (const char*)fmt;
		}
	}
};
xcept_t xcept;

tsc::a_string format_string;
template<typename...T>
const char*format(const char*fmt,T&&...args) {
	return tsc::strf::format(format_string,fmt,std::forward<T>(args)...);
}

//#include <tsc/coroutine.h>
#include <tsc/userthreads.h>

#include <tsc/high_resolution_timer.h>
#include <tsc/rng.h>
#include <tsc/bitset.h>

using tsc::rng;
using tsc::a_string;
using tsc::a_vector;
using tsc::a_deque;
using tsc::a_list;
using tsc::a_set;
using tsc::a_multiset;
using tsc::a_map;
using tsc::a_multimap;
using tsc::a_unordered_set;
using tsc::a_unordered_multiset;
using tsc::a_unordered_map;
using tsc::a_unordered_multimap;
//using tsc::a_circular_queue;
template<typename T>
using a_unique_ptr = std::unique_ptr<T, tsc::allocate_unique_deleter<T, tsc::alloc<T>>>;
template<typename T>
auto allocate_unique() {
	return tsc::allocate_unique<T, tsc::alloc<T>>();
}

const double PI = 3.1415926535897932384626433;

BWAPI::Game*game;

int latency_frames;


double current_minerals, current_gas;
std::array<double,3> current_used_supply, current_max_supply;

double current_minerals_per_frame, current_gas_per_frame;
double predicted_minerals_per_frame, predicted_gas_per_frame;

enum {race_unknown = -1, race_terran = 0, race_protoss = 1, race_zerg = 2};

struct unit;
struct unit_stats;
struct unit_type;
template<typename> struct xy_typed;
typedef xy_typed<int> xy;
namespace square_pathing {
	void invalidate_area(xy from, xy to);
}

#include "ranges.h"
#include "common.h"
#include "multitasking.h"
#include "grid.h"
#include "stats.h"
#include "render.h"
#include "units.h"
#include "square_pathing.h"
#include "unit_controls.h"
#include "resource_gathering.h"
#include "resource_spots.h"
#include "creep.h"
#include "pylons.h"
#include "build_spot_finder.h"
#include "build.h"
#include "combat_eval.h"
#include "buildpred.h"
#include "combat.h"
#include "scouting.h"

void init() {

	multitasking::init();
	grid::init();
	stats::init();
	units::init();
	square_pathing::init();
	unit_controls::init();
	resource_gathering::init();
	resource_spots::init();
	creep::init();
	pylons::init();
	build_spot_finder::init();
	build::init();
	buildpred::init();
	combat::init();
	scouting::init();

}



struct module : BWAPI::AIModule {

	virtual void onStart() override {

		current_frame = game->getFrameCount();

		game->enableFlag(BWAPI::Flag::UserInput);

		game->setLocalSpeed(15);
		//game->setLocalSpeed(30);
		//game->setLocalSpeed(0);

		if (!game->self()) return;

		init();

	}

	virtual void onEnd(bool is_winner) override {
		multitasking::stop();
	}

	virtual void onFrame() override {

		static bool holding_x;
		if (GetKeyState('X') & 0x80) holding_x = true;
		else holding_x = false;
		static bool last_holding_x;
		static bool fast = false;
		if (last_holding_x&&!holding_x) fast=!fast;
		last_holding_x = holding_x;
		if (fast) {
			game->setLocalSpeed(0);
		} else {
			game->setLocalSpeed(30);
		}

		current_frame = game->getFrameCount();
		latency_frames = game->getRemainingLatencyFrames();
		if (!game->self()) return;

		tsc::high_resolution_timer ht;

		static a_map<multitasking::task_id, double> last_cpu_time;
		for (auto id : multitasking::detail::running_tasks) {
			last_cpu_time[id] = multitasking::detail::tasks[id].cpu_time;
		}

		multitasking::run();
		render::render();

		double elapsed = ht.elapsed();
		if (elapsed >= 1.0 / 20) {
			log(" WARNING: frame took %fs!\n", elapsed);
			for (auto id : multitasking::detail::running_tasks) {
				auto&t = multitasking::detail::tasks[id];
				log(" - %s took %f\n", t.name, t.cpu_time - last_cpu_time[id]);
			}
		}

	}

	
	virtual void onUnitShow(BWAPI::Unit gu) override {
		units::on_unit_show(gu);
	};

	virtual void onUnitHide(BWAPI::Unit gu) override {
		units::on_unit_hide(gu);
	};

	virtual void onUnitCreate(BWAPI::Unit gu) override {
		units::on_unit_create(gu);
	}

	virtual void onUnitMorph(BWAPI::Unit gu) override {
		units::on_unit_morph(gu);
	}

	virtual void onUnitDestroy(BWAPI::Unit gu) override {
		units::on_unit_destroy(gu);
	}

	virtual void onUnitRenegade(BWAPI::Unit gu) override {
		units::on_unit_renegade(gu);
	}

	virtual void onUnitComplete(BWAPI::Unit gu) override {
		units::on_unit_complete(gu);
	}

};

int main() {

	//BWAPI::BWAPI_init();
	log("connecting\n");
	while (!BWAPI::BWAPIClient.connect()) std::this_thread::sleep_for(std::chrono::seconds(1));

	game = BWAPI::BroodwarPtr;

	log("waiting for game start\n");
	while (BWAPI::BWAPIClient.isConnected() && !game->isInGame()) {
		BWAPI::BWAPIClient.update();
	}
	module m;
	while (BWAPI::BWAPIClient.isConnected() && game->isInGame()) {
		for (auto&e : game->getEvents()) {
			switch (e.getType()) {
			case BWAPI::EventType::MatchStart:
				m.onStart();
				break;
			case BWAPI::EventType::MatchEnd:
				m.onEnd(e.isWinner());
				break;
			case BWAPI::EventType::MatchFrame:
				m.onFrame();
				break;
			case BWAPI::EventType::UnitShow:
				m.onUnitShow(e.getUnit());
				break;
			case BWAPI::EventType::UnitHide:
				m.onUnitHide(e.getUnit());
				break;
			case BWAPI::EventType::UnitCreate:
				m.onUnitCreate(e.getUnit());
				break;
			case BWAPI::EventType::UnitMorph:
				m.onUnitMorph(e.getUnit());
				break;
			case BWAPI::EventType::UnitDestroy:
				m.onUnitDestroy(e.getUnit());
				break;
			case BWAPI::EventType::UnitRenegade:
				m.onUnitRenegade(e.getUnit());
				break;
			case BWAPI::EventType::UnitComplete:
				m.onUnitComplete(e.getUnit());
				break;
			}
		}
		BWAPI::BWAPIClient.update();
		if (!BWAPI::BWAPIClient.isConnected()) {
			log("disconnected\n");
			break;
		}
	}
	log("game over\n");

	return 0;
}

