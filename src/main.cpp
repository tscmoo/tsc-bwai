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

#ifdef _WIN32
#include <windows.h>
#endif

#undef min
#undef max
#undef near
#undef far

#include "tsc/intrusive_list.h"
#include "tsc/alloc.h"
#include "tsc/alloc_containers.h"

#include "tsc/strf.h"

constexpr bool test_mode = false;

int current_frame;

struct simple_logger {
	std::mutex mut;	
	tsc::a_string str, str2;
	bool newline = true;
	FILE*f = nullptr;
	simple_logger() {	
		if (test_mode) f = fopen("log.txt", "w");
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


enum {
	log_level_all,
	log_level_debug,
	log_level_info
};

int current_log_level = test_mode ? log_level_all : log_level_info;

template<typename...T>
void log(int level, const char*fmt, T&&...args) {
	if (current_log_level <= level) logger(fmt, std::forward<T>(args)...);
}

template<typename...T>
void log(const char*fmt, T&&...args) {
	log(log_level_debug, fmt, std::forward<T>(args)...);
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
			log(log_level_info, "about to throw exception %s\n", str);
//#ifdef _DEBUG
			DebugBreak();
//#endif
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

#include "tsc/userthreads.h"

#include "tsc/high_resolution_timer.h"
#include "tsc/rng.h"
#include "tsc/bitset.h"

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

#include "tsc/json.h"

struct default_link_f {
	template<typename T>
	auto* operator()(T*ptr) {
		return (std::pair<T*, T*>*)&ptr->link;
	}
};

const double PI = 3.1415926535897932384626433;

BWAPI::Game*game;

constexpr bool is_bwapi_4 = std::is_pointer<BWAPI::Player>::value;

using BWAPI_Player = std::conditional<is_bwapi_4, BWAPI::Player, BWAPI::Player*>::type;
using BWAPI_Unit = std::conditional<is_bwapi_4, BWAPI::Unit, BWAPI::Unit*>::type;
using BWAPI_Bullet = std::conditional<is_bwapi_4, BWAPI::Bullet, BWAPI::Bullet*>::type;

struct bwapi_pos {
	int x, y;
	bwapi_pos(BWAPI::Position pos) : bwapi_pos((bwapi_pos&)pos) {}
	bwapi_pos(BWAPI::TilePosition pos) : bwapi_pos((bwapi_pos&)pos) {}
	template<typename T=xy>
	operator T() const {
		return T(x, y);
	}
};

template<bool b = is_bwapi_4, typename std::enable_if<b>::type* = 0>
bool bwapi_call_build(BWAPI_Unit unit, BWAPI::UnitType type, BWAPI::TilePosition pos) {
	return unit->build(type, pos);
}
template<bool b = is_bwapi_4, typename std::enable_if<!b>::type* = 0>
bool bwapi_call_build(BWAPI_Unit unit, BWAPI::UnitType type, BWAPI::TilePosition pos) {
	return unit->build(pos, type);
}

template<bool b = is_bwapi_4, typename std::enable_if<b>::type* = 0>
int bwapi_tech_type_energy_cost(BWAPI::TechType type) {
	return type.energyCost();
}
template<bool b = is_bwapi_4, typename std::enable_if<!b>::type* = 0>
int bwapi_tech_type_energy_cost(BWAPI::TechType type) {
	return type.energyUsed();
}

template<bool b = is_bwapi_4, typename std::enable_if<b>::type* = 0>
bool bwapi_is_powered(BWAPI_Unit unit) {
	return unit->isPowered();
}
template<bool b = is_bwapi_4, typename std::enable_if<!b>::type* = 0>
bool bwapi_is_powered(BWAPI_Unit unit) {
	return !unit->isUnpowered();	
}

template<bool b = is_bwapi_4, typename std::enable_if<b>::type* = 0>
bool bwapi_is_healing_order(BWAPI::Order order) {
	return order == BWAPI::Orders::MedicHeal || order == BWAPI::Orders::MedicHealToIdle;
}
template<bool b = is_bwapi_4, typename std::enable_if<!b>::type* = 0>
bool bwapi_is_healing_order(BWAPI::Order order) {
	return order == BWAPI::Orders::MedicHeal1 || order == BWAPI::Orders::MedicHeal2;
}

int latency_frames;
int current_command_frame;

double current_minerals, current_gas;
std::array<double,3> current_used_supply, current_max_supply;
double current_used_total_supply;

double current_minerals_per_frame, current_gas_per_frame;
double predicted_minerals_per_frame, predicted_gas_per_frame;

enum {race_unknown = -1, race_terran = 0, race_protoss = 1, race_zerg = 2};

struct player_t;
struct upgrade_type;
struct unit;
struct unit_stats;
struct unit_type;


#include "ranges.h"
#include "common.h"

a_deque<a_string> send_text_queue;
void send_text(a_string str) {
	if (current_frame < 30 || !send_text_queue.empty()) send_text_queue.push_back(str);
	else game->sendText("%s", str.c_str());
}

namespace square_pathing {
	void invalidate_area(xy from, xy to);
}
namespace wall_in {
	void lift_please(unit*);
	int active_wall_count();
}
a_vector<xy> start_locations;
xy my_start_location;
namespace combat {
	bool can_transfer_through(xy pos);
}

#include "multitasking.h"
#include "multitasking_sync.h"
#include "grid.h"
#include "stats.h"
#include "render.h"
#include "players.h"
#include "units.h"
#include "upgrades.h"
#include "square_pathing.h"
#include "flyer_pathing.h"
#include "unit_controls.h"
#include "resource_gathering.h"
#include "resource_spots.h"
#include "creep.h"
#include "pylons.h"
#include "build_spot_finder.h"
#include "build.h"
#include "combat_eval.h"
#include "buildpred.h"
#include "wall_in.h"
#include "combat.h"
#include "scouting.h"
#include "get_upgrades.h"
#include "strategy.h"

void init() {

	for (auto&v : game->getStartLocations()) {
		start_locations.emplace_back(((bwapi_pos)v).x * 32, ((bwapi_pos)v).y * 32);
	}
	my_start_location = xy(((bwapi_pos)game->self()->getStartLocation()).x * 32, ((bwapi_pos)game->self()->getStartLocation()).y * 32);

	multitasking::init();
	grid::init();
	stats::init();
	players::init();
	units::init();
	upgrades::init();
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
	get_upgrades::init();
	wall_in::init();
	strategy::init();

}



struct module : BWAPI::AIModule {

	virtual void onStart() override {

		log(log_level_info, "game start\n");

		current_frame = game->getFrameCount();

		game->enableFlag(BWAPI::Flag::UserInput);
		game->setCommandOptimizationLevel(2);
		game->setLocalSpeed(0);

		if (!game->self()) return;

		send_text("tsc-bwai v0.5.4");

		init();

	}

	virtual void onEnd(bool is_winner) override {
		strategy::on_end(is_winner);
		multitasking::stop();
	}

	template<bool is_bwapi_4>
	struct x_key;
	template<>
	struct x_key<true> {
		static const auto value = BWAPI::Key::K_X;
	};
	template<>
	struct x_key<false> {
		static const auto value = 'X';
	};

	virtual void onFrame() override {

		if (test_mode) {
			static bool holding_x;
			if (game->getKeyState(x_key<is_bwapi_4>::value)) holding_x = true;
			else holding_x = false;
			static bool last_holding_x;
			static bool fast = true;
			if (last_holding_x && !holding_x) {
				fast = !fast;
				if (fast) {
					game->setLocalSpeed(0);
				} else {
					game->setLocalSpeed(30);
				}
			}
			last_holding_x = holding_x;
		}

		current_frame = game->getFrameCount();
		latency_frames = game->getRemainingLatencyFrames();
		current_command_frame = current_frame + latency_frames;
		if (!game->self()) return;

		tsc::high_resolution_timer ht;

		static a_map<multitasking::task_id, double> last_cpu_time;
		for (auto id : multitasking::detail::running_tasks) {
			last_cpu_time[id] = multitasking::get_cpu_time(id);
		}

		multitasking::run();
		render::render();

		double elapsed = ht.elapsed();
		if (elapsed >= 1.0 / 20) {
			log(log_level_info, " WARNING: frame took %fs!\n", elapsed);
			for (auto id : multitasking::detail::running_tasks) {
				auto&t = multitasking::detail::tasks[id];
				log(log_level_info, " - %s took %f\n", t.name, multitasking::get_cpu_time(id) - last_cpu_time[id]);
			}
		}

		if (current_frame >= 30 && !send_text_queue.empty()) {
			a_string str = send_text_queue.front();
			send_text_queue.pop_front();
			game->sendText("%s", str.c_str());
		}

	}

	
	virtual void onUnitShow(BWAPI_Unit gu) override {
		units::on_unit_show(gu);
	};

	virtual void onUnitHide(BWAPI_Unit gu) override {
		units::on_unit_hide(gu);
	};

	virtual void onUnitCreate(BWAPI_Unit gu) override {
		units::on_unit_create(gu);
	}

	virtual void onUnitMorph(BWAPI_Unit gu) override {
		units::on_unit_morph(gu);
	}

	virtual void onUnitDestroy(BWAPI_Unit gu) override {
		units::on_unit_destroy(gu);
	}

	virtual void onUnitRenegade(BWAPI_Unit gu) override {
		units::on_unit_renegade(gu);
	}

	virtual void onUnitComplete(BWAPI_Unit gu) override {
		units::on_unit_complete(gu);
	}

};


template<bool b = is_bwapi_4, typename std::enable_if<b>::type* = 0>
void bwapi_connect() {
	while (!BWAPI::BWAPIClient.connect()) std::this_thread::sleep_for(std::chrono::milliseconds(250));
	game = BWAPI::BroodwarPtr;
}
template<bool b = is_bwapi_4, typename std::enable_if<!b>::type* = 0>
void bwapi_connect() {
	BWAPI::BWAPI_init();
	while (!BWAPI::BWAPIClient.connect()) std::this_thread::sleep_for(std::chrono::milliseconds(250));
	game = BWAPI::Broodwar;
}

//#define IS_DLL

#ifndef IS_DLL

int main() {

	log(log_level_info, "connecting\n");
	bwapi_connect();

	log(log_level_info, "waiting for game start\n");
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
			case BWAPI::EventType::SendText:
				game->sendText("%s", e.getText().c_str());
				break;
			}
		}
		BWAPI::BWAPIClient.update();
		if (!BWAPI::BWAPIClient.isConnected()) {
			log(log_level_info, "disconnected\n");
			break;
		}
	}
	log(log_level_info, "game over\n");

