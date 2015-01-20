
namespace strategy {
;

struct strat_exception : std::exception {};
struct strat_failed : strat_exception {};

a_vector<xy> possible_start_locations;

void update_possible_start_locations() {
	possible_start_locations.clear();
	for (auto&p : start_locations) {
		if (p == my_start_location) continue;
		if (grid::get_build_square(p).last_seen > 30 && !enemy_buildings.empty()) {
			double nearest_building = get_best_score_value(enemy_buildings, [&](unit*e) {
				return diag_distance(e->pos - p);
			});
			if (nearest_building > 15 * 30) continue;
			possible_start_locations.clear();
			possible_start_locations.push_back(p);
			break;
		}
		possible_start_locations.push_back(p);
	}

	for (auto&p : possible_start_locations) {
		if (p == my_start_location) continue;
		auto*s = get_best_score_p(resource_spots::spots, [&](const resource_spots::spot*s) {
			return diag_distance(p - s->pos);
		});
		if (s) {
			bool found = test_pred(s->resources, [](resource_spots::resource_t&r) {
				return r.u->resources <= r.u->initial_resources - 8 * 4;
			});
			auto&bs = grid::get_build_square(s->cc_build_pos);
			if (bs.building && bs.building->owner == players::opponent_player) found = true;
			if (found) {
				xy pos = p;
				possible_start_locations.clear();
				possible_start_locations.push_back(pos);
				break;
			}
		}
	}
	if (possible_start_locations.size() > 1) {
		for (unit*u : enemy_buildings) {
			if (u->type->is_resource_depot) {
				xy pos = get_best_score(possible_start_locations, [&](xy pos) {
					return diag_distance(pos - u->pos);
				});
				possible_start_locations.clear();
				possible_start_locations.push_back(pos);
				break;
			}
		}
	}
}

void execute_build(bool expand, const std::function<bool(buildpred::state&)>&func) {
	using namespace  buildpred;
	auto initial_state = get_my_current_state();
	ruleset rules;
	rules.bases = initial_state.bases.size() + (expand ? 1 : 0);
	rules.end_frame = current_frame + 15 * 60 * 4;
	rules.func = func;

	a_vector<state> all_states;
	all_states.push_back(initial_state);
	run(all_states, rules, true);
	add_builds(all_states.back());
}
void execute_build(const std::function<bool(buildpred::state&)>&func) {
	execute_build(false, func);
}


template<typename T>
struct render_helper {
	template<typename T>
	static std::true_type test(decltype(&T::render)*);
	template<typename T>
	static std::false_type test(...);
	static const bool has_render = decltype(test<T>(0))::value;
	render::render_func_handle h;
	template<bool b = has_render, typename std::enable_if<b>::type* = 0>
	render_helper(T&strat) {
		h = render::add(std::bind(&T::render, &strat));
	}
	template<bool b = has_render, typename std::enable_if<!b>::type* = 0>
	render_helper(T&strat) {}
	template<bool b = has_render, typename std::enable_if<b>::type* = 0>
	void unregister() {
		render::rm(h);
	}
	template<bool b = has_render, typename std::enable_if<!b>::type* = 0>
	void unregister() {}
	~render_helper() {
		unregister();
	}
};

template<typename strat_T>
std::function<void()> wrap() {
	return []() {
		strat_T strat;
		render_helper<strat_T> rh(strat);
		strat.run();
	};
}

#include "strat_proxy_rax.h"
#include "strat_wraith.h"
#include "strat_tvt_opening.h"
#include "strat_tvt.h"
#include "strat_tvp_opening.h"
#include "strat_tvp.h"
#include "strat_tvz_opening.h"
#include "strat_tvz.h"
//#include "strat_fd_push.h"
//#include "strat_zvp_opening.h"
//#include "strat_zvp.h"

a_map<a_string, std::function<void()>> strat_map = {
	{ "proxy rax", wrap<proxy_rax>() },
	{ "wraith", wrap<wraith>() },
	{ "tvt opening", wrap<strat_tvt_opening>() },
	{ "tvt", wrap<strat_tvt>() },
	{ "tvp opening", wrap<strat_tvp_opening>() },
	{ "tvp", wrap<strat_tvp>() },
	{ "tvz opening", wrap<strat_tvz_opening>() },
	{ "tvz", wrap<strat_tvz>() },
//	{ "fd push", wrap<strat_fd_push>() },
//	{ "zvp opening", wrap<strat_zvp_opening>() },
//	{ "zvp", wrap<strat_zvp>() },
};

bool run_strat(const char*name) {
	try {
		log("running strat %s\n", name);
		strat_map[name]();
		log("strat %s done\n", name);
		return true;
	} catch (strat_failed&) {
		log("strat %s failed\n", name);
		return false;
	}
}

void strategy_task() {

	multitasking::sleep(1);

	if (players::my_player->race == race_terran) {

		if (players::opponent_player->random) {
			run_strat("tvp opening");
		} else if (players::opponent_player->race == race_terran) {
			run_strat("tvt opening");
		} else if (players::opponent_player->race == race_protoss) {
			run_strat("tvp opening");
			//run_strat("fd push");
		} else if (players::opponent_player->race == race_zerg) {
			run_strat("tvz opening");
		}

		if (players::opponent_player->race == race_terran) {
			run_strat("tvt");
		} else if (players::opponent_player->race == race_protoss) {
			run_strat("tvp");
		} else if (players::opponent_player->race == race_zerg) {
			run_strat("tvz");
		}

	} else if (players::my_player->race == race_protoss) {

	} else if (players::my_player->race == race_zerg) {
		run_strat("zvp opening");
		run_strat("zvp");
	}

	while (true) {

		using namespace buildpred;

		multitasking::sleep(15 * 10);
	}

}

void init() {

	multitasking::spawn(strategy_task, "strategy");

}

}

