
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
		int nf = current_frame + 15 * 60 * 7;
		rules.end_frame = nf;
		rules.func = func;

		a_vector<state> all_states;
		all_states.push_back(initial_state);
		run(all_states, rules, true);
		auto&my_st = all_states.back();

		double score = std::numeric_limits<double>::infinity();
		for (auto&op_st : run_opponent_builds(nf - 15 * 60 * 2)) {
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
			int nf = current_frame + 15 * 60 * 4;
			if (initial_state.army_size >= 50) nf = current_frame + 15 * 60 * 10;
			rules.end_frame = nf;
			rules.func = func;

			a_vector<state> all_states;
			all_states.push_back(initial_state);
			run(all_states, rules, true);
			auto&my_st = all_states.back();

			double score = std::numeric_limits<double>::infinity();
			state best_op_st;
			for (auto&op_st : run_opponent_builds(nf - 15 * 60 * 2)) {
				combat_eval::eval eval;
				eval.max_frames = 15 * 30;
				for (auto&v : my_st.units) {
					if (!v.first || !v.second.size()) continue;
					if (v.first->is_building) continue;
					if (v.first->is_worker) continue;
					unit_type*ut = v.first;
					if (ut == unit_types::siege_tank_tank_mode) ut = unit_types::siege_tank_siege_mode;
					for (size_t i = 0; i < v.second.size(); ++i) eval.add_unit(units::get_unit_stats(ut, players::opponent_player), 0);
				}
				for (auto&v : op_st.units) {
					if (!v.first || !v.second.size()) continue;
					if (v.first->is_building) continue;
					if (v.first->is_worker) continue;
					unit_type*ut = v.first;
					if (ut == unit_types::siege_tank_tank_mode) ut = unit_types::siege_tank_siege_mode;
					for (size_t i = 0; i < v.second.size(); ++i) eval.add_unit(units::get_unit_stats(ut, players::my_player), 1);
				}
				eval.run();
				double s = eval.teams[0].end_supply - eval.teams[1].end_supply;
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

#include "strat_proxy_rax.h"
#include "strat_wraith.h"

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

a_map<a_string, std::function<void()>> strat_map = {
	{ "proxy rax", wrap<proxy_rax>() },
	{ "wraith", wrap<wraith>() }
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

	//run_strat("proxy rax");
	//run_strat("wraith");
	while (true) {

		using namespace buildpred;
		buildpred::attack_now = current_used_supply[race_terran] >= 80;
		// 		execute_build([&](state&st) {
		// 			return nodelay(st, unit_types::scv, [](state&st) {
		// 				if (st.minerals >= 300 || count_units_plus_production(st, unit_types::vulture) < 2) {
		// 					return nodelay(st, unit_types::vulture, [](state&st) {
		// 						return depbuild(st, state(st), unit_types::factory);
		// 					});
		// 				}
		// 				return nodelay(st, unit_types::siege_tank_tank_mode, [](state&st) {
		// 					unit_type*t = unit_types::factory;
		// 					for (auto&v : st.units[t]) {
		// 						if (!v.has_addon) {
		// 							t = unit_types::machine_shop;
		// 							break;
		// 						}
		// 					}
		// 					return nodelay(st, t, [](state&st) {
		// 						return nodelay(st, unit_types::vulture, [](state&st) {
		// 							return depbuild(st, state(st), unit_types::marine);
		// 						});
		// 					});
		// 				});
		// 			});
		// 		});

// 		int desired_medics_n = 0;
// 
// 		double damage_taken[2];
// 		for (int i = 0; i < 2; ++i) {
// 			combat_eval::eval eval;
// 			eval.max_frames = 15 * 10;
// 			for (unit*u : my_units) {
// 				if (u->type->is_building || u->type->is_worker) continue;
// 				eval.add_unit(u, 0);
// 			}
// 			for (unit*u : enemy_units) {
// 				if (u->type->is_building || u->type->is_worker) continue;
// 				eval.add_unit(u, 1);
// 			}
// 			if (i == 0) for (int i = 0; i < 4; ++i) eval.add_unit(units::get_unit_stats(unit_types::marine, players::my_player), 0);
// 			if (i == 1) for (int i = 0; i < 4; ++i) eval.add_unit(units::get_unit_stats(unit_types::medic, players::my_player), 0);
// 			eval.run();
// 			damage_taken[i] = eval.teams[1].damage_taken;
// 			log("damage_taken[%d]: %g\n", i, damage_taken[i]);
// 		}
// 		if (damage_taken[1] < damage_taken[0]) desired_medics_n = my_units_of_type[unit_types::medic].size() + 4;
// 		if (my_units_of_type[unit_types::marine].size() < 4) desired_medics_n = 0;

		int desired_medics_n = my_units_of_type[unit_types::marine].size() / 10;

		auto build_marines = [&](state&st) {
			return nodelay(st, unit_types::scv, [&](state&st) {
				auto backbone = [&](state&st) {
					auto marines = [&](state&st) {
						return nodelay(st, unit_types::marine, [](state&st) {
							return nodelay(st, unit_types::barracks, [](state&st) {
								return depbuild(st, state(st), unit_types::ghost);
							});
						});
					};
					if (count_units_plus_production(st, unit_types::medic) < desired_medics_n) {
						return nodelay(st, unit_types::medic, marines);
					}
					return marines(st);
				};
				int marines_count = count_units_plus_production(st, unit_types::marine);
				int droppable_units = marines_count;
				if (droppable_units >= 8) {
					if (count_units_plus_production(st, unit_types::dropship) < droppable_units / 10) {
						return nodelay(st, unit_types::dropship, backbone);
					}
				}
				if (marines_count >= 30 && count_units_plus_production(st, unit_types::ghost) < marines_count / 4) {
					return nodelay(st, unit_types::ghost, backbone);
				}
				return backbone(st);
			});
		};
		auto build_goliath_vulture = [&](state&st) {
			return nodelay(st, unit_types::scv, [&](state&st) {
				auto backbone = [&](state&st) {
					auto marines = [&](state&st) {
						return nodelay(st, unit_types::goliath, [](state&st) {
							return nodelay(st, unit_types::factory, [](state&st) {
								return nodelay(st, unit_types::vulture, [](state&st) {
									return depbuild(st, state(st), unit_types::marine);
								});
							});
						});
					};
					return marines(st);
				};
				int droppable_units = count_units_plus_production(st, unit_types::goliath) + count_units_plus_production(st, unit_types::vulture);
				if (droppable_units >= 8) {
					if (count_units_plus_production(st, unit_types::dropship) < droppable_units / 10) {
						return nodelay(st, unit_types::dropship, backbone);
					}
				}
				return backbone(st);
			});
		};
		auto build_battlecruisers = [&](state&st) {
			return nodelay(st, unit_types::battlecruiser, [&](state&st) {
				unit_type*t = unit_types::starport;
				bool prod_ok = false;
				for (auto&v : st.units[t]) {
					if (v.busy_until <= st.frame) prod_ok = true;
					if (!v.has_addon) {
						t = unit_types::control_tower;
						break;
					}
				}
				if (prod_ok || count_units_plus_production(st, unit_types::starport) >= 2) return build_goliath_vulture(st);
				return nodelay(st, t, [&](state&st) {
					return build_goliath_vulture(st);
				});
			});
		};
		std::array<std::function<bool(state&)>, 3> arr;
		arr[0] = build_marines;
		arr[1] = build_goliath_vulture;
		arr[2] = build_battlecruisers;
		execute_best_with_expand(arr);

// 		execute_build(expand, [&](state&st) {
// 			return nodelay(st, unit_types::scv, [](state&st) {
// 				if (st.minerals >= 300 || count_units_plus_production(st, unit_types::vulture) < 2) {
// 					return nodelay(st, unit_types::vulture, [](state&st) {
// 						return depbuild(st, state(st), unit_types::factory);
// 					});
// 				}
// 				auto backbone = [](state&st) {
// 					return nodelay(st, unit_types::goliath, [](state&st) {
// 						return nodelay(st, unit_types::factory, [](state&st) {
// 							return nodelay(st, unit_types::vulture, [](state&st) {
// 								return depbuild(st, state(st), unit_types::marine);
// 							});
// 						});
// 					});
// 				};
// 				int droppable_units = count_units_plus_production(st, unit_types::vulture) + count_units_plus_production(st, unit_types::goliath);
// 				if (droppable_units >= 4) {
// 					if (count_units_plus_production(st, unit_types::dropship) < droppable_units / 6) {
// 						return nodelay(st, unit_types::dropship, backbone);
// 					}
// 				}
// 				return backbone(st);
// 			});
// 		});

		multitasking::sleep(15 * 10);
	}

	while (true) {

		using namespace buildpred;
		execute_build([](state&st) {
			return nodelay(st, unit_types::scv, [](state&st) {
				return nodelay(st, unit_types::vulture, [](state&st) {
					return depbuild(st, state(st), unit_types::factory);
				});
			});
		});

		multitasking::sleep(60);
	}

}

void init() {

	multitasking::spawn(strategy_task, "strategy");

}

}