	return 0;
}

#else

template<bool b = is_bwapi_4, typename std::enable_if<b>::type* = 0>
__declspec(dllexport) void gameInit_impl(BWAPI::Game* game) {
	BWAPI::BroodwarPtr = game;
	::game = game;
}
template<bool b = is_bwapi_4, typename std::enable_if<!b>::type* = 0>
__declspec(dllexport) void gameInit_impl(BWAPI::Game* game) {
	BWAPI::Broodwar = game;
	::game = game;
}
template<bool b = is_bwapi_4, typename std::enable_if<b>::type* = 0>
__declspec(dllexport) BWAPI::AIModule*newAIModule_impl(BWAPI::Game*game) {
	return new module();
}
template<bool b = is_bwapi_4, typename std::enable_if<!b>::type* = 0>
__declspec(dllexport) BWAPI::AIModule*newAIModule_impl(BWAPI::Game*game) {
	BWAPI::Broodwar = game;
	::game = game;
	return new module();
}

extern "C" __declspec(dllexport) void gameInit(BWAPI::Game* game) {
	gameInit_impl(game);
}
//extern "C" __declspec(dllexport) BWAPI::AIModule*newAIModule(BWAPI::Game*game) {
extern "C" __declspec(dllexport) BWAPI::AIModule*newAIModule() {
	return newAIModule_impl(nullptr);
}

#endif
