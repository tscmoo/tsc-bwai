
namespace buildpred {
;

struct st_base {
	refcounted_ptr<resource_spots::spot> s;
	bool verified;
};
struct st_unit {
	unit_type*type;
	int busy_until = 0;
	bool has_addon = false;
	bool verified = false;
	st_unit(unit_type*type) : type(type) {}
};

struct state {
	int race = race_terran;
	int frame = 0;
	double minerals = 0, gas = 0;
	std::array<double, 3> used_supply, max_supply;
	a_map<unit_type*, a_vector<st_unit>> units;
	a_multimap<int, unit_type*> production;
	a_multimap<int, std::tuple<unit_type*, resource_spots::spot*>> produced;
	struct resource_info_t {
		double gathered = 0.0;
		int workers = 0;
		double start_resources = 0.0;
		double cur_resources = 0.0;
	};
	a_map<resource_spots::resource_t*, resource_info_t> resource_info;
	a_vector<st_base> bases;
	int idle_workers = 0;
	int army_size = 0;
};

st_base&add_base(state&st, resource_spots::spot&s) {
	st.bases.push_back({ &s, false });
	for (auto&r : s.resources) {
		if (r.u->type->is_gas) continue;
		st.resource_info[&r];
	}
	return st.bases.back();
}
st_unit&add_unit(state&st, unit_type*ut) {
	auto&vec = st.units[ut];
	vec.emplace_back(ut);
	if (ut->is_worker) ++st.idle_workers;
	if (!ut->is_building && !ut->is_worker) ++st.army_size;
	return vec.back();
}
st_unit&add_unit_and_supply(state&st, unit_type*ut) {
	auto&u = add_unit(st, ut);
	if (ut->required_supply) st.used_supply[ut->race] += ut->required_supply;
	if (ut->provided_supply) st.max_supply[ut->race] += ut->provided_supply;
	return u;
}

struct build_rule {
	unit_type*ut;
	double army_supply_percentage;
	double weight;
};

struct ruleset {
	int end_frame = 0;
	int bases = 0;
	a_vector<build_rule> build;
};

unit_type*failed = (unit_type*)1;
unit_type*timeout = (unit_type*)2;

unit_type* advance(state&st, unit_type*build, int end_frame, bool build_right_now) {

	if (st.frame >= end_frame) return timeout;

	auto transfer_workers = [&](bool minerals) {
		while (st.idle_workers != 0) {
			//log("%d resources\n", st.resource_info.size());
			auto*best = get_best_score_p(st.resource_info, [&](const std::pair<resource_spots::resource_t*const, state::resource_info_t>*ptr) {
				auto&v = *ptr;
				if (v.first->u->type->is_minerals == minerals) {
					int n = (int)v.first->full_income_workers[st.race] - v.second.workers;
					double score = 0.0;
					if (n > 0) score = v.first->income_rate[st.race];
					else if (n > -1) score = v.first->income_rate[st.race] * v.first->last_worker_mult[st.race];
					return score;
				} else return 0.0;
			}, 0.0);
			if (best) {
				--st.idle_workers;
				++best->second.workers;
				//log("transfer worker yey\n");
			} else break;
		}
	};
	//if (st.gas < st.minerals) transfer_workers(false);
	transfer_workers(false);
	transfer_workers(true);
	//log("%d idle workers\n", st.idle_workers);

	unit_type*addon_required = nullptr;
	if (build) {
		for (unit_type*prereq : build->required_units) {
			if (prereq->is_addon && prereq->builder_type == build->builder_type) {
				addon_required = prereq;
				continue;
			}
			if (st.units[prereq].empty()) {
				bool found = false;
				for (auto&v : st.production) {
					if (v.second == prereq) {
						found = true;
						break;
					}
				}
				if (!found) {
					return prereq;
				}
			}
		}
	}

	while (true) {
		double min_income = 0;
		double gas_income = 0;
		for (auto&v : st.resource_info) {
			auto&r = *v.first;
			double rate = r.income_rate[st.race];
			double w = std::min((double)v.second.workers, r.full_income_workers[st.race]);
			double inc = rate * w;
			if (v.second.workers > w) inc += rate * r.last_worker_mult[st.race];
			if (r.u->type->is_gas) gas_income += inc;
			else min_income += inc;
		}

		if (!st.production.empty()) {
			auto&v = *st.production.begin();
			if (v.first <= st.frame) {
				add_unit(st, v.second);
				st.production.erase(st.production.begin());
				if (v.second->is_addon) {
					auto*ptr = get_best_score_p(st.units[v.second->builder_type], [&](st_unit*st_u) {
						if (st_u->busy_until <= current_frame) return 0;
						return 1;
					});
					if (ptr) {
						ptr->has_addon = true;
					}
				}
				if (v.second->is_worker) {
					transfer_workers(false);
					transfer_workers(true);
				}
			}
		}

		if (build) {
			auto add_built = [&](unit_type*t) {
				st.produced.emplace(st.frame - t->build_time, std::make_tuple(t, nullptr));
				add_unit(st, t);
				st.minerals -= t->minerals_cost;
				st.gas -= t->gas_cost;
				st.used_supply[t->race] += t->required_supply;
				st.max_supply[t->race] += t->provided_supply;
			};
			bool can_build = true;
			if (build->minerals_cost) {
				if (st.minerals < build->minerals_cost) can_build = false;
			}
			if (can_build && build->gas_cost) {
				if (st.gas < build->gas_cost) {
					bool found = false;
					for (auto&v : st.bases) {
						for (auto&r : v.s->resources) {
							if (!r.u->type->is_gas) continue;
							if (!st.resource_info.emplace(std::piecewise_construct, std::make_tuple(&r), std::make_tuple()).second) continue;
							found = true;
							break;
						}
						if (found) break;
					}
					if (found) {
						add_built(unit_types::refinery);
					} else if (gas_income == 0) return failed;
				}
			}
			if (build->required_supply) {
				if (st.used_supply[build->race] + build->required_supply>200) return failed;
				if (st.used_supply[build->race] + build->required_supply > st.max_supply[build->race]) {
					can_build = false;
					add_built(unit_types::supply_depot);
				}
			}
			if (can_build) {
				st_unit*builder = nullptr;
				st_unit*builder_without_addon = nullptr;
				bool builder_exists = false;
				for (st_unit&u : st.units[build->builder_type]) {
					if (!addon_required || u.has_addon) builder_exists = true;
					if (u.busy_until <= st.frame) {
						if (addon_required && !u.has_addon) builder_without_addon = &u;
						else {
							builder = &u;
							break;
						}
					}
				}
				if (!builder) {
					bool found = false;
					if (builder_without_addon) {
						for (auto&v : st.production) {
							if (v.second == addon_required) {
								found = true;
								break;
							}
						}
					}
					for (auto&v : st.production) {
						if (v.second == build->builder_type) {
							found = true;
							break;
						}
					}
					if (!found) {
						if (!build_right_now) {
							if (addon_required) return addon_required;
							return build->builder_type;
						} else {
							if (!builder_exists) return failed;
							//return failed;
						}
					}
				} else {
					builder->busy_until = st.frame + build->build_time;
					st.production.emplace(st.frame + build->build_time, build);
					st.produced.emplace(st.frame, std::make_tuple(build, nullptr));
					st.minerals -= build->minerals_cost;
					st.gas -= build->gas_cost;
					st.used_supply[build->race] += build->required_supply;
					//st.max_supply[build->race] += build->provided_supply;
					return (unit_type*)nullptr;
				}
			}
		}
		int f = std::min(15, end_frame - st.frame);
		st.minerals += min_income * f;
		st.gas += gas_income * f;
		st.frame += f;

		if (st.frame >= end_frame) return timeout;

	}

};

void run(a_vector<state>&all_states, ruleset rules) {

	int race = race_terran;

	unit_type*cc_type = unit_types::cc;
	unit_type*worker_type = unit_types::scv;

	auto initial_state = all_states.back();

	a_vector<refcounted_ptr<resource_spots::spot>> available_bases;
	for (auto&s : resource_spots::spots) {
		if (grid::get_build_square(s.cc_build_pos).building) continue;
		bool okay = true;
		for (auto&v : initial_state.bases) {
			if ((resource_spots::spot*)v.s == &s) okay = false;
		}
		if (!okay) continue;
		available_bases.push_back(&s);
	}

	auto get_next_base = [&]() {
		return get_best_score(available_bases, [&](resource_spots::spot*s) {
			double d = unit_pathing_distance(worker_type, s->cc_build_pos, initial_state.bases.front().s->cc_build_pos);
			//if (d == std::numeric_limits<double>::infinity()) d = 10000.0 + diag_distance(s->pos - st.bases.front().s->cc_build_pos);
			return d;
		}, std::numeric_limits<double>::infinity());
	};
	auto next_base = get_next_base();

	auto st = initial_state;
	unit_type*next_t = nullptr;
	a_vector<std::tuple<double, build_rule*, int>> sorted_build;
	//build_rule worker_rule{ unit_types::scv, 1.0, 1.0 };
	for (size_t i = 0; i < rules.build.size(); ++i) {
		sorted_build.emplace_back(0.0, &rules.build[i], 0);
	}
	while (st.frame < rules.end_frame) {
		//multitasking::yield_point();

		int highest_delta = 0;
		for (auto&v : sorted_build) {
			auto&r = *std::get<1>(v);
			int count = st.units[r.ut].size();
			int desired_count = (int)(r.army_supply_percentage*st.army_size);
			int delta = desired_count - count;
			std::get<2>(v) = delta;
			if (delta > highest_delta) highest_delta = delta;
		}
		if (highest_delta == 0) highest_delta = 1;
		for (auto&v : sorted_build) {
			auto&r = *std::get<1>(v);
			int delta = std::get<2>(v);
			double adjusted_weight = r.weight * ((double)delta / (double)highest_delta);
			std::get<0>(v) = -adjusted_weight;
		}
		if (!std::is_sorted(sorted_build.begin(), sorted_build.end())) std::sort(sorted_build.begin(), sorted_build.end());

		int build_worker_frame = -1;
		bool worker_override = false;
		if (advance(st, unit_types::scv, rules.end_frame, true)) st = all_states.back();
		else {
			if (st.minerals < 10) worker_override = true;
			build_worker_frame = st.frame;
			st = all_states.back();
		}

		int expand_frame = -1;
		if ((int)st.bases.size() < rules.bases && next_base) {
			unit_type*t = unit_types::cc;
			if (advance(st, t, rules.end_frame, true)) st = all_states.back();
			else {
				expand_frame = st.frame;
				st = all_states.back();
			}
		}

		auto try_build = [&](unit_type*ut) {
			unit_type*t = ut;
			while (true) {
				t = advance(st, t, rules.end_frame, false);
				if (t) {
					st = all_states.back();
 					if (t == failed) return false;
 					if (t == timeout) return false;
					continue;
				}
				return true;
			}
		};

		double best_score = std::numeric_limits<double>::infinity();
		state best_st;
		if (!worker_override && expand_frame == -1) {
			for (auto&v : sorted_build) {
				double weight = std::get<0>(v);
				unit_type*ut = std::get<1>(v)->ut;
				if (!try_build(ut)) {
					log("%s failed to build\n", ut->name);
					st = all_states.back();
					continue;
				}
				double score = st.frame / (weight + 1.0);
				//log("%s can be built at %d with score %f\n", ut->name, st.frame, score);
				if (score < best_score) {
					best_score = score;
					best_st = std::move(st);
				}
				st = all_states.back();
			}
		}
		if (expand_frame != -1 && (expand_frame < build_worker_frame || build_worker_frame == -1)) {
			unit_type*t = unit_types::cc;
			if (advance(st, t, rules.end_frame, true)) xcept("unreachable: buildpred failed to build cc");
			auto s = next_base;
			add_base(st, *s);
			std::get<1>((--st.produced.end())->second) = s;
			find_and_erase(available_bases, s);
			all_states.push_back(st);

			next_base = get_next_base();
		} else if (build_worker_frame != -1 && build_worker_frame < best_score) {
			if (advance(st, unit_types::scv, rules.end_frame, true)) xcept("unreachable: buildpred failed to build worker");
		} else {
			if (best_score == std::numeric_limits<double>::infinity()) break;
			st = std::move(best_st);
		}
		all_states.push_back(st);

//		unit_type*t = nullptr;

//		int end_frame = rules.end_frame;
// 		if (st.units[unit_types::scv].size() < 60) {
// 			t = unit_types::scv;
// 			if (advance(st, t, rules.end_frame)) st = all_states.back();
// 			//else end_frame = std::min(end_frame, st.frame + t->build_time);
// 		}
// 
// 		if (st.used_supply[race] > 12) {
// 			if ((int)st.bases.size() < rules.bases && !available_bases.empty() && next_base) {
// 				t = unit_types::cc;
// 				if (advance(st, t, end_frame)) st = all_states.back();
// 				else {
// 					auto s = next_base;
// 					add_base(st, *s);
// 					std::get<1>((--st.produced.end())->second) = s;
// 					find_and_erase(available_bases, s);
// 					all_states.push_back(st);
// 
// 					next_base = get_next_base();
// 				}
// 			}
// 		}
// 		if (next_t) t = next_t;
// 		else t = rules.build;
// 		next_t = advance(st, t, end_frame, false);
// 		if (next_t == failed) break;
// 		if (next_t == timeout) continue;
// 		if (next_t && next_t->is_worker) break;
// 		if (next_t) {
// 			st = all_states.back();
// 			continue;
// 		}
// 		all_states.push_back(st);
	}
	if (all_states.back().frame < rules.end_frame) {
		auto st = all_states.back();
		while (st.frame < rules.end_frame) {
			advance(st, nullptr, rules.end_frame, false);
		}
		all_states.push_back(std::move(st));
	}
	if (all_states.back().frame != rules.end_frame) xcept("all_states.back().frame is %d, expected %d", all_states.back().frame, rules.end_frame);

}

void add_builds(const state&st) {
	static void*flag = &flag;
	void*new_flag = &new_flag;

	for (auto&v : st.produced) {
		int frame = v.first;
		if (frame > current_frame + 15 * 90) continue;
		unit_type*ut = std::get<0>(v.second);
		resource_spots::spot*s = std::get<1>(v.second);
		bool found = false;
		xy build_pos;
		if (s) build_pos = s->cc_build_pos;
		for (auto&t : build::build_tasks) {
			if (t.type->unit == ut && t.flag == flag) {
				t.flag = new_flag;
				if (t.priority != frame) build::change_priority(&t, frame);
				if (build_pos != xy()) {
					bool okay = true;
					for (int y = 0; y < ut->tile_height && okay; ++y) {
						for (int x = 0; x < ut->tile_width && okay; ++x) {
							auto&bs = grid::get_build_square(build_pos + xy(x * 32, y * 32));
							if (bs.reserved.first) okay = false;
						}
					}
					if (okay) {
						if (t.build_pos != xy()) grid::unreserve_build_squares(t.build_pos, ut);
						t.build_pos = build_pos;
						grid::reserve_build_squares(build_pos, ut);
					}
				}
				found = true;
				break;
			}
		}
		if (!found) {
			bool okay = true;
			if (build_pos != xy()) {
				for (int y = 0; y < ut->tile_height && okay; ++y) {
					for (int x = 0; x < ut->tile_width && okay; ++x) {
						auto&bs = grid::get_build_square(build_pos + xy(x * 32, y * 32));
						if (bs.reserved.first) okay = false;
					}
				}
			}
			if (okay) {
				auto&t = *build::add_build_task(frame, ut);
				t.flag = new_flag;
				if (build_pos != xy()) {
					t.build_pos = build_pos;
					grid::reserve_build_squares(build_pos, ut);
				}
			}
		}
	}
	a_vector<build::build_task*> dead_list;
	for (auto&t : build::build_tasks) {
		if (t.built_unit) continue;
		if (t.flag == flag) dead_list.push_back(&t);
		if (t.flag == new_flag) t.flag = flag;
	}
	for (auto*v : dead_list) build::cancel_build_task(v);
}

struct variant {
	bool expand;
	a_vector<build_rule> build;
};
a_vector<variant> variants;

void init_variants() {
	for (int i = 0; i < 2; ++i) {
		variant v;
		v.expand = i == 1;

		v.build.push_back({ unit_types::marine, 1.0, 1.0 });
		variants.push_back(v);
	}
}

a_vector<std::tuple<variant, state>> opponent_states;
bool is_updating_opponent_states;

void update_units() {



}

void run_buildpred(const state&my_current_state) {

	//if (current_frame < 15 * 60) opponent_states.clear();
	//opponent_states.clear();

	auto rules_from_variant = [&](const state&st, variant var, int end_frame) {
		ruleset rules;
		rules.end_frame = end_frame;
		rules.bases = st.bases.size() + (var.expand ? 1 : 0);
		rules.build = var.build;
		return rules;
	};

	if (opponent_states.empty()) {
		for (auto&v : game->getStartLocations()) {
			xy start_pos(v.x * 32, v.y * 32);
			auto&bs = grid::get_build_square(start_pos);

			auto*s = get_best_score_p(resource_spots::spots, [&](resource_spots::spot*s) {
				return diag_distance(start_pos - s->cc_build_pos);
			});
			if (s) {
				for (auto&var : variants) {
					opponent_states.emplace_back();
					std::get<0>(opponent_states.back()) = var;
					state initial_state;
					initial_state.frame = 0;
					initial_state.minerals = 50;
					initial_state.gas = 0;
					initial_state.used_supply = { 0, 0, 0 };
					initial_state.max_supply = { 0, 0, 0 };
					add_base(initial_state, *s);
					add_unit_and_supply(initial_state, unit_types::cc);
					add_unit_and_supply(initial_state, unit_types::scv);
					add_unit_and_supply(initial_state, unit_types::scv);
					add_unit_and_supply(initial_state, unit_types::scv);
					add_unit_and_supply(initial_state, unit_types::scv);
					std::get<1>(opponent_states.back()) = std::move(initial_state);
				}
				break;
			}
		}
	}

	is_updating_opponent_states = true;
	for (auto&v : opponent_states) {
		variant&var = std::get<0>(v);
		auto&st = std::get<1>(v);
		a_vector<state> all_states;
		all_states.push_back(st);
		//run(all_states, rules_from_variant(all_states.back(), var, current_frame + 15 * 600));
		run(all_states, rules_from_variant(all_states.back(), var, current_frame + 15 * 300));
		while (all_states.back().frame > current_frame) all_states.pop_back();
		if (all_states.empty()) xcept("unreachable: all_states.empty()");
		st = std::move(all_states.back());
		{
			log("%f %f %f %f\n", st.minerals, st.gas, st.used_supply[race_terran], st.max_supply[race_terran]);
			log("opponent state (frame %d)--- \n", st.frame);
			for (auto&v : st.units) {
				log(" %dx%s", v.second.size(), short_type_name(v.first));
			}
			log("\n");
		}
	}
	is_updating_opponent_states = false;

	a_vector<a_vector<state>> my_snapshots;
	a_vector<a_vector<state>> opponent_snapshots;
	auto save_snapshots = [&](a_vector<state>&dst, a_vector<state>&all_states, int add_frames) {
		int last_frame = std::numeric_limits<int>::min();
		for (size_t i = 0; i <= all_states.size(); ++i) {
			auto&st = i < all_states.size() ? all_states[i] : all_states.back();
			int frame = i < all_states.size() ? st.frame : std::numeric_limits<int>::max();
			auto save = [&](int at) {
				at += current_frame;
				at -= add_frames;
				if (frame >= at && last_frame < at) dst.push_back(st);
			};
			save(15 * 30);
			save(15 * 60 * 2);
			save(15 * 60 * 5);
			save(15 * 60 * 10);
			last_frame = frame;
		}
		log("dst.size() is %d\n", dst.size());
	};
	for (auto&var : variants) {
		a_vector<state> all_states;
		all_states.push_back(my_current_state);
		run(all_states, rules_from_variant(all_states.back(), var, current_frame + 15 * 600));
		my_snapshots.emplace_back();
		save_snapshots(my_snapshots.back(), all_states, 0);
	}
	for (auto&v : opponent_states) {
		a_vector<state> all_states;
		all_states.push_back(std::get<1>(v));
		//auto&all_states = std::get<1>(v);
		run(all_states, rules_from_variant(all_states.back(), std::get<0>(v), current_frame + 15 * 600));
		opponent_snapshots.emplace_back();
		save_snapshots(opponent_snapshots.back(), all_states, 15 * 60);
	}

	std::tuple<double, int, double> my_best_score{ -std::numeric_limits<double>::infinity(), 0, 0 };
	state*my_best_state = nullptr;
	for (auto&mys : my_snapshots) {
		std::tuple<double, int, double> op_best_score{ std::numeric_limits<double>::infinity(), 0, 0 };
		for (auto&ops : opponent_snapshots) {
			bool op_won = false;
			double op_damage_bonus = 0.0;
			int my_bases = 0;
			if (mys.size() != ops.size()) xcept("huh");
			int count = 0;
			log(" -- eval -- \n");
			for (size_t i = 0; i < ops.size(); ++i) {
				multitasking::yield_point();

				auto&my_state = mys[i]; 
				auto&op_state = ops[i];
				log("my frame: %d  op frame: %d\n", my_state.frame, op_state.frame);
				combat_eval::eval eval;
				log(" %d -- \n", i);
				log("my units:");
				for (auto&v : my_state.units) {
					log(" %dx%s", v.second.size(), short_type_name(v.first));
					if (v.first->is_building) continue;
					if (v.first->is_worker) continue;
					for (size_t i = 0; i < v.second.size(); ++i) eval.add_unit(units::get_unit_stats(v.first, units::opponent_player), 0);
				}
				log("\n");
				log("op units:");
				for (auto&v : op_state.units) {
					log(" %dx%s", v.second.size(), short_type_name(v.first));
					if (v.first->is_building) continue;
					if (v.first->is_worker) continue;
					for (size_t i = 0; i < v.second.size(); ++i) eval.add_unit(units::get_unit_stats(v.first, units::my_player), 1);
				}
				log("\n");
				eval.run();

				log("result: supply %g %g  damage %g %g\n", eval.teams[0].end_supply, eval.teams[1].end_supply, eval.teams[0].damage_dealt, eval.teams[1].damage_dealt);

				op_won = eval.teams[1].end_supply > eval.teams[0].end_supply && eval.teams[1].start_supply>15;
				op_damage_bonus = eval.teams[1].damage_dealt - eval.teams[0].damage_dealt;
				my_bases = my_state.bases.size();

				if (op_won) break;
				++count;
			}
			//auto score = std::make_tuple(op_won ? 0.0 : 1.0, -op_damage_bonus);
			auto score = std::make_tuple(count, my_bases, -op_damage_bonus);
			log("score: %d %d %f\n", std::get<0>(score), std::get<1>(score), std::get<2>(score));
			if (score < op_best_score) {
				op_best_score = score;
			}
		}
		log(" - score %f %d %f\n", std::get<0>(op_best_score), std::get<1>(op_best_score), std::get<2>(op_best_score));
		if (op_best_score > my_best_score) {
			my_best_score = op_best_score;
			my_best_state = &mys.back();
		}
	}
	if (my_best_state) {
		log("best score %f %d %f\n", std::get<0>(my_best_score), std::get<1>(my_best_score), std::get<2>(my_best_score));
		log("best state --- \n");
		for (auto&v : my_best_state->units) {
			log(" %dx%s", v.second.size(), short_type_name(v.first));
		}
		log("\n");
		add_builds(*my_best_state);
	}

// 	a_vector<a_vector<state>> my_builds;
// 	a_vector<a_vector<state>> opponent_builds;
// 	ruleset rules;
// 	rules.end_frame = current_frame + 15 * 240;
// 	rules.bases = 2;
// 	auto hist = run(initial_state, rules);
// 
// 	add_builds(hist.back());

}

void buildpred_task() {
	while (true) {

		state initial_state;
		initial_state.frame = current_frame;
		initial_state.minerals = current_minerals;
		initial_state.gas = current_gas;
		initial_state.used_supply = current_used_supply;
		initial_state.max_supply = current_max_supply;

		for (auto&s : resource_spots::spots) {
			for (unit*u : my_resource_depots) {
				if (diag_distance(u->building->build_pos - s.cc_build_pos) <= 32 * 4) {
					add_base(initial_state, s);
					break;
				}
			}
		}
		for (unit*u : my_units) {
			if (u->type->is_addon) continue;
			auto&st_u = add_unit(initial_state, u->type);
			if (u->addon) st_u.has_addon = true;
			if (!u->is_completed && u->type->provided_supply) {
				initial_state.max_supply[u->type->race] += u->type->provided_supply;
			}
			if (u->type->is_gas) {
				for (auto&r : resource_spots::live_resources) {
					if (r.u == u) {
						initial_state.resource_info[&r];
						break;
					}
				}
			}
		}
		if (initial_state.bases.empty()) {
			auto*s = get_best_score_p(resource_spots::spots, [&](resource_spots::spot*s) {
				return get_best_score(make_transform_filter(my_resource_depots, [&](unit*u) {
					return unit_pathing_distance(unit_types::scv, u->pos, s->cc_build_pos);
				}), identity<double>());
			});
			if (s) add_base(initial_state, *s);
		}
		log("%d bases, %d units\n", initial_state.bases.size(), initial_state.units.size());
		if (!initial_state.bases.empty()) {
			run_buildpred(initial_state);
		}

		multitasking::sleep(15);
	}

}

void update_units_task() {
	while (true) {
		multitasking::sleep(1);

		update_units();

	}
}

void init() {

	init_variants();

	multitasking::spawn(buildpred_task, "buildpred");
	multitasking::spawn(update_units_task, "buildpred update units");

}

}

