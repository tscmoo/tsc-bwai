
namespace strategy {
;

struct strat_exception : std::exception {};
struct strat_failed : strat_exception {};

a_vector<xy> possible_start_locations;

void update_possible_start_locations() {
	possible_start_locations.clear();
	for (auto&p : start_locations) {
		if (p == my_start_location) continue;
		if (grid::get_build_square(p).last_seen > 30) {
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
				return r.u->resources != r.u->initial_resources;
			});
			if (found) {
				xy pos = p;
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

buildpred::state eval_expand(const std::function<bool(buildpred::state&)>&func) {
	using namespace buildpred;
 	double best_score = -std::numeric_limits<double>::infinity();
 	state best_st;
	for (int i = 0; i < 2; ++i) {
		auto initial_state = get_my_current_state();
		ruleset rules;
		rules.bases = initial_state.bases.size() + (i ^ 1);
		if (rules.bases == 1) rules.bases = 2;
		//int nf = (current_frame + 15 * 60 * 6);
		//nf -= nf % (15 * 60 * 4);
		int nf = current_frame + 15 * 60 * 4;
		int op_nf = nf - 15 * 60 * 2;
		if (current_frame >= 15 * 60 * 20) op_nf = nf - 15 * 60 * 4;
		rules.end_frame = nf;
		rules.func = func;

		a_vector<state> all_states;
		all_states.push_back(initial_state);
		run(all_states, rules, true);
		auto&my_st = all_states.back();

		double score = std::numeric_limits<double>::infinity();
		for (auto&op_st : run_opponent_builds(op_nf)) {
			combat_eval::eval eval;
			eval.max_frames = 15 * 30;
			for (auto&v : my_st.units) {
				if (!v.first || !v.second.size()) continue;
				if (v.first->is_building) continue;
				if (v.first->is_worker) continue;
				for (size_t i = 0; i < v.second.size(); ++i) eval.add_unit(units::get_unit_stats(v.first, players::opponent_player), 0);
			}
			for (auto&v : op_st.units) {
				if (!v.first || !v.second.size()) continue;
				if (v.first->is_building) continue;
				if (v.first->is_worker) continue;
				for (size_t i = 0; i < v.second.size(); ++i) eval.add_unit(units::get_unit_stats(v.first, players::my_player), 1);
			}
			eval.run();
			double s = eval.teams[0].end_supply - eval.teams[1].end_supply;
			if (s < score) score = s;
		}
// 		if (score > best_score) {
// 			best_score = score;
// 			best_st = std::move(my_st);
// 		}
		if (i == 0 && score>0) {
			log("eval_expand: expand okay with score %g\n", score);
			return my_st;
		}
		if (i == 1) return my_st;
	}
 	log("eval_expand: best score is %g\n", best_score);
 	return best_st;
}

void execute_build_eval_expand(const std::function<bool(buildpred::state&)>&func) {
	using namespace buildpred;
	add_builds(eval_expand(func));
}

template<typename list_T>
void execute_best_with_expand(const list_T&list) {
	using namespace buildpred;
	log("execute_best_with_expand-\n");
	for (int i = 0; i < 2; ++i) {
		double best_score = -std::numeric_limits<double>::infinity();
		state best_st;
		for (auto&func : list) {
			auto initial_state = get_my_current_state();
			ruleset rules;
			rules.bases = initial_state.bases.size() + (i ^ 1);
			if (rules.bases == 1) rules.bases = 2;
			int nf = current_frame + 15 * 60 * 2;
			//if (initial_state.army_size >= 50) nf = current_frame + 15 * 60 * 10;
			//nf = current_frame + 15 * 60 * 20;
			rules.end_frame = nf;
			rules.func = func;

			a_vector<state> all_states;
			all_states.push_back(initial_state);
			run(all_states, rules, true);
			auto&my_st = all_states.back();

			double score = std::numeric_limits<double>::infinity();
			state best_op_st;
			//for (auto&op_st : run_opponent_builds(nf - 15 * 60 * 2)) {
			if (true) {
				auto op_st = get_op_current_state();
				combat_eval::eval eval;
				eval.max_frames = 15 * 30;
				for (auto&v : my_st.units) {
					if (!v.first || !v.second.size()) continue;
					if (v.first->is_building) continue;
					if (v.first->is_worker) continue;
					unit_type*ut = v.first;
					if (ut == unit_types::siege_tank_tank_mode) ut = unit_types::siege_tank_siege_mode;
					int n = (int)std::ceil((double)v.second.size() * 2 - (0.02*(v.second.size()*v.second.size())));
					for (int i = 0; i < n; ++i) eval.add_unit(units::get_unit_stats(ut, players::opponent_player), 0);
				}
				for (auto&v : op_st.units) {
					if (!v.first || !v.second.size()) continue;
					if (v.first->is_building) continue;
					if (v.first->is_worker) continue;
					unit_type*ut = v.first;
					if (ut == unit_types::siege_tank_tank_mode) ut = unit_types::siege_tank_siege_mode;
					int n = (int)std::ceil((double)v.second.size() * 2 - (0.02*(v.second.size()*v.second.size())));
					for (int i = 0; i < n; ++i) eval.add_unit(units::get_unit_stats(ut, players::my_player), 1);
				}
				eval.run();
				double s = eval.teams[0].end_supply - eval.teams[1].end_supply;
				//double s = eval.teams[1].start_supply - eval.teams[1].end_supply;
				//double s = eval.teams[0].score;
				if (s < score) {
					score = s;
					best_op_st = std::move(op_st);
				}
			}
			log("-- i %d - score %g\n", i, score);
			log("-- my state --- frame %d  min %g gas %g supply %g/%g bases %d\n", my_st.frame, my_st.minerals, my_st.gas, my_st.used_supply[my_st.race], my_st.max_supply[my_st.race], my_st.bases.size());
			for (auto&v : my_st.units) {
				if (!v.first || !v.second.size()) continue;
				log(" %dx%s", v.second.size(), short_type_name(v.first));
			}
			log("\n");
// 			log("produced: \n");
// 			for (auto&v : my_st.produced) {
// 				log("  %d: %s\n", v.first, std::get<0>(v.second)->name);
// 			}
// 			log("production: \n");
// 			for (auto&v : my_st.production) {
// 				log("  %d: %s\n", v.first, v.second->name);
// 			}
			log("-- op state --- frame %d  min %g gas %g supply %g/%g bases %d\n", best_op_st.frame, best_op_st.minerals, best_op_st.gas, best_op_st.used_supply[best_op_st.race], best_op_st.max_supply[best_op_st.race], best_op_st.bases.size());
			for (auto&v : best_op_st.units) {
				if (!v.first || !v.second.size()) continue;
				log(" %dx%s", v.second.size(), short_type_name(v.first));
			}
			log("\n");
			if (score > best_score) {
				best_score = score;
				best_st = my_st;
			}
		}
		if (i == 0 && best_score > 0) {
			log("execute_best_with_expand: expand okay with score %g\n", best_score);
			add_builds(best_st);
			return;
		}
		if (i == 1) {
			log("execute_best_with_expand: no expand, score %g\n", best_score);
			add_builds(best_st);
			return;
		}
	}

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

a_map<a_string, std::function<void()>> strat_map = {
	{ "proxy rax", wrap<proxy_rax>() },
	{ "wraith", wrap<wraith>() },
	{ "tvt opening", wrap<strat_tvt_opening>() },
	{ "tvt", wrap<strat_tvt>() },
	{ "tvp opening", wrap<strat_tvp_opening>() },
	{ "tvp", wrap<strat_tvp>() },
	{ "tvz opening", wrap<strat_tvz_opening>() },
	{ "tvz", wrap<strat_tvz>() },
};

bool run_strat(const char*name) {
	try {
		strat_map[name]();
		return true;
	} catch (strat_failed&) {
		return false;
	}
}

void strategy_task() {

	multitasking::sleep(1);

	if (players::opponent_player->random) {
		run_strat("tvp opening");
	} else if (players::opponent_player->race == race_terran) {
		run_strat("tvt opening");
	} else if (players::opponent_player->race == race_protoss) {
		run_strat("tvp opening");
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

	while (true) {

		using namespace buildpred;

		multitasking::sleep(15 * 10);
	}

}

void init() {

	multitasking::spawn(strategy_task, "strategy");

}

}

