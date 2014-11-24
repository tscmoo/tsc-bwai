
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
	std::array<double, 3> used_supply, max_supply;
	a_unordered_map<unit_type*, a_vector<st_unit>> units;
	a_multimap<int, unit_type*> production;
	a_multimap<int, std::tuple<unit_type*, resource_spots::spot*>> produced;
	struct resource_info_t {
		double gathered = 0.0;
		int workers = 0;
		double last_seen_resources;
		resource_info_t(resource_spots::resource_t*r) : last_seen_resources(r->u->resources) {}
	};
	a_unordered_map<resource_spots::resource_t*, resource_info_t> resource_info;
	a_vector<st_base> bases;
	int idle_workers = 0;
	int army_size = 0;
};

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
		st.resource_info.emplace(&r,&r);
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

unit_type*failed = (unit_type*)1;
unit_type*timeout = (unit_type*)2;

unit_type* advance(state&st, unit_type*build, int end_frame, bool nodep, bool no_busywait) {

	if (st.frame >= end_frame) return timeout;

	auto transfer_workers = [&](bool minerals) {
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
	//if (st.gas < st.minerals) transfer_workers(false);
	transfer_workers(false);
	transfer_workers(true);
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
					transfer_workers(false);
					transfer_workers(true);
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
					transfer_workers(false);
					transfer_workers(true);
					//log("transfer -> gas\n");
					continue;
				}
			}
			if (has_enough_gas && !has_enough_minerals && !dont_free_workers) {
				dont_free_workers = true;
				if (free_worker(st, false)) {
					free_worker(st, false);
					transfer_workers(true);
					transfer_workers(false);
					//log("transfer -> min\n");
					continue;
				}
			}
			if (!has_enough_gas) {
				unit_type*refinery = unit_types::refinery;
				if (st.minerals >= refinery->minerals_cost) {
					bool found = false;
					for (auto&v : st.bases) {
						for (auto&r : v.s->resources) {
							if (!r.u->type->is_gas) continue;
							if (!st.resource_info.emplace(std::piecewise_construct, std::make_tuple(&r), std::make_tuple(&r)).second) continue;
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
						transfer_workers(false);
						transfer_workers(true);
						// 					if (no_refinery_depots) return failed;
						return nullptr;
						continue;
					}
				}
			}
			if (build->required_supply) {
				//if (st.used_supply[build->race] + build->required_supply>400) return failed;
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
					for (auto&v : st.production) {
						if (v.second == build->builder_type) {
							found = true;
							break;
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
			if (v.second.last_seen_resources < inc) inc = v.second.last_seen_resources;
			v.second.gathered += inc;
			if (r.u->type->is_gas) gas_income += inc;
			else min_income += inc;
		}
		st.minerals += min_income;
		st.gas += gas_income;
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
			if (is_for_me) {
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
			score -= (has_gas ? res : res / 2) / 100;
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
		return func(st);
	}
};
static const auto nodelay = [](state&st, unit_type*ut, const std::function<bool(state&)>&func) {
	if (ut->is_worker && st.units[ut].size() >= 70) return func(st);
	return nodelay_n(st, ut, 0, func);
};

static const auto maxprod = [](state&st, unit_type*ut, const std::function<bool(state&)>&func) {
	auto prev_st = st;
	unit_type*t = advance(st, ut, st.frame + advance_frame_amount, true, true);
	if (!t) {
		auto imm_st = st;
		st = std::move(prev_st);
		return nodelay_stage2(st, std::move(imm_st), ut, 0, func);
	} else {
		st = std::move(prev_st);
		if (t != failed) return nodelay(st, ut, func);
		return nodelay(st, ut->builder_type, func);
	}
};
static const auto maxprod1 = [](state&st, unit_type*ut) {
	auto prev_st = st;
	unit_type*t = advance(st, ut, st.frame + advance_frame_amount, true, true);
	if (!t) return true;
	else {
		st = std::move(prev_st);
		if (t != failed) return depbuild(st, state(st), ut);
		return depbuild(st, state(st), ut->builder_type);
	}
};

int count_units_plus_production(state&st, unit_type*ut) {
	int r = 0;
	for (auto&v : st.production) {
		if (v.second == ut) ++r;
	}
	return r + st.units[ut].size();
};

struct variant {
	bool expand;
	//a_vector<build_rule> build;
	std::function<bool(state&)> func;
	double score = 0.0;
};
a_vector<variant> variants;

void init_variants() {
	for (int i = 0; i < 2; ++i) {
		variant v;
		v.expand = i == 1;

// 		v.func = std::bind(nodelay, std::placeholders::_1, [=](state&st) {
// 			return nodelay(st, unit_types::barracks, std::bind(depbuild, std::placeholders::_1, unit_types::marine));
// 		});
		v.func = [](state&st) {
			return nodelay(st, unit_types::scv, [](state&st) {
				return nodelay(st, unit_types::marine, [](state&st) {
					return depbuild(st, state(st), unit_types::barracks);
				});
			});
		};
		variants.push_back(v);
		v.func = [](state&st) {
			return nodelay(st, unit_types::scv, [](state&st) {
				return nodelay(st, unit_types::vulture, [](state&st) {
					return depbuild(st, state(st), unit_types::factory);
				});
			});
		};
		variants.push_back(v);
		v.func = [](state&st) {
			return nodelay(st, unit_types::scv, [](state&st) {
				if (st.minerals >= 300) {
					return nodelay(st, unit_types::vulture, [](state&st) {
						return depbuild(st, state(st), unit_types::factory);
					});
				}
				return nodelay(st, unit_types::siege_tank_tank_mode, [](state&st) {
					unit_type*t = unit_types::factory;
					for (auto&v : st.units[t]) {
						if (!v.has_addon) {
							t = unit_types::machine_shop;
							break;
						}
					}
					return nodelay(st, t, [](state&st) {
						return depbuild(st, state(st), unit_types::vulture);
					});
				});
			});
		};
		variants.push_back(v);
		v.func = [](state&st) {
			return nodelay(st, unit_types::scv, [](state&st) {
				return nodelay(st, unit_types::siege_tank_tank_mode, [](state&st) {
					unit_type*t = unit_types::factory;
					for (auto&v : st.units[t]) {
						if (!v.has_addon) {
							t = unit_types::machine_shop;
							break;
						}
					}
					return nodelay(st, t, [](state&st) {
						return depbuild(st, state(st), unit_types::vulture);
					});
				});
			});
		};
		variants.push_back(v);
		v.func = [](state&st) {
			return nodelay(st, unit_types::scv, [](state&st) {
				if (st.minerals >= 300) {
					return nodelay(st, unit_types::vulture, [](state&st) {
						return depbuild(st, state(st), unit_types::factory);
					});
				}
				if (st.units[unit_types::siege_tank_tank_mode].size() * 3 >= st.units[unit_types::goliath].size()) {
					return nodelay(st, unit_types::goliath, [](state&st) {
						return nodelay(st, unit_types::factory, [](state&st) {
							return depbuild(st, state(st), unit_types::vulture);
						});
					});
				}
				return nodelay(st, unit_types::siege_tank_tank_mode, [](state&st) {
					unit_type*t = unit_types::factory;
					for (auto&v : st.units[t]) {
						if (!v.has_addon) {
							t = unit_types::machine_shop;
							break;
						}
					}
					return nodelay(st, t, [](state&st) {
						return depbuild(st, state(st), unit_types::vulture);
					});
				});
			});
		};
		variants.push_back(v);
		v.func = [](state&st) {
			return nodelay(st, unit_types::scv, [](state&st) {
				if (st.minerals >= 300) {
					return nodelay(st, unit_types::vulture, [](state&st) {
						return depbuild(st, state(st), unit_types::factory);
					});
				}
				return nodelay(st, unit_types::goliath, [](state&st) {
					return nodelay(st, unit_types::factory, [](state&st) {
						return depbuild(st, state(st), unit_types::vulture);
					});
				});
			});
		};
		variants.push_back(v);
		v.func = [](state&st) {
			return nodelay(st, unit_types::scv, [](state&st) {
				//if (st.units[unit_types::wraith].size() * 5 <= st.units[unit_types::goliath].size()) {
				if (st.units[unit_types::wraith].size() < 4) {
					return nodelay(st, unit_types::wraith, [](state&st) {
						return nodelay(st, unit_types::starport, [](state&st) {
							return depbuild(st, state(st), unit_types::vulture);
						});
					});
				}
				if (st.minerals >= 300) {
					return nodelay(st, unit_types::vulture, [](state&st) {
						return depbuild(st, state(st), unit_types::factory);
					});
				}
				return nodelay(st, unit_types::goliath, [](state&st) {
					return nodelay(st, unit_types::factory, [](state&st) {
						return depbuild(st, state(st), unit_types::vulture);
					});
				});
			});
		};
		variants.push_back(v);
		v.func = [](state&st) {
			return nodelay(st, unit_types::scv, [](state&st) {
				if (st.units[unit_types::goliath].size() < st.units[unit_types::battlecruiser].size() * 4) {
					return nodelay(st, unit_types::goliath, [](state&st) {
						return nodelay(st, unit_types::factory, [](state&st) {
							return depbuild(st, state(st), unit_types::vulture);
						});
					});
				}
				return nodelay(st, unit_types::battlecruiser, [](state&st) {
					unit_type*t = unit_types::starport;
					for (auto&v : st.units[t]) {
						if (!v.has_addon) {
							t = unit_types::control_tower;
							break;
						}
					}
					return nodelay(st, t, [](state&st) {
						return depbuild(st, state(st), unit_types::vulture);
					});
				});
			});
		};
		variants.push_back(v);
	}
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

void umi_update(unit*u) {
	auto umi_add_unit = [&](unit_type*ut, double minerals, double gas, bool always_add) {
		size_t&live_count = umi_live_count[ut];
		for (auto&v : opponent_states) {
			auto&st = std::get<1>(v);
			auto&vec = st.units[ut];
			if (always_add) {
				free_up_resources(st, u->minerals_value, u->gas_value);
				add_unit_and_supply(st, ut);
				log(" -- st %p -- %s force added\n", &st, ut->name);
			} else {
				if (vec.size() > live_count) {
					log(" -- st %p -- %s matched\n", &st, ut->name);
					st.minerals += ut->total_minerals_cost;
					st.gas += ut->total_gas_cost;
				} else {
					free_up_resources(st, u->minerals_value, u->gas_value);
					add_unit_and_supply(st, ut);
					log(" -- st %p -- %s added (no match)\n", &st, ut->name);
				}
			}
			st.minerals -= minerals;
			st.gas -= gas;
		}
		++live_count;
	};
	auto umi_rm_unit = [&](unit_type*ut, double minerals, double gas) {
		for (auto&v : opponent_states) {
			auto&st = std::get<1>(v);
			log(" -- st %p -- %s removed\n", &st, ut->name);
			rm_unit_and_supply(st, ut);
			st.minerals += minerals;
			st.gas += gas;
		}
		--umi_live_count[ut];
	};
	auto&umi = umi_map[u];
	if (umi.dead) {
		log(" !! umi_update for dead unit (u->dead: %d)", u->dead);
		return;
	}
	//if (umi.dead) xcept("umi_update for dead unit (u->dead: %d)", u->dead);
	if (u->type != umi.type) {
		bool is_new = true;
		if (umi.type) {
			umi_rm_unit(umi.type, umi.minerals_value, umi.gas_value);
			is_new = false;
		}
		umi.type = u->type;
		umi.minerals_value = u->minerals_value;
		umi.gas_value = u->gas_value;
		umi_add_unit(umi.type, umi.minerals_value, umi.gas_value, !is_new);
		if (umi.type->is_refinery) {

		}
	}
	if (u->dead) {
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

a_vector<unit*> umi_update_queue;
size_t opponent_states_update_count = 0;

void on_new_unit(unit*u) {
	log("new unit: %s\n", u->type->name);
	if (!u->owner->is_enemy) return;
	umi_update_queue.push_back(u);
}
void on_type_change(unit*u, unit_type*old_type) {
	log("unit type %s changed to %s\n", old_type->name, u->type->name);
	if (!u->owner->is_enemy) return;
	umi_update_queue.push_back(u);
}
void on_destroy(unit*u) {
	log("unit %s was destroyed\n", u->type->name);
	if (!u->owner->is_enemy) return;
	umi_update_queue.push_back(u);
}

a_vector<refcounted_ptr<resource_spots::spot>> starting_spots;

void update_opponent_builds() {
	std::lock_guard<multitasking::mutex> l(opponent_states_mut);
	if (opponent_states.empty()) return;
	if (!umi_update_queue.empty()) {
		static a_vector<unit*> queue;
		queue.clear();
		std::swap(queue, umi_update_queue);
		for (unit*u : queue) {
			++opponent_states_update_count;
			umi_update(u);
		}
	}
	for (auto&s : resource_spots::spots) {
		if (!grid::is_visible(s.cc_build_pos, 4, 3)) continue;
		bool has_this_base = false;
		for (unit*e : enemy_buildings) {
			if (!e->building || !e->type->is_resource_depot) continue;
			if (diag_distance(e->building->build_pos - s.cc_build_pos) > 32 * 8) continue;
			has_this_base = true;
			break;
		}
		auto find = [&](state&st, resource_spots::spot&s) {
			for (auto&v : st.bases) {
				if (&*v.s == &s) return true;
			}
			return false;
		};
		for (auto&v : opponent_states) {
			auto&st = std::get<1>(v);
			if (has_this_base != find(st, s)) {
				++opponent_states_update_count;
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
					st.minerals = 0;
					break;
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

	for (auto&v : opponent_states) {
		auto&st = std::get<1>(v);
		double total_adjust[2] = { 0, 0 };
		for (auto&v : st.resource_info) {
			double res = v.first->u->resources;
			if (v.first->u->dead || v.first->u->gone) res = 0.0;
			if (res == v.second.last_seen_resources) continue;
			double actual_gathered = v.second.last_seen_resources - res;
			double adjust = actual_gathered - v.second.gathered;
			log(" resource-- %p - last_seen_resources is %g, res is %g, gathered is %g, actual_gathered is %g\n", v.first->u, v.second.last_seen_resources, res, v.second.gathered, actual_gathered);
			v.second.gathered = 0.0;
			v.second.last_seen_resources = res;
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

			if (st.minerals < 0 || st.gas < 0) {
				free_up_resources(st, 1, 1);
			}
		}
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

void progress_opponent_builds() {
	if (current_frame < 15 * 60 && opponent_states_update_count == 0) opponent_states.clear();

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

	{
		std::lock_guard<multitasking::mutex> l(opponent_states_mut);
		if (opponent_states.empty() && !starting_spots.empty()) {
			resource_spots::spot*s = nullptr;
			for (int i = 0; i < 10 && (!s || grid::is_visible(s->cc_build_pos)); ++i) s = starting_spots[rng(starting_spots.size())];
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

bool attack_now = false;

void run_buildpred(const state&my_current_state) {

	{
		auto&st = my_current_state;
		log("min %g gas %g supply %g/%g  bases %d\n", st.minerals, st.gas, st.used_supply[st.race], st.max_supply[st.race], st.bases.size());
		log("my current state (frame %d)--- \n", st.frame);
		for (auto&v : st.units) {
			if (!v.first || !v.second.size()) continue;
			log(" %dx%s", v.second.size(), short_type_name(v.first));
		}
		log("\n");
	}
	
	if (opponent_states.empty()) {
		log("opponent_states is empty :(\n");
		return;
	}

	a_vector<state> all_states;
	state my_end_state;
	state op_end_state;

	struct result_t {
		size_t my_idx;
		double score;
		state my_st;
		state op_st;
		bool operator<(const result_t&n) {
			return score > n.score;
		}
	};
	a_vector<result_t> results_ordered, results_unordered;

	a_unordered_set<size_t> invalid_idx;

	auto try_defend = [&](int my_frame_offset, int op_frame_offset) {

		a_vector<state> my_states, op_states;
		for (auto&var : variants) {
			all_states.clear();
			all_states.push_back(my_current_state);
			int end_frame = current_frame + my_frame_offset;
			run(all_states, rules_from_variant(all_states.back(), var, end_frame), true);
			if (all_states.size() > 1 && all_states.back().frame > end_frame) all_states.pop_back();
			my_states.push_back(std::move(all_states.back()));
		}
		for (auto&v : opponent_states) {
			all_states.clear();
			all_states.push_back(std::get<1>(v));
			int end_frame = current_frame + op_frame_offset;
			run(all_states, rules_from_variant(all_states.back(), std::get<0>(v), end_frame), false);
			if (all_states.size() > 1 && all_states.back().frame > end_frame) all_states.pop_back();
			op_states.push_back(all_states.back());
		}

		results_unordered.clear();
		
		for (size_t my_idx = 0; my_idx < my_states.size(); ++my_idx) {
			auto&mys = my_states[my_idx];
			if (invalid_idx.find(my_idx) != invalid_idx.end()) {
				results_unordered.push_back({ my_idx, -std::numeric_limits<double>::infinity(), mys, op_states[my_idx] });
				continue;
			}
			
			double op_best_score = std::numeric_limits<double>::infinity();
			state*op_best_state = nullptr;
			for (auto&ops : op_states) {
				multitasking::yield_point();

				combat_eval::eval eval;
				for (auto&v : mys.units) {
					if (!v.first || !v.second.size()) continue;
					if (v.first->is_building) continue;
					if (v.first->is_worker) continue;
					for (size_t i = 0; i < v.second.size(); ++i) eval.add_unit(units::get_unit_stats(v.first, players::opponent_player), 0);
				}
				for (auto&v : ops.units) {
					if (!v.first || !v.second.size()) continue;
					if (v.first->is_building) continue;
					if (v.first->is_worker) continue;
					for (size_t i = 0; i < v.second.size(); ++i) eval.add_unit(units::get_unit_stats(v.first, players::my_player), 1);
				}
				eval.run();

				double my_supply_left = eval.teams[0].end_supply;
				double op_supply_left = eval.teams[1].end_supply;

				double score = my_supply_left - op_supply_left;
				if (score < op_best_score) {
					op_best_score = score;
					op_best_state = &ops;
				}
			}
			results_unordered.push_back({ my_idx, op_best_score, mys, *op_best_state });
		}
		results_ordered = results_unordered;
		std::sort(results_ordered.begin(), results_ordered.end());

		return results_ordered.front().score > 0;
	};

	try_defend(15 * 60 * 10, 15 * 60 * 10);
	auto preference = results_ordered;
	{
		log(" -- preference --\n");
		size_t n = 0;
		for (auto&v : preference) {
			auto&my_state = v.my_st;
			log("-- idx %d - score %g\n", v.my_idx, v.score);
			log("-- --- frame %d  min %g gas %g supply %g/%g bases %d\n", my_state.frame, my_state.minerals, my_state.gas, my_state.used_supply[my_state.race], my_state.max_supply[my_state.race], my_state.bases.size());
			for (auto&v : my_state.units) {
				if (!v.first || !v.second.size()) continue;
				log(" %dx%s", v.second.size(), short_type_name(v.first));
			}
			log("\n");

			if (v.score <= 0) invalid_idx.insert(v.my_idx);
		}
	}
	attack_now = false;
	bool can_expand = my_current_state.bases.size() == 1;
	if (try_defend(0, 15 * 60 * 0)) {
		log("0 minute attack ok\n");
		attack_now = true;
		can_expand = true;
	} else {
		if (try_defend(15 * 60 * 4, 15 * 60 * 4)) {
			log("4 minute engagement ok\n");
			can_expand = true;
		} else {
			if (try_defend(15 * 60 * 4, 0)) {
				log("4 minute defence ok\n");
			} else {
				log("I'm fucked :(\n");
			}
		}
	}
	{
		log(" -- results ordered --\n");
		for (auto&v : results_ordered) {
			auto&my_state = v.my_st;
			log("-- idx %d - score %g\n", v.my_idx, v.score);
			log("-- --- frame %d  min %g gas %g supply %g/%g bases %d\n", my_state.frame, my_state.minerals, my_state.gas, my_state.used_supply[my_state.race], my_state.max_supply[my_state.race], my_state.bases.size());
			for (auto&v : my_state.units) {
				if (!v.first || !v.second.size()) continue;
				log(" %dx%s", v.second.size(), short_type_name(v.first));
			}
			log("\n");
		}
	}
	bool found = false;
	for (auto&v : preference) {
		if (results_unordered[v.my_idx].my_st.bases.size() > my_current_state.bases.size() && !can_expand) continue;
		if (results_unordered[v.my_idx].score > 0) {
			my_end_state = v.my_st;
			op_end_state = v.op_st;
			found = true;
			break;
		}
	}
	if (!found) {
		for (auto&v : preference) {
			if (v.my_st.bases.size() > my_current_state.bases.size()) continue;
			my_end_state = v.my_st;
			op_end_state = v.op_st;
			break;
		}
	}

	{
		auto&my_state = my_end_state;
		log("-- my state --- frame %d  min %g gas %g supply %g/%g bases %d\n", my_state.frame, my_state.minerals, my_state.gas, my_state.used_supply[my_state.race], my_state.max_supply[my_state.race], my_state.bases.size());
		for (auto&v : my_state.units) {
			if (!v.first || !v.second.size()) continue;
			log(" %dx%s", v.second.size(), short_type_name(v.first));
		}
		log("\n");
		auto&op_state = op_end_state;
		log("-- op best build --- frame %d  min %g gas %g supply %g/%g bases %d\n", op_state.frame, op_state.minerals, op_state.gas, op_state.used_supply[op_state.race], op_state.max_supply[op_state.race], op_state.bases.size());
		for (auto&v : op_state.units) {
			if (!v.first || !v.second.size()) continue;
			log(" %dx%s", v.second.size(), short_type_name(v.first));
		}
		log("\n");
	}

	add_builds(my_end_state);

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
					initial_state.resource_info.emplace(&r, &r);
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
					initial_state.resource_info.emplace(&r, &r);
					break;
				}
			}
		}
	}
	if (initial_state.bases.empty()) {
		auto*s = get_best_score_p(resource_spots::spots, [&](resource_spots::spot*s) {
			return get_best_score(make_transform_filter(my_resource_depots, [&](unit*u) {
				return -unit_pathing_distance(unit_types::scv, u->pos, s->cc_build_pos);
			}), identity<double>());
		});
		if (s) add_base(initial_state, *s).verified = true;
	}
	return initial_state;
}

void buildpred_task() {

	while (true) {

		progress_opponent_builds();

		state initial_state = get_my_current_state();
		log("%d bases, %d units\n", initial_state.bases.size(), initial_state.units.size());
		if (!initial_state.bases.empty()) {
			//run_buildpred(initial_state);
		}

		multitasking::sleep(15);

// 		static int count = 0;
// 		if (++count >= 4) multitasking::sleep(15 * 30);
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

