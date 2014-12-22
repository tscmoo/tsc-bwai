
namespace combat {
;

struct choke_t {
	a_deque<xy> path;
	a_vector<grid::build_square*> build_squares;
	double width;
	xy inside;
	xy center;
	xy outside;
};
choke_t defence_choke;
a_deque<xy> dont_siege_here_path;
tsc::dynamic_bitset dont_siege_here;

struct defence_spot {
	xy pos;
	a_map<unit_type*, int> unit_counts;
	a_vector<std::tuple<grid::build_square*, double>> squares;
	double value;
};
a_vector<defence_spot> defence_spots;

tsc::dynamic_bitset my_base;
tsc::dynamic_bitset op_base;

void update_base(tsc::dynamic_bitset&base, const a_vector<unit*>&buildings) {
	base.reset();

	tsc::dynamic_bitset visited(grid::build_grid_width*grid::build_grid_height);
	a_deque<std::tuple<grid::build_square*, xy>> open;
	for (unit*u : buildings) {
		for (auto*v : u->building->build_squares_occupied) {
			xy pos = v->pos;
			open.emplace_back(&grid::get_build_square(pos), pos);
			size_t index = grid::build_square_index(pos);
			visited.set(index);
			base.set(index);
		}
	}
	while (!open.empty()) {
		grid::build_square*bs;
		xy origin;
		std::tie(bs, origin) = open.front();
		open.pop_front();

		auto add = [&](int n) {
			grid::build_square*nbs = bs->get_neighbor(n);
			if (!nbs) return;
			if (!nbs->entirely_walkable) return;
			if (diag_distance(nbs->pos - origin) >= 32 * 12) return;
			size_t index = grid::build_square_index(*nbs);
			if (visited.test(index)) return;
			visited.set(index);
			base.set(index);

			open.emplace_back(nbs, origin);
		};
		add(0);
		add(1);
		add(2);
		add(3);
	}
}

void update_my_base() {
	update_base(my_base, my_buildings);
}
void update_op_base() {
	update_base(op_base, enemy_buildings);
}

void generate_defence_spots() {

	a_multimap<grid::build_square*, std::tuple<unit_type*, double>> hits;

	auto spread_from = [&](xy pos, unit_type*ut) {
		tsc::dynamic_bitset visited(grid::build_grid_width*grid::build_grid_height);
		a_deque<std::tuple<grid::build_square*, double>> open;
		open.emplace_back(&grid::get_build_square(pos), 0.0);
		visited.set(grid::build_square_index(pos));
		while (!open.empty()) {
			grid::build_square*bs;
			double distance;
			std::tie(bs, distance) = open.front();
			open.pop_front();

			auto add = [&](int n) {
				grid::build_square*nbs = bs->get_neighbor(n);
				if (!nbs) return;
				if (!nbs->entirely_walkable) return;
				size_t index = grid::build_square_index(*nbs);
				if (visited.test(index)) return;
				visited.set(index);
				double d = distance + 1;
				if (my_base.test(index)) {
					hits.emplace(nbs, std::make_tuple(ut, d));
					return;
				}
				open.emplace_back(nbs, d);
			};
			add(0);
			add(1);
			add(2);
			add(3);
		}
	};

	for (unit*u : enemy_units) {
		if (u->building) continue;
		if (u->gone) continue;
		spread_from(u->pos, u->type);
	}
	if (hits.empty()) {
		// TODO: change this to spread from starting locations (not in my_base) instead
		spread_from(xy(grid::map_width / 2, grid::map_height / 2), unit_types::zergling);
	}

	a_vector<defence_spot> new_defence_spots;
	for (auto&v : hits) {
		grid::build_square*bs = v.first;
		unit_type*ut = std::get<0>(v.second);
		double value = std::get<1>(v.second);
		defence_spot*ds = nullptr;
		for (auto&v : new_defence_spots) {
			for (auto&v2 : v.squares) {
				if (diag_distance(bs->pos - std::get<0>(v2)->pos) < 32*2) {
					ds = &v;
					break;
				}
			}
			if (ds) break;
		}
		if (!ds) {
			new_defence_spots.emplace_back();
			ds = &new_defence_spots.back();
		}
		ds->unit_counts[ut] += 1;
		bool found = false;
		for (auto&v : ds->squares) {
			if (std::get<0>(v) == bs) {
				std::get<1>(v) += value;
				found = true;
				break;
			}
		}
		if (!found) ds->squares.emplace_back(bs, value);
	}
	for (auto&v : new_defence_spots) {
		std::sort(v.squares.begin(), v.squares.end(), [&](const std::tuple<grid::build_square*, double>&a, const std::tuple<grid::build_square*, double>&b) {
			return std::get<1>(a) < std::get<1>(b);
		});
		v.pos = std::get<0>(v.squares.front())->pos + xy(16, 16);
		v.value = std::get<1>(v.squares.front());
	}
	std::sort(new_defence_spots.begin(), new_defence_spots.end(), [&](const defence_spot&a, const defence_spot&b) {
		return a.value < b.value;
	});
	defence_spots = std::move(new_defence_spots);

	log("there are %d defence spots\n", defence_spots.size());

}

void generate_defence_spots_task() {

	while (true) {

		generate_defence_spots();

		tsc::high_resolution_timer ht;

// 		combat_eval::eval test;
// 		for (int i = 0; i < 50; ++i) test.add_unit(units::get_unit_stats(unit_types::vulture, units::my_player), 0);
// 		for (int i = 0; i < 20; ++i) test.add_unit(units::get_unit_stats(unit_types::marine, units::my_player), 1);
// 		for (int i = 0; i < 15; ++i) test.add_unit(units::get_unit_stats(unit_types::siege_tank_tank_mode, units::my_player), 1);
// 		test.add_unit(units::get_unit_stats(unit_types::missile_turret, units::my_player), 1);
// // 		test.add_unit(units::get_unit_stats(unit_types::vulture, units::my_player), 0);
// // 		test.add_unit(units::get_unit_stats(unit_types::siege_tank_tank_mode, units::my_player), 1);
// 
// 		test.run();
// 
// 		log("took %f\n", ht.elapsed());
// 		
// 		log("start supply: %g %g\n", test.teams[0].start_supply, test.teams[1].start_supply);
// 		log("end supply: %g %g\n", test.teams[0].end_supply, test.teams[1].end_supply);
// 
// 		log("damage dealt: %g %g\n", test.teams[0].damage_dealt, test.teams[1].damage_dealt);
// 
// 		xcept("stop");

		multitasking::sleep(90);
	}

}

template<typename pred_T>
void find_nearby_entirely_walkable_tiles(xy pos, pred_T&&pred, double max_distance = 32 * 12) {
	tsc::dynamic_bitset visited(grid::build_grid_width*grid::build_grid_height);
	a_deque<std::tuple<grid::build_square*, xy>> open;
	open.emplace_back(&grid::get_build_square(pos), pos);
	size_t index = grid::build_square_index(pos);
	visited.set(index);
	while (!open.empty()) {
		grid::build_square*bs;
		xy origin;
		std::tie(bs, origin) = open.front();
		open.pop_front();
		if (!pred(bs->pos)) continue;

		auto add = [&](int n) {
			grid::build_square*nbs = bs->get_neighbor(n);
			if (!nbs) return;
			if (!nbs->entirely_walkable) return;
			if (diag_distance(nbs->pos - origin) >= max_distance) return;
			size_t index = grid::build_square_index(*nbs);
			if (visited.test(index)) return;
			visited.set(index);

			open.emplace_back(nbs, origin);
		};
		add(0);
		add(1);
		add(2);
		add(3);
	}
}

struct combat_unit {
	unit*u = nullptr;
	enum { action_idle, action_offence, action_tactic };
	int action = action_idle;
	enum { subaction_idle, subaction_move, subaction_fight, subaction_move_directly, subaction_use_ability, subaction_repair };
	int subaction = subaction_idle;
	xy defend_spot;
	xy goal_pos;
	int last_fight = 0;
	int last_run = 0;
	int last_processed_fight = 0;
	unit*target = nullptr;
	xy target_pos;
	int last_wanted_a_lift = 0;
	upgrade_type*ability = nullptr;
	int last_used_special = 0;
	int last_nuke = 0;
	xy siege_pos;
	int last_find_siege_pos = 0;
	bool siege_up_close = false;
	a_deque<xy> path;
	int last_find_path = 0;
	int strategy_busy_until = 0;
};
a_unordered_map<unit*, combat_unit> combat_unit_map;

a_vector<combat_unit*> live_combat_units;
a_vector<combat_unit*> idle_combat_units;

void update_combat_units() {
	live_combat_units.clear();
	size_t worker_count = 0;
	for (unit*u : my_units) {
		if (u->building) continue;
		if (!u->is_completed) continue;
		if (u->controller->action == unit_controller::action_build) continue;
		if (u->controller->action == unit_controller::action_scout) continue;
		if (u->type->is_non_usable) continue;
		combat_unit&c = combat_unit_map[u];
		if (!c.u) c.u = u;
		live_combat_units.push_back(&c);
	}
	idle_combat_units.clear();
	for (auto*cu : live_combat_units) {
		if (cu->action == combat_unit::action_idle) idle_combat_units.push_back(cu);
	}
}

void defence() {

	if (defence_spots.empty()) return;

	auto send_to = [&](combat_unit*cu, defence_spot*s) {
		if (cu->defend_spot == s->pos) return;

		cu->defend_spot = s->pos;
		cu->goal_pos = s->pos;

		cu->subaction = combat_unit::subaction_move;
		cu->target_pos = s->pos;
	};

	for (auto*cu : idle_combat_units) {
		if (cu->u->type->is_worker && !cu->u->force_combat_unit) continue;
		if (current_frame - cu->last_fight <= 40) continue;
		auto&s = defence_spots.front();
		send_to(cu, &s);
	}

}

a_unordered_map<combat_unit*, int> pickup_taken;

void give_lifts(combat_unit*dropship, a_vector<combat_unit*>&allies, int process_uid) {

	a_vector<combat_unit*> wants_a_lift;
	for (auto*cu : allies) {
		double path_time = unit_pathing_distance(cu->u, cu->goal_pos) / cu->u->stats->max_speed;
		double pickup_time = diag_distance(dropship->u->pos - cu->u->pos) / dropship->u->stats->max_speed;
		double air_time = diag_distance(cu->goal_pos - cu->u->pos) / dropship->u->stats->max_speed;
		if (cu->u->is_loaded) {
			if (current_frame - cu->last_wanted_a_lift <= 15 * 5) {
				wants_a_lift.push_back(cu);
				continue;
			}
			if (path_time + 15 * 5 < pickup_time + air_time) continue;
		} else if (path_time <= (pickup_time + air_time) + 15 * 10) continue;
		log("%s : path_time %g pickup_time %g air_time %g\n", cu->u->type->name, path_time, pickup_time, air_time);
		wants_a_lift.push_back(cu);
		cu->last_wanted_a_lift = current_frame;
	}
	int space = dropship->u->type->space_provided;
	for (unit*lu : dropship->u->loaded_units) {
		space -= lu->type->space_required;
	}
	a_unordered_set<combat_unit*> units_to_pick_up;
	while (space > 0) {
		combat_unit*nu = get_best_score(wants_a_lift, [&](combat_unit*nu) {
			if (nu->u->type->is_flyer) return std::numeric_limits<double>::infinity();
			if (nu->u->is_loaded || units_to_pick_up.find(nu) != units_to_pick_up.end()) return std::numeric_limits<double>::infinity();
			if (nu->u->type->space_required > space) return std::numeric_limits<double>::infinity();
			if (pickup_taken[nu] == process_uid) return std::numeric_limits<double>::infinity();
			return diag_distance(nu->u->pos - dropship->u->pos);
		}, std::numeric_limits<double>::infinity());
		if (!nu) break;
		units_to_pick_up.insert(nu);
		space -= nu->u->type->space_required;
	}

	for (unit*u : dropship->u->loaded_units) {
		bool found = false;
		for (auto*cu : wants_a_lift) {
			if (cu->u == u) {
				found = true;
				break;
			}
		}
		if (!found) {
			if (dropship->u->game_unit->unload(u->game_unit)) {
				dropship->u->controller->noorder_until = current_frame + 4;
			}
		}
	}

	for (auto*cu : units_to_pick_up) {
		pickup_taken[cu] = process_uid;
		// todo: find the best spot to meet the dropship
		cu->subaction = combat_unit::subaction_move;
		cu->target_pos = dropship->u->pos;
	}

	combat_unit*pickup = get_best_score(units_to_pick_up, [&](combat_unit*cu) {
		return diag_distance(cu->u->pos - dropship->u->pos);
	});
	if (pickup) {
		dropship->subaction = combat_unit::subaction_move;
		dropship->target_pos = pickup->u->pos;
		if (diag_distance(pickup->u->pos - dropship->u->pos) <= 32 * 4) {
			dropship->subaction = combat_unit::subaction_idle;
			dropship->u->controller->action = unit_controller::action_idle;
			if (current_frame >= dropship->u->controller->noorder_until) {
				pickup->u->game_unit->rightClick(dropship->u->game_unit);
				pickup->u->controller->noorder_until = current_frame + 15;
				dropship->u->controller->noorder_until = current_frame + 4;
			}
		}
	}
}

struct group_t {
	size_t idx;
	double value;
	a_vector<unit*> enemies;
	a_vector<combat_unit*> allies;
	tsc::dynamic_bitset threat_area;
	bool is_defensive_group = false;
	bool is_aggressive_group = false;
};
a_vector<group_t> groups;
multitasking::mutex groups_mut;
tsc::dynamic_bitset entire_threat_area;
tsc::dynamic_bitset entire_threat_area_edge;

void update_group_area(group_t&g) {
	g.threat_area.reset();
	for (unit*e : g.enemies) {
		tsc::dynamic_bitset visited(grid::build_grid_width*grid::build_grid_height);
		a_deque<xy> walk_open;
		a_deque<std::tuple<xy, xy>> threat_open;
		double threat_radius = 32;
		if (e->stats->ground_weapon && e->stats->ground_weapon->max_range > threat_radius) threat_radius = e->stats->ground_weapon->max_range;
		if (e->stats->air_weapon && e->stats->air_weapon->max_range > threat_radius) threat_radius = e->stats->air_weapon->max_range;
		if (e->type == unit_types::bunker) threat_radius = 32 * 6;
		threat_radius += e->type->width;
		threat_radius += 48;
		if (e->type == unit_types::siege_tank_siege_mode) threat_radius += 128;
		if (e->type == unit_types::dragoon) threat_radius += 64;
		//double walk_radius = e->stats->max_speed * 15 * 2;
		double walk_radius = e->stats->max_speed * 20;
		//double walk_radius = 32;
		walk_open.emplace_back(e->pos);
		visited.set(grid::build_square_index(e->pos));
		while (!walk_open.empty() || !threat_open.empty()) {
			xy pos;
			xy origin;
			bool is_walk = !walk_open.empty();
			if (is_walk) {
				pos = walk_open.front();
				walk_open.pop_front();
			} else {
				std::tie(pos, origin) = threat_open.front();
				threat_open.pop_front();
			}
			g.threat_area.set(grid::build_square_index(pos));
			for (int i = 0; i < 4; ++i) {
				xy npos = pos;
				if (i == 0) npos.x += 32;
				if (i == 1) npos.y += 32;
				if (i == 2) npos.x -= 32;
				if (i == 3) npos.y -= 32;
				if ((size_t)npos.x >= (size_t)grid::map_width || (size_t)npos.y >= (size_t)grid::map_height) continue;
				size_t index = grid::build_square_index(npos);
				if (visited.test(index)) continue;
				visited.set(index);
				double walk_d = (npos - e->pos).length();
				if (is_walk) {
					bool out_of_range = !e->type->is_flyer && !grid::get_build_square(npos).entirely_walkable;
					if (!out_of_range) out_of_range = walk_d >= walk_radius;
					if (out_of_range) threat_open.emplace_back(npos, npos);
					else walk_open.emplace_back(npos);
				} else {
					double d = (npos - origin).length();
					if (d < threat_radius) {
						threat_open.emplace_back(npos, origin);
					}
				}
			}
		}
	}
}

a_deque<xy> find_bigstep_path(unit_type*ut, xy from, xy to) {
	if (ut->is_flyer) {
		return flyer_pathing::find_bigstep_path(from, to);
	} else {
		auto path = square_pathing::find_path(square_pathing::get_pathing_map(ut), from, to);
		a_deque<xy> r;
		for (auto*n : path) r.push_back(n->pos);
		return r;
	}
};

bool no_aggressive_groups = false;
bool aggressive_wraiths = false;


bool search_and_destroy = false;

void update_groups() {

	a_vector<group_t> new_groups;

	a_vector<std::tuple<double, unit*>> sorted_enemy_units;
	for (unit*e : enemy_units) {
		double d = get_best_score_value(my_units, [&](unit*u) {
			if (u->type->is_non_usable) return std::numeric_limits<double>::infinity();
			return diag_distance(e->pos - u->pos);
		}, std::numeric_limits<double>::infinity());
		sorted_enemy_units.emplace_back(d, e);
	}
	std::sort(sorted_enemy_units.begin(), sorted_enemy_units.end());

	search_and_destroy = false;
	if (current_used_total_supply >= 50) {
		int enemy_non_building_units = 0;
		int enemy_building_units = 0;
		for (unit*e : enemy_units) {
			if (e->gone) continue;
			if (current_frame - e->last_seen >= 15 * 60 * 10) continue;
			if (e->building) ++enemy_building_units;
			else ++enemy_non_building_units;
		}
		if (enemy_non_building_units == 0) no_aggressive_groups = false;
		if (enemy_non_building_units == 0 && enemy_building_units == 0) search_and_destroy = true;
	}

	log("no_aggressive_groups is %d\n", no_aggressive_groups);
	unit*first_enemy_building = nullptr;
	for (auto&v : sorted_enemy_units) {
		unit*e = std::get<1>(v);
		//if (e->type->is_worker) continue;
		if (!buildpred::attack_now) {
			//if (!e->stats->ground_weapon && !e->stats->air_weapon && !e->type->is_resource_depot) continue;
		}
		if (e->gone) continue;
		if (e->invincible) continue;
		if (e->type->is_non_usable) continue;
		if (e->building) first_enemy_building = e;
		bool is_aggressive = true;
		if (no_aggressive_groups) {
			if (e->visible) {
				double nd = get_best_score_value(my_units, [&](unit*u) {
					if (u->type->is_non_usable) return std::numeric_limits<double>::infinity();
					if (u->controller->action == unit_controller::action_scout) return std::numeric_limits<double>::infinity();
					if (u->type == unit_types::vulture) return std::numeric_limits<double>::infinity();
					if (!u->building) return std::numeric_limits<double>::infinity();
					return units_pathing_distance(u, e);
				});
				if (nd < 32 * 25) {
					is_aggressive = false;
				}
				if (diag_distance(defence_choke.center - e->pos) <= 32 * 15) is_aggressive = false;
			}
		}
		//if (!buildpred::attack_now && op_base.test(grid::build_square_index(e->pos))) continue;
		group_t*g = nullptr;
		for (auto&v : new_groups) {
			unit*ne = v.enemies.front();
			double d = units_distance(e, ne);
			double max_d = 32 * 15;
			if (e->type == unit_types::bunker) max_d = 32 * 5;
			if (d >= max_d) continue;
			unit_type*ut = e->type;
			if (ut->is_building) ut = unit_types::scv;
			if (!square_pathing::unit_can_reach(ut, e->pos, ne->pos)) continue;
			g = &v;
			break;
		}
		if (!g) {
			new_groups.emplace_back();
			g = &new_groups.back();
			g->threat_area.resize(grid::build_grid_width*grid::build_grid_height);
			g->is_defensive_group = false;
			g->is_aggressive_group = true;
		}
		g->enemies.push_back(e);
		if (!is_aggressive) g->is_aggressive_group = false;
	}
	if (first_enemy_building) {
		if (defence_choke.center != xy()) {
			new_groups.emplace_back();
			group_t*g = &new_groups.back();
			g->threat_area.resize(grid::build_grid_width*grid::build_grid_height);
			g->enemies.push_back(first_enemy_building);
			g->is_defensive_group = true;
		}
	}
	for (auto&g : new_groups) {
		int buildings = 0;
		for (unit*e : g.enemies) {
			if (e->building && !e->stats->air_weapon && !e->stats->ground_weapon && e->type != unit_types::bunker) ++buildings;
		}
		if (buildings != g.enemies.size()) {
			for (auto i = g.enemies.begin(); i != g.enemies.end();) {
				unit*e = *i;
				if (e->building && !e->stats->air_weapon && !e->stats->ground_weapon && e->type != unit_types::bunker) i = g.enemies.erase(i);
				else ++i;
			}
		}
	}
	entire_threat_area.reset();
	entire_threat_area_edge.reset();
	for (auto&g : new_groups) {
		update_group_area(g);
	}
// 	for (auto i = new_groups.begin(); i != new_groups.end();) {
// 		auto&g = *i;
// 		bool merged = false;
// 		for (auto&g2 : new_groups) {
// 			if (&g2 == &g) break;
// 			if ((g.threat_area&g2.threat_area).none()) continue;
// 			for (unit*u : g.enemies) {
// 				g2.enemies.push_back(u);
// 			}
// 			update_group_area(g2);
// 			merged = true;
// 			break;
// 		}
// 		if (merged) i = new_groups.erase(i);
// 		else ++i;
// 	}
	for (auto&g : new_groups) {
		entire_threat_area |= g.threat_area;
	}
	for (size_t idx : entire_threat_area) {
		size_t xidx = idx % (size_t)grid::build_grid_width;
		size_t yidx = idx / (size_t)grid::build_grid_width;
		auto test = [&](size_t index) {
			if (!entire_threat_area.test(index)) entire_threat_area_edge.set(index);
		};
		if (xidx != grid::build_grid_last_x) test(idx + 1);
		if (yidx != grid::build_grid_last_y) test(idx + grid::build_grid_width);
		if (xidx != 0) test(idx - 1);
		if (yidx != 0) test(idx - grid::build_grid_width);
	}
	for (auto&g : new_groups) {
		double value = 0.0;
		for (unit*e : g.enemies) {
			//if (e->type->is_worker) continue;
			bool is_attacking_my_workers = e->order_target && e->order_target->owner == players::my_player && e->order_target->type->is_worker;
			if (is_attacking_my_workers) {
				if (!my_base.test(grid::build_square_index(e->order_target->pos))) is_attacking_my_workers = false;
			}
			bool is_attacking_my_base = e->order_target && my_base.test(grid::build_square_index(e->order_target->pos));
			if (is_attacking_my_base || is_attacking_my_workers) e->high_priority_until = current_frame + 15 * 15;
			if (e->high_priority_until >= current_frame) {
				value += e->minerals_value;
				value += e->gas_value * 2;
				value += 1000;
			} else {
				if (e->type->is_worker) value += 200;
				value -= e->minerals_value;
				value -= e->gas_value * 2;
			}
			//if (e->type->is_building) value /= 4;
			//if (my_base.test(grid::build_square_index(e->pos))) value *= 4;
			//if (my_base.test(grid::build_square_index(e->pos))) value *= 4;
			//if (op_base.test(grid::build_square_index(e->pos))) value /= 4;
			if (my_base.test(grid::build_square_index(e->pos))) value += 1000;
		}
// 		unit*nb = get_best_score(my_buildings, [&](unit*u) {
// 			return unit_pathing_distance(unit_types::scv, u->pos, g.enemies.front()->pos);
// 		});
// 		if (nb) {
// 			for (auto*n : square_pathing::find_path(square_pathing::get_pathing_map(unit_types::scv), nb->pos, g.enemies.front()->pos)) {
// 				for (unit*e : enemy_units) {
// 					if (e->gone) continue;
// 					if (e->type->is_non_usable) continue;
// 					weapon_stats*w = e->stats->ground_weapon;
// 					if (!w) continue;
// 					if (diag_distance(e->pos - n->pos) <= w->max_range) {
// 						value -= e->minerals_value;
// 						value -= e->gas_value;
// 					}
// 				}
// 			}
// 		}
		g.value = value;
	}
	std::sort(new_groups.begin(), new_groups.end(), [&](const group_t&a, const group_t&b) {
		return a.value > b.value;
	});
	for (size_t i = 0; i < new_groups.size(); ++i) {
		new_groups[i].idx = i;
	}

	for (auto&g : new_groups) {
		if (true) {
			a_map<unit_type*, int> counts;
			for (unit*e : g.enemies) ++counts[e->type];
			log("group value %g - enemies -", g.value);
			for (auto&v : counts) {
				log("%dx%s ", std::get<1>(v), std::get<0>(v)->name);
			}
			if (g.is_aggressive_group) log(" (aggressive)");
			if (g.is_defensive_group) log(" (defensive)");
			log("\n");
		}
	}

	a_unordered_set<combat_unit*> available_units;
	for (auto*c : live_combat_units) {
		if (c->action == combat_unit::action_tactic) continue;
		if (current_frame < c->strategy_busy_until) continue;
		available_units.insert(c);
	}

	a_unordered_map<combat_unit*, a_unordered_set<group_t*>> can_reach_group;
	for (auto*c : available_units) {
		//if (c->u->is_flying || c->u->is_loaded) continue;
		for (auto&g : new_groups) {
			bool okay = true;
			size_t count = 0;
			auto path = find_bigstep_path(c->u->type, c->u->pos, g.enemies.front()->pos);
			if (!path.empty()) path.pop_back();
			for (xy pos : path) {
				size_t index = grid::build_square_index(pos);
				if (entire_threat_area.test(index)) {
					bool found = false;
					for (auto&g2 : new_groups) {
						if (&g2 == &g) continue;
						if (g2.is_defensive_group) continue;
						for (unit*e : g2.enemies) {
							double wr = 0.0;
							if (e->type == unit_types::bunker) {
								wr = 32 * 5;
							} else {
								weapon_stats*w = c->u->is_flying ? e->stats->air_weapon : e->stats->ground_weapon;
								if (!w) continue;
								wr = w->max_range;
							}
							double d = diag_distance(pos - e->pos);
							//log("%s - %d - d to %s is %g\n", c->u->type->name, count, e->type->name, d);
							if (d <= wr || d <= 32 * 10) {
								//log("%s - %d - -> %d bumped into %d (%s %g)\n", c->u->type->name, count, g.idx, g2.idx, e->type->name, d);
								found = true;
								okay = false;
								break;
							}
						}
						if (found) break;
					}
					if (found) break;
				}
				++count;
			}
			multitasking::yield_point();
			if (g.is_defensive_group) {
				double d = get_best_score_value(enemy_units, [&](unit*e) {
					if (!e->visible) return std::numeric_limits<double>::infinity();
					if (e->type->is_non_usable) return std::numeric_limits<double>::infinity();
					return diag_distance(c->u->pos - e->pos);
				});
				if (d > 32 * 20) {
					okay = true;
				}
			}
			//if (okay) log("%s can reach %d\n", c->u->type->name, g.idx);
			//else log("%s can not reach %d\n", c->u->type->name, g.idx);
			if (okay) can_reach_group[c].insert(&g);
			multitasking::yield_point();
		}
	}

	for (auto i = available_units.begin(); i != available_units.end();) {
		auto*cu = *i;
		size_t index = grid::build_square_index(cu->u->pos);
		group_t*inside_group = nullptr;
		for (auto&g : new_groups) {
			bool can_attack_any = false;
			for (unit*e : g.enemies) {
				weapon_stats*w = e->is_flying ? cu->u->stats->air_weapon : cu->u->stats->ground_weapon;
				if (w) {
					can_attack_any = true;
					break;
				}
			}
			if (!can_attack_any) continue;
			if (g.threat_area.test(index)) {
				if (!inside_group || diag_distance(cu->u->pos - g.enemies.front()->pos) < diag_distance(cu->u->pos - inside_group->enemies.front()->pos)) {
					inside_group = &g;
				}
			}
		}
		if (inside_group) {
			//log("%s is inside group %p\n", cu->u->type->name, inside_group);
			if (cu->u->type->is_worker) {
				if (inside_group->enemies.size() == 1 && inside_group->enemies.front()->type->is_worker) {
					++i;
					continue;
				}
			}
			inside_group->allies.push_back(cu);
			i = available_units.erase(i);
		} else ++i;
	}

	for (auto&g : new_groups) {
		bool is_attacking = false;
		bool is_base_defence = false;
		bool is_just_one_worker = g.enemies.size() == 1 && g.enemies.front()->type->is_worker;
		for (unit*e : g.enemies) {
			if (current_frame - e->last_attacked <= 15 * 10) is_attacking = true;
			if (!is_base_defence) is_base_defence = my_base.test(grid::build_square_index(e->pos));
		}
		while (true) {
			multitasking::yield_point();

			bool aggressive_valkyries = my_completed_units_of_type[unit_types::valkyrie].size() >= 4;
			a_unordered_set<combat_unit*> blacklist;
			auto get_nearest_unit = [&]() {
				return get_best_score(available_units, [&](combat_unit*c) {
					if (no_aggressive_groups && c->u->type != unit_types::vulture && (c->u->type != unit_types::wraith || !aggressive_wraiths) && (c->u->type!=unit_types::valkyrie || !aggressive_valkyries)) {
						if (g.is_aggressive_group) return std::numeric_limits<double>::infinity();
					} else if (g.is_defensive_group) return std::numeric_limits<double>::infinity();
					if (c->u->type->is_worker && !c->u->force_combat_unit && g.is_defensive_group) return std::numeric_limits<double>::infinity();
					if (!c->u->stats->ground_weapon && !c->u->stats->air_weapon) return std::numeric_limits<double>::infinity();
					if (is_just_one_worker && !is_attacking && c->u->type->is_worker) return std::numeric_limits<double>::infinity();
					if (c->u->type->is_worker && !c->u->force_combat_unit && !is_base_defence) return std::numeric_limits<double>::infinity();
					if (blacklist.count(c)) return std::numeric_limits<double>::infinity();
					if (!can_reach_group[c].count(&g)) return std::numeric_limits<double>::infinity();;
					double d;
					if (c->u->is_loaded) d = diag_distance(g.enemies.front()->pos - c->u->pos);
					else d = units_pathing_distance(c->u, g.enemies.front());
					if (c->u->type->is_worker && !c->u->force_combat_unit && d >= 32 * 20 && current_used_total_supply > 30) return std::numeric_limits<double>::infinity();
					if (is_just_one_worker && !is_attacking && d >= 32 * 15) return std::numeric_limits<double>::infinity();
					if (c->u->type->is_worker && !c->u->force_combat_unit && is_just_one_worker && !g.allies.empty()) return std::numeric_limits<double>::infinity();
					return d;
				}, std::numeric_limits<double>::infinity());
			};
			auto is_useful = [&](combat_unit*c) {
				//if (!can_reach_group[c].count(&g)) return false;
				if (!c->u->stats->ground_weapon && !c->u->stats->air_weapon) return true;
				for (unit*e : g.enemies) {
					weapon_stats*w = e->is_flying ? c->u->stats->air_weapon : c->u->stats->ground_weapon;
					if (w) return true;
				}
				return false;
			};
			combat_unit*nearest_unit = get_nearest_unit();
			while (nearest_unit && !is_useful(nearest_unit)) {
				//log("%p (%s) -> %p is not useful\n", nearest_unit, nearest_unit->u->type->name, &g);
				if (blacklist.count(nearest_unit)) {
					nearest_unit = nullptr;
					break;
				}
				blacklist.insert(nearest_unit);
				nearest_unit = get_nearest_unit();
			}
			if (!nearest_unit) {
				log("failed to find unit for group %d\n", g.idx);
				break;
			}

			combat_eval::eval eval;
			auto addu = [&](unit*u, int team) {
				auto&c = eval.add_unit(u, team);
			};
			for (auto*a : g.allies) addu(a->u, 0);
			for (unit*e : g.enemies) addu(e, 1);
			addu(nearest_unit->u, 0);
			eval.run();
			bool done = eval.teams[0].score > eval.teams[1].score*1.5;

			g.allies.push_back(nearest_unit);
			available_units.erase(nearest_unit);
			if (done) break;
			if (is_just_one_worker) break;
		}
		log("group %d: %d allies %d enemies\n", g.idx, g.allies.size(), g.enemies.size());
	}

	for (auto*c : available_units) {
		if (c->u->type->is_worker && !c->u->force_combat_unit) {
			if (c->action!=combat_unit::action_idle || c->subaction != combat_unit::subaction_idle) {
				c->action = combat_unit::action_idle;
				c->subaction = combat_unit::subaction_idle;
				c->u->controller->action = unit_controller::action_idle;
			}
		} else {
			if (c->action == combat_unit::action_offence) {
				c->action = combat_unit::action_idle;
				c->subaction = combat_unit::subaction_idle;
				c->u->controller->action = unit_controller::action_idle;
			}
		}
	}

	if (!new_groups.empty()) {
		auto*largest_group = get_best_score_p(new_groups, [&](const group_t*g) {
			if (no_aggressive_groups && g->is_aggressive_group) return 0;
			return -(int)g->allies.size();
		});
		bool is_just_one_worker = largest_group && largest_group->enemies.size() == 1 && largest_group->enemies.front()->type->is_worker;
		if (largest_group && !largest_group->allies.empty() && !is_just_one_worker) {
			size_t worker_count = 0;
			for (auto*c : available_units) {
				if (c->u->type->is_worker && !c->u->force_combat_unit) {
					if (++worker_count > my_workers.size() / 10) continue;
				}
				if (c->u->type->is_worker && !c->u->force_combat_unit && largest_group->is_defensive_group) continue;
				largest_group->allies.push_back(c);
			}
		}
	} else {
		for (auto*c : available_units) {
			c->action = combat_unit::action_idle;
		}
	}
	available_units.clear();

	for (auto&g : new_groups) {
		xy pos = g.enemies.front()->pos;
		a_vector<combat_unit*> dropships;
		a_vector<combat_unit*> moving_units;
		for (auto*cu : g.allies) {
			cu->goal_pos = pos;
			cu->action = combat_unit::action_offence;
			if (current_frame - cu->last_fight <= 40) continue;
			cu->subaction = combat_unit::subaction_move;
			cu->target_pos = pos;
			if (cu->u->type == unit_types::dropship) dropships.push_back(cu);
			moving_units.push_back(cu);
		}
		int process_uid = current_frame;
		for (auto*cu : dropships) {
			give_lifts(cu, moving_units, process_uid);
		}
	}
	
	std::lock_guard<multitasking::mutex> l(groups_mut);
	groups = std::move(new_groups);
}

template<typename node_data_t = no_value_t, typename pred_T, typename est_dist_T, typename goal_T>
a_deque<xy> find_path(unit_type*ut, xy from, pred_T&&pred, est_dist_T&&est_dist, goal_T&&goal) {
	if (ut->is_flyer) {
		return flyer_pathing::find_path<node_data_t>(from, pred, est_dist, goal);
	} else {
		return square_pathing::find_square_path<node_data_t>(square_pathing::get_pathing_map(ut), from, pred, est_dist, goal);
	}
};

a_unordered_map<combat_unit*, std::tuple<unit*, double, int>> registered_focus_fire;

a_unordered_map<unit*, int> dropship_spread;
a_unordered_map<unit*, double> focus_fire;
a_unordered_map<unit*, double> prepared_damage;
a_unordered_map<unit*, bool> is_being_healed;
a_vector<xy> spread_positions;

//a_vector<combat_unit*> spider_mine_layers;
a_vector<unit*> active_spider_mines;
a_unordered_set<unit*> active_spider_mine_targets;
a_unordered_set<unit*> my_spider_mines_in_range_of;

//a_unordered_map<unit*, double> nuke_damage;
a_unordered_set<unit*> lockdown_target_taken;
tsc::dynamic_bitset build_square_taken;
a_unordered_map<unit*, int> space_left;

void prepare_attack() {
	for (auto i = registered_focus_fire.begin(); i != registered_focus_fire.end();) {
		unit*target;
		double damage;
		int end_frame;
		std::tie(target, damage, end_frame) = i->second;
		if (current_frame >= end_frame) {
			if (std::abs(focus_fire[target] -= damage) < 1) {
				focus_fire.erase(target);
			}
			i = registered_focus_fire.erase(i);
		} else ++i;
	}
	//focus_fire.clear();
	prepared_damage.clear();
	dropship_spread.clear();
	is_being_healed.clear();
	spread_positions.clear();
	//spider_mine_layers.clear();

	active_spider_mines.clear();
	active_spider_mine_targets.clear();
	my_spider_mines_in_range_of.clear();
	for (unit*u : my_units_of_type[unit_types::spider_mine]) {
		if (u->order_target) {
			active_spider_mines.push_back(u);
			active_spider_mine_targets.insert(u->order_target);
		}
		for (unit*e : enemy_units) {
			if (!e->visible) continue;
			if (e->is_flying || e->type->is_hovering) continue;
			if (units_distance(u, e) <= 32 * 3) my_spider_mines_in_range_of.insert(e);
		}
	}
	for (unit*u : enemy_units) {
		if (!u->visible) continue;
		if (u->type != unit_types::spider_mine) continue;
		if (u->order_target) active_spider_mine_targets.insert(u->order_target);
	}

	//nuke_damage.clear();
	lockdown_target_taken.clear();
	build_square_taken.reset();
	space_left.clear();
	for (unit*u : my_units_of_type[unit_types::bunker]) {
		int space = u->type->space_provided;
		for (unit*lu : u->loaded_units) {
			space -= lu->type->space_required;
		}
		space_left[u] = space;
	}
}

a_vector<std::tuple<xy, double>> nuke_test;

auto lay_mine = [&](combat_unit*c) {
	int inrange = 0;
	for (unit*u : my_units_of_type[unit_types::spider_mine]) {
		if (diag_distance(u->pos - c->u->pos) <= 32 * 3) ++inrange;
	}
	if (inrange >= 3) return;
	int my_inrange = 0;
	for (unit*u : my_units) {
		if (u->type->is_building) continue;
		if (u->is_flying) continue;
		if (u->type->is_hovering) continue;
		if (u->type->is_non_usable) continue;
		if (units_distance(c->u, u) <= 32 * 3) ++my_inrange;
	}
	int op_inrange = 0;
	for (unit*u : enemy_units) {
		if (!u->visible) continue;
		if (u->type->is_building) continue;
		if (u->is_flying) continue;
		if (u->type->is_hovering) continue;
		if (u->type->is_non_usable) continue;
		if (units_distance(c->u, u) <= 32 * 3) ++op_inrange;
	}
	if (op_inrange + 2 <= my_inrange) return;
	if (current_frame - c->last_used_special >= 15) {
		c->u->game_unit->useTech(upgrade_types::spider_mines->game_tech_type, BWAPI::Position(c->u->pos.x, c->u->pos.y));
		c->last_used_special = current_frame;
		c->u->controller->noorder_until = current_frame + 15;
	}
};

void finish_attack() {
	a_vector<combat_unit*> spider_mine_layers;
	a_vector<combat_unit*> close_enough;

	for (combat_unit*c : live_combat_units) {
		if (players::my_player->has_upgrade(upgrade_types::spider_mines) && c->u->spider_mine_count) {
			unit*target = nullptr;
			unit*nearby_target = nullptr;
			for (unit*e : enemy_units) {
				if (!e->visible) continue;
				if (e->type->is_building) continue;
				if (e->is_flying) continue;
				if (e->type->is_hovering) continue;
				if (e->type->is_non_usable) continue;
				if (e->invincible) continue;
				double d = diag_distance(e->pos - c->u->pos);
				if (d <= 32 * 5) {
					nearby_target = e;
				}
				if (d <= 32 * 3) {
					target = e;
					break;
				}
			}
			if (nearby_target) spider_mine_layers.push_back(c);
			if (target) close_enough.push_back(c);
		}
	}

	bool lay = false;
	if (close_enough.size() >= 4 || close_enough.size() >= spider_mine_layers.size()) {
		for (auto*c : close_enough) {
			lay = true;
			break;
		}
	} else {
		for (auto*c : close_enough) {
			bool is_solo = true;
			for (auto*c2 : spider_mine_layers) {
				if (units_distance(c->u, c2->u) <= 32) {
					is_solo = false;
					break;
				}
			}
			if (is_solo || c->u->hp <= 60) {
				lay = true;
				break;
			}
		}
	}
	if (lay) {
		for (auto*c : close_enough) {
			lay_mine(c);
		}
	}

	a_vector<combat_unit*> ghosts;
	for (auto*c : live_combat_units) {
		if (c->u->type == unit_types::ghost) ghosts.push_back(c);
	}
	if (players::my_player->has_upgrade(upgrade_types::personal_cloaking)) {
		// todo: some better logic here. cloak if being attacked and there are no detectors in range
		//       uncloak when safe?
		for (auto*c : ghosts) {
			if (c->u->hp < 40) {
				if (!c->u->cloaked && c->u->energy>140) {
					c->subaction = combat_unit::subaction_use_ability;
					c->ability = upgrade_types::personal_cloaking;
				}
			}
		}
	}
	auto test_in_range = [&](combat_unit*c, unit*e, xy stand_pos, bool&in_attack_range, bool&is_revealed) {
		if (!in_attack_range) {
			weapon_stats*w = c->u->is_flying ? e->stats->air_weapon : e->stats->ground_weapon;
			if (w) {
				double d = diag_distance(e->pos - stand_pos);
				if (d <= w->max_range + 32) {
					in_attack_range = true;
				}
			}
		}
		if (!is_revealed) {
			if (e->type->is_detector) {
				double d = diag_distance(e->pos - stand_pos);
				if (d <= e->stats->sight_range + 32) is_revealed = true;
			}
		}
	};
	if (players::my_player->has_upgrade(upgrade_types::lockdown)) {
		for (auto*c : ghosts) {
			if (c->u->energy < 100) continue;
			if (current_frame - c->last_run <= 15) {
				unit*target = get_best_score(enemy_units, [&](unit*u) {
					if (u->gone) return std::numeric_limits<double>::infinity();
					if (u->visible && !u->detected) return std::numeric_limits<double>::infinity();
					if (!u->type->is_mechanical) return std::numeric_limits<double>::infinity();
					if (u->lockdown_timer) return std::numeric_limits<double>::infinity();
					if (lockdown_target_taken.count(u)) return std::numeric_limits<double>::infinity();
					double value = u->minerals_value + u->gas_value;
					if (value < 200) return std::numeric_limits<double>::infinity();
					double r = diag_distance(u->pos - c->u->pos) - 32 * 8;
					if (r < 0) r = 0;

					return r / value / ((u->shields + u->hp) / (u->stats->shields + u->stats->hp));
				}, std::numeric_limits<double>::infinity());
				if (target) {
					lockdown_target_taken.insert(target);
					double r = units_distance(c->u, target);
					if (r <= 32 * 8) {
						if (current_frame - c->last_used_special >= 8) {
							c->u->game_unit->useTech(upgrade_types::lockdown->game_tech_type, target->game_unit);
							c->last_used_special = current_frame;
							c->u->controller->noorder_until = current_frame + 8;
						}
					} else {
						xy stand_pos;
						xy relpos = c->u->pos - target->pos;
						double ang = atan2(relpos.y, relpos.x);
						stand_pos.x = target->pos.x + (int)(cos(ang) * 32 * 8);
						stand_pos.y = target->pos.y + (int)(sin(ang) * 32 * 8);
						bool can_cloak = players::my_player->has_upgrade(upgrade_types::personal_cloaking) && c->u->energy >= 100 + 25 + 20;
						bool in_attack_range = false;
						bool is_revealed = !can_cloak;
						for (unit*e : enemy_units) {
							if (e->gone) continue;
							test_in_range(c, e, stand_pos, in_attack_range, is_revealed);
							if (in_attack_range && is_revealed) break;
						}
						if (!in_attack_range || !is_revealed) {
							if (in_attack_range && !c->u->cloaked) {
								c->subaction = combat_unit::subaction_use_ability;
								c->ability = upgrade_types::personal_cloaking;
							} else {
								c->subaction = combat_unit::subaction_move;
								c->target_pos = stand_pos;
							}
						}
					}
				}
			}
		}
	}
	if (my_completed_units_of_type[unit_types::nuclear_missile].size() > 0) {
		double nuke_range = players::my_player->has_upgrade(upgrade_types::ocular_implants) ? 32 * 10 : 32 * 8;
		nuke_test.clear();

		a_vector<unit*> nearby_enemies;
		a_vector<unit*> nearby_my_buildings;
		tsc::dynamic_bitset stand_visited((grid::build_grid_width / 2)*(grid::build_grid_height / 2));
		tsc::dynamic_bitset target_visited(grid::build_grid_width*grid::build_grid_height);
		for (auto*c : ghosts) {
			xy best_stand_pos;
			xy best_target_pos;
			double best_score = 0;

			nearby_enemies.clear();
			nearby_my_buildings.clear();
			for (unit*u : enemy_units) {
				if (u->gone) continue;
				if (diag_distance(u->pos - c->u->pos) <= 32 * 25) nearby_enemies.push_back(u);
			}
			for (unit*u : my_buildings) {
				if (diag_distance(u->pos - c->u->pos) <= 32 * 25) nearby_my_buildings.push_back(u);
			}
			
			find_nearby_entirely_walkable_tiles(c->u->pos, [&](xy tile_pos) {
				size_t stand_index = tile_pos.x / 64 + tile_pos.y / 64 * (grid::build_grid_width / 2);
				if (stand_visited.test(stand_index)) return true;
				stand_visited.set(stand_index);
				double d = diag_distance(tile_pos - c->u->pos);
				if (d >= 32 * 8) return false;
				xy stand_pos = tile_pos + xy(16, 16);
				bool in_attack_range = false;
				bool is_revealed = !c->u->cloaked;
				for (unit*e : nearby_enemies) {
					test_in_range(c, e, stand_pos, in_attack_range, is_revealed);
					if (in_attack_range && is_revealed) return false;
				}
				for (double ang = 0.0; ang < PI * 2; ang += PI / 4) {
					xy pos = stand_pos;
					pos.x += (int)(cos(ang)*nuke_range);
					pos.y += (int)(sin(ang)*nuke_range);
					if ((size_t)pos.x >= (size_t)grid::map_width || (size_t)pos.y >= (size_t)grid::map_height) continue;
					size_t target_index = grid::build_square_index(pos);
					if (target_visited.test(target_index)) continue;
					target_visited.set(target_index);
					double score = 0.0;
					for (unit*e : nearby_enemies) {
						// todo: use the actual blast radius of a nuke
						if (diag_distance(e->pos - pos) <= 32 * 7) {
							double damage = std::max(500.0, (e->shields + e->hp) * 2 / 3);
							double mult = 1.0;
							if (!e->type->is_building && e->stats->max_speed>1) mult = 1.0 / e->stats->max_speed;
							if (e->lockdown_timer) mult = 1.0;
							if (e->type->is_worker) mult *= 2;
							if (e->shields + e->hp - damage > 0) mult = damage / (e->stats->shields + e->stats->hp);
							score += (e->minerals_value + e->gas_value)*mult;
						}
					}
					for (unit*e : nearby_my_buildings) {
						// todo: use the actual blast radius of a nuke
						if (diag_distance(e->pos - pos) <= 32 * 7) {
							double damage = std::max(500.0, (e->shields + e->hp) * 2 / 3);
							double mult = 1.0;
							if (e->shields + e->hp - damage > 0) mult = damage / (e->stats->shields + e->stats->hp);
							score -= (e->minerals_value + e->gas_value)*mult;
						}
					}
					nuke_test.emplace_back(pos, score);
					if (score > best_score) {
						best_score = score;
						best_stand_pos = stand_pos;
						best_target_pos = pos;
					}
				}
				return true;
			});
			//log("nuke: best pos is %d %d -> %d %d with score %g\n", best_stand_pos.x, best_stand_pos.y, best_target_pos.x, best_target_pos.y, best_score);
			if (best_score >= 400.0) {
				log("nuking %d %d -> %d %d with score %g\n", best_stand_pos.x, best_stand_pos.y, best_target_pos.x, best_target_pos.y, best_score);
				bool okay = true;
				if (players::my_player->has_upgrade(upgrade_types::personal_cloaking)) {
					if (!c->u->cloaked) {
						c->subaction = combat_unit::subaction_use_ability;
						c->ability = upgrade_types::personal_cloaking;
						okay = false;
					}
				}
				if (okay) {
					if (diag_distance(c->u->pos - best_stand_pos) > 32) {
						c->subaction = combat_unit::subaction_move;
						c->target_pos = best_stand_pos;
					} else {
						c->subaction = combat_unit::subaction_use_ability;
						c->ability = upgrade_types::nuclear_strike;
						c->target_pos = best_target_pos;
						c->last_nuke = current_frame;
					}
				}
			}
		}
	}

	a_vector<combat_unit*> science_vessels;
	for (auto*c : live_combat_units) {
		if (c->u->type == unit_types::science_vessel) science_vessels.push_back(c);
	}
	if (!science_vessels.empty()) {
		a_vector<combat_unit*> wants_defensive_matrix;
		for (auto*c : live_combat_units) {
			if (current_frame - c->last_nuke <= 15 * 14) {
				if (c->u->cloaked) {
					bool in_attack_range = false;
					bool is_revealed = false;
					for (unit*e : enemy_units) {
						if (e->gone) continue;
						test_in_range(c, e, c->u->pos, in_attack_range, is_revealed);
						if (in_attack_range && is_revealed) break;
					}
					if (!is_revealed) continue;
				}
				wants_defensive_matrix.push_back(c);
			}
		}
		a_unordered_set<combat_unit*> target_taken;
		for (auto*c : science_vessels) {
			if (c->u->energy < 100) continue;
			combat_unit*target = get_best_score(wants_defensive_matrix, [&](combat_unit*target) {
				if (target_taken.count(target)) return std::numeric_limits<double>::infinity();
				return diag_distance(c->u->pos - target->u->pos);
			}, std::numeric_limits<double>::infinity());
			if (target) {
				target_taken.insert(target);
				if (current_frame - c->last_used_special >= 8) {
					c->u->game_unit->useTech(upgrade_types::defensive_matrix->game_tech_type, target->u->game_unit);
					c->last_used_special = current_frame;
					c->u->controller->noorder_until = current_frame + 8;
				}
			}
		}
	}

	if (!my_completed_units_of_type[unit_types::scv].empty()) {
		a_unordered_map<unit*, group_t*> unit_group;
		a_vector<combat_unit*> scvs;
		int non_scv_count = 0;
		for (auto&g : groups) {
			if (g.allies.size() == 1) continue;
			for (auto*a : g.allies) {
				unit_group[a->u] = &g;
				if (a->u->type == unit_types::scv) scvs.push_back(a);
				else ++non_scv_count;
			}
		}
		if ((int)scvs.size() > non_scv_count + 1) scvs.resize(non_scv_count + 1);
		a_vector<unit*> wants_repair;
		for (unit*u : my_units) {
			if (!u->is_completed) continue;
			if (!u->type->is_building && !u->type->is_mechanical) continue;
			if (u->controller->action == unit_controller::action_scout) continue;
			if (u->type->minerals_cost && current_minerals < 20) continue;
			if (u->type->gas_cost && current_minerals < 10) continue;
			if (u->hp < u->stats->hp) wants_repair.push_back(u);
		}
		std::sort(wants_repair.begin(), wants_repair.end(), [&](unit*a, unit*b) {
			return a->minerals_value + a->gas_value > b->minerals_value + b->gas_value;
		});
		for (auto*u : wants_repair) {
			if (scvs.empty()) break;
			int max_n = 2;
			if (u->type == unit_types::siege_tank_tank_mode || u->type == unit_types::siege_tank_siege_mode) max_n = 5;
			if (u->building) max_n = 4;
			auto*ug = u->building ? nullptr : unit_group[u];
			for (int i = 0; i < max_n; ++i) {
				combat_unit*c = get_best_score(scvs, [&](combat_unit*c) {
					if (c->u == u) return std::numeric_limits<double>::infinity();
					if (ug && unit_group[c->u] != ug) return std::numeric_limits<double>::infinity();
					if (!square_pathing::unit_can_reach(c->u, c->u->pos, u->pos)) return std::numeric_limits<double>::infinity();
					return diag_distance(u->pos - c->u->pos);
				}, std::numeric_limits<double>::infinity());
				if (!c) break;
				c->subaction = combat_unit::subaction_repair;
				c->target = u;
				find_and_erase(scvs, c);
			}
		}
	}

	auto scout_around = [&](combat_unit*a) {
		bool find_new_path = false;
		if (current_frame - a->last_find_path >= 15 * 60) find_new_path = true;
		if (a->path.empty()) find_new_path = true;
		for (xy p : a->path) {
			if (entire_threat_area.test(grid::build_square_index(p))) {
				find_new_path = true;
				break;
			}
		}
		if (find_new_path) {
			a->last_find_path = current_frame;
			struct node_data_t {
				double len = 0;
			};
			int iterations = 0;
			a_deque<xy> path = find_path<node_data_t>(a->u->type, a->u->pos, [&](xy pos, xy npos, node_data_t&n) {
				if (++iterations % 1024 == 0) multitasking::yield_point();
				if (entire_threat_area.test(grid::build_square_index(npos))) return false;
				n.len += diag_distance(npos - pos);
				return true;
			}, [&](xy pos, xy npos, const node_data_t&n) {
				return 0.0;
			}, [&](xy pos, const node_data_t&n) {
				if (n.len >= 32 * 60) return true;
				return current_frame - grid::get_build_square(pos).last_seen >= 15 * 60;
			});
			log("found new path - %d nodes\n", path.size());
			a->path = std::move(path);
		}
		while (!a->path.empty() && diag_distance(a->u->pos - a->path.front()) <= 32 * 3) a->path.pop_front();
		if (!a->path.empty()) {
			xy move_to = square_pathing::get_go_to_along_path(a->u, a->path);
			a->subaction = combat_unit::subaction_move;
			a->target_pos = move_to;
		}
	};

	if (no_aggressive_groups) {
		for (auto&g : groups) {
			if (!g.is_aggressive_group) continue;
			int buildings = 0;
			for (unit*e : g.enemies) {
				if (e->type->is_building) ++buildings;
			}
			if (!buildings) continue;
			for (auto*a : g.allies) {
				if (current_frame - a->last_fight < 15 * 3 && current_frame - a->last_run >= 15 * 3) continue;
				if (current_frame - a->last_run >= 15 * 20) continue;
				if (a->subaction != combat_unit::subaction_move && a->subaction != combat_unit::subaction_idle) continue;
				scout_around(a);
			}
		}
	}

	if (search_and_destroy) {
		for (auto*a : live_combat_units) {
			scout_around(a);
		}
	}

}

void do_run(combat_unit*a, const a_vector<unit*>&enemies);

auto move_close_if_unreachable = [&](combat_unit*a,unit*target) {
	if (square_pathing::unit_can_reach(a->u, a->u->pos, target->pos, square_pathing::pathing_map_index::include_liftable_wall)) return false;
	weapon_stats*my_weapon = target->is_flying ? a->u->stats->air_weapon : a->u->stats->ground_weapon;
	double wr = my_weapon ? my_weapon->max_range : 64;
	xy closest;
	double closest_dist;
	a_deque<xy> path = find_path(a->u->type, a->u->pos, [&](xy pos, xy npos) {
		return diag_distance(npos - a->u->pos) <= 32 * 5;
	}, [&](xy pos, xy npos) {
		double d = diag_distance(npos - target->pos);
		if (closest == xy() || d < closest_dist) {
			closest = npos;
			closest_dist = d;
		}
		return d;
	}, [&](xy pos) {
		return units_distance(pos, a->u->type, target->pos, target->type) <= wr;
	});
	if (!path.empty()) closest = path.back();
	log("move_close_if_unreachable: closest is %d %d\n", closest.x, closest.y);
	if (closest != xy()) {
		a->subaction = combat_unit::subaction_move;
		a->target_pos = closest;
	}
	return true;
};

void do_attack(combat_unit*a, const a_vector<unit*>&allies, const a_vector<unit*>&enemies) {
	a->defend_spot = xy();
	a->subaction = combat_unit::subaction_fight;
	if (active_spider_mine_targets.count(a->u)) {
		unit*ne = get_best_score(enemies, [&](unit*e) {
			if (e->is_flying) return std::numeric_limits<double>::infinity();
			return diag_distance(e->pos - a->u->pos);
		});
		a->subaction = combat_unit::subaction_move_directly;
		a->target_pos = ne->pos;
		return;
	}
	for (unit*t : active_spider_mine_targets) {
		if (units_distance(t, a->u) <= 32 * 3) {
			do_run(a, enemies);
			return;
		}
	}
	for (unit*t : active_spider_mines) {
		if (units_distance(t, a->u) <= 32 * 3) {
			if (t->order_target) {
				int my_inrange = 0;
				for (unit*u : allies) {
					if (u->type->is_building) continue;
					if (u->is_flying) continue;
					if (u->type->is_hovering) continue;
					if (u->type->is_non_usable) continue;
					if (units_distance(t, u) <= 32 * 3) ++my_inrange;
				}
				int op_inrange = 0;
				for (unit*u : enemies) {
					if (u->type->is_building) continue;
					if (u->is_flying) continue;
					if (u->type->is_hovering) continue;
					if (u->type->is_non_usable) continue;
					if (units_distance(t, u) <= 32 * 3) ++op_inrange;
				}
				if (my_inrange > op_inrange + 2 && my_inrange >= 3) {
					if (a->u->weapon_cooldown <= latency_frames) {
						a->target = t;
						log("attack spider mine waa (%d vs %d)!\n", my_inrange, op_inrange);
						return;
					}
				}
			}
			do_run(a, enemies);
			return;
		}
	}

	bool wants_to_lay_spider_mines = a->u->spider_mine_count && players::my_player->has_upgrade(upgrade_types::spider_mines);

	unit*u = a->u;
	std::function<std::tuple<double,double,double>(unit*)> score = [&](unit*e) {
		//double ehp = e->shields + e->hp - focus_fire[e];
		//if (ehp <= 0) return std::make_tuple(std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity());
		if (!e->visible || !e->detected) return std::make_tuple(std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity());
		if (e->invincible) return std::make_tuple(std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity());
		weapon_stats*w = e->is_flying ? u->stats->air_weapon : u->stats->ground_weapon;
		if (!w) return std::make_tuple(std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity());
		double d = units_distance(u, e);
		bool is_weaponless_target = false;
		is_weaponless_target |= e->type == unit_types::bunker;
		is_weaponless_target |= e->type == unit_types::carrier;
		is_weaponless_target |= e->type == unit_types::overlord;
		is_weaponless_target |= e->type == unit_types::pylon;
		if (!e->stats->ground_weapon && !e->stats->air_weapon && !is_weaponless_target) return std::make_tuple(std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity(), d);
		//if (!e->stats->ground_weapon && !e->stats->air_weapon && e->type != unit_types::bunker) return std::make_tuple(std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity(), d);
		//if (!e->stats->ground_weapon && !e->stats->air_weapon) return std::make_tuple(std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity(), d);
		weapon_stats*ew = a->u->is_flying ? e->stats->air_weapon : e->stats->ground_weapon;
		if (d < w->min_range) return std::make_tuple(std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity());
		double ehp = e->shields + e->hp;
		ehp -= focus_fire[e];
		if (ehp <= 0) ehp = e->stats->shields + e->stats->hp - ehp;
		if (wants_to_lay_spider_mines && !e->type->is_flyer && !e->type->is_hovering && !e->type->is_building) {
			//if (e->type->is_hovering) return std::make_tuple(1000 + d / a->u->stats->max_speed, 0.0, 0.0);
			//return std::make_tuple(prepared_damage[e] + d / a->u->stats->max_speed, 0.0, 0.0);
			ehp += prepared_damage[e] * 4;
		}
		//if (d > w->max_range) return std::make_tuple(std::numeric_limits<double>::infinity(), d, 0.0);
		double hits = ehp / (w->damage*combat_eval::get_damage_type_modifier(w->damage_type, e->type->size));
		//if (d > w->max_range) return std::make_tuple(std::numeric_limits<double>::infinity(), std::ceil(hits), d);
		if (!ew) hits += 4;
		if (e->lockdown_timer) hits += 10;
		if (e->type->requires_pylon && !e->is_powered) hits += 10;
		//if (d > w->max_range) return std::make_tuple(std::numeric_limits<double>::infinity(), hits + (d - w->max_range) / a->u->stats->max_speed / 90, 0.0);
		if (d > w->max_range) hits += (d - w->max_range) / a->u->stats->max_speed / 4;
		if (e->is_flying) hits /= 10;
		if (current_frame - e->last_seen >= 15 * 30) hits += 20;
		return std::make_tuple(hits, 0.0, 0.0);
	};

	unit*target = nullptr;

	if (a->u->type == unit_types::siege_tank_tank_mode || a->u->type == unit_types::siege_tank_siege_mode) {
		if (players::my_player->has_upgrade(upgrade_types::siege_mode)) {
			target = get_best_score(enemies, score);
			if (target && current_frame - target->last_seen < 15 * 8 && !target->is_flying) {
				double d = units_distance(a->u, target);
				int siege_tank_count = 0;
				int sieged_tank_count = 0;
				for (unit*u : allies) {
					if (u->type == unit_types::siege_tank_tank_mode || u->type == unit_types::siege_tank_siege_mode) ++siege_tank_count;
					if (u->type == unit_types::siege_tank_siege_mode) ++sieged_tank_count;
				}
				if (siege_tank_count >= 4) a->siege_up_close = false;
				if (a->u->type == unit_types::siege_tank_tank_mode) {
					double add = target->stats->max_speed * 15;
					if (target->burrowed || target->type->is_non_usable) add = 0;
					if (current_frame - a->u->last_attacked <= 15 * 4) a->siege_up_close = false;
					if (a->siege_up_close) add = 0;
					if (d <= 32 * 12 + add && target->visible) {
						if (siege_tank_count < (int)enemies.size() || (target && target->building)) {
							if (a->u->game_unit->siege()) {
								a->u->controller->noorder_until = current_frame + 30;
								a->u->controller->last_siege = current_frame;
								a->siege_up_close = false;
							}
						}
					}
				} else {
					double add = target->stats->max_speed * 15 * 6;
					if (target->burrowed || target->type->is_non_usable) add = 0;
					if (a->siege_up_close && sieged_tank_count >= siege_tank_count / 2) add = 0;
					//if (d > 32 * 12 + add && (!target->visible || current_frame - target->last_shown >= current_frame * 2)) {
					if (d > 32 * 12 + add || (!target->visible && current_frame - a->u->controller->last_siege >= 15 * 8)) {
						if (current_frame - a->u->last_attacked >= 15 * 4) {
							if (!target->visible) a->siege_up_close = true;
							if (a->u->game_unit->unsiege()) {
								a->u->controller->noorder_until = current_frame + 30;
							}
						}
					} else {
						if (current_frame - a->u->controller->last_siege >= 15 * 20) {
							a->u->controller->last_siege = current_frame;
						}
						if (current_frame - a->u->controller->last_siege >= 15 * 10 && current_frame - a->u->last_attacked >= 15 * 4) {
							a->siege_up_close = true;
						}
					}
				}
			} else {
				if (a->u->type == unit_types::siege_tank_siege_mode) {
					if (current_frame - a->u->last_attacked >= 15 * 4) {
						if (a->u->game_unit->unsiege()) {
							a->u->controller->noorder_until = current_frame + 30;
						}
					}
				}
			}
		}
	}

	if (a->u->type == unit_types::dropship) {
		score = [&](unit*e) {
			if (e->is_flying) return std::make_tuple(std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity());
			double b = e->type == unit_types::siege_tank_siege_mode ? 0.0 : 1.0;
			double d = units_distance(u, e);
			if (dropship_spread[e] == 1) return std::make_tuple(std::numeric_limits<double>::infinity(), b, d);
			return std::make_tuple(0.0, b, d);
		};
	}
	bool targets_allies = false;
	if (a->u->type == unit_types::medic) {
		targets_allies = true;
		score = [&](unit*a) {
			if (!a->type->is_biological) return std::make_tuple(std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity());
			if (is_being_healed[a]) return std::make_tuple(std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity());
			double d = units_pathing_distance(u, a);
			return std::make_tuple(a->hp, d, 0.0);
		};
	}

	auto&targets = targets_allies ? allies : enemies;
	if (!target) target = get_best_score(targets, score);
	//log("u->target is %p, u->order_target is %p\n", u->target, u->order_target);
	if (u->order_target && a->u->type != unit_types::dropship) {
		double new_s = std::get<0>(score(target));
		double old_s = std::get<0>(score(u->order_target));
		//if (old_s < new_s * 2) target = u->order_target;
		//log("old_s %g, new_s %g\n", old_s, new_s);
	}
// 	if (u->target) {
// 		double new_s = std::get<0>(score(target));
// 		double old_s = std::get<0>(score(u->target));
// 		if (old_s < new_s * 2) target = u->target;
// 	}
	//if (target != a->target) log("%s: change target to %p\n", a->u->type->name, target);
	a->target = target;

	if (a->u->is_loaded && a->u->loaded_into->type == unit_types::bunker) {
		if (units_distance(a->u->loaded_into, target) >= 32 * 6) {
			if (a->u->loaded_into->game_unit->unload(a->u->game_unit)) {
				a->u->controller->noorder_until = current_frame + 4;
			}
		}
	}

	//if (target && a->u->type != unit_types::siege_tank_tank_mode && a->u->type != unit_types::siege_tank_siege_mode && a->u->type != unit_types::goliath && !a->u->is_flying) {
	if (target && a->u->type != unit_types::siege_tank_tank_mode && a->u->type != unit_types::siege_tank_siege_mode && !target->is_flying && !a->u->is_flying) {
		if (a->u->stats->ground_weapon && (target->stats->ground_weapon || target->type == unit_types::bunker) && !target->is_flying && target->visible && !(target->type->requires_pylon && !target->is_powered)) {
			bool okay = true;
			//if (target->type->race == race_terran) okay = false;
			int enemy_sieged_tanks = 0;
			for (unit*e : enemies) {
				if (e->type == unit_types::siege_tank_siege_mode) ++enemy_sieged_tanks;
			}
			if (enemy_sieged_tanks >= 3) okay = false;
			if (okay) {
				unit*nearest_siege_tank = get_best_score(allies, [&](unit*u) {
					if (u->type != unit_types::siege_tank_tank_mode && u->type != unit_types::siege_tank_siege_mode) return std::numeric_limits<double>::infinity();
					return diag_distance(u->pos - a->u->pos);
				}, std::numeric_limits<double>::infinity());
				double nearest_siege_tank_distance = nearest_siege_tank ? units_distance(a->u, nearest_siege_tank) : 0;
				if (nearest_siege_tank && nearest_siege_tank_distance <= 32 * 20 && nearest_siege_tank_distance >= 32 * 2) {
					double wr = target->type == unit_types::bunker ? 32 * 6 : target->stats->ground_weapon->max_range;
					double r = units_distance(target, nearest_siege_tank);
					if (r > wr + 32 * 2) {
						if (r > 32 * 12 - wr) {
							double max_d = diag_distance(a->u->pos - nearest_siege_tank->pos) + 32 * 2;
							a_deque<xy> path = find_path(a->u->type, a->u->pos, [&](xy pos, xy npos) {
								double d = diag_distance(npos - nearest_siege_tank->pos);
								if (d >= max_d) return false;
								if (build_square_taken.test(grid::build_square_index(npos))) return false;
								return true;
							}, [&](xy pos, xy npos) {
								double d = diag_distance(npos - nearest_siege_tank->pos);
								return d;
							}, [&](xy pos) {
								double d = diag_distance(pos - nearest_siege_tank->pos);
								return d <= 32 * 12 - wr - 32 * 2;
							});
							if (!path.empty()) {
								a->subaction = combat_unit::subaction_move;
								build_square_taken.set(grid::build_square_index(path.back()));
								a->target_pos = path.back();
							}
						}
					}
				}
			}
		}
	}

	if (a->u->type == unit_types::marine && target) {
		unit*bunker = get_best_score(my_completed_units_of_type[unit_types::bunker], [&](unit*u) {
			if (space_left[u] < a->u->type->space_required) return std::numeric_limits<double>::infinity();
			double d = diag_distance(u->pos - a->u->pos);
			if (d >= 32 * 10) return std::numeric_limits<double>::infinity();
			return d;
		}, std::numeric_limits<double>::infinity());
		if (bunker) {
			double d = units_distance(a->u, bunker);
			if (units_distance(bunker, target) < 32 * 6 && d < units_distance(target, bunker)) {
				space_left[bunker] -= a->u->type->space_required;
				a->subaction = combat_unit::subaction_idle;
				a->u->controller->action = unit_controller::action_idle;
				if (current_frame >= a->u->controller->noorder_until) {
					if (a->u->game_unit->rightClick(bunker->game_unit)) {
						a->u->controller->noorder_until = current_frame + 30;
					}
				}
			}
		}
	}

	if (target) {
		double r = units_distance(a->u, target);
		if (a->u->type == unit_types::dropship) {
			dropship_spread[target] = 1;
			a->target = nullptr;
			a->subaction = combat_unit::subaction_move;
			a->target_pos = target->pos;
			a->target_pos.x += (int)(cos(current_frame / 23.8) * 48);
			a->target_pos.y += (int)(sin(current_frame / 23.8) * 48);
		}

		if (a->u->type == unit_types::medic && r < 64) {
			is_being_healed[target] = true;
		}
	}
	//if (target && (a->u->type == unit_types::marine || a->u->type == unit_types::vulture)) {
	if (target && a->u->type == unit_types::marine) {
		weapon_stats*splash_weapon = nullptr;
		for (unit*e : enemies) {
			weapon_stats*e_weapon = a->u->is_flying ? target->stats->air_weapon : target->stats->ground_weapon;
			if (!e_weapon) continue;
			if (e_weapon->explosion_type != weapon_stats::explosion_type_radial_splash && e_weapon->explosion_type != weapon_stats::explosion_type_enemy_splash) continue;
			if (units_distance(a->u, e) > e_weapon->max_range) continue;
			splash_weapon = e_weapon;
			break;
		}
		if (splash_weapon) {
			a_vector<unit*> too_close;
			double radius = splash_weapon->outer_splash_radius;
			for (unit*u : allies) {
				double d = (u->pos - a->u->pos).length();
				if (d <= radius) too_close.push_back(u);
			}
			if (too_close.size() > 1) { 
				unit*nearest = get_best_score(too_close, [&](unit*u) {
					return diag_distance(target->pos - u->pos);
				});
				if (nearest != a->u) {
					double avg_dang = 0.0;
					size_t avg_count = 0;
					xy relpos = target->pos - a->u->pos;
					double ang = atan2(relpos.y, relpos.x);
					for (unit*u : allies) {
						double d = (u->pos - a->u->pos).length();
						if (d <= radius) too_close.push_back(u);
						if (d <= 64) {
							xy relpos = target->pos - u->pos;
							double dang = ang - atan2(relpos.y, relpos.x);
							if (dang < -PI) dang += PI * 2;
							if (dang > PI) dang -= PI * 2;
							avg_dang += dang;
							++avg_count;
						}
					}
					avg_dang /= avg_count;
					if (avg_dang < 0) ang += PI / 2;
					else ang -= PI / 2;
					xy npos;
					for (double add = 0.0; add <= PI / 2; add += PI / 8) {
						npos = a->u->pos;
						npos.x += (int)(cos(ang + (avg_dang < 0 ? -add : add)) * 128);
						npos.y += (int)(sin(ang + (avg_dang < 0 ? -add : add)) * 128);
						bool okay = !test_pred(spread_positions, [&](xy p) {
							return (p - npos).length() <= splash_weapon->outer_splash_radius;
						});
						if (okay) break;
						break;
					}
					spread_positions.push_back(npos);
					a->subaction = combat_unit::subaction_move_directly;
					a->target_pos = npos;
				} else {
					double d = units_distance(a->u, target);
					if (splash_weapon->min_range && d>splash_weapon->min_range) {
						a->subaction = combat_unit::subaction_move_directly;
						a->target_pos = target->pos;
					}
				}
			}
		}
	}

	if (target && a->subaction == combat_unit::subaction_fight) {
		weapon_stats*my_weapon = target->is_flying ? a->u->stats->air_weapon : a->u->stats->ground_weapon;
		weapon_stats*e_weapon = a->u->is_flying ? target->stats->air_weapon : target->stats->ground_weapon;
		double max_range = 1000.0;
		double min_range = 0.0;
		if (e_weapon && my_weapon) {
			//if (my_weapon->max_range < e_weapon->max_range) max_range = my_weapon->max_range / 2;
			if (e_weapon->min_range) max_range = e_weapon->min_range;
			//if (my_weapon->max_range >= e_weapon->max_range + 32) min_range = e_weapon->max_range + 32 * 2;
		}
		if (a->u->spider_mine_count && !target->is_flying && !target->type->is_hovering && !target->type->is_building && players::my_player->has_upgrade(upgrade_types::spider_mines)) {
			if (target->hp >= 40) {
				//spider_mine_layers.push_back(a);
				max_range = 0;
			}
		}
		double damage = 0.0;
		if (my_weapon) {
			damage = my_weapon->damage;
			if (target->shields <= 0) damage *= combat_eval::get_damage_type_modifier(my_weapon->damage_type, target->stats->type->size);
			damage -= target->stats->armor;
			if (damage <= 0) damage = 1.0;
			damage *= my_weapon == a->u->stats->ground_weapon ? a->u->stats->ground_weapon_hits : a->u->stats->air_weapon_hits;
			prepared_damage[target] += damage;
		}
		double d = units_distance(target, a->u);
		if (d > max_range) {
			if (!move_close_if_unreachable(a, target) && a->u->weapon_cooldown > latency_frames) {
				a->subaction = combat_unit::subaction_move_directly;
				a->target_pos = target->pos;
			}
		} else {
			if (d < min_range) {
				double wr = my_weapon ? my_weapon->max_range : 0.0;
				if (a->u->weapon_cooldown <= frames_to_reach(a->u, d - wr) + latency_frames) {
					do_run(a, enemies);
				}
			} else {
				if (my_weapon && d <= my_weapon->max_range && a->u->weapon_cooldown <= latency_frames) {
					focus_fire[target] += damage;
					auto i = registered_focus_fire.find(a);
					if (i != registered_focus_fire.end()) {
						if (std::abs(focus_fire[std::get<0>(i->second)] -= std::get<1>(i->second)) < 1) {
							focus_fire.erase(std::get<0>(i->second));
						}
					}
					registered_focus_fire[a] = std::make_tuple(target, damage, current_frame + 30);
				}
			}
		}
	}

}

bool worker_is_safe(combat_unit*a, const a_vector<unit*>&enemies) {
	unit*res = nullptr;
	if (a->u->controller->action == unit_controller::action_gather) res = a->u->controller->target;
	else {
		res = get_best_score(resource_units, [&](unit*u) {
			return units_pathing_distance(a->u, u);
		});
	}
	if (res) {
		double gd = units_pathing_distance(a->u, res);
		unit*ne = get_best_score(enemies, [&](unit*e) {
			weapon_stats*w = a->u->is_flying ? e->stats->air_weapon : e->stats->ground_weapon;
			if (!w) return std::numeric_limits<double>::infinity();
			return diag_distance(e->pos - a->u->pos);
		}, std::numeric_limits<double>::infinity());
		if (ne) {
			weapon_stats*w = a->u->is_flying ? ne->stats->air_weapon : ne->stats->ground_weapon;
			double ned = units_pathing_distance(a->u, ne);
			if (ned <= 32 * 15 && ned < gd) return false;
			if (ned <= w->max_range + 64) return false;
		}
	}
	return true;
}

tsc::dynamic_bitset run_spot_taken;
void prepare_run() {
	run_spot_taken.resize(grid::build_grid_width*grid::build_grid_height);
	run_spot_taken.reset();
}

void do_run(combat_unit*a, const a_vector<unit*>&enemies) {
	a->defend_spot = xy();
	a->subaction = combat_unit::subaction_move;
	a->target_pos = xy(grid::map_width / 2, grid::map_height / 2);
	
	if (true) {
		unit*ne = get_best_score(enemies, [&](unit*e) {
			weapon_stats*w = e->is_flying ? a->u->stats->air_weapon : a->u->stats->ground_weapon;
			if (!w) return std::numeric_limits<double>::infinity();
			return units_distance(e, a->u);
		}, std::numeric_limits<double>::infinity());
		unit*net = get_best_score(enemies, [&](unit*e) {
			weapon_stats*ew = a->u->is_flying ? e->stats->air_weapon : e->stats->ground_weapon;
			if (!ew) return std::numeric_limits<double>::infinity();
			return units_distance(e, a->u) - ew->max_range;
		}, std::numeric_limits<double>::infinity());
		if (ne && ne->type != unit_types::bunker) {
			weapon_stats*w = ne->is_flying ? a->u->stats->air_weapon : a->u->stats->ground_weapon;
			weapon_stats*ew = net ? a->u->is_flying ? net->stats->air_weapon : net->stats->ground_weapon : nullptr;
			double d = units_distance(ne, a->u);
			double wr = w->max_range;
			double net_d = net ? units_distance(net, a->u) : 1000.0;
			double net_wr = net ? ew->max_range : 0.0;
			double margin = 64;
			if (net && !square_pathing::unit_can_reach(a->u, a->u->pos, net->pos, square_pathing::pathing_map_index::include_liftable_wall)) margin = 0.0;
			bool too_close = net_d <= margin;
			if (d - wr < net_d - net_wr - margin && !too_close) {
				if (move_close_if_unreachable(a, ne)) return;
				if (a->u->weapon_cooldown <= frames_to_reach(a->u, d - wr) + latency_frames) {
					a->subaction = combat_unit::subaction_fight;
					a->target = ne;
				}
			}
		}
	}
	if (a->u->type == unit_types::marine && a->subaction == combat_unit::subaction_move) {
		unit*bunker = get_best_score(my_completed_units_of_type[unit_types::bunker], [&](unit*u) {
			if (space_left[u] < a->u->type->space_required) return std::numeric_limits<double>::infinity();
			double d = diag_distance(u->pos - a->u->pos);
			if (d >= 32 * 10) return std::numeric_limits<double>::infinity();
			return d;
		}, std::numeric_limits<double>::infinity());
		if (bunker) {
			space_left[bunker] -= a->u->type->space_required;
			if (current_frame >= a->u->controller->noorder_until) {
				if (a->u->game_unit->rightClick(bunker->game_unit)) {
					a->u->controller->noorder_until = current_frame + 30;
				}
			}
		}
	}

	if (a->u->type->is_worker) {
		if (worker_is_safe(a, enemies)) {
			a->subaction = combat_unit::subaction_idle;
			if (a->u->controller->action != unit_controller::action_gather && a->u->controller->action != unit_controller::action_build) {
				a->u->controller->action = unit_controller::action_idle;
			}
			return;
		}
	}

	unit*u = a->u;
	
	a_deque<xy> path = find_path(u->type, u->pos, [&](xy pos, xy npos) {
		return true;
	}, [&](xy pos, xy npos) {
		double cost = 0.0;
		for (unit*e : enemies) {
			weapon_stats*w = a->u->is_flying ? e->stats->air_weapon : e->stats->ground_weapon;
			if (e->type == unit_types::bunker) w = units::get_unit_stats(unit_types::marine, e->owner)->ground_weapon;
			if (!w) continue;
			double d = diag_distance(pos - e->pos);
			if (d <= w->max_range*1.5) {
				cost += w->max_range*1.5 - d;
			}
		}
		return cost + diag_distance(a->goal_pos - a->u->pos);
	}, [&](xy pos) {
		//if ((pos - a->u->pos).length() < 128) return false;
		size_t index = grid::build_square_index(pos);
		return entire_threat_area_edge.test(index) && !run_spot_taken.test(index);
	});

	a->target_pos = path.empty() ? a->goal_pos : path.back();
	a->target_pos.x = a->target_pos.x&-32 + 16;
	a->target_pos.y = a->target_pos.y&-32 + 16;

	log("%s: running to spot %g away (%d %d)\n", a->u->type->name, (a->target_pos - a->u->pos).length(), a->target_pos.x, a->target_pos.y);

	int keep_mines = 2;
	if (a->u->hp <= 53) keep_mines = 0;
	if (a->u->spider_mine_count > keep_mines && players::my_player->has_upgrade(upgrade_types::spider_mines) && (a->target_pos - a->u->pos).length() < 32) {
		lay_mine(a);
	}

	run_spot_taken.set(grid::build_square_index(a->target_pos));

	if (a->target && a->target->visible) {
		unit*target = a->target;
		if (a->u->spider_mine_count && !target->is_flying && !target->type->is_hovering && !target->type->is_building && players::my_player->has_upgrade(upgrade_types::spider_mines)) {
			if (target->hp >= 40) {
				//spider_mine_layers.push_back(a);
			}
		}
	}

}


choke_t find_choke_from_to(unit_type*path_ut, xy from, xy to) {
	while (square_pathing::get_pathing_map(path_ut).path_nodes.empty()) multitasking::sleep(1);
	size_t iterations = 0;
	tsc::high_resolution_timer ht;
	a_deque<xy> path = square_pathing::find_square_path(square_pathing::get_pathing_map(path_ut), from, [&](xy pos, xy npos) {
		if (++iterations % 1024 == 0) multitasking::yield_point();
		return true;
	}, [&](xy pos, xy npos) {
		return 0.0;
	}, [&](xy pos) {
		return manhattan_distance(to - pos) <= 32;
	});
	log("choke path size is %d\n", path.size());
	const size_t n_space = 32;
	double len = 0.0;
	double max_len = 32 * 25;
	if (buildpred::get_my_current_state().bases.size() > 1) max_len = 32 * 10;
	for (size_t i = 1; i < path.size(); ++i) {
		len += diag_distance(path[i] - path[i - 1]);
		if (len >= max_len) {
			path.resize(std::min(i + n_space * 2, path.size()));
			break;
		}
	}

	double best_score = std::numeric_limits<double>::infinity();
	choke_t best;
	for (size_t i = 0; i + n_space * 2 < path.size(); ++i) {
		choke_t ch;
		ch.center = path[i + n_space];

		ch.inside = get_best_score(make_iterators_range(path.begin() + i, path.begin() + i + n_space), [&](xy pos) {
			return -diag_distance(pos - ch.center);
		});
		ch.outside = get_best_score(make_iterators_range(path.begin() + i + n_space, path.begin() + i + n_space + n_space), [&](xy pos) {
			return -diag_distance(pos - ch.center);
		});

		xy rel = path[i + n_space] - path[i + n_space - 1];
		double a = atan2(rel.y, rel.x);
		auto go = [&](double ang) {
			xy add;
			add.x = (int)(cos(ang) * 32);
			add.y = (int)(sin(ang) * 32);
			xy pos = ch.center;
			while (true) {
				pos += add;
				if ((size_t)pos.x >= (size_t)grid::map_width || (size_t)pos.y >= (size_t)grid::map_height) return pos - add;
				auto&bs = grid::get_build_square(pos);
				if (!bs.entirely_walkable) return pos - add;
				ch.build_squares.push_back(&bs);
			}
		};
		ch.build_squares.push_back(&grid::get_build_square(ch.center));
		xy left = go(a + PI / 2);
		xy right = go(a - PI / 2);
		ch.width = (right - left).length();

		double s = ch.width;
		s -= (grid::get_build_square(ch.center).height - grid::get_build_square(ch.outside).height) * 32 * 3;
		log("found choke with width %g score %g (%d build squares)\n", ch.width, s, ch.build_squares.size());
		if (s < best_score) {
			best_score = s;
			best = std::move(ch);
		}
	}
	best.path = path;
	return best;
}


xy my_closest_base;
xy op_closest_base;
int last_calc_defence_paths;

void update_defence_choke() {
	static xy last_my_closest_base;
	static xy last_op_closest_base;
	a_vector<xy> my_bases;
	a_vector<xy> op_bases;
	while (square_pathing::get_pathing_map(unit_types::siege_tank_tank_mode).path_nodes.empty()) multitasking::sleep(1);
	for (auto&v : buildpred::get_my_current_state().bases) {
		my_bases.push_back(square_pathing::get_nearest_node_pos(unit_types::siege_tank_tank_mode, v.s->cc_build_pos));
	}
	for (auto&v : buildpred::get_op_current_state().bases) {
		op_bases.push_back(square_pathing::get_nearest_node_pos(unit_types::siege_tank_tank_mode, v.s->cc_build_pos));
	}
	for (auto&b : build::build_tasks) {
		if (b.built_unit) continue;
		if (b.build_pos != xy() && b.type->unit && b.type->unit->is_resource_depot) my_bases.push_back(b.build_pos);
	}
	log("my_bases.size() is %d op_bases.size() is %d\n", my_bases.size(), op_bases.size());
	if (!my_bases.empty() && !op_bases.empty()) {
		xy my_closest = get_best_score(my_bases, [&](xy pos) {
			return get_best_score_value(op_bases, [&](xy pos2) {
				return unit_pathing_distance(unit_types::zealot, pos, pos2);
			});
		});
		xy op_closest = get_best_score(op_bases, [&](xy pos) {
			return get_best_score_value(my_bases, [&](xy pos2) {
				return unit_pathing_distance(unit_types::zealot, pos, pos2);
			});
		});
		log("my_closest is %d %d, op_closest is %d %d\n", my_closest.x, my_closest.y, op_closest.x, op_closest.y);
		bool redo = current_frame - last_calc_defence_paths >= 15 * 60;
		if (defence_choke.center == xy() && current_frame - last_calc_defence_paths >= 15 * 4) redo = true;
		if (my_closest != my_closest_base || op_closest != op_closest_base || redo) {
			last_calc_defence_paths = current_frame;
			my_closest_base = my_closest;
			op_closest_base = op_closest;
			defence_choke = find_choke_from_to(unit_types::zealot, my_closest_base, op_closest_base);

			int iterations = 0;
			dont_siege_here_path = find_path(unit_types::siege_tank_tank_mode, my_bases.front(), [&](xy pos, xy npos) {
				if (++iterations % 1024 == 0) multitasking::yield_point();
				return true;
			}, [&](xy pos, xy npos) {
				return 0.0;
			}, [&](xy pos) {
				return manhattan_distance(pos - op_closest_base) <= 64;
			});
			log("dont_siege_here_path.size() is %d\n", dont_siege_here_path.size());
			dont_siege_here_path.resize(dont_siege_here_path.size() / 2);

			dont_siege_here.reset();
			for (xy pos : dont_siege_here_path) {
				auto set = [&](xy pos) {
					if ((size_t)pos.x >= (size_t)grid::map_width || (size_t)pos.y >= (size_t)grid::map_height) return;
					dont_siege_here.set(grid::build_square_index(pos));
				};
				set(pos);
				set(pos + xy(-32, -32));
				set(pos + xy(+32, -32));
				set(pos + xy(+32, +32));
				set(pos + xy(-32, +32));
			}
		}
	}
}

void do_defence(const a_vector<combat_unit*>&allies) {

	auto siege_score = [&](xy pos) {
		double outside_d = unit_pathing_distance(unit_types::zealot, pos, defence_choke.outside);
		double inside_d = unit_pathing_distance(unit_types::zealot, pos, defence_choke.inside);
		int count = 0;
		for (auto*bs : defence_choke.build_squares) {
			if (diag_distance(pos - bs->pos + xy(16, 16)) <= 32 * 12 + 32) ++count;
		}
		if (count < std::min(4, (int)defence_choke.build_squares.size())) return 0.0;
		double height = grid::get_build_square(pos).height - grid::get_build_square(defence_choke.outside).height;
		return -outside_d + inside_d - count * 4 - height * 64;
	};

	for (auto*a : allies) {
		if (a->siege_pos != xy()) {
			double s = siege_score(a->siege_pos);
			bool reset = s >= 0;
			if (a->u->type == unit_types::siege_tank_tank_mode) {
				size_t index = grid::build_square_index(a->siege_pos);
				if (build_square_taken.test(index)) reset = true;
				if (dont_siege_here.test(index)) reset = true;
			}
			if (a->u->type == unit_types::siege_tank_siege_mode && current_frame - a->u->controller->last_siege >= 15 * 40) reset = true;
			if (reset) a->siege_pos = xy();
		}
		if (a->siege_pos != xy()) {
			auto set = [&](xy pos) {
				if ((size_t)pos.x >= (size_t)grid::map_width || (size_t)pos.y >= (size_t)grid::map_height) return;
				build_square_taken.set(grid::build_square_index(pos));
			};
			set(a->siege_pos);
			set(a->siege_pos + xy(+32, 0)); set(a->siege_pos + xy(+32, +32));
			set(a->siege_pos + xy(0, +32)); set(a->siege_pos + xy(-32, +32));
			set(a->siege_pos + xy(-32, 0)); set(a->siege_pos + xy(-32, -32));
			set(a->siege_pos + xy(0, -32)); set(a->siege_pos + xy(+32, -32));
		}
	}

	int path_iterations = 0;
	for (auto*a : allies) {
		bool default_move = true;
		if (a->u->type == unit_types::siege_tank_tank_mode || a->u->type == unit_types::siege_tank_siege_mode) {
			if (a->u->type == unit_types::siege_tank_tank_mode && current_frame - a->last_find_siege_pos >= 15 * 2) {
				a->last_find_siege_pos = current_frame;
				tsc::dynamic_bitset visited(grid::build_grid_width*grid::build_grid_height);
				double best_score = 0.0;
				xy best_pos;
				find_path(a->u->type, a->u->pos, [&](xy pos, xy npos) {
					if (++path_iterations % 1024 == 0) multitasking::yield_point();
					double d = diag_distance(npos - a->u->pos);
					if (d >= 32 * 30) return false;
					size_t index = grid::build_square_index(npos);
					if (build_square_taken.test(index)) return false;
					if (d >= 32 * 3 && !visited.test(index) && !dont_siege_here.test(index)) {
						visited.set(index);
						double s = siege_score(npos);
						if (s < best_score) {
							best_score = s;
							best_pos = npos;
						}
					}
					return true;
				}, [&](xy pos, xy npos) {
					return 0.0;
				}, [&](xy pos) {
					return false;
				});
				if (a->siege_pos != xy() && siege_score(a->siege_pos)<best_score) best_pos = a->siege_pos;
				if (best_pos != xy()) {
					a->siege_pos = best_pos;
				}
			}
			if (a->siege_pos != xy()) {
				default_move = false;
				if (a->u->type == unit_types::siege_tank_siege_mode) {
					a->siege_pos = a->u->pos;
					a->subaction = combat_unit::subaction_idle;
					if (diag_distance(a->u->pos - a->siege_pos) >= 16) {
						if (a->u->game_unit->unsiege()) {
							a->u->controller->noorder_until = current_frame + 30;
						}
					}
				} else {
					a->subaction = combat_unit::subaction_idle;
					if (diag_distance(a->u->pos - a->siege_pos) > 8) {
						a->subaction = combat_unit::subaction_move;
						a->target_pos = a->siege_pos;
					} else {
						if (players::my_player->has_upgrade(upgrade_types::siege_mode)) {
							if (a->u->game_unit->siege()) {
								a->u->controller->noorder_until = current_frame + 30;
								a->u->controller->last_siege = current_frame;
							}
						}
					}
				}
			}
		}
		if (default_move) {
			a->subaction = combat_unit::subaction_move;
			if (a->target_pos == defence_choke.center || a->target_pos == defence_choke.inside) {
				if (diag_distance(a->u->pos - a->target_pos) <= 32 * 3) {
					if (a->target_pos == defence_choke.center) a->target_pos = defence_choke.inside;
					else a->target_pos = defence_choke.center;
				}
			} else a->target_pos = defence_choke.center;
			if (wall_in::active_wall_count()) a->target_pos = defence_choke.inside;
		}
	}

}

void fight() {

	prepare_attack();
	prepare_run();

	a_vector<unit*> nearby_enemies, nearby_allies;
	a_vector<combat_unit*> nearby_combat_units;
	std::lock_guard<multitasking::mutex> l(groups_mut);
// 	a_unordered_set<combat_unit*> available;
// 	for (auto&g : groups) {
// 		for (auto*cu : g.allies) available.insert(cu);
// 	}
	for (auto&g : groups) {

		if (g.is_defensive_group) {
			do_defence(g.allies);
			continue;
		}

		nearby_enemies.clear();
		nearby_combat_units.clear();
		nearby_allies.clear();

		nearby_enemies = g.enemies;
		for (auto*cu : g.allies) {
			double dist = get_best_score_value(nearby_enemies, [&](unit*e) {
				return diag_distance(e->pos - cu->u->pos);
			});
			if (dist <= 32 * 25) {
				nearby_combat_units.push_back(cu);
				nearby_allies.push_back(cu->u);
			}
// 			nearby_combat_units.push_back(cu);
// 			nearby_allies.push_back(cu->u);
		}
		if (!nearby_enemies.empty() && !nearby_allies.empty()) {
// 			nearby_combat_units.clear();
// 			nearby_combat_units.push_back(cu);
// 			nearby_allies.clear();
// 			nearby_allies.push_back(cu->u);
// 			for (auto*a : live_combat_units) {
// 				if (a == cu) continue;
// 				if (diag_distance(cu->u->pos - a->u->pos) <= 32 * 15) {
// 					nearby_combat_units.push_back(a);
// 					nearby_allies.push_back(a->u);
// 				}
// 			}

			bool is_base_defence = false;
			for (unit*e : nearby_enemies) {
				if (!is_base_defence) is_base_defence = my_base.test(grid::build_square_index(e->pos));
			}

			bool has_siege_mode = players::my_player->upgrades.count(upgrade_types::siege_mode) != 0;
			auto add = [&](combat_eval::eval&eval, unit*u, int team) -> combat_eval::combatant& {
				//log("add %s to team %d\n", u->type->name, team);
				if (u->type == unit_types::bunker && u->is_completed) {
					for (int i = 0; i < u->marines_loaded; ++i) {
						eval.add_unit(units::get_unit_stats(unit_types::marine, u->owner), team);
					}
				}
				auto*st = u->stats;
				int cooldown_override = 0;
				if (!u->visible) cooldown_override = 0;
				if (u->type->requires_pylon && !u->is_powered) cooldown_override = 15 * 60;
				auto&c = eval.add_unit(st, team);
				c.move = get_best_score(make_transform_filter(team ? nearby_allies : nearby_enemies, [&](unit*e) {
					if (e->type->is_flyer && !c.st->ground_weapon) return std::numeric_limits<double>::infinity();
					return units_distance(e, u);
				}), identity<double>());
				if (c.move == std::numeric_limits<double>::infinity()) c.move = 32 * 8;
				eval.set_unit_stuff(c, u);
				if (cooldown_override > c.cooldown) c.cooldown = cooldown_override;
				//log("added %s to team %d -- move %g, shields %g, hp %g, cooldown %d\n", st->type->name, team, c.move, c.shields, c.hp, c.cooldown);
				return c;
			};
			int eval_frames = 15 * 60;
			combat_eval::eval eval;
			eval.max_frames = eval_frames;
			int worker_count = 0;
			for (unit*a : nearby_allies) {
				if (a->type->is_worker && worker_count++ >= 6) continue;
				add(eval, a, 0);
			}
			for (unit*e : nearby_enemies) add(eval, e, 1);
			eval.run();

			combat_eval::eval ground_eval;
			ground_eval.max_frames = eval_frames;
			worker_count = 0;
			for (unit*a : nearby_allies) {
				if (a->type->is_worker && worker_count++ >= 6) continue;
				if (!a->is_flying) add(ground_eval, a, 0);
			}
			for (unit*e : nearby_enemies) add(ground_eval, e, 1);
			ground_eval.run();

			bool ground_fight = ground_eval.teams[0].score > ground_eval.teams[1].score;

			combat_eval::eval air_eval;
			air_eval.max_frames = eval_frames;
			worker_count = 0;
			for (unit*a : nearby_allies) {
				if (a->type->is_worker && worker_count++ >= 6) continue;
				if (a->is_flying) add(air_eval, a, 0);
			}
			for (unit*e : nearby_enemies) add(air_eval, e, 1);
			air_eval.run();

			bool air_fight = ground_eval.teams[0].score > ground_eval.teams[1].score;

			if (true) {
				int my_valkyrie_count = 0;
				int op_muta_count = 0;
				for (unit*a : nearby_allies) {
					if (a->type == unit_types::valkyrie) ++my_valkyrie_count;
				}
				for (unit*e : nearby_enemies) {
					if (e->type == unit_types::mutalisk && current_frame - e->last_attacked <= 15 * 10) ++op_muta_count;
				}
				if (my_valkyrie_count >= 2 && op_muta_count >= 4 && op_muta_count < my_valkyrie_count * 5) air_fight = true;
			}

			bool has_dropship = false;
			for (unit*u : nearby_allies) {
				if (u->type == unit_types::dropship) has_dropship = true;
			}
			bool is_drop = false;
			a_map<unit*, unit*> loaded_units;
			a_map<unit*, int> dropships;
			bool some_are_not_loaded_yet = false;
			if (has_dropship && false) {
				combat_eval::eval sp_eval;
				sp_eval.max_frames = eval_frames;
				for (unit*u : nearby_allies) {
					if (u->type == unit_types::dropship) {
						int pickup_time = 0;
						int space = u->type->space_provided;
						for (unit*lu : u->loaded_units) {
							loaded_units[lu] = u;
							space -= lu->type->space_required;
						}
						while (space > 0) {
							unit*nu = get_best_score(nearby_allies, [&](unit*nu) {
								if (nu->type->is_flyer) return std::numeric_limits<double>::infinity();
								if (nu->is_loaded || loaded_units.find(nu) != loaded_units.end()) return std::numeric_limits<double>::infinity();
								if (diag_distance(nu->pos - u->pos) > 32 * 10) return std::numeric_limits<double>::infinity();
								if (nu->type->space_required > space) return std::numeric_limits<double>::infinity();
								return diag_distance(nu->pos - u->pos);
							}, std::numeric_limits<double>::infinity());
							if (!nu) break;
							loaded_units[nu] = u;
							space -= nu->type->space_required;
							//pickup_time += (int)(diag_distance(nu->pos - u->pos) / u->stats->max_speed);
							some_are_not_loaded_yet = true;
						}
						dropships[u] = pickup_time;
					}
				}
				worker_count = 0;
				for (unit*a : nearby_allies) {
					if (a->type->is_worker && worker_count++ >= 6) continue;
					if (loaded_units.find(a) != loaded_units.end()) continue;
					auto&c = add(sp_eval, a, 0);
					if (dropships.find(a) != dropships.end()) {
						c.force_target = true;
						dropships[a] += (int)(c.move / a->stats->max_speed);
					}
				}
				a_unordered_map<unit*,int> loaded_unit_count;
				for (auto&v : loaded_units) {
					auto&c = add(sp_eval, std::get<0>(v), 0);
					c.move = -1000;
					c.loaded_until = dropships[std::get<1>(v)] + (loaded_unit_count[std::get<1>(v)]++ * 15);
					//c.loaded_until = dropships[std::get<1>(v)];
				}
				for (unit*e : nearby_enemies) add(sp_eval, e, 1);
				sp_eval.run();

				log("drop result: supply %g %g  damage %g %g  in %d frames\n", sp_eval.teams[0].end_supply, sp_eval.teams[1].end_supply, sp_eval.teams[0].damage_dealt, sp_eval.teams[1].damage_dealt, sp_eval.total_frames);

				//if (sp_eval.teams[0].damage_dealt > eval.teams[0].damage_dealt) {
				if (sp_eval.teams[1].damage_dealt < eval.teams[1].damage_dealt) {
					eval = sp_eval;
					is_drop = true;
				}
			}

			{
				a_map<unit_type*, int> my_count, op_count;
				for (unit*a : nearby_allies) ++my_count[a->type];
				for (unit*e : nearby_enemies) ++op_count[e->type];
				log("combat::\n");
				log("my units -");
				for (auto&v : my_count) log(" %dx%s", v.second, short_type_name(v.first));
				log("\n");
				log("op units -");
				for (auto&v : op_count) log(" %dx%s", v.second, short_type_name(v.first));
				log("\n");

				log("result: supply %g %g  damage %g %g  in %d frames\n", eval.teams[0].end_supply, eval.teams[1].end_supply, eval.teams[0].damage_dealt, eval.teams[1].damage_dealt, eval.total_frames);
			}

			double fact = 1.0;
			bool already_fighting = test_pred(nearby_combat_units, [&](combat_unit*cu) {
				return current_frame - cu->last_fight <= 60 && current_frame - cu->last_run > 60;
			});
			if (already_fighting) fact = 0.5;

			bool fight = eval.teams[0].score >= eval.teams[1].score;
			log("scores: %g %g\n", eval.teams[0].score, eval.teams[1].score);
			if (is_base_defence) {
				fight |= eval.teams[1].start_supply - eval.teams[1].end_supply > eval.teams[0].start_supply - eval.teams[0].end_supply;
				fight |= eval.teams[0].end_supply >= eval.teams[1].end_supply;
			}
			if ((air_fight || ground_fight) && !is_drop) fight = false;
			else {
				if (fight) log("fight!\n");
				else log("run!\n");
			}
			if (ground_fight) log("ground fight!\n");
			if (air_fight) log("air fight!\n");

			unit*defensive_matrix_target = nullptr;
			if (!fight && !ground_fight && !air_fight) {
				bool has_defensive_matrix = false;
				for (unit*a : nearby_allies) {
					if (a->type == unit_types::science_vessel && a->energy >= 100) {
						has_defensive_matrix = true;
						break;
					}
				}
				if (has_defensive_matrix) {
					combat_eval::eval sp_eval;
					sp_eval.max_frames = eval_frames;
					double lowest_move = std::numeric_limits<double>::infinity();
					unit*target = nullptr;
					size_t target_idx = 0;
					size_t idx = 0;
					worker_count = 0;
					for (unit*a : nearby_allies) {
						if (a->type->is_worker && worker_count++ >= 6) continue;
						auto&c = add(sp_eval, a, 0);
						if (c.move < lowest_move) {
							lowest_move = c.move;
							target = a;
							target_idx = idx;
						}
						++idx;
					}
					for (unit*e : nearby_enemies) add(sp_eval, e, 1);
					if (target) {
						sp_eval.teams[0].units[target_idx].hp += 250;
						sp_eval.run();

						log("defensive matrix result: supply %g %g  damage %g %g  in %d frames\n", sp_eval.teams[0].end_supply, sp_eval.teams[1].end_supply, sp_eval.teams[0].damage_dealt, sp_eval.teams[1].damage_dealt, sp_eval.total_frames);

						bool fight = sp_eval.teams[0].score >= sp_eval.teams[1].score;
						if (fight) {
							defensive_matrix_target = target;
						}
					}
				}
			}

			size_t my_siege_tank_count = 0;
			size_t my_sieged_tank_count = 0;
			size_t my_unsieged_tank_count = 0;
			for (unit*u : nearby_allies) {
				if (u->type == unit_types::siege_tank_tank_mode || u->type == unit_types::siege_tank_siege_mode) ++my_siege_tank_count;
				if (u->type == unit_types::siege_tank_tank_mode) ++my_unsieged_tank_count;
				if (u->type == unit_types::siege_tank_siege_mode) ++my_sieged_tank_count;
			}
			size_t op_siege_tank_count = 0;
			size_t op_sieged_tank_count = 0;
			size_t op_unsieged_tank_count = 0;
			for (unit*u : nearby_enemies) {
				if (u->type == unit_types::siege_tank_tank_mode || u->type == unit_types::siege_tank_siege_mode) ++op_siege_tank_count;
				if (u->type == unit_types::siege_tank_tank_mode) ++op_unsieged_tank_count;
				if (u->type == unit_types::siege_tank_siege_mode) ++op_sieged_tank_count;
			}

			bool some_are_attacking = false;
			for (unit*u : nearby_allies) {
				if (current_frame - u->last_attacked < 60) some_are_attacking = true;
			}

			unit*defensive_matrix_vessel = nullptr;
			if (defensive_matrix_target) {
				defensive_matrix_vessel = get_best_score(nearby_allies, [&](unit*u) {
					if (u->type != unit_types::science_vessel || u->energy < 100) return std::numeric_limits<double>::infinity();
					return diag_distance(defensive_matrix_target->pos - u->pos);
				}, std::numeric_limits<double>::infinity());
			}

			int op_ground_units = 0;
			int op_air_units = 0;
			for (unit*u : nearby_enemies) {
				if (u->is_flying) ++op_air_units;
				else ++op_ground_units;
			}

			worker_count = 0;
			for (auto*a : nearby_combat_units) {
				a->last_fight = current_frame;
				a->last_processed_fight = current_frame;
				bool attack = fight;
				attack |= a->u->is_flying && air_fight;
				attack |= !a->u->is_flying && ground_fight;
				if (attack) {
					bool dont_attack = false;
					/*bool unload = true;
					if (is_drop) {
						auto lui = loaded_units.find(a->u);
						if (lui != loaded_units.end()) {
							if (dropships[lui->second] > 15 * 1) {
								a->subaction = combat_unit::subaction_move;
								a->target_pos = lui->second->pos;
								dont_attack = true;
							}
						}
						auto di = dropships.find(a->u);
						if (di != dropships.end()) {
							unload = di->second <= 15 * 1;
							if (!unload) {
								bool loaded_any = false;
								for (auto&v : loaded_units) {
									if (v.second == a->u) {
										unit*lu = v.first;
										if (!lu->is_loaded) {
											a->subaction = combat_unit::subaction_move;
											a->target_pos = lu->pos;
											if (diag_distance(lu->pos - a->u->pos) <= 32 * 4) {
												a->subaction = combat_unit::subaction_idle;
												a->u->controller->action = unit_controller::action_idle;
												if (current_frame >= a->u->controller->noorder_until) {
													loaded_any = true;
													lu->game_unit->rightClick(a->u->game_unit);
													//a->u->game_unit->load(lu->game_unit);
													lu->controller->noorder_until = current_frame + 15;
												}
											}
											dont_attack = true;
										}
									}
								}
								if (loaded_any) a->u->controller->noorder_until = current_frame + 4;
							}
						}
					}
					if (!a->u->loaded_units.empty() && unload) {
						dont_attack = true;
						a->subaction = combat_unit::subaction_idle;
						a->u->controller->action = unit_controller::action_idle;
						if (current_frame >= a->u->controller->noorder_until) {
							a->u->game_unit->unload(a->u->loaded_units.front()->game_unit);
							a->u->controller->noorder_until = current_frame + 4;
						}
					}*/
					
					if (a->u->type->is_worker && worker_count++ >= 6) {
						dont_attack = worker_is_safe(a, nearby_enemies);
					}
					
					if (!dont_attack) {
						if (a->u->type == unit_types::dropship && a->u->loaded_units.empty()) {
							//do_run(a, nearby_enemies);
							//dont_attack = true;
						}
					}
					if (!dont_attack) {
						if (is_drop && some_are_not_loaded_yet && !some_are_attacking) do_run(a, nearby_enemies);
						else do_attack(a, nearby_allies, nearby_enemies);
					}
				} else {
					a->last_run = current_frame;
					bool run = true;
					if (a->u == defensive_matrix_vessel) {
						a->subaction = combat_unit::subaction_use_ability;
						a->ability = upgrade_types::defensive_matrix;
						a->target = defensive_matrix_target;
						run = false;
					}
					if (run) {
						if (my_siege_tank_count >= 1 && players::my_player->has_upgrade(upgrade_types::siege_mode)) {
							bool okay = true;
							if (a->u->type == unit_types::siege_tank_tank_mode || a->u->type == unit_types::siege_tank_siege_mode) okay = true;
							//okay &= my_sieged_tank_count >= op_sieged_tank_count && op_sieged_tank_count < 4;
							okay &= op_ground_units > op_air_units;
							if (!a->u->is_flying && okay) {
								double r = get_best_score_value(nearby_allies, [&](unit*u) {
									if (u->type != unit_types::siege_tank_tank_mode && u->type != unit_types::siege_tank_siege_mode) return std::numeric_limits<double>::infinity();
									return diag_distance(u->pos - a->u->pos);
								}, std::numeric_limits<double>::infinity());
								if (r <= 32 * 15) {
									do_attack(a, nearby_allies, nearby_enemies);
									run = false;
								}
							}
						}
					}
					if (run) do_run(a, nearby_enemies);
				}
				multitasking::yield_point();
			}

		}
	}

	finish_attack();

}

void execute() {

	for (auto*cu : live_combat_units) {

// 		if (cu->subaction == combat_unit::subaction_idle) {
// 			cu->u->controller->action = unit_controller::action_move;
// 			cu->u->controller->go_to = cu->defend_spot;
// 		}

		if (cu->subaction == combat_unit::subaction_fight) {
			//cu->u->controller->action = unit_controller::action_fight;
			cu->u->controller->action = unit_controller::action_attack;
			cu->u->controller->target = cu->target;
		}
		if (cu->subaction == combat_unit::subaction_move) {
			cu->u->controller->action = unit_controller::action_move;
			cu->u->controller->go_to = cu->target_pos;
		}
		if (cu->subaction == combat_unit::subaction_move_directly) {
			cu->u->controller->action = unit_controller::action_move_directly;
			cu->u->controller->go_to = cu->target_pos;
		}
		if (cu->subaction == combat_unit::subaction_use_ability) {
			cu->u->controller->action = unit_controller::action_use_ability;
			cu->u->controller->ability = cu->ability;
			cu->u->controller->target = cu->target;
			cu->u->controller->target_pos = cu->target_pos;
		}
		if (cu->subaction == combat_unit::subaction_repair) {
			cu->u->controller->action = unit_controller::action_repair;
			cu->u->controller->target = cu->target;
		}

		if (cu->subaction == combat_unit::subaction_idle) {
			if (cu->u->controller->action == unit_controller::action_attack) {
				cu->u->controller->action = unit_controller::action_idle;
			}
		}

	}

}

void combat_task() {

	int last_update_bases = 0;

	while (true) {

		if (current_frame - last_update_bases >= 30) {
			last_update_bases = current_frame;

			update_my_base();
			update_op_base();
		}

		update_combat_units();

		fight();

		defence();

		execute();

		multitasking::sleep(1);
	}

}

void update_combat_groups_task() {
	while (true) {

		multitasking::sleep(10);

		update_groups();

	}
}

int build_bunker_count = 0;

void update_defence_choke_task() {
	while (true) {

		multitasking::sleep(15 * 2);

		update_defence_choke();

		// This does not belong here!
		// Just a quick hack to build bunkers in a reasonable spot
		build::add_build_sum(0, unit_types::bunker, build_bunker_count - my_units_of_type[unit_types::bunker].size());
		for (auto&b : build::build_tasks) {
			if (b.built_unit) continue;
			if (b.type->unit == unit_types::bunker || b.type->unit == unit_types::missile_turret) {
				if (b.build_near != defence_choke.center) {
					b.build_near = defence_choke.center;
					build::unset_build_pos(&b);
				}
			}
		}

	}
}

void render() {

// 	BWAPI::Position screen_pos = game->getScreenPosition();
// 	for (int y = screen_pos.y; y < screen_pos.y + 400; y += 32) {
// 		for (int x = screen_pos.x; x < screen_pos.x + 640; x += 32) {
// 			if ((size_t)x >= (size_t)grid::map_width || (size_t)y >= (size_t)grid::map_height) break;
// 
// 			x &= -32;
// 			y &= -32;
// 
// 			size_t index = grid::build_square_index(xy(x, y));
// 			if (!my_base.test(index)) {
// 				game->drawBoxMap(x, y, x + 32, y + 32, BWAPI::Colors::Green);
// 			}
// 
// 		}
// 	}
	
	for (auto&ds : defence_spots) {
		xy pos = ds.pos;
		pos.x &= -32;
		pos.y &= -32;
		game->drawBoxMap(pos.x, pos.y, pos.x + 32, pos.y + 32, BWAPI::Colors::Red);
	}

	for (auto&g : groups) {
		xy pos = g.enemies.front()->pos;
		for (unit*e : g.enemies) {
			game->drawLineMap(e->pos.x, e->pos.y, pos.x, pos.y, BWAPI::Colors::Red);
		}
		for (auto*cu : g.allies) {
			game->drawLineMap(cu->u->pos.x, cu->u->pos.y, pos.x, pos.y, BWAPI::Colors::Green);
		}
	}

	xy screen_pos = bwapi_pos(game->getScreenPosition());
	for (auto&g : groups) {
		for (size_t idx : g.threat_area) {
			xy pos((idx % (size_t)grid::build_grid_width) * 32, (idx / (size_t)grid::build_grid_width) * 32);
			if (pos.x < screen_pos.x || pos.y < screen_pos.y) continue;
			if (pos.x + 32 >= screen_pos.x + 640 || pos.y + 32 >= screen_pos.y + 400) continue;
			
			game->drawBoxMap(pos.x, pos.y, pos.x + 32, pos.y + 32, BWAPI::Colors::Brown, false);
		}
	}

	for (auto&v : nuke_test) {
		xy pos = std::get<0>(v);
		double val = std::get<1>(v);
		game->drawTextMap(pos.x, pos.y, "%g", val);
	}

	if (true) {
		bwapi_pos screen_pos = game->getScreenPosition();
		xy lp;
		for (auto&v : defence_choke.path) {
			if (v.x < screen_pos.x || v.y < screen_pos.y) continue;
			if (v.x >= screen_pos.x + 640 || v.y >= screen_pos.y + 400) continue;
			if (lp != xy()) game->drawLineMap(lp.x, lp.y, v.x, v.y, BWAPI::Colors::Yellow);
			lp = v;
		}
		for (auto*bs : defence_choke.build_squares) {
			xy pos = bs->pos;
			game->drawBoxMap(pos.x, pos.y, pos.x + 32, pos.y + 32, BWAPI::Colors::Yellow);
		}
		game->drawCircleMap(defence_choke.inside.x, defence_choke.inside.y, 16, BWAPI::Color(128, 255, 0));
		game->drawCircleMap(defence_choke.center.x, defence_choke.center.y, 12, BWAPI::Color(255, 255, 0));
		game->drawCircleMap(defence_choke.outside.x, defence_choke.outside.y, 16, BWAPI::Color(255, 128, 0));

		lp = xy();
		for (auto&v : dont_siege_here_path) {
			if (v.x < screen_pos.x || v.y < screen_pos.y) continue;
			if (v.x >= screen_pos.x + 640 || v.y >= screen_pos.y + 400) continue;
			if (lp != xy()) game->drawLineMap(lp.x, lp.y, v.x, v.y, BWAPI::Color(192, 192, 0));
			lp = v;
		}
	}
// 	if (true) {
// 		bwapi_pos screen_pos = game->getScreenPosition();
// 		for (int y = screen_pos.y&-32; y < screen_pos.y + 400; y += 32) {
// 			for (int x = screen_pos.x&-32; x < screen_pos.x + 640; x += 32) {
// 				if ((size_t)x >= (size_t)grid::map_width || (size_t)y >= (size_t)grid::map_height) continue;
// 				if (build_square_taken.test(grid::build_square_index(xy(x, y)))) {
// 					game->drawBoxMap(x, y, x + 32, y + 32, BWAPI::Colors::White);
// 				}
// 			}
// 
// 		}
// 	}

	if (true) {

		for (auto&v : focus_fire) {
			game->drawTextMap(v.first->pos.x, v.first->pos.y, "\x08%g", v.second);
		}

	}
}

void init() {

	my_base.resize(grid::build_grid_width*grid::build_grid_height);
	op_base.resize(grid::build_grid_width*grid::build_grid_height);

	entire_threat_area.resize(grid::build_grid_width*grid::build_grid_height);
	entire_threat_area_edge.resize(grid::build_grid_width*grid::build_grid_height);
	build_square_taken.resize(grid::build_grid_width*grid::build_grid_height);
	dont_siege_here.resize(grid::build_grid_width*grid::build_grid_height);

	multitasking::spawn(generate_defence_spots_task, "generate defence spots");
	multitasking::spawn(combat_task, "combat");
	multitasking::spawn(update_combat_groups_task, "update combat groups");
	multitasking::spawn(update_defence_choke_task, "update defence choke spot");

	render::add(render);

}

}
