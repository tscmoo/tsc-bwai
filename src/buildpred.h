
namespace buildpred {
;

struct st_base {
	refcounted_ptr<resource_spots::spot> s;
	bool verified;
};
struct unit_match_info;
struct st_unit {
	unit_type*type;
	int busy_until = 0;
	bool has_addon = false;
	st_unit(unit_type*type) : type(type) {}
};

struct state {
	int race = race_terran;
	int frame = 0;
	double minerals = 0, gas = 0;
	double total_minerals_gathered = 0;
	double total_gas_gathered = 0;
	std::array<double, 3> used_supply, max_supply;
	a_unordered_map<unit_type*, a_vector<st_unit>> units;
	a_multimap<int, unit_type*> production;
	a_multimap<int, std::tuple<unit_type*, resource_spots::spot*>> produced;
	struct resource_info_t {
		double gathered = 0.0;
		int workers = 0;
	};
	a_unordered_map<resource_spots::resource_t*, resource_info_t> resource_info;
	a_vector<st_base> bases;
	int idle_workers = 0;
	int army_size = 0;
	bool dont_build_refineries = false;
};

a_unordered_map<unit*, double> last_seen_resources;

bool free_worker(state&st, bool minerals) {
	auto*worst = get_best_score_p(st.resource_info, [&](const std::pair<resource_spots::resource_t*const, state::resource_info_t>*ptr) {
		auto&v = *ptr;
		if (v.second.workers == 0) return 0.0;
		if (v.first->u->type->is_minerals != minerals) return 0.0;
		int n = (int)v.first->full_income_workers[st.race] - (v.second.workers - 1);
		double score = 0.0;
		if (n > 0) score = v.first->income_rate[st.race];
		else if (n > -1) score = v.first->income_rate[st.race] * v.first->last_worker_mult[st.race];
		return -score - 1.0;
	}, 0.0);
	if (!worst) return false;
	--worst->second.workers;
	++st.idle_workers;
	return true;
}

st_base&add_base(state&st, resource_spots::spot&s) {
	st.bases.push_back({ &s, false });
	for (auto&r : s.resources) {
		if (r.u->type->is_gas) continue;
		//st.resource_info.emplace(&r,&r);
		st.resource_info[&r];
	}
	return st.bases.back();
}
void rm_base(state&st, resource_spots::spot&s) {
	double resources_lost[2] = { 0, 0 };
	for (auto&r : s.resources) {
		auto i = st.resource_info.find(&r);
		if (i != st.resource_info.end()) {
			st.idle_workers += i->second.workers;
			resources_lost[i->first->u->type->is_gas] += i->second.gathered;
			st.resource_info.erase(i);
		}
	}
	log("rm_base: resources lost: %g %g\n", resources_lost[0], resources_lost[1]);
	st.minerals -= resources_lost[0];
	st.gas -= resources_lost[1];
	st.total_minerals_gathered -= resources_lost[0];
	st.total_gas_gathered -= resources_lost[1];
	for (auto i = st.bases.begin(); i != st.bases.end(); ++i) {
		if (&*i->s == &s) {
			st.bases.erase(i);
			return;
		}
	}
	xcept("rm_base: no such base");
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

void rm_unit(state&st, unit_type*ut) {
	auto&vec = st.units[ut];
	vec.pop_back();
	if (ut->is_worker) {
		if (!st.idle_workers) {
			if (!free_worker(st, true) && !free_worker(st, false)) xcept("buildpred rm_unit: failed to find worker to remove");
		}
		--st.idle_workers;
	}
	if (!ut->is_building && !ut->is_worker) --st.army_size;
}
void rm_unit_and_supply(state&st, unit_type*ut) {
	rm_unit(st, ut);
	if (ut->required_supply) st.used_supply[ut->race] -= ut->required_supply;
	if (ut->provided_supply) st.max_supply[ut->race] -= ut->provided_supply;
}

struct build_rule {
	unit_type*ut;
	double army_size_percentage;
	double weight;
};

struct ruleset {
	int end_frame = 0;
	int bases = 0;
	//a_vector<build_rule> build;
	std::function<bool(state&)> func;
};

int transfer_workers(state&st, bool minerals) {
	int count = 0;
	while (st.idle_workers != 0) {
		//log("%d resources\n", st.resource_info.size());
		auto*best = get_best_score_p(st.resource_info, [&](const std::pair<resource_spots::resource_t*const, state::resource_info_t>*ptr) {
			auto&v = *ptr;
			if (v.first->u->type->is_minerals != minerals) return 0.0;
			int n = (int)v.first->full_income_workers[st.race] - v.second.workers;
			double score = 0.0;
			if (n > 0) score = v.first->income_rate[st.race];
			else if (n > -1) score = v.first->income_rate[st.race] * v.first->last_worker_mult[st.race];
			return score;
		}, 0.0);
		if (best) {
			--st.idle_workers;
			++best->second.workers;
			++count;
			//log("transfer worker yey\n");
		} else break;
	}
	return count;
};

unit_type*failed = (unit_type*)1;
unit_type*timeout = (unit_type*)2;

unit_type* advance(state&st, unit_type*build, int end_frame, bool nodep, bool no_busywait) {

	if (st.frame >= end_frame) return timeout;


	//if (st.gas < st.minerals) transfer_workers(false);
	transfer_workers(st, false);
	transfer_workers(st, true);
	//log("%d idle workers\n", st.idle_workers);

	unit_type*addon_required = nullptr;
	if (build) {
		for (unit_type*prereq : build->required_units) {
			if (prereq->is_addon && prereq->builder_type == build->builder_type && !build->builder_type->is_addon) {
				addon_required = prereq;
				continue;
			}
			if (st.units[prereq].empty()) {
				bool found = false;
				if (prereq->is_addon && !st.units[prereq->builder_type].empty()) {
					for (auto&v : st.units[prereq->builder_type]) {
						if (v.has_addon) {
							found = true;
							break;
						}
					}
				}
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

	bool dont_free_workers = false;
	while (true) {
		//min_income *= 0.5;
		//gas_income *= 0.5;
		//log("frame %d: min_income %g gas_income %g\n", st.frame, min_income, gas_income);

		if (!st.production.empty()) {
			auto&v = *st.production.begin();
			if (v.first <= st.frame) {
				add_unit(st, v.second);
				//st.max_supply[v.second->race] += v.second->provided_supply;
				st.production.erase(st.production.begin());
				if (v.second->is_addon) {
// 					auto*ptr = get_best_score_p(st.units[v.second->builder_type], [&](st_unit*st_u) {
// 						if (st_u->busy_until <= current_frame) return 0;
// 						return 1;
// 					});
// 					if (ptr) {
// 						ptr->has_addon = true;
// 					}
				}
				if (v.second->is_worker) {
					transfer_workers(st, false);
					transfer_workers(st, true);
				}
			}
		}

		if (build) {
			auto add_built = [&](unit_type*t, bool subtract_build_time) {
				st.produced.emplace(st.frame - (subtract_build_time ? t->build_time : 0), std::make_tuple(t, nullptr));
				add_unit(st, t);
				st.minerals -= t->minerals_cost;
				st.gas -= t->gas_cost;
				st.used_supply[t->race] += t->required_supply;
				st.max_supply[t->race] += t->provided_supply;
			};
			//log("frame %d: min %g gas %g\n", st.frame, st.minerals, st.gas);
			bool has_enough_minerals = build->minerals_cost == 0 || st.minerals >= build->minerals_cost;
			bool has_enough_gas = build->gas_cost == 0 || st.gas>=build->gas_cost;
			if (has_enough_minerals && !has_enough_gas && !dont_free_workers) {
				dont_free_workers = true;
				if (free_worker(st, true)) {
					free_worker(st, true);
					transfer_workers(st, false);
					transfer_workers(st, true);
					//log("transfer -> gas\n");
					continue;
				}
			}
			if (has_enough_gas && !has_enough_minerals && !dont_free_workers) {
				dont_free_workers = true;
				if (free_worker(st, false)) {
					free_worker(st, false);
					transfer_workers(st, true);
					transfer_workers(st, false);
					//log("transfer -> min\n");
					continue;
				}
			}
			if (!has_enough_gas) {
				unit_type*refinery = unit_types::refinery;
				if (st.minerals >= refinery->minerals_cost && !st.dont_build_refineries) {
					bool found = false;
					for (auto&v : st.bases) {
						for (auto&r : v.s->resources) {
							if (!r.u->type->is_gas) continue;
							//if (!st.resource_info.emplace(std::piecewise_construct, std::make_tuple(&r), std::make_tuple(&r)).second) continue;
							if (!st.resource_info.emplace(std::piecewise_construct, std::make_tuple(&r), std::make_tuple()).second) continue;
							found = true;
							break;
						}
						if (found) break;
					}
					//if (found && !nodep) {
					if (found) {
						//return unit_types::refinery;
						add_built(refinery, false);
						dont_free_workers = false;
						//log("refinery built\n");
						for (int i = 0; i < 3; ++i) free_worker(st, true);
						transfer_workers(st, false);
						transfer_workers(st, true);
						// 					if (no_refinery_depots) return failed;
						return nullptr;
						continue;
					}
				}
			}
			if (build->required_supply) {
				if (st.used_supply[build->race] + build->required_supply > 200) return failed;
				if (st.used_supply[build->race] + build->required_supply > st.max_supply[build->race]) {
					//if (nodep) return failed;
					//return unit_types::supply_depot;
					add_built(unit_types::supply_depot, true);
// 					if (no_refinery_depots) return failed;
// 					return nullptr;
					continue;
				}
			}
			if (has_enough_minerals && has_enough_gas) {
				st_unit*builder = nullptr;
				st_unit*builder_without_addon = nullptr;
				bool builder_exists = false;
				if (build->builder_type->is_addon) {
					for (st_unit&u : st.units[build->builder_type->builder_type]) {
						if (u.has_addon) {
							builder_exists = true;
							if (u.busy_until <= st.frame) {
								builder = &u;
								break;
							}
						}
					}
				} else {
					for (st_unit&u : st.units[build->builder_type]) {
						if (!addon_required || u.has_addon) builder_exists = true;
						if (u.busy_until <= st.frame) {
							if (addon_required && !u.has_addon && false) builder_without_addon = &u;
							else {
								builder = &u;
								break;
							}
						}
					}
				}
				if (!builder) {
					bool found = false;
					if (builder_without_addon) {
						add_built(addon_required, false);
						builder_without_addon->has_addon = true;

// 						for (auto&v : st.production) {
// 							if (v.second == addon_required) {
// 								found = true;
// 								break;
// 							}
// 						}
					}
					if (!no_busywait) {
						for (auto&v : st.production) {
							if (v.second == build->builder_type) {
								found = true;
								break;
							}
						}
					}
					if (!found) {
						if (no_busywait) return failed;
						if (build->builder_type->is_worker) return failed;
						if (!nodep) {
// 							if (addon_required) return addon_required;
// 							//if (addon_required) add_built(addon_required);
// 							add_built(build->builder_type);
// 							return nullptr;
							if (!builder_exists) {
								if (addon_required) return addon_required;
								return build->builder_type;
							}
						} else {
							if (!builder_exists) return failed;
						}
					}
				} else {
					builder->busy_until = st.frame + build->build_time;
					if (build == unit_types::nuclear_missile) builder->busy_until = st.frame + 15 * 60 * 30;
					st.production.emplace(st.frame + build->build_time, build);
					st.produced.emplace(st.frame, std::make_tuple(build, nullptr));
					st.minerals -= build->minerals_cost;
					st.gas -= build->gas_cost;
					st.used_supply[build->race] += build->required_supply;
					if (build->is_addon) builder->has_addon = true;
					//st.max_supply[build->race] += build->provided_supply;
					//log("%s successfully built\n", build->name);
					return (unit_type*)nullptr;
				}
			}
		}
		int f = std::min(15, end_frame - st.frame);

		double min_income = 0;
		double gas_income = 0;
		for (auto&v : st.resource_info) {
			auto&r = *v.first;
			double rate = r.income_rate[st.race];
			double w = std::min((double)v.second.workers, r.full_income_workers[st.race]);
			double inc = rate * w;
			if (v.second.workers > w) inc += rate * r.last_worker_mult[st.race];
			inc *= f;
			//if (v.second.last_seen_resources < inc) inc = v.second.last_seen_resources;
			double max_res = last_seen_resources[v.first->u];
			if (inc > max_res) inc = max_res;
			v.second.gathered += inc;
			if (r.u->type->is_gas) gas_income += inc;
			else min_income += inc;
		}
		st.minerals += min_income;
		st.gas += gas_income;
		st.total_minerals_gathered += min_income;
		st.total_gas_gathered += gas_income;
		st.frame += f;

		if (st.frame >= end_frame) return timeout;

	}

};

void run(a_vector<state>&all_states, ruleset rules, bool is_for_me) {

	int race = race_terran;

	unit_type*cc_type = unit_types::cc;
	unit_type*worker_type = unit_types::scv;

	auto initial_state = all_states.back();
	if (initial_state.bases.empty()) return;

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
			double score = unit_pathing_distance(worker_type, s->cc_build_pos, initial_state.bases.front().s->cc_build_pos);
			double res = 0;
			if (is_for_me && initial_state.bases.size() > 1) {
				double ned = get_best_score_value(is_for_me ? enemy_units : my_units, [&](unit*e) {
					if (e->type->is_worker) return std::numeric_limits<double>::infinity();
					return diag_distance(s->pos - e->pos);
				});
				score -= ned;
			}
			bool has_gas = false;
			for (auto&r : s->resources) {
				res += r.u->resources;
				if (r.u->type->is_gas) has_gas = true;
			}
			score -= (has_gas ? res : res / 2) / 10;
			//if (d == std::numeric_limits<double>::infinity()) d = 10000.0 + diag_distance(s->pos - st.bases.front().s->cc_build_pos);
			return score;
		}, std::numeric_limits<double>::infinity());
	};
	auto next_base = get_next_base();

	auto st = initial_state;
	unit_type*next_t = nullptr;
	a_vector<std::tuple<double, build_rule*, int>> sorted_build;
	//build_rule worker_rule{ unit_types::scv, 1.0, 1.0 };
// 	for (size_t i = 0; i < rules.build.size(); ++i) {
// 		sorted_build.emplace_back(0.0, &rules.build[i], 0);
// 	}
	while (st.frame < rules.end_frame) {
		multitasking::yield_point();

		int workers = st.units[unit_types::scv].size();

		int expand_frame = -1;
		if ((int)st.bases.size() < rules.bases && next_base && workers >= 14) {
			unit_type*t = unit_types::cc;
			if (advance(st, t, rules.end_frame, true, true)) st = all_states.back();
			else {
				auto s = next_base;
				add_base(st, *s);
				std::get<1>((--st.produced.end())->second) = s;
				find_and_erase(available_bases, s);

				next_base = get_next_base();

				all_states.push_back(st);
				continue;
			}
		}

		if (!rules.func(st)) break;

		all_states.push_back(st);
	}
	if (all_states.back().frame < rules.end_frame) {
		auto st = all_states.back();
		while (st.frame < rules.end_frame) {
			advance(st, nullptr, rules.end_frame, false, false);
		}
		all_states.push_back(std::move(st));
	}
	//if (all_states.back().frame != rules.end_frame) xcept("all_states.back().frame is %d, expected %d", all_states.back().frame, rules.end_frame);

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

int count_production(state&st, unit_type*ut) {
	int r = 0;
	for (auto&v : st.production) {
		if (v.second == ut) ++r;
	}
	return r;
}
int count_units_plus_production(state&st, unit_type*ut) {
	return st.units[ut].size() + count_production(st, ut);
};

static const auto depbuild_until = [](state&st, const state&prev_st, unit_type*ut, int end_frame) {
	if (&st == &prev_st) xcept("&st == &prev_st");
	unit_type*t = ut;
	while (true) {
		t = advance(st, t, end_frame, false, false);
		//log("advance returned %p (%s)\n", t, t == nullptr ? "null" : t == failed ? "failed" : t == timeout ? "timeout" : t->name);
		if (t) {
			st = prev_st;
			if (t == failed) return false;
			if (t == timeout) return false;
			if (t->is_worker) return false;
			continue;
		}
		return true;
	}
};
static const int advance_frame_amount = 15 * 60 * 10;
static const auto depbuild = [](state&st, const state&prev_st, unit_type*ut) {
	return depbuild_until(st, prev_st, ut, st.frame + advance_frame_amount);
};
static const auto depbuild_now = [](state&st, const state&prev_st, unit_type*ut) {
	return depbuild_until(st, prev_st, ut, st.frame + 4);
};
static const auto build_now = [](state&st, unit_type*ut) {
	return advance(st, ut, st.frame + 4, true, false) == nullptr;
};
static const auto build_nodep = [](state&st, unit_type*ut) {
	return advance(st, ut, st.frame + advance_frame_amount, true, false) == nullptr;
};
static const auto could_build_instead = [](state&st, unit_type*ut) {
	unit_type*lt = (--st.production.end())->second;
	return (ut->minerals_cost == 0 || st.minerals + lt->minerals_cost >= ut->minerals_cost) && (ut->gas_cost == 0 || st.gas + lt->gas_cost >= ut->gas_cost);
};
static const auto nodelay_stage2 = [](state&st, state imm_st, unit_type*ut, int n, const std::function<bool(state&)>&func) {
	if (func(st)) {
		auto del_st = st;
		if (depbuild(st, del_st, ut)) {
			if (st.frame <= imm_st.frame + n) {
				st = std::move(del_st);
				return true;
			} else {
				st = std::move(imm_st);
				return true;
			}
		} else {
			st = std::move(imm_st);
			return true;
		}
	} else {
		st = std::move(imm_st);
		return true;
	}
};
static const auto nodelay_n = [](state&st, unit_type*ut, int n, const std::function<bool(state&)>&func) {
	auto prev_st = st;
	if (depbuild(st, prev_st, ut)) {
		auto imm_st = st;
		st = std::move(prev_st);
		return nodelay_stage2(st, std::move(imm_st), ut, n, func);
	} else {
		st = std::move(prev_st);
		if (ut->required_supply && st.used_supply[ut->race] + ut->required_supply > 200) return false;
		return func(st);
	}
};
static const auto nodelay = [](state&st, unit_type*ut, const std::function<bool(state&)>&func) {
	if (ut->is_worker && st.units[ut].size() >= 70) return func(st);
	if (ut->is_worker) {
		if (count_production(st, ut) >= 3) return func(st);
	}
	return nodelay_n(st, ut, 0, func);
};

// TODO: fix the way maxprod makes addons.
static const auto maxprod = [](state&st, unit_type*ut, const std::function<bool(state&)>&func) {
	unit_type*bt = ut->builder_type;
	unit_type*addon = nullptr;
	if (ut == unit_types::siege_tank_tank_mode) addon = unit_types::machine_shop;
	if (ut == unit_types::dropship || ut == unit_types::science_vessel || ut == unit_types::battlecruiser || ut == unit_types::valkyrie) addon = unit_types::control_tower;
	if (addon) {
		bool inprod = false;
		for (auto&v : st.production) {
			if (v.second == addon) {
				inprod = true;
				break;
			}
		}
		if (!inprod) {
			for (auto&v : st.units[bt]) {
				if (!v.has_addon) {
					bt = addon;
// 					return nodelay(st, bt, func);
// 					break;
				}
			}
		}
	}
	auto prev_st = st;
	unit_type*t = advance(st, ut, st.frame + advance_frame_amount, true, true);
	if (!t) {
		auto imm_st = st;
		st = std::move(prev_st);
		return nodelay_stage2(st, std::move(imm_st), ut, 0, func);
	} else {
		st = std::move(prev_st);
		if (t != failed) return nodelay(st, ut, func);
		if (ut->required_supply && st.used_supply[ut->race] + ut->required_supply > 200) return false;
		return nodelay(st, bt, func);
	}
};
static const auto maxprod1 = [](state&st, unit_type*ut) {
	unit_type*bt = ut->builder_type;
	unit_type*addon = nullptr;
	if (ut == unit_types::siege_tank_tank_mode) addon = unit_types::machine_shop;
	if (ut == unit_types::dropship || ut == unit_types::science_vessel || ut == unit_types::battlecruiser || ut == unit_types::valkyrie) addon = unit_types::control_tower;
	if (addon) {
		bool inprod = false;
		for (auto&v : st.production) {
			if (v.second == addon) {
				inprod = true;
				break;
			}
		}
		if (!inprod) {
			for (auto&v : st.units[bt]) {
				if (!v.has_addon) {
					bt = addon;
// 					return depbuild(st, state(st), bt);
// 					break;
				}
			}
		}
	}
	auto prev_st = st;
	unit_type*t = advance(st, ut, st.frame + advance_frame_amount, true, true);
	if (!t) return true;
	else {
		st = std::move(prev_st);
		if (t != failed) return depbuild(st, state(st), ut);
		if (ut->required_supply && st.used_supply[ut->race] + ut->required_supply > 200) return false;
		return depbuild(st, state(st), bt);
	}
};


struct variant {
	bool expand;
	//a_vector<build_rule> build;
	std::function<bool(state&)> func;
	double score = 0.0;
};
a_vector<variant> variants;

void init_variants() {
	variant v;
	v.expand = false;

	v.func = [](state&st) {
		unit_type*ut = unit_types::scv;
		if (st.units[ut].size() < 70) return depbuild(st, state(st), ut);
		return advance(st, nullptr, st.frame + 15 * 5, false, false) == timeout;
	};
	variants.push_back(v);
}

a_vector<std::tuple<variant, state>> opponent_states;
multitasking::mutex opponent_states_mut;

struct unit_match_info {
	unit_type*type = nullptr;
	double minerals_value = 0.0;
	double gas_value = 0.0;
	bool dead = false;
};
a_unordered_map<unit*, unit_match_info> umi_map;
a_unordered_map<unit_type*, size_t> umi_live_count;

struct umi_update_t {
	unit*u;
	unit_type*type;
	double minerals_value;
	double gas_value;
	bool dead;
};

void free_up_resources(state&st, double minerals, double gas) {
	if ((minerals == 0 || st.minerals >= minerals) && (gas == 0 || st.gas >= gas)) return;
	log(" -- st %p -- free up resources - need %g %g, have %g %g\n", &st, minerals, gas, st.minerals, st.gas);
	for (auto i = st.produced.rbegin(); i != st.produced.rend() && (st.minerals < minerals || st.gas < gas);) {
		unit_type*ut = std::get<0>(i->second);
		auto&vec = st.units[ut];
		bool rm = false;
		if (vec.size() > umi_live_count[ut]) {
			if (minerals && minerals > st.minerals && ut->total_minerals_cost) rm = true;
			if (gas && gas > st.gas && ut->total_gas_cost) rm = true;
		}
		if (rm) {
			st.minerals += ut->total_minerals_cost;
			st.gas += ut->total_gas_cost;
			//if (ut->required_supply) st.used_supply[ut->race] -= ut->required_supply;
			//if (ut->provided_supply) st.max_supply[ut->race] -= ut->provided_supply;
			rm_unit_and_supply(st, ut);
			log(" -- st %p -- %s removed to free up resources\n", &st, ut->name);
			auto ri = i;
			st.produced.erase((++ri).base());
			i = st.produced.rbegin();
			//vec.pop_back();
		} else ++i;
	}
	log(" -- st %p -- freed up - have %g %g\n", &st, st.minerals, st.gas);
};

void umi_reset() {
	umi_map.clear();
	umi_live_count.clear();
}

void umi_update(umi_update_t upd) {
	auto umi_add_unit = [](unit_type*ut, double minerals, double gas, bool always_add) {
		size_t&live_count = umi_live_count[ut];
		for (auto&v : opponent_states) {
			auto&st = std::get<1>(v);
			auto&vec = st.units[ut];
			if (always_add) {
				free_up_resources(st, minerals, gas);
				add_unit_and_supply(st, ut);
				log(" -- st %p -- %s force added\n", &st, ut->name);
			} else {
				if (vec.size() > live_count) {
					log(" -- st %p -- %s matched\n", &st, ut->name);
					st.minerals += ut->total_minerals_cost;
					st.gas += ut->total_gas_cost;
				} else {
					free_up_resources(st, minerals, gas);
					add_unit_and_supply(st, ut);
					log(" -- st %p -- %s added (no match)\n", &st, ut->name);
				}
			}
			st.minerals -= minerals;
			st.gas -= gas;
		}
		++live_count;
	};
	auto umi_rm_unit = [](unit_type*ut, double minerals, double gas) {
		for (auto&v : opponent_states) {
			auto&st = std::get<1>(v);
			log(" -- st %p -- %s removed\n", &st, ut->name);
			rm_unit_and_supply(st, ut);
			st.minerals += minerals;
			st.gas += gas;
		}
		--umi_live_count[ut];
	};
	auto&umi = umi_map[upd.u];
	if (umi.dead) {
		log(" !! umi_update for dead unit (upd.dead: %d)", upd.dead);
		return;
	}
	//if (umi.dead) xcept("umi_update for dead unit (u->dead: %d)", u->dead);
	if (upd.type != umi.type) {
		bool is_new = true;
		if (umi.type) {
			umi_rm_unit(umi.type, umi.minerals_value, umi.gas_value);
			is_new = false;
		}
		umi.type = upd.type;
		umi.minerals_value = upd.minerals_value;
		umi.gas_value = upd.gas_value;
		umi_add_unit(umi.type, umi.minerals_value, umi.gas_value, !is_new);
		if (umi.type->is_refinery) {
			
		}
	}
	if (upd.dead) {
		umi_rm_unit(umi.type, 0, 0);
		umi.dead = true;
	}

	for (auto&v : opponent_states) {
		auto&st = std::get<1>(v);
		log(" -- st %p -- min %g gas %g supply %g/%g\n", &st, st.minerals, st.gas, st.used_supply[st.race], st.max_supply[st.race]);

		for (auto&v : st.units) {
			unit_type*ut = std::get<0>(v);
			auto&vec = std::get<1>(v);
			if (vec.size() < umi_live_count[ut]) xcept("umi_live_count mismatch in state %p for %s - %d vs %d\n", &st, ut->name, vec.size(), umi_live_count[ut]);
		}
	}
	log("umi_update: %s updated -- \n", umi.type->name);
}

a_vector<umi_update_t> umi_update_history;
a_vector<umi_update_t> umi_update_queue;

umi_update_t get_umi_update(unit*u) {
	umi_update_t r;
	r.u = u;
	r.type = u->type;
	r.minerals_value = u->minerals_value;
	r.gas_value = u->gas_value;
	r.dead = u->dead;
	return r;
}

void on_new_unit(unit*u) {
	log("new unit: %s\n", u->type->name);
	if (!u->owner->is_enemy) return;
	umi_update_queue.push_back(get_umi_update(u));
	umi_update_history.push_back(get_umi_update(u));
}
void on_type_change(unit*u, unit_type*old_type) {
	log("unit type %s changed to %s\n", old_type->name, u->type->name);
	if (!u->owner->is_enemy) return;
	umi_update_queue.push_back(get_umi_update(u));
	umi_update_history.push_back(get_umi_update(u));
}
void on_destroy(unit*u) {
	log("unit %s was destroyed\n", u->type->name);
	if (!u->owner->is_enemy) return;
	umi_update_queue.push_back(get_umi_update(u));
	umi_update_history.push_back(get_umi_update(u));
}

a_vector<refcounted_ptr<resource_spots::spot>> starting_spots;

void update_opponent_builds() {
	std::lock_guard<multitasking::mutex> l(opponent_states_mut);
	if (opponent_states.empty()) return;
	if (!umi_update_queue.empty()) {
		static a_vector<umi_update_t> queue;
		queue.clear();
		std::swap(queue, umi_update_queue);
		for (auto&upd : queue) {
			umi_update(upd);
		}
	}
	for (auto&s : resource_spots::spots) {
		bool has_this_base = false;
		for (unit*e : enemy_buildings) {
			if (!e->building || !e->type->is_resource_depot) continue;
			if (diag_distance(e->building->build_pos - s.cc_build_pos) > 32 * 8) continue;
			has_this_base = true;
			break;
		}
		if (!has_this_base && !grid::is_visible(s.cc_build_pos, 4, 3)) continue;
		for (auto&v : opponent_states) {
			auto&st = std::get<1>(v);
			bool st_has_this_base = false;
			for (auto&v : st.bases) {
				if (&*v.s == &s) {
					st_has_this_base = true;
					if (has_this_base && !v.verified) v.verified = true;
					break;
				}
			}
			if (has_this_base != st_has_this_base) {
				if (has_this_base) {
					log(" -- st %p - add base %p\n", &st, &s);
					add_base(st, s).verified = true;
					for (auto&v : st.bases) {
						if (!v.verified) {
							log(" -- st %p - remove unverified base %p\n", &st, &s);
							rm_base(st, *v.s);
							break;
						}
					}
				} else {
					log(" -- st %p - remove base %p\n", &st, &s);
					rm_base(st, s);
				}
			}
		}
	}
	for (auto&v : opponent_states) {
		auto&st = std::get<1>(v);
		if (!st.bases.empty()) continue;
		resource_spots::spot*s = nullptr;
		for (int i = 0; i < 10 && (!s || grid::is_visible(s->cc_build_pos)); ++i) s = starting_spots[rng(starting_spots.size())];
		if (s) {
			add_base(st, *s);
			if (st.minerals < 0) {
				for (auto&v : st.resource_info) {
					if (v.first->u->type->is_gas) continue;
					v.second.gathered = -st.minerals;
					st.total_minerals_gathered += -st.minerals;
					st.minerals = 0;
					break;
				}
			}
		}
	}

	for (auto&v : opponent_states) {
		auto&st = std::get<1>(v);
		for (auto&b : st.bases) {
			for (auto&r : b.s->resources) {
				if (r.u->owner == players::opponent_player) {
					//st.resource_info.emplace(&r, &r);
					st.resource_info[&r];
				}
			}
		}
	}

	for (auto&v : opponent_states) {
		auto&st = std::get<1>(v);
		a_map<resource_spots::spot*, int> count[2];
		a_map<resource_spots::spot*, double> sum[2];
		for (auto&v : st.resource_info) {
			++count[v.first->u->type->is_gas][v.first->s];
			sum[v.first->u->type->is_gas][v.first->s] += v.second.gathered;
		}
		for (auto&v : st.resource_info) {
			v.second.gathered = sum[v.first->u->type->is_gas][v.first->s] / count[v.first->u->type->is_gas][v.first->s];

			//log(" resource-- %p gathered is %g\n", v.first->u, v.second.gathered);
		}
	}

	// TODO: Give all resources not mined by me to the opponent.
	//       Not just from bases he owns.
	a_unordered_set<unit*> update_res;
// 	for (auto&r : resource_spots::all_resources) {
// 		update_res.insert(r.u);
// 	}
	for (auto&v : opponent_states) {
		auto&st = std::get<1>(v);
		double total_adjust[2] = { 0, 0 };
		for (auto&v : st.resource_info) {
			update_res.insert(v.first->u);
		}
	}

	for (auto&v : opponent_states) {
		auto&st = std::get<1>(v);
		double total_adjust[2] = { 0, 0 };
		for (auto&v : st.resource_info) {
			if (!update_res.count(v.first->u)) continue;
			double res = v.first->u->resources;
			if (v.first->u->dead || v.first->u->gone) res = 0.0;
			double last_seen = last_seen_resources[v.first->u];
			if (res == last_seen) continue;
			double actual_gathered = last_seen - res;
			double adjust = actual_gathered - v.second.gathered;
			log(" resource-- %p - last_seen_resources is %g, res is %g, gathered is %g, actual_gathered is %g\n", v.first->u, last_seen, res, v.second.gathered, actual_gathered);
			v.second.gathered = 0.0;
			total_adjust[v.first->u->type->is_gas] += adjust;
		}
		for (auto i = st.resource_info.begin(); i != st.resource_info.end();) {
			if (i->first->u->dead || i->first->u->gone) {
				st.idle_workers += i->second.workers;
				i = st.resource_info.erase(i);
			} else ++i;
		}
		if (total_adjust[0] || total_adjust[1]) {
			log(" -- adjust resources -- %+g minerals %+g gas\n", total_adjust[0], total_adjust[1]);
			st.minerals += total_adjust[0];
			st.gas += total_adjust[1];
			st.total_minerals_gathered += total_adjust[0];
			st.total_gas_gathered += total_adjust[1];

			if (st.minerals < 0 || st.gas < 0) {
				free_up_resources(st, 1, 1);
			}

			if (total_adjust[1] >= 200) {
				for (int i = 0; i < 3; ++i) free_worker(st, true);
				transfer_workers(st, false);
				transfer_workers(st, true);
			} else if (total_adjust[1] < -200) {
				for (int i = 0; i < 3; ++i) free_worker(st, false);
				transfer_workers(st, true);
				transfer_workers(st, false);
			}
		}
	}

	//for (unit*u : update_res) {
	for (auto&r : resource_spots::all_resources) {
		unit*u = r.u;
		last_seen_resources[u] = u->dead || u->gone ? 0 : u->resources;
	}
	
}

auto rules_from_variant = [&](const state&st, variant var, int end_frame) {
	size_t verified_bases = 0;
	for (auto&v : st.bases) {
		if (v.verified) ++verified_bases;
	}
	ruleset rules;
	rules.end_frame = end_frame;
	rules.bases = verified_bases + (var.expand ? 1 : 0);
	//rules.build = var.build;
	rules.func = var.func;
	return rules;
};

bool found_op_base = false;

void progress_opponent_builds() {
	auto reset = []() {
		log("opponent states reset\n");
		opponent_states.clear();
		last_seen_resources.clear();
		for (auto&r : resource_spots::all_resources) {
			last_seen_resources[r.u] = r.u->resources;
		}
		umi_reset();
		umi_update_queue = umi_update_history;
	};
	if (current_frame < 15 * 60) reset();

	starting_spots.clear();
	for (auto&v : game->getStartLocations()) {
		bwapi_pos p = v;
		xy start_pos(p.x * 32, p.y * 32);
		auto&bs = grid::get_build_square(start_pos);
		auto*s = get_best_score_p(resource_spots::spots, [&](resource_spots::spot*s) {
			return diag_distance(start_pos - s->cc_build_pos);
		});
		if (s) starting_spots.push_back(s);
	}
	refcounted_ptr<resource_spots::spot> override_spot;
	if (!opponent_states.empty()) {
		auto&op_st = std::get<1>(opponent_states.front());
		bool has_verified_base = test_pred(op_st.bases, [&](st_base&v) {
			return v.verified;
		});
		if (!has_verified_base) found_op_base = false;
		else if (!found_op_base) {
			found_op_base = true;
			override_spot = op_st.bases.front().s;
			reset();
		}
	}

	{
		std::lock_guard<multitasking::mutex> l(opponent_states_mut);
		if (opponent_states.empty() && !starting_spots.empty()) {
			resource_spots::spot*s = nullptr;
			if (override_spot) s = override_spot;
			else for (int i = 0; i < 10 && (!s || grid::is_visible(s->cc_build_pos)); ++i) s = starting_spots[rng(starting_spots.size())];
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
		}

		for (auto&v : opponent_states) {
			variant&var = std::get<0>(v);
			auto&st = std::get<1>(v);
			a_vector<state> all_states;
			all_states.push_back(st);
			run(all_states, rules_from_variant(all_states.back(), var, current_frame + 15 * 300), false);
			state*closest = &all_states.front();
			for (auto&v : all_states) {
				if (v.frame <= current_frame) closest = &v;
				else break;
			}
			st = std::move(*closest);
			//if (st.frame != current_frame) advance(st, nullptr, current_frame, false, false);
			{
				log("min %g gas %g supply %g/%g  bases %d\n", st.minerals, st.gas, st.used_supply[st.race], st.max_supply[st.race], st.bases.size());
				log("opponent state (frame %d)--- \n", st.frame);
				for (auto&v : st.units) {
					if (!v.first || !v.second.size()) continue;
					log(" %dx%s", v.second.size(), short_type_name(v.first));
				}
				log("\n");
			}
		}
	}
}

a_vector<state> run_opponent_builds(int end_frame) {
	a_vector<state> r;
	for (auto&v : opponent_states) {
		variant&var = std::get<0>(v);
		auto&st = std::get<1>(v);
// 		auto st = std::get<1>(v);
// 		for (auto&v : st.units) {
// 			if (v.first->is_building || v.first->is_worker) continue;
// 			size_t live = umi_live_count[v.first];
// 			while (v.second.size() > live) rm_unit(st, v.first);
// 		}
		a_vector<state> all_states;
		all_states.push_back(st);
		run(all_states, rules_from_variant(all_states.back(), var, end_frame), false);
		r.push_back(std::move(all_states.back()));
	}
	return r;
}


state get_my_current_state() {
	state initial_state;
	initial_state.frame = current_frame;
	initial_state.minerals = current_minerals;
	initial_state.gas = current_gas;
	initial_state.used_supply = current_used_supply;
	initial_state.max_supply = current_max_supply;

	for (auto&s : resource_spots::spots) {
		for (unit*u : my_resource_depots) {
			if (diag_distance(u->building->build_pos - s.cc_build_pos) <= 32 * 4) {
				add_base(initial_state, s).verified = true;
				break;
			}
		}
	}
	for (unit*u : my_units) {
		//if (!u->is_completed) continue;
		if (u->type->is_addon) continue;
		auto&st_u = add_unit(initial_state, u->type);
		if (u->addon) st_u.has_addon = true;
		if (u->has_nuke) st_u.busy_until = current_frame + 15 * 60 * 30;
		if (!u->is_completed && u->type->provided_supply) {
			initial_state.max_supply[u->type->race] += u->type->provided_supply;
		}
		if (u->type->is_gas) {
			for (auto&r : resource_spots::live_resources) {
				if (r.u == u) {
					//initial_state.resource_info.emplace(&r, &r);
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
		if (s) add_base(initial_state, *s).verified = true;
	}
	return initial_state;
}
state get_op_current_state() {
	state initial_state;
	initial_state.frame = current_frame;
	initial_state.minerals = 0;
	initial_state.gas = 0;
	initial_state.used_supply = { 0, 0, 0 };
	initial_state.max_supply = { 0, 0, 0 };

	for (auto&s : resource_spots::spots) {
		for (unit*u : enemy_units) {
			if (!u->type->is_resource_depot) continue;
			if (diag_distance(u->building->build_pos - s.cc_build_pos) <= 32 * 4) {
				add_base(initial_state, s).verified = true;
				break;
			}
		}
	}
	for (unit*u : enemy_units) {
		if (u->type->provided_supply) initial_state.max_supply[u->type->race] += u->type->provided_supply;
		if (u->type->required_supply) initial_state.used_supply[u->type->race] += u->type->required_supply;
		//if (!u->is_completed) continue;
		if (u->type->is_addon) continue;
		auto&st_u = add_unit(initial_state, u->type);
		if (u->addon) st_u.has_addon = true;
		if (!u->is_completed && u->type->provided_supply) {
			initial_state.max_supply[u->type->race] += u->type->provided_supply;
		}
		if (u->type->is_gas) {
			for (auto&r : resource_spots::live_resources) {
				if (r.u == u) {
					//initial_state.resource_info.emplace(&r, &r);
					initial_state.resource_info[&r];
					break;
				}
			}
		}
	}
	if (initial_state.bases.empty()) {
		auto*s = get_best_score_p(resource_spots::spots, [&](resource_spots::spot*s) {
			double start_dist = get_best_score_value(start_locations, [&](xy pos) {
				return diag_distance(pos - s->cc_build_pos);
			});
			if (start_dist >= 32 * 10) return 10000 + start_dist;
			return get_best_score(make_transform_filter(my_resource_depots, [&](unit*u) {
				return -unit_pathing_distance(unit_types::scv, u->pos, s->cc_build_pos);
			}), identity<double>());
		});
		if (s) add_base(initial_state, *s).verified = true;
	}
	return initial_state;
}

double op_total_minerals_gathered, op_total_gas_gathered;
double op_visible_minerals_value, op_visible_gas_value;
double op_unaccounted_minerals, op_unaccounted_gas;
double op_unverified_minerals_gathered, op_unverified_gas_gathered;

void buildpred_task() {

	while (true) {

		progress_opponent_builds();

		state initial_state = get_my_current_state();
		log("%d bases, %d units\n", initial_state.bases.size(), initial_state.units.size());
		if (!opponent_states.empty()) {
			auto&op_st = std::get<1>(opponent_states.front());
 			op_total_minerals_gathered = op_st.total_minerals_gathered;
 			op_total_gas_gathered = op_st.total_gas_gathered;
			double min_sum = 0;
			double gas_sum = 0;
			for (unit*u : enemy_units) {
				min_sum += u->minerals_value;
				gas_sum += u->gas_value;
			}
			op_visible_minerals_value = min_sum;
			op_visible_gas_value = gas_sum;
			op_unaccounted_minerals = op_total_minerals_gathered - op_visible_minerals_value - players::opponent_player->minerals_lost;
			op_unaccounted_gas = op_total_gas_gathered - op_visible_gas_value - players::opponent_player->gas_lost;

			if (op_unaccounted_gas < 0) {
				for (int i = 0; i < 3; ++i) free_worker(op_st, true);
				transfer_workers(op_st, false);
				transfer_workers(op_st, true);
			} else if (op_unaccounted_minerals < 0) {
				for (int i = 0; i < 3; ++i) free_worker(op_st, false);
				transfer_workers(op_st, true);
				transfer_workers(op_st, false);
			}
			double unv_min = 0;
			double unv_gas = 0;
			for (auto&v : op_st.resource_info) {
				if (v.first->u->type->is_gas) unv_gas += v.second.gathered;
				else unv_min += v.second.gathered;
			}
			op_unverified_minerals_gathered = unv_min;
			op_unverified_gas_gathered = unv_gas;
		}

		multitasking::sleep(15);

	}

}

void update_opponent_builds_task() {
	while (true) {
		multitasking::sleep(15);

		update_opponent_builds();

	}
}

void render() {

	for (auto&s : resource_spots::spots) {
		int count = 0;
	
		for (auto&v : opponent_states) {
			state&st = std::get<1>(v);
			auto find = [&]() {
				for (auto&v : st.bases) if (&*v.s == &s) return true;
				return false;
			};
			if (find()) {
				game->drawCircleMap(s.pos.x, s.pos.y, 32 * 8 + 8 * count, BWAPI::Colors::Red);
				++count;
			}

		}
	}

	double my_total_minerals_gathered = players::my_player->game_player->gatheredMinerals();
	double my_total_gas_gathered = players::my_player->game_player->gatheredGas();
	double min_sum = 0;
	double gas_sum = 0;
	for (unit*u : my_units) {
		min_sum += u->minerals_value;
		gas_sum += u->gas_value;
	}
	double my_visible_minerals_value = min_sum;
	double my_visible_gas_value = gas_sum;
	double my_unaccounted_minerals = my_total_minerals_gathered - my_visible_minerals_value - players::my_player->minerals_lost;
	double my_unaccounted_gas = my_total_gas_gathered - my_visible_gas_value - players::my_player->gas_lost;;

	render::draw_screen_stacked_text(260, 20, format("my gathered minerals: %g  gas: %g", std::round(my_total_minerals_gathered), std::round(my_total_gas_gathered)));
	render::draw_screen_stacked_text(260, 20, format("my visible minerals: %g  gas: %g", std::round(my_visible_minerals_value), std::round(my_visible_gas_value)));
	render::draw_screen_stacked_text(260, 20, format("my unaccounted minerals: %g  gas: %g", std::round(my_unaccounted_minerals), std::round(my_unaccounted_gas)));
	render::draw_screen_stacked_text(260, 20, format("op gathered minerals: %g  gas: %g", std::round(op_total_minerals_gathered), std::round(op_total_gas_gathered)));
	render::draw_screen_stacked_text(260, 20, format("op visible minerals: %g  gas: %g", std::round(op_visible_minerals_value), std::round(op_visible_gas_value)));
	render::draw_screen_stacked_text(260, 20, format("op unaccounted minerals: %g  gas: %g", std::round(op_unaccounted_minerals), std::round(op_unaccounted_gas)));
	render::draw_screen_stacked_text(260, 20, format("op unverified minerals: %g  gas: %g", std::round(op_unverified_minerals_gathered), std::round(op_unverified_gas_gathered)));

}

void init() {

	init_variants();

	multitasking::spawn(buildpred_task, "buildpred");
	multitasking::spawn(update_opponent_builds_task, "update opponent builds");

	units::on_new_unit_callbacks.push_back(on_new_unit);
	units::on_type_change_callbacks.push_back(on_type_change);
	units::on_destroy_callbacks.push_back(on_destroy);

	render::add(render);

}

}

