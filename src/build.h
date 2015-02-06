

namespace build {
;

struct build_type {
	unit_type*unit;
	upgrade_type*upgrade;
	unit_type*builder;
	double minerals_cost;
	double gas_cost;
	a_vector<build_type*> prerequisites;
	a_string name;
	int build_time;
};

struct build_task: refcounted {
	a_list<build_task>::iterator it;
	bool dead;
	std::pair<build_task*,build_task*> build_order_link;
	double priority;
	build_type*type;
	unit*builder;
	int last_look_for_builder;
	xy build_pos;
	xy build_near;
	xy land_pos;
	int last_find_build_pos;
	int build_frame;
	a_vector<build_task*> prerequisites;
	a_vector<build_task*> prerequisite_for;
	bool promoted_for_prerequisite;
	double pre_prerequisite_priority;
	std::pair<build_task*,build_task*> type_map_link;
	std::pair<build_task*,build_task*> priority_group_link;
	unit*built_unit;
	int mark;
	bool needs_pylon, needs_creep;
	double supply_req;
	void*flag;
	int upgrade_done_frame;
	int build_started_frame;
};

a_list<build_type> build_type_container;
a_unordered_map<unit_type*,build_type*> build_type_map_unit;
build_type*get_build_type(unit_type*unit) {
	build_type*&r = build_type_map_unit[unit];
	if (r) return r;
	build_type_container.emplace_back();
	r = &build_type_container.back();
	r->unit = unit;
	r->upgrade = nullptr;
	r->builder = unit->builder_type;
	r->minerals_cost = unit->minerals_cost;
	r->gas_cost = unit->gas_cost;
	r->build_time = unit->build_time;
	for (unit_type*pt : unit->required_units) {
		r->prerequisites.push_back(get_build_type(pt));
	}
	r->name = unit->name;
	return r;
}
a_unordered_map<upgrade_type*, build_type*> build_type_map_upgrade;
build_type*get_build_type(upgrade_type*upgrade) {
	build_type*&r = build_type_map_upgrade[upgrade];
	if (r) return r;
	build_type_container.emplace_back();
	r = &build_type_container.back();
	r->unit = nullptr;
	r->upgrade = upgrade;
	r->builder = upgrade->builder_type;
	r->minerals_cost = upgrade->minerals_cost;
	r->gas_cost = upgrade->gas_cost;
	r->build_time = upgrade->build_time;
	for (unit_type*pt : upgrade->required_units) {
		r->prerequisites.push_back(get_build_type(pt));
	}
	r->name = upgrade->name;
	return r;
}

a_list<build_task> build_tasks;
tsc::intrusive_list<build_task,&build_task::build_order_link> build_order;
a_unordered_map<build_type*,tsc::intrusive_list<build_task,&build_task::type_map_link>> build_tasks_for_type;
a_map<double,tsc::intrusive_list<build_task,&build_task::priority_group_link>> priority_groups;

void change_priority(build_task*t, double new_priority) {
	log("%s priority changed from %g to %g\n", t->type->name, t->priority, new_priority);
	auto pg_i = priority_groups.find(t->priority);
	pg_i->second.erase(*t);
	if (pg_i->second.empty()) priority_groups.erase(pg_i);
	t->priority = new_priority;
	priority_groups[new_priority].push_back(*t);
}

void prereq_override_priority(build_task*t,double new_priority) {
	change_priority(t, new_priority);
}

build_task*add_build_task(double priority,build_type*type);
build_task*get_prerequisite(build_type*type) {
	build_task*t = get_best_score_p(build_tasks,[&](build_task*b) {
		if (b->type!=type) return std::make_tuple(false,0.0,0);
		if (b->built_unit) return std::make_tuple(true,-std::numeric_limits<double>::infinity(),b->built_unit->remaining_build_time);
		return std::make_tuple(true,b->priority,b->build_frame);
	},std::make_tuple(false,0.0,0));
	if (t) {
		if (t->prerequisite_for.empty() && !t->promoted_for_prerequisite) {
			log("promote %s!\n",t->type->name);
			t->promoted_for_prerequisite = true;
			t->pre_prerequisite_priority = t->priority;
		}
		return t;
	}
	return add_build_task(std::numeric_limits<double>::infinity(),type);
}
build_task*get_prerequisite_if_needed(build_task*t,build_type*type) {
	if (type->unit) {
		for (unit*u : my_units_of_type[type->unit]) {
			if (t->type->unit && t->type->unit->is_addon && u->addon) continue;
			if (u->is_completed) return nullptr;
		}
	}
	return get_prerequisite(type);
}
void cancel_build_task(build_task*t);
void cancel_prerequisite(build_task*parent,build_task*v) {
	log("cancel prereq %s for %s\n",v->type->name,parent->type->name);
	v->prerequisite_for.erase(std::find(v->prerequisite_for.begin(),v->prerequisite_for.end(),parent));
	for (auto&x : v->prerequisite_for) log("still prereq for %s\n",x->type->name);
	if (v->prerequisite_for.empty()) {
		if (v->promoted_for_prerequisite) {
			log("no longer promoted\n");
			v->promoted_for_prerequisite = false;
			prereq_override_priority(v,v->pre_prerequisite_priority);
			//v->priority = v->pre_prerequisite_priority;
		}/* else cancel_build_task(v);*/
	}
}
void update_prerequisites(build_task*t) {
	if (t->built_unit) return;
	size_t n = 0;
	auto add = [&](build_type*pt,bool if_needed) {
		build_task*p = if_needed ? get_prerequisite_if_needed(t, pt) : get_prerequisite(pt);
		if (p) {
			if (t->priority<p->priority) prereq_override_priority(p,t->priority);
			//p->priority = t->priority;
			if (t->prerequisites.size()>n) {
				if (t->prerequisites[n]!=p) {
					cancel_prerequisite(t,t->prerequisites[n]);
					t->prerequisites[n] = p;
					p->prerequisite_for.push_back(t);
				}
			} else {
				t->prerequisites.push_back(p);
				p->prerequisite_for.push_back(t);
			}
			++n;
		}
	};
	for (build_type*pt : t->type->prerequisites) {
		if (pt->unit == unit_types::larva) continue;
		add(pt,true);
	}
	if (t->needs_pylon) add(get_build_type(unit_types::pylon),false);
	if (t->needs_creep) {
		if (t->type->unit==unit_types::creep_colony) add(get_build_type(unit_types::hatchery),false);
		else add(get_build_type(unit_types::creep_colony),false);
	}
	if (t->supply_req) {
		unit_type*su = t->type->unit->race==race_zerg ? unit_types::overlord : t->type->unit->race==race_protoss ? unit_types::pylon : unit_types::supply_depot;
		add(get_build_type(su),false);
	}
	if (n==t->prerequisites.size()) return;
	size_t ns = n;
	for (;n<t->prerequisites.size();++n) {
		cancel_prerequisite(t,t->prerequisites[n]);
	}
	t->prerequisites.resize(ns);
}

void garbage_collect() {
	for (auto&b : build_tasks) b.mark = 0;
	std::function<void(build_task&)> visit = [&](build_task&b) {
		if (b.mark) return;
		b.mark = 1;
		for (auto*p : b.prerequisites) visit(*p);
	};
	for (auto&b : build_tasks) {
		if (b.prerequisite_for.empty() || b.promoted_for_prerequisite) visit(b);
	}
	for (auto i=build_tasks.begin();i!=build_tasks.end();) {
		build_task&b = *i++;
		if (b.mark==0) {
			log("build task %s is unreachable\n",b.type->name);
			b.prerequisite_for.clear();
			b.prerequisites.clear();
			cancel_build_task(&b);
		}
	}
}

build_task*add_build_task(double priority,build_type*type) {
	for (build_task&b : build_tasks) {
		if (b.type!=type) continue;
		if (b.prerequisite_for.empty()) continue;
		if (b.promoted_for_prerequisite) continue;
		b.promoted_for_prerequisite = true;
		b.pre_prerequisite_priority = priority;
		return &b;
	}
	log("add build task %s with priority %g\n",type->name,priority);
	if (type->unit && type->unit->race==race_unknown) xcept("attempt to add build task unit with unknown race - %s\n",type->name);
	build_tasks.emplace_back();
	build_task*t = &build_tasks.back();
	t->it = --build_tasks.end();
	t->dead = false;
	t->priority = priority;
	t->type = type;
	t->builder = nullptr;
	t->last_look_for_builder = 0;
	t->last_find_build_pos = 0;
	t->build_frame = 0;
	t->built_unit = 0;
	t->promoted_for_prerequisite = false;
	t->pre_prerequisite_priority = 0.0;
	t->needs_pylon = false;
	t->needs_creep = false;
	t->supply_req = 0;
	t->flag = nullptr;
	t->upgrade_done_frame = 0;
	t->build_started_frame = 0;
	build_tasks_for_type[type].push_back(*t);
	build_order.push_back(*t);
	priority_groups[priority].push_back(*t);
	return t;
}
build_task*add_build_task(double priority,unit_type*type) {
	return add_build_task(priority,get_build_type(type));
}
build_task*add_build_task(double priority, upgrade_type*type) {
	return add_build_task(priority, get_build_type(type));
}

void add_build_sum(double priority, unit_type*type, int count) {
	int sum = 0;
	for (build::build_task&b : build::build_tasks) {
		if (b.type->unit == type) ++sum;
	}
	for (; sum < count; ++sum) add_build_task(priority, type);
}
void add_build_sum(double priority, upgrade_type*type, int count) {
	int sum = 0;
	for (build::build_task&b : build::build_tasks) {
		if (b.dead) continue;
		if (b.type->upgrade == type) ++sum;
	}
	for (; sum < count; ++sum) add_build_task(priority, type);
}

void cancel_build_task(build_task*t) {

	if (t->reference_count) {
		log("delay cancel build task %s\n",t->type->name);
		t->dead = true;
		return;
	}
	log("cancel build task %s\n",t->type->name);
// 	if (!t->prerequisite_for.empty()) {
// 		if (!t->promoted_for_prerequisite) xcept("attempt to cancel non-promoted prerequisite task");
// 		t->promoted_for_prerequisite = false;
// 		return;
// 	}
	while (!t->prerequisite_for.empty()) {
		build_task*p = t->prerequisite_for[0];
		p->prerequisites.erase(std::find(p->prerequisites.begin(),p->prerequisites.end(),t));
		cancel_prerequisite(p,t);
	}

	for (auto*v : t->prerequisites) {
		cancel_prerequisite(t,v);
	}

	if (t->builder && t->builder->controller->action==unit_controller::action_build) {
		t->builder->controller->action = unit_controller::action_idle;
		t->builder->controller->target = nullptr;
		t->builder->controller->target_pos = xy();
		t->builder->controller->target_type = 0;
		t->builder->controller->flag = 0;
	}

	if (t->build_pos != xy() && !t->type->unit->is_refinery) {
		grid::unreserve_build_squares(t->build_pos, t->type->unit);
	}

	auto pg_i = priority_groups.find(t->priority);
	pg_i->second.erase(*t);
	if (pg_i->second.empty()) priority_groups.erase(pg_i);
	build_order.erase(*t);
	build_tasks_for_type[t->type].erase(*t);
	build_tasks.erase(t->it);
}

xy default_build_pos;

void generate_build_order_task() {

	const int resolution = 15;
	const int max_time = resource_gathering::max_predicted_frames / resolution;

	std::array<double, max_time> minerals_at;
	std::array<double, max_time> gas_at;
	a_unordered_map<unit*, int> build_busy;

	while (true) {
		minerals_at.fill(current_minerals);
		gas_at.fill(current_gas);
		for (auto&v : resource_gathering::predicted_mineral_delivery) {
			int f = (v.first - current_frame) / resolution;
			if (f < 0) f = 0;
			else if (f >= max_time) f = max_time - 1;
			for (int i = f; i < max_time; ++i) minerals_at[i] += v.second;
		}
		for (auto&v : resource_gathering::predicted_gas_delivery) {
			int f = (v.first - current_frame) / resolution;
			if (f < 0) f = 0;
			else if (f >= max_time) f = max_time - 1;
			for (int i = f; i < max_time; ++i) gas_at[i] += v.second;
		}
		build_busy.clear();

		for (build_task&b : build_tasks) {
			update_prerequisites(&b);
		}
		garbage_collect();

		build_order.clear();
		for (build_task&b : build_tasks) {
			b.build_frame = 0;
		}
		double total_minerals_spent = 0;
		double total_gas_spent = 0;
		for (auto&v : priority_groups) {
			auto&list = v.second;
			for (build_task&b : list) {
				if (b.dead) continue;
				if (b.build_frame) continue;
				if (b.built_unit) continue;
				if (b.upgrade_done_frame) continue;
				void*builder_prereq = nullptr;
				int latest_prereq = 0;
				int idx = max_time - 1;
				int mc = 0, gc = 0;
				for (auto*p : b.prerequisites) {
					if (p->built_unit) {
						int f = p->built_unit->remaining_build_time / resolution;
						if (f < 0) f = 0;
						else if (f >= max_time) f = max_time - 1;
						if (f > latest_prereq) latest_prereq = f;
						continue;
					}
					if (p->build_frame == 0) {
						latest_prereq = -1;
						break;
					}
					int f = p->build_frame - current_frame;
					f += p->type->build_time;
					f /= resolution;
					if (f < 0) f = 0;
					else if (f >= max_time) f = max_time - 1;
					if (p->type->unit && p->type->unit == b.type->builder) builder_prereq = p;
					if (f < latest_prereq) continue;
					latest_prereq = f;
				}
				if (latest_prereq == -1) continue;
				int build_delay = -1;
				if (b.type->builder == unit_types::larva) {
					for (int i = 0; i < 3 && build_delay != 0; ++i) {
						for (unit*u : my_units_of_type[i == 0 ? unit_types::hive : i == 1 ? unit_types::lair : unit_types::hatchery]) {
							if (!u->game_unit->getLarva().empty()) {
								build_delay = 0;
							} else {
								if (build_delay == -1 || u->remaining_whatever_time < build_delay) build_delay = u->remaining_whatever_time;
							}
							if (build_delay == 0) break;
						}
					}
				} else {
					for (unit*u : my_units_of_type[b.type->builder]) {
						bool requires_addon = false;
						unit_type*ut = b.type->unit;
						if (ut == unit_types::siege_tank_tank_mode) requires_addon = true;
						if (ut == unit_types::dropship || ut == unit_types::science_vessel || ut == unit_types::battlecruiser || ut == unit_types::valkyrie) requires_addon = true;
						if (requires_addon && !u->addon) continue;
						if (build_delay == -1 || u->remaining_whatever_time < build_delay) build_delay = u->remaining_whatever_time;
						if (build_delay == 0) {
							break;
						}
					}
				}
				build_delay /= resolution;
				if (build_delay < 0) build_delay = 0;
				else if (build_delay >= max_time) build_delay = max_time - 1;
				if (build_delay > latest_prereq) latest_prereq = build_delay;
				
				for (int i = latest_prereq; i<max_time; ++i) {
					bool m = minerals_at[i] >= b.type->minerals_cost;
					bool g = gas_at[i] >= b.type->gas_cost;
					if (m && g) {
						bool okay = true;
						for (int i2 = i; i2 < max_time; ++i2) {
							if (minerals_at[i2] - b.type->minerals_cost < 0) okay = false;
							if (gas_at[i2] - b.type->gas_cost < 0) okay = false;
							if (!okay) break;
						}
						if (!okay) continue;
						//log("minerals_at[%d] is %g, gas_at[%d] is %g\n",i,minerals_at[i],i,gas_at[i]);
						idx = i;
						break;
					}
				}
				b.build_frame = current_frame + idx*resolution;
				build_order.push_back(b);
				total_minerals_spent += b.type->minerals_cost;
				total_gas_spent += b.type->gas_cost;

				for (int i = idx; i < max_time; ++i) {
					minerals_at[i] -= b.type->minerals_cost;
					gas_at[i] -= b.type->gas_cost;
				}
			}

		}
		for (build_task&b : build_tasks) {
			if (b.build_frame == 0) build_order.push_back(b);
		}
		log("total minerals spent is %g, total gas spent is %g\n", total_minerals_spent, total_gas_spent);
		resource_gathering::minerals_to_gas_weight = std::max(total_minerals_spent, 1.0) / std::max(total_gas_spent, 1.0);
		//log("resource_gathering::minerals_to_gas_weight is %g\n",resource_gathering::minerals_to_gas_weight);

		multitasking::sleep(8);
	}

}

bool build_has_pylon(grid::build_square&bs, unit_type*ut) {
	return pylons::is_in_pylon_range(bs.pos + xy(ut->tile_width * 16, ut->tile_height * 16), pylons::existing_completed_pylons);
}
bool build_has_creep(grid::build_square&bs, unit_type*ut) {
	for (int y = 0; y < ut->tile_height; ++y) {
		for (int x = 0; x < ut->tile_width; ++x) {
			if (!creep::complete_creep_spread.test(grid::build_grid_indexer()(bs.pos + xy(x * 32, y * 32)))) return false;
		}
	}
	return true;
}
bool build_has_not_creep(grid::build_square&bs, unit_type*ut) {
	for (int y = 0; y < ut->tile_height; ++y) {
		for (int x = 0; x < ut->tile_width; ++x) {
			xy pos = bs.pos + xy(x * 32, y * 32);
			if (creep::complete_creep_spread.test(grid::build_grid_indexer()(pos))) return false;
			if (grid::get_build_square(pos).has_creep) return false;
		}
	}
	return true;
}
bool build_full_check(grid::build_square&bs, unit_type*ut, bool allow_large_path_around = false) {
	bool okay = build_spot_finder::can_build_at(ut, bs, allow_large_path_around);
	if (!ut->requires_creep && okay) okay &= build_has_not_creep(bs, ut);
	if (ut->requires_creep && okay) okay &= build_has_creep(bs, ut);
	if (ut->requires_pylon && okay) okay &= build_has_pylon(bs, ut);
	return okay;
}

bool build_is_valid(grid::build_square&bs, unit_type*ut) {
	bool okay = build_spot_finder::is_buildable_at(ut, bs);
	if (!ut->requires_creep && okay) okay &= build_has_not_creep(bs, ut);
	if (ut->requires_creep && okay) okay &= build_has_creep(bs, ut);
	if (ut->requires_pylon && okay) okay &= build_has_pylon(bs, ut);
	return okay;
}

bool set_build_pos(build_task*t, xy pos) {
	auto&b = *t;
	if (b.build_pos != xy()) {
		grid::unreserve_build_squares(b.build_pos, b.type->unit);
	}
	bool okay = build_is_valid(grid::get_build_square(pos), t->type->unit);
	if (!okay) {
		if (b.build_pos != xy()) grid::reserve_build_squares(b.build_pos, b.type->unit);
		return false;
	}
	b.build_pos = pos;
	grid::reserve_build_squares(b.build_pos, b.type->unit);
	return true;
}

void unset_build_pos(build_task*t) {
	auto&b = *t;
	if (b.build_pos != xy() && !b.type->unit->is_refinery) {
		grid::unreserve_build_squares(b.build_pos, b.type->unit);
	}
	b.build_pos = xy();
}

void set_builder(build_task*t, unit*builder) {
	auto&b = *t;

	if (builder != b.builder) {
		if (b.builder && b.builder->controller->flag == &b) {
			b.builder->controller->action = unit_controller::action_idle;
			b.builder->controller->target = nullptr;
			b.builder->controller->target_pos = xy();
			b.builder->controller->target_type = nullptr;
			b.builder->controller->flag = nullptr;
		}
		b.builder = builder;
	}
	if (builder) {
		auto*c = builder->controller;
		if (c->action != unit_controller::action_build || (c->flag == &b && (c->target_pos != b.build_pos || c->target_type != b.type->unit || c->target != b.built_unit || c->wait_until != b.build_frame))) {
			double d = unit_pathing_distance(builder, b.build_pos);
			int t = frames_to_reach(builder, d);
			if (t + t / 8 + latency_frames >= b.build_frame - current_frame) {
				c->action = unit_controller::action_build;
				c->target = b.built_unit;
				c->target_pos = b.build_pos;
				c->target_type = b.type->unit;
				c->flag = &b;
				c->wait_until = b.build_frame;
			}
		}
	}

}

bool something_is_in_the_way(xy build_pos, unit_type*ut) {
	for (unit*nu : my_units) {
		if (nu->building) continue;
		if (nu->is_flying) continue;
		xy upper_left = nu->pos - xy(nu->type->dimension_left(), nu->type->dimension_up());
		xy bottom_right = nu->pos + xy(nu->type->dimension_right(), nu->type->dimension_down());
		int x1 = build_pos.x;
		int y1 = build_pos.y;
		int x2 = build_pos.x + ut->tile_width * 32;
		int y2 = build_pos.y + ut->tile_height * 32;
		if (bottom_right.x < x1) continue;
		if (bottom_right.y < y1) continue;
		if (upper_left.x > x2) continue;
		if (upper_left.y > y2) continue;
		return true;
	}
	return false;
}

int last_find_default_build_pos = 0;
bool force_find_new_default_build_pos = 0;
a_map<xy, int> blacklisted_default_build_pos;

void execute_build(build_task&b) {
	refcounter<build_task> rc(b);
	if (b.built_unit && b.type->unit && b.type->unit->is_building && b.type->builder->is_worker) {
		log("b.built_unit is %p, build_pos is %d %d\n",b.built_unit,b.build_pos.x,b.build_pos.y);
		if (b.build_pos==xy() && !b.type->unit->is_refinery && !b.type->unit->is_addon) {
			bool ok = true;
			for (int y = 0; y < b.type->unit->tile_height && ok; ++y) {
				for (int x = 0; x < b.type->unit->tile_width && ok; ++x) {
					if (grid::get_build_square(b.built_unit->building->build_pos + xy(x * 32, y * 32)).reserved.first) ok = false;
				}
			}
			if (ok) {
				b.build_pos = b.built_unit->building->build_pos;
				grid::reserve_build_squares(b.build_pos, b.type->unit);
			}
		}
	}
	auto pred_pylon = [&](grid::build_square&bs) {
		return build_has_pylon(bs, b.type->unit);
	};
	auto pred_creep = [&](grid::build_square&bs) {
		return build_has_creep(bs, b.type->unit);
	};
	auto pred_not_creep = [&](grid::build_square&bs) {
		return build_has_not_creep(bs, b.type->unit);
	};
	if (b.type->unit && b.type->unit->is_building && !b.built_unit && b.build_frame && current_frame - b.build_frame <= 15 * 30 && !b.type->unit->is_addon && b.type->builder->is_worker) {
		bool is_pylon = b.type->unit == unit_types::pylon;
		bool is_creep_colony = b.type->unit == unit_types::creep_colony;
		bool is_creep = is_creep_colony || b.type->unit == unit_types::hatchery;
		if (b.build_pos!=xy() && !b.type->unit->is_refinery) {
			grid::unreserve_build_squares(b.build_pos,b.type->unit);
			bool okay = build_spot_finder::is_buildable_at(b.type->unit, b.build_pos);
			auto&bs = grid::get_build_square(b.build_pos);
			if (!b.type->unit->requires_creep && okay) okay &= pred_not_creep(bs);
			if (b.type->unit->requires_creep && !is_creep && okay) okay &= pred_creep(bs);
			if (b.type->unit->requires_pylon && okay) okay &= pred_pylon(bs);
			if (!okay) {
				b.build_pos = xy();
			} else {
				grid::reserve_build_squares(b.build_pos, b.type->unit);
			}
		}
		if (b.build_pos != xy() && b.type->unit->is_refinery) {
			// fixme: check build pos
		}
		//if (b.build_pos==xy() && current_frame-b.last_find_build_pos>=15*2) {
		if (b.build_pos==xy()) {
			b.last_find_build_pos = current_frame;
			if (default_build_pos == xy() || current_frame - last_find_default_build_pos >= 15 * 15) {
				last_find_default_build_pos = current_frame;
				auto is_safe = [&](xy pos) {
					int threat_count = 0;
					for (unit*e : enemy_units) {
						if (!e->stats->ground_weapon) continue;
						if (e->type->is_worker) continue;
						if (diag_distance(pos - e->pos) <= 32 * 15) ++threat_count;
					}
					return threat_count < 15;
				};
				if (default_build_pos == xy() || !is_safe(default_build_pos) || force_find_new_default_build_pos) {
					force_find_new_default_build_pos = false;
					if (!my_buildings.empty()) {
						default_build_pos = get_best_score(my_buildings, [&](unit*u) -> double {
							double r = 0;
							for (unit*w : my_workers) {
								double d = units_pathing_distance(w, u);
								if (d != std::numeric_limits<double>::infinity()) r += d;
							}
							if (default_build_pos != xy()) {
								double d = unit_pathing_distance(unit_types::scv, default_build_pos, u->pos) * 20;
								if (d != std::numeric_limits<double>::infinity()) r -= d;
							}
							if (!is_safe(u->building->build_pos)) r += 100000;
							auto i = blacklisted_default_build_pos.find(u->building->build_pos);
							if (i != blacklisted_default_build_pos.end()) {
								r += 10000.0 * i->second;
							}
							return u->type->is_resource_depot ? r / 2 : r;
						})->building->build_pos;
					}
				}
			}
			if (!b.type->unit->is_refinery) {
				for (auto i = ++build_order.iterator_to(b); i != build_order.end(); ++i) {
					build_task&b2 = *i;
					if (b2.build_pos == xy() || b2.built_unit) continue;
					if (b2.type->unit->is_refinery) continue;
					grid::unreserve_build_squares(b2.build_pos, b2.type->unit);
				}
			}
			std::array<xy,1> starts;
			starts[0] = b.build_near == xy() ? default_build_pos : b.build_near;
			xy pos;
			if (b.type->unit->requires_pylon) {
				/*pos = build_spot_finder::find(starts,b.type->unit,[&](grid::build_square&bs) {
					return pylons::is_in_pylon_range(bs.pos + xy(b.type->unit->tile_width*16,b.type->unit->tile_height*16),pylons::existing_pylons);
				});*/
				pos = build_spot_finder::find(starts,b.type->unit,[&](grid::build_square&bs) {
					return pred_pylon(bs) && pred_not_creep(bs);
				});
				if (pos==xy() && !b.needs_pylon) {
					log("maybe need pylon\n");
					xy pos2 = build_spot_finder::find(starts,b.type->unit,pred_not_creep);
					if (pos2!=xy()) b.needs_pylon = true;
					if (pos2==xy()) log("failed to find even disregarding that :(\n");
				}
				if (pos!=xy()) b.needs_pylon = false;
				if (pos!=xy()) log("No longer needs pylon :D\n");
			} else if (b.type->unit->requires_creep && (!is_creep || b.prerequisite_for.empty())) {
				pos = build_spot_finder::find_best(starts,32,[&](grid::build_square&bs) {
					if (!build_spot_finder::can_build_at(b.type->unit, bs)) return false;
					return pred_creep(bs);
				},[&](xy pos) {
					int n = 0;
					for (int y=0;y<b.type->unit->tile_height;++y) {
						for (int x=0;x<b.type->unit->tile_width;++x) {
							if (grid::get_build_square(pos + xy(x*32,y*32)).has_creep) ++n;
						}
					}
					return -n;
				});
				log("find creep pos -> %d %d\n", pos.x, pos.y);
				if (pos==xy() && !b.needs_creep) {
					log("maybe need creep\n");
					xy pos2 = build_spot_finder::find(starts,b.type->unit);
					if (pos2!=xy()) b.needs_creep= true;
					if (pos2==xy()) log("failed to find even disregarding that :(\n");
				}
				if (pos!=xy()) b.needs_creep = false;
				if (pos!=xy()) log("No longer needs creep:D\n");
			} else {
				if ((is_pylon || is_creep) && !b.prerequisite_for.empty()) {
					int max_w = 2, max_h = 2;
					unit_type*ut = 0;
					for (auto*t : b.prerequisite_for) {
						if (!t->type->unit || !t->type->unit->is_building) continue;
						if (!t->needs_pylon && !t->needs_creep) continue;
						max_w = std::max(max_w,t->type->unit->tile_width);
						max_h = std::max(max_w,t->type->unit->tile_height);
						ut = t->type->unit;
					}
					a_vector<xy> spots;
					if (ut) {
						spots = build_spot_finder::find(starts,32,[&](grid::build_square&bs) {
							return build_spot_finder::can_build_at(ut, bs) && (!is_pylon || pred_not_creep(bs));
						});
					}
					a_unordered_map<xy,int,grid::build_grid_indexer> count_map;
					if (is_pylon) {
						pos = build_spot_finder::find_best(starts,32,[&](grid::build_square&bs) {
							if (!build_spot_finder::can_build_at(b.type->unit,bs)) return false;
							if (!pred_not_creep(bs)) return false;
							if (!ut) return true;
							int count = 0;
							for (auto&v : spots) {
								if (bs.pos>=v && bs.pos<=v+xy(max_w*32,max_h*32)) continue;
								if (pylons::is_in_pylon_range(bs.pos - (v + xy(max_w*16,max_h*16)))) ++count;
							}
							count_map[bs.pos] = count;
							if (count) return true;
							return false;
						},[&](xy pos) {
							return -count_map[pos];
						});
					} else {
						pos = build_spot_finder::find_best(starts,32,[&](grid::build_square&bs) {
							if (!build_spot_finder::can_build_at(b.type->unit,bs)) return false;
							if (is_creep_colony && !pred_creep(bs)) return false;
							if (!ut) return true;
							creep::creep_source cs(bs.pos,is_creep_colony ? creep::creep_source_creep_colony : creep::creep_source_hatchery);
							int count = 0;
							for (auto&v : spots) {
								if (bs.pos>=v && bs.pos<=v+xy(max_w*32,max_h*32)) continue;
								bool okay = true;
								for (int y=0;y<max_h && okay;++y) {
									for (int x=0;x<max_w && okay;++x) {
										xy pos = v + xy(x*32,y*32);
										if (creep::complete_creep_spread.test(grid::build_grid_indexer()(pos))) continue;
										if (cs.spreads_to(pos)) continue;
										okay = false;
									}
								}
								if (okay) ++count;
							}
							count_map[bs.pos] = count;
							if (count) return true;
							return false;
						},[&](xy pos) {
							return -count_map[pos];
						});
					}
					if (b.type->unit->requires_creep) {
						if (pos==xy() && !b.needs_creep) {
							xy pos2 = build_spot_finder::find(starts,b.type->unit);
							if (pos2!=xy()) b.needs_creep = true;
						}
						if (pos!=xy()) b.needs_creep = false;
					}
				} else if (b.type->unit->is_refinery) {
					unit*g = get_best_score(resource_units,[&](unit*r) {
						if (r->type!=unit_types::vespene_geyser) return std::numeric_limits<double>::infinity();
						double d = get_best_score_value(my_resource_depots,[&](unit*u) {
							double d = units_pathing_distance(unit_types::scv, r, u) + rng(32.0);
							if (!u->is_completed) d += 1000.0;
							return d;
						});
						if (d >= 32 * 20) return std::numeric_limits<double>::infinity();
						return d;
					},std::numeric_limits<double>::infinity());
					if (g) {
						pos = g->building->build_pos;
					}
				} else {
					//log("default find pos\n");
					if (b.type->unit->requires_creep || b.type->unit->requires_pylon) xcept("unreachable: default build spot requires creep or pylon");
					pos = build_spot_finder::find(starts, b.type->unit, pred_not_creep);

					if (pos == xy() && starts[0] == default_build_pos) {
						log("failed to find build pos, trying to move default_build_pos\n");
						force_find_new_default_build_pos = true;
						++blacklisted_default_build_pos[default_build_pos];
					}
				}

				if (pos != xy() && !b.type->unit->is_refinery) {
					auto&bs = grid::get_build_square(pos);
					if (b.type->unit->requires_creep && !pred_creep(bs)) xcept("unreachable: bad build spot for %s", b.type->name);
					if (b.type->unit->race != race_zerg && !b.type->unit->requires_creep && !pred_not_creep(bs)) xcept("unreachable: bad build spot for %s", b.type->name);
					if (b.type->unit->requires_pylon && !pred_pylon(bs)) xcept("unreachable: bad build spot for %s", b.type->name);
				}
			}
// 			xy pos = build_spot_finder::find(make_range_filter(my_buildings,[&](unit*u) {
// 				return u->type->build_near_me;
// 			}),b.type->unit);
			//log("found build pos at %d %d :D\n",pos.x,pos.y);
			b.build_pos = pos;
			if (!b.type->unit->is_refinery) {
				if (pos!=xy()) grid::reserve_build_squares(pos,b.type->unit);
				for (auto i=++build_order.iterator_to(b);i!=build_order.end();++i) {
					build_task&b2 = *i;
					if (b2.build_pos==xy() || b2.built_unit) continue;
					if (b2.type->unit->is_refinery) continue;
					if (!build_spot_finder::can_build_at(b2.type->unit,b2.build_pos)) {
						b2.build_pos = xy();
					} else grid::reserve_build_squares(b2.build_pos,b2.type->unit);
				}
			}
			multitasking::yield_point();
		}
	}
	if (b.build_pos != xy()) {
		if (!b.type->unit || !b.type->builder->is_worker) xcept("build task %s should not have a build pos (builder is %s)", b.type->name, b.type->builder->name);
		bool builds_on_its_own = false;
		if (b.type->unit) builds_on_its_own = b.type->unit->builder_type==unit_types::probe || b.type->unit->builder_type==unit_types::drone;
		unit*builder = b.builder;
		//if (!builder || current_frame-b.last_look_for_builder>=15*8 || builder->dead || builder->type!=b.type->unit->builder_type || builder->controller->action!=unit_controller::action_build || builder->controller->target_pos!=b.build_pos || builder->controller->target_type!=b.type->unit) {
		//if ((!builds_on_its_own || !b.built_unit) && (!builder || (current_frame-b.last_look_for_builder>=15*8 && !b.built_unit) || builder->dead || builder->type!=b.type->unit->builder_type) && current_frame-b.last_look_for_builder>=15*2) {
		auto builder_score = [&](unit*u) {
			if (!u->is_completed) return std::numeric_limits<double>::infinity();
			if (u->controller->action != unit_controller::action_idle && u->controller->action != unit_controller::action_gather && u->controller->action != unit_controller::action_build) return std::numeric_limits<double>::infinity();
			double d = 0;
			double f = 0.0;
			if (u->controller->action == unit_controller::action_build && u->controller->flag != &b) {
				if (u->type->race == race_zerg) return std::numeric_limits<double>::infinity();
				build_task&tb = *(build_task*)u->controller->flag;
				if (!tb.built_unit) f += std::max((tb.build_frame - current_frame), frames_to_reach(u, unit_pathing_distance(u, tb.build_pos)));
				else f += tb.built_unit->remaining_build_time;
				d = unit_pathing_distance(u->type, tb.build_pos, b.build_pos);
				if (tb.type->unit) {
					if (u->type->race == race_terran) f += tb.type->unit->build_time;
				}
			} else {
				if (b.built_unit) d = units_pathing_distance(u, b.built_unit);
				else d = unit_pathing_distance(u, b.build_pos);
			}
			f += (double)frames_to_reach(u, d);
			//if (f+f/8 + latency_frames<b.build_frame-current_frame) return std::numeric_limits<double>::infinity();
			if (u->is_carrying_minerals_or_gas && u->controller->flag != &b) f += 15 * 4;
			return f;
		};
		if ((b.build_frame && b.build_frame - current_frame <= 15 * 30 || b.built_unit) && (!b.built_unit || !builds_on_its_own)) {
			if (!b.built_unit || !b.built_unit->build_unit) {
				if ((!builder || current_frame - b.last_look_for_builder >= 15 * 8) && current_frame - b.last_look_for_builder >= 15 * 2) {
					b.last_look_for_builder = current_frame;
					builder = get_best_score(my_units_of_type[b.type->unit->builder_type], builder_score, std::numeric_limits<double>::infinity());
				}
			}
		}
		if (!b.built_unit && b.build_frame-current_frame>=15*45) builder = nullptr;
		if (builds_on_its_own && b.built_unit && b.builder) {
			builder = nullptr;
		}
		if (builder && builder->controller->action!=unit_controller::action_build && builder->controller->action!=unit_controller::action_gather && builder->controller->action!=unit_controller::action_idle) builder = nullptr;
		if (builder && b.builder) {
			auto old_score = builder_score(b.builder);
			auto new_score = builder_score(builder);
			if (old_score<new_score*1.25) builder = b.builder;
		}
		if (builder!=b.builder) {
			log("%s: change builder from %p (%f) to %p (%f)\n", b.type->name, builder, builder ? builder_score(builder) : 0, b.builder, b.builder ? builder_score(b.builder) : 0);
			if (b.builder && b.builder->controller->flag==&b) {
				b.builder->controller->action = unit_controller::action_idle;
				b.builder->controller->target = nullptr;
				b.builder->controller->target_pos = xy();
				b.builder->controller->target_type = nullptr;
				b.builder->controller->flag = nullptr;
			}
			b.builder = builder;
		}
		if (builder) {
			log("%s build %s in %d!\n",builder->type->name,b.type->unit->name,b.build_frame - current_frame);
			auto*c = builder->controller;
			if (c->action!=unit_controller::action_build || (c->flag==&b && (c->target_pos!=b.build_pos || c->target_type!=b.type->unit || c->target!=b.built_unit || c->wait_until!=b.build_frame))) {
				double d = unit_pathing_distance(builder,b.build_pos);
				int t = frames_to_reach(builder,d);
				if (t+t/8 + latency_frames>=b.build_frame-current_frame) {
					c->action = unit_controller::action_build;
					c->target = b.built_unit;
					c->target_pos = b.build_pos;
					c->target_type = b.type->unit;
					c->flag = &b;
					c->wait_until = b.build_frame;
				}
			}
		}

	}

	auto wait_for_building = [&](unit*builder) {
		bool wait = false;
		unit_building*building = builder->building;
		if (current_frame - building->building_addon_frame <= 15) wait = true;
		if (building->is_lifted) {
			wait = true;
			if (current_frame - building->building_addon_frame >= 30 && !building->is_liftable_wall && current_frame >= building->nobuild_until) {
				auto pred = [&](grid::build_square&bs) {
					if (something_is_in_the_way(bs.pos, builder->type)) return false;
					return build_full_check(bs, builder->type);
				};
				xy pos = b.land_pos;
				if (pos == xy() || !pred(grid::get_build_square(pos))) {
					if (pred(grid::get_build_square(building->build_pos))) pos = building->build_pos;
					else {
						std::array<xy, 1> starts;
						starts[0] = building->last_registered_pos;
						pos = build_spot_finder::find_best(starts, 32, [&](grid::build_square&bs) {
							return pred(bs);
						}, [&](xy pos) {
							return diag_distance(building->build_pos - pos);
						});
					}
					b.land_pos = pos;
				}
				if (pos != xy()) {
					xy move_to = pos + xy(builder->type->tile_width * 16, builder->type->tile_height * 16);
					if (diag_distance(move_to - builder->pos) >= 128) {
						builder->game_unit->move(BWAPI::Position(move_to.x, move_to.y));
						builder->controller->noorder_until = current_frame + 15;
					} else {

						builder->game_unit->land(BWAPI::TilePosition(pos.x / 32, pos.y / 32));
						builder->controller->noorder_until = current_frame + 15;
					}
				}
			}
		} else b.land_pos = xy();
		return wait;
	};

	if (!b.type->builder->is_worker && (!b.builder || current_frame>=b.builder->controller->noorder_until)) {

		if (!b.built_unit && b.type->unit) {
			unit*builder = b.builder;
			if (b.build_frame - current_frame <= 15 * 30) {
				unit_type*requires_addon = nullptr;
				for (unit_type*prereq : b.type->unit->required_units) {
					if (prereq->is_addon && prereq->builder_type == b.type->unit->builder_type) {
						requires_addon = prereq;
						break;
					}
				}
				//if ((!builder || current_frame-b.last_look_for_builder>=15*8) && current_frame-b.last_look_for_builder>=15*2) {
				if ((!builder || current_frame - b.last_look_for_builder >= 15 * 2) && current_frame - b.last_look_for_builder >= 15 * 2) {
					b.last_look_for_builder = current_frame;
					builder = get_best_score(my_completed_units_of_type[b.type->unit->builder_type],[&](unit*u) {
						if (requires_addon && (!u->addon || u->addon->type != requires_addon || !u->addon->is_completed)) return std::numeric_limits<double>::infinity();
						if (b.type->unit->is_addon && u->addon) return std::numeric_limits<double>::infinity();
						if (b.type->unit->is_addon && u->type == unit_types::cc) {
							//if (!u->game_unit->canBuildAddon(b.type->unit->game_unit_type)) return std::numeric_limits<double>::infinity();
						}
						double cost = 0.0;
						if (!b.type->unit->is_addon) {
							if (u->building && u->building->is_lifted) cost = 15 * 5;
							if (u->building && current_frame - u->building->building_addon_frame <= 15) return std::numeric_limits<double>::infinity();
							if (u->building && grid::get_build_square(u->building->build_pos).reserved.first) return std::numeric_limits<double>::infinity();
							if (!requires_addon && u->addon) cost += 15 * 5;
						}
						if (u->building && current_frame < u->building->nobuild_until) return std::numeric_limits<double>::infinity();
						if (u->controller->noorder_until > current_frame) return std::numeric_limits<double>::infinity();
						if (b.type->unit == unit_types::nuclear_missile && u->has_nuke) return std::numeric_limits<double>::infinity();
						return (double)u->remaining_whatever_time + cost;
					},std::numeric_limits<double>::infinity());
				}
			}
			if (builder && !b.built_unit) {
				if (b.type->unit->is_addon && builder->addon) builder = nullptr;
			}
			if (builder && !b.built_unit) {
				if (b.type->unit && !b.type->unit->is_building) {
					bool wait = false;
					if (builder->building) wait = wait_for_building(builder);
					if (!wait) {
						if (builder->remaining_whatever_time <= latency_frames) {
							if (current_frame + latency_frames >= b.build_frame) {
								if (builder->controller->noorder_until <= current_frame && builder->game_unit->train(b.type->unit->game_unit_type)) {
									log("train hurray\n");
									builder->controller->noorder_until = current_frame + latency_frames + 4;
								} else {
									log("train failed\n");
									builder = nullptr;
								}
							}
						}
					}
				} else if (b.type->unit && b.type->unit->is_addon) {
					unit_building*building = builder->building;
					if (building) {
						building->building_addon_frame = current_frame;
						//if (b.type->unit == unit_types::comsat_station && !building->is_lifted) {
						if (builder->type == unit_types::cc && !building->is_lifted) {
							if (builder->remaining_whatever_time <= latency_frames && builder->controller->noorder_until <= current_frame) {
								if (current_frame + latency_frames >= b.build_frame) {
									builder->game_unit->buildAddon(b.type->unit->game_unit_type);
									builder->controller->noorder_until = current_frame + 15;
								}
							}
						} else {
							if ((building->is_lifted || !building->build_squares_occupied.empty())) {
								for (auto*bs_p : building->build_squares_occupied) {
									bs_p->building = nullptr;
								}
								const xy addon_rel_pos(builder->type->tile_width * 32, (builder->type->tile_height - b.type->unit->tile_height) * 32);
								auto pred = [&](grid::build_square&bs) {
									if (something_is_in_the_way(bs.pos, builder->type)) return false;
									if (!build_full_check(bs, builder->type, true)) return false;
									grid::reserve_build_squares(bs.pos, builder->type);
									auto&addon_bs = grid::get_build_square(bs.pos + addon_rel_pos);
									bool r = build_full_check(addon_bs, b.type->unit, true);
									r &= !something_is_in_the_way(addon_bs.pos, b.type->unit);
									grid::unreserve_build_squares(bs.pos, builder->type);
									return r;
								};
								xy pos = b.land_pos;
								if (pos == xy()) pos = building->build_pos;
								if (pos == xy() || !pred(grid::get_build_square(pos))) {
									if (pred(grid::get_build_square(building->build_pos))) pos = building->build_pos;
									else {
										std::array<xy, 1> starts;
										starts[0] = building->last_registered_pos;
										pos = build_spot_finder::find_best(starts, 32, [&](grid::build_square&bs) {
											return pred(bs);
										}, [&](xy pos) {
											return diag_distance(building->build_pos - pos);
										});
									}
									b.land_pos = pos;
								}
								if (pos != xy()) {
									if (builder->remaining_whatever_time <= latency_frames && builder->controller->noorder_until <= current_frame) {
										//log("pos is %d %d, builder->pos is %d %d\n", pos.x, pos.y, builder->pos.x, builder->pos.y);
										if (pos != building->build_pos) {
											if (!building->is_lifted) {
												//log(" -- lift!\n");
												builder->game_unit->lift();
												builder->controller->noorder_until = current_frame + 15;
											} else {
												xy move_to = pos + xy(builder->type->tile_width * 16, builder->type->tile_height * 16);
												//log(" -- move to %d %d ?\n", move_to.x, move_to.y);
												if (diag_distance(move_to - builder->pos) >= 128) {
													//log(" -- move!\n");
													builder->game_unit->move(BWAPI::Position(move_to.x, move_to.y));
													builder->controller->noorder_until = current_frame + 15;
												} else {
													//log(" -- land!\n");
													builder->game_unit->land(BWAPI::TilePosition(pos.x / 32, pos.y / 32));
													builder->controller->noorder_until = current_frame + 15;
												}
											}
										} else {
											//log(" -- build addon!\n");
											if (current_frame + latency_frames >= b.build_frame) {
												builder->game_unit->buildAddon(b.type->unit->game_unit_type);
												builder->controller->noorder_until = current_frame + 15;
											}
										}
									}
								}

								for (auto*bs_p : building->build_squares_occupied) {
									bs_p->building = builder;
								}
							} else b.land_pos = xy();
						}
					}
					else xcept("unreachable (non-building build addon)");
				} else if (b.type->unit && b.type->unit->is_building) {
					if (builder->remaining_whatever_time <= latency_frames) {
						if (current_frame + latency_frames >= b.build_frame) {
							if (builder->controller->noorder_until <= current_frame && builder->game_unit->morph(b.type->unit->game_unit_type)) {
								log("train hurray\n");
								builder->controller->noorder_until = current_frame + latency_frames + 4;
							} else {
								log("train failed\n");
								builder = nullptr;
							}
						}
					}
				}
				else xcept("unreachable (build/train unknown)");
			}
			if (builder!=b.builder) b.builder = builder;
		}

		if (b.type->upgrade && !b.upgrade_done_frame) {

			unit*builder = b.builder;
			if (b.build_frame - current_frame <= 15 * 30) {
				if (!builder || current_frame - b.last_look_for_builder >= 15 * 2) {
					b.last_look_for_builder = current_frame;
					builder = get_best_score(my_units_of_type[b.type->builder], [&](unit*u) {
						return (double)u->remaining_whatever_time;
					}, std::numeric_limits<double>::infinity());
				}
			}
			if (builder) {
				bool wait = false;
				if (builder->building) wait = wait_for_building(builder);
				if (!wait) {
					if (current_frame + latency_frames >= b.build_frame) {
						if (builder->controller->noorder_until <= current_frame) {
							bool okay = false;
							okay |= builder->game_unit->upgrade(b.type->upgrade->game_upgrade_type);
							okay |= builder->game_unit->research(b.type->upgrade->game_tech_type);
							if (okay) b.upgrade_done_frame = current_frame + b.type->build_time;
							builder->controller->noorder_until = current_frame + 15;
						}
					}
				}
			}

			if (builder != b.builder) b.builder = builder;
		}

	}

}

bool match_new_unit(unit*u) {
	for (build_task&b : build_tasks) {
		if (b.built_unit) continue;
		if (b.type->unit != u->type) continue;
		if (u->build_unit == b.builder) {
			b.built_unit = u;
			b.build_started_frame = current_frame;
			return true;
		}
		if (b.build_pos!=xy()) {
			if (u->building && u->building->build_pos==b.build_pos) {
				b.built_unit = u;
				b.build_started_frame = current_frame;
				return true;
			}
		} else {
			// 			if (b.builder) log("%s: b.builder->pos is %d %d u->pos is %d %d\n",b.type->name,b.builder->pos.x,b.builder->pos.y,u->pos.x,u->pos.y);
			// 			else log("%s: no builder\n",b.type->name);
			if (b.type->unit->is_addon) {
				if (b.builder && b.builder->addon == u) {
					b.built_unit = u;
					b.build_started_frame = current_frame;
					return true;
				}
			} else {
				if (b.builder && b.builder->pos == u->pos) {
					b.built_unit = u;
					b.build_started_frame = current_frame;
					return true;
				}
			}
		}
	}
	return false;
}

void on_create_unit(unit*u) {
	if (u->owner!=players::my_player) return;

	bool found = match_new_unit(u);
	if (!found) {
		log("created unit %s does not have a corresponding build task\n",u->type->name);
		if (u->build_unit && u->build_unit->controller->action == unit_controller::action_build) {
			u->build_unit->controller->action = unit_controller::action_idle;
			u->build_unit->controller->flag = nullptr;
		}
	}
}
void on_morph_unit(unit*u) {
	if (u->owner!=players::my_player) return;

	bool found = match_new_unit(u);
	if (!found) {
		log("morphed unit %s does not have a corresponding build task\n",u->type->name);
	}
}


void execute_build_task() {
	while (true) {

		int ccs_without_addon = 0;
		for (unit*u : my_units_of_type[unit_types::cc]) {
			if (!u->addon) ++ccs_without_addon;
		}

		for (auto i = build_tasks.begin(); i != build_tasks.end();) {
			build_task&b = *i++;
			if (b.upgrade_done_frame && current_frame >= b.upgrade_done_frame) {
				log("upgrade %s is done\n", b.type->name);
				cancel_build_task(&b);
				continue;
			}
			if (b.type->upgrade && players::my_player->upgrades.count(b.type->upgrade)) {
				log("already has upgrade %s\n", b.type->name);
				cancel_build_task(&b);
				continue;
			}
			if (b.built_unit) {
				if (b.built_unit->is_completed) {
					log("%s is no longer being constructed!\n",b.type->name);
					cancel_build_task(&b);
					continue;
				}
				if (current_frame - b.build_started_frame >= b.type->build_time + 15 * 60 * 2) {
					log("build task %s timed out\n", b.type->name);
					cancel_build_task(&b);
					continue;
				}
				if (b.built_unit->dead || b.built_unit->type != b.type->unit || b.built_unit->owner != players::my_player) {
					log("build task %s lost its built unit!\n", b.type->name);
					b.built_unit = nullptr;
					if (b.type->unit != unit_types::bunker) unset_build_pos(&b);
				}
			}
			if (b.builder) {
				if (b.builder->dead || b.builder->type != b.type->builder || b.builder->owner != players::my_player) {
					log("build task %s lost its builder\n", b.type->name);
					b.builder = nullptr;
					if (b.type->unit != unit_types::bunker) unset_build_pos(&b);
				}
			}
			if (b.dead && b.reference_count==0) {
				log("build task %s dead, no references\n",b.type->name);
				cancel_build_task(&b);
				continue;
			}
			if (b.type->unit == unit_types::comsat_station && !b.built_unit) {
				--ccs_without_addon;
				if (ccs_without_addon < 0) {
					log("refusing to build cc for comsat\n");
					cancel_build_task(&b);
					continue;
				}
			}
		}

		for (build_task&b : build_tasks) {
			if (b.dead) continue;
			if (b.build_frame==0 && !b.built_unit) continue;
			execute_build(b);
		}

		for (unit*u : my_buildings) {
			if (!u->is_completed) {
				bool found = false;
				for (build_task&b : build_tasks) {
					if (b.built_unit == u) {
						found = true;
						break;
					}
				}
				if (!found) {
					u->game_unit->cancelConstruction();
				}
			}
		}

		for (unit*u : my_workers) {
			if (u->controller->action == unit_controller::action_build) {
				bool found = false;
				for (build_task&b : build_tasks) {
					if (b.builder == u) {
						found = true;
						break;
					}
				}
				if (!found) {
					u->controller->action = unit_controller::action_idle;
					u->controller->flag = nullptr;
				}
			}
		}

		multitasking::sleep(3);
	}
}


void render() {

	for (build_task&b : build_order) {
		double t = b.build_frame ? (b.build_frame-current_frame)/15.0 : std::numeric_limits<double>::infinity();
		render::draw_screen_stacked_text(16, 56, format("%s - %.1fs", b.type->unit ? short_type_name(b.type->unit) : b.type->name, t));

		if (b.build_pos!=xy()) {
			game->drawTextMap(b.build_pos.x, b.build_pos.y, "%s", b.type->unit->name.c_str());
		}
	}

}

void init() {

	//add_build_task(0,units::get_unit_type(BWAPI::UnitTypes::Protoss_Pylon));
	//add_build_task(10,unit_types::supply_depot);
//	for (int i = 0; i < 10; ++i)  add_build_task(0, unit_types::supply_depot);
	//add_build_task(0,unit_types::barracks);
//	add_build_task(0, unit_types::refinery);
	//add_build_task(0, unit_types::science_facility);
	//add_build_task(0, unit_types::spawning_pool);
// 	add_build_task(0, unit_types::siege_tank_tank_mode);
// 	add_build_task(0, unit_types::medic);
// 	add_build_task(0, unit_types::ghost);
// 	for (int i = 0; i < 4; ++i)  add_build_task(0, unit_types::battlecruiser);
	//for (int i=0;i<10;++i) add_build_task(0,unit_types::marine);
	//for (int i=0;i<30;++i) add_build_task(i,unit_types::spawning_pool);
	//for (int i=0;i<30;++i) add_build_task(i,unit_types::evolution_chamber);
	
	//for (int i=0;i<10;++i) add_build_task(i,unit_types::gateway);
	//for (int i=0;i<100;++i) add_build_task(i,unit_types::photon_cannon);
	//for (int i=0;i<100;++i) add_build_task(0,unit_types::pylon);
	//for (int i=0;i<100;++i) add_build_task(0,unit_types::supply_depot);
	//for (int i=0;i<100;++i) add_build_task(0,unit_types::supply_depot);
	//add_build_task(0,units::get_unit_type(BWAPI::UnitTypes::Terran_Battlecruiser));

	multitasking::spawn(generate_build_order_task,"generate build order");
	multitasking::spawn(execute_build_task,"execute build");

	render::add(render);

	units::on_create_callbacks.push_back(on_create_unit);
	units::on_morph_callbacks.push_back(on_morph_unit);

}


}

