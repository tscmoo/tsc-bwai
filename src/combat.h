
namespace combat {
;

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

struct combat_unit {
	unit*u = nullptr;
	enum { action_idle, action_offence };
	int action = action_idle;
	enum { subaction_move, subaction_fight };
	int subaction = subaction_move;
	xy defend_spot;
	xy goal_pos;
	int last_fight = 0;
	unit*target = nullptr;
	xy target_pos;
};
a_unordered_map<unit*, combat_unit> combat_unit_map;

a_vector<combat_unit*> live_combat_units;
a_vector<combat_unit*> idle_combat_units;

void update_combat_units() {
	live_combat_units.clear();
	for (unit*u : my_units) {
		if (u->building) continue;
		if (!u->is_completed) continue;
		if (u->type->is_worker) continue;
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
		if (current_frame - cu->last_fight <= 40) continue;
		auto&s = defence_spots.front();
		send_to(cu, &s);
	}

}

struct group_t {
	double value;
	a_vector<unit*> enemies;
	a_vector<combat_unit*> allies;
};
a_vector<group_t> groups;

void update_groups() {

	groups.clear();

	for (unit*e : enemy_units) {
		//if (e->building) continue;
		if (e->type->is_worker) continue;
		if (e->invincible) continue;
		if (e->gone) continue;
		if (!e->stats->ground_weapon && !e->stats->air_weapon && !e->type->is_resource_depot) continue;
		if (!buildpred::attack_now && op_base.test(grid::build_square_index(e->pos))) continue;
		group_t*g = nullptr;
		for (auto&v : groups) {
			for (unit*ne : v.enemies) {
				double d = units_distance(e, ne);
				bool is_near = false;
				if (d <= e->stats->sight_range) is_near = true;
				if (d <= ne->stats->sight_range) is_near = true;
				if (d <= 32 * 6) is_near = true;
				if (!is_near) continue;
				g = &v;
				break;
			}
			if (g) break;
		}
		if (!g) {
			groups.emplace_back();
			g = &groups.back();
		}
		g->enemies.push_back(e);
	}
	for (auto&g : groups) {
		double value = 0.0;
		for (unit*e : g.enemies) {
			value += e->minerals_value;
			value += e->gas_value * 2;
			if (my_base.test(grid::build_square_index(e->pos))) value *= 4;
		}
		g.value = value;
	}
	std::sort(groups.begin(), groups.end(), [&](const group_t&a, const group_t&b) {
		return a.value > b.value;
	});

	a_unordered_set<combat_unit*> available_units;
	for (auto*c : live_combat_units) {
		available_units.insert(c);
	}

	size_t group_idx = 0;
	for (auto&g : groups) {
		while (true) {
			std::tuple<bool, double> best_score;
			combat_unit*best_unit = nullptr;
			for (auto*c : available_units) {
				combat_eval::eval eval;
				auto addu = [&](unit*u, int team) {
					auto&c = eval.add_unit(u->stats, team);
					c.shields = u->shields;
					c.hp = u->hp;
					c.cooldown = u->weapon_cooldown;
				};
				for (auto*a : g.allies) addu(a->u, 0);
				for (unit*e : g.enemies) addu(e, 1);
				addu(c->u, 0);
				eval.run();
				bool done = eval.teams[0].end_supply >= eval.teams[0].start_supply / 2 + 4;
				double val = eval.teams[0].score - eval.teams[1].score;
				auto score = std::make_tuple(done, val);
				if (!best_unit || score > best_score) {
					best_score = score;
					best_unit = c;
				}
			}
			//if (!best_unit) log("failed to find unit for group %d\n", group_idx);
			if (!best_unit) break;
			//log("add %s to group %d with score %d %g\n", best_unit->u->type->name, group_idx, std::get<0>(best_score), std::get<1>(best_score));
			g.allies.push_back(best_unit);
			available_units.erase(best_unit);
			if (std::get<0>(best_score)) break;
		}
		++group_idx;
	}

	if (!groups.empty()) {
		for (auto*c : available_units) {
			groups.front().allies.push_back(c);
		}
	} else {
		for (auto*c : available_units) {
			c->action = combat_unit::action_idle;
		}
	}
	available_units.clear();

	for (auto&g : groups) {
		xy pos = g.enemies.front()->pos;
		for (auto*cu : g.allies) {
			cu->goal_pos = pos;
			cu->action = combat_unit::action_offence;
			if (current_frame - cu->last_fight <= 40) continue;
			cu->subaction = combat_unit::subaction_move;
			cu->target_pos = pos;
		}
	}
	
}

void do_attack(combat_unit*a, const a_vector<unit*>&enemies) {
	a->defend_spot = xy();
	a->subaction = combat_unit::subaction_fight;

	unit*u = a->u;
	auto score = [&](unit*e) {
		if (!e->visible || !e->detected) return std::make_tuple(std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity());
		if (e->invincible) return std::make_tuple(std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity());
		weapon_stats*w = e->type->is_flyer ? u->stats->air_weapon : u->stats->ground_weapon;
		if (!w) return std::make_tuple(std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity());
		double d = units_distance(u, e);
		if (!e->stats->ground_weapon && !e->stats->air_weapon) return std::make_tuple(std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity(), d);
		if (d < w->min_range) return std::make_tuple(std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity(), std::numeric_limits<double>::infinity());
		if (d > w->max_range) return std::make_tuple(std::numeric_limits<double>::infinity(), d, 0.0);
		return std::make_tuple(e->shields + e->hp / combat_eval::get_damage_type_modifier(w->damage_type, e->type->size), 0.0, 0.0);
	};

	unit*target = get_best_score(enemies, score);
	//log("u->target is %p, u->order_target is %p\n", u->target, u->order_target);
	if (u->order_target) {
		double new_s = std::get<0>(score(target));
		double old_s = std::get<0>(score(u->order_target));
		if (old_s < new_s * 2) target = u->order_target;
	}
	if (u->target) {
		double new_s = std::get<0>(score(target));
		double old_s = std::get<0>(score(u->target));
		if (old_s < new_s * 2) target = u->target;
	}
	a->target = target;

	if (target) {
		weapon_stats*my_weapon = target->type->is_flyer ? a->u->stats->air_weapon : a->u->stats->ground_weapon;
		weapon_stats*e_weapon = a->u->type->is_flyer ? target->stats->air_weapon : target->stats->ground_weapon;
		double max_range = 1000.0;
		if (e_weapon && my_weapon) {
			if (my_weapon->max_range < e_weapon->max_range) max_range = my_weapon->max_range / 2;
			if (e_weapon->min_range) max_range = e_weapon->min_range;
		}
		if (units_distance(target, a->u)>max_range) {
			if (a->u->weapon_cooldown>latency_frames) {
				a->subaction = combat_unit::subaction_move;
				a->target_pos = target->pos;
			}
		}
	}
}

void do_run(combat_unit*a,a_vector<unit*>&enemies) {
	a->defend_spot = xy();
	a->subaction = combat_unit::subaction_move;
	a->target_pos = xy(grid::map_width / 2, grid::map_height / 2);
	unit*nb = get_best_score(my_buildings, [&](unit*u) {
		return diag_distance(a->u->pos - u->pos);
	});
	if (nb) a->target_pos = nb->pos;

	unit*u = a->u;
	double start_d = get_best_score_value(enemies, [&](unit*e) {
		return diag_distance(e->pos - u->pos);
	});

	start_d *= 0.5;
	
	auto path = square_pathing::find_square_path(square_pathing::get_pathing_map(u->type), u->pos, [&](xy pos, xy npos) {
		double nd = get_best_score_value(enemies, [&](unit*e) {
			return diag_distance(npos - e->pos);
		});
		return nd >= start_d;
	}, [&](xy pos, xy npos) {
		double ned = get_best_score_value(enemies, [&](unit*e) {
			return diag_distance(pos - e->pos);
		});
		return ned <= 32 ? 32 * 4 : 0;
	}, [&](xy pos) {
		if (!grid::get_build_square(pos).entirely_walkable) return false;
		for (unit*e : enemies) {
			if (diag_distance(pos - e->pos) < e->stats->sight_range * 2) return false;
		}
		return true;
		//unit*ne = get_best_score(enemies, [&](unit*e) {
		//	return diag_distance(pos - e->pos);
		//});
		//return diag_distance(pos - ne->pos) >= ne->stats->sight_range * 2;
	});

	auto get_go_to = [&]() {
		double dis = 0.0;
		xy lpos = u->pos;
		for (xy pos : path) {
			dis += diag_distance(pos - lpos);
			if (dis >= 32 * 4) {
				auto&dims = u->type->dimensions;
				if (!square_pathing::can_fit_at(pos, dims)) continue;
				if (!square_pathing::can_fit_at(pos + xy(7, 0), dims)) continue;
				if (!square_pathing::can_fit_at(pos + xy(0, 7), dims)) continue;
				if (!square_pathing::can_fit_at(pos + xy(7, 7), dims)) continue;
				return pos + xy(4, 4);
			}
			lpos = pos;
			//if (frames_to_reach(u, diag_distance(pos - u->pos)) >= 30) return pos;
		}
		if (path.empty()) return u->pos;
		return path.back();
	};

	a->target_pos = get_go_to();

}

void fight() {

	a_vector<unit*> nearby_enemies, nearby_allies;
	a_vector<combat_unit*> nearby_combat_units;
	a_unordered_set<combat_unit*> visited;
	visited.reserve(live_combat_units.size());
	for (auto*cu : live_combat_units) {
		if (current_frame - cu->last_fight <= 5) continue;
		if (visited.count(cu)) continue;

		{
			a_unordered_set<combat_unit*> allies;
			a_unordered_set<unit*> enemies;
			auto in_range = [&](unit*u, unit*e) {
				double d = units_distance(u, e);
				if (d <= u->stats->sight_range*1.5) return true;
				weapon_stats*w = e->type->is_flyer ? u->stats->air_weapon : u->stats->ground_weapon;
				weapon_stats*w2 = u->type->is_flyer ? e->stats->air_weapon : e->stats->ground_weapon;
				if (w2 && d <= w2->max_range*1.5) return true;
				if (!w) return false;
				return d <= w->max_range*1.5;
			};
			std::function<void(combat_unit*)> add_ally = [&](combat_unit*cu) {
				if (!allies.insert(cu).second) return;
				/*std::function<void(unit*)> add_enemy = [&](unit*e) {
					if (!enemies.insert(e).second) return;
					for (auto*cu : live_combat_units) {
						double d = units_distance(cu->u, e);
						if (d <= 32 * 20) add_ally(cu);
					}
				};
				unit*u = cu->u;
				for (unit*e : enemy_units) {
					if (e->gone) continue;
					double d = units_distance(e, cu->u);
					if (d <= 32 * 20) add_enemy(e);
				}*/
				std::function<void(unit*)> add_enemy = [&](unit*e) {
					if (!enemies.insert(e).second) return;
					for (auto*cu : live_combat_units) {
						if (!in_range(e, cu->u) && !in_range(cu->u, e)) continue;
						add_ally(cu);
					}
					for (unit*e2 : enemy_units) {
						if (e2->gone) continue;
						double d = units_distance(e2, e);
						weapon_stats*w2 = cu->u->type->is_flyer ? e->stats->air_weapon : e->stats->ground_weapon;
						if (w2 && d <= w2->max_range*1.5) {
							add_enemy(e2);
						}
					}
				};
				unit*u = cu->u;
				for (unit*e : enemy_units) {
					if (e->gone) continue;
					if (!in_range(u, e) && !in_range(e, u)) continue;
					add_enemy(e);
				}
			};
			add_ally(cu);
			/*for (auto&g : groups) {
				bool found = false;
				for (auto*c : g.allies) {
					if (c == cu) {
						found = true;
						break;
					}
				}
				if (found) {
					for (auto*c : g.allies) {
						if (diag_distance(c->u->pos - cu->u->pos) > 32 * 20) continue;
						add_ally(c);
					}
					break;
				}
			}*/

			nearby_enemies.clear();
			nearby_combat_units.clear();
			nearby_allies.clear();
			if (!enemies.empty()) {
				for (unit*e : enemies) nearby_enemies.push_back(e);
				for (auto*cu : allies) {
					nearby_combat_units.push_back(cu);
					nearby_allies.push_back(cu->u);
				}
			}
		}

// 		nearby_enemies.clear();
// 		for (unit*e : enemy_units) {
// 			if (e->gone) continue;
// 			if (diag_distance(cu->u->pos - e->pos) <= 32 * 15) nearby_enemies.push_back(e);
// 		}
		if (!nearby_enemies.empty()) {
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
			combat_eval::eval eval;
			auto add = [&](unit*u, int team) {
				//log("add %s to team %d\n", u->type->name, team);
				auto&c = eval.add_unit(u->stats, team);
				c.move = get_best_score(make_transform_filter(team ? nearby_allies : nearby_enemies, [&](unit*e) {
					return units_distance(e, u);
				}), identity<double>());
				c.shields = u->shields;
				c.hp = u->hp;
				c.cooldown = u->weapon_cooldown;
			};
			for (unit*a : nearby_allies) add(a, 0);
			for (unit*e : nearby_enemies) add(e, 1);
			eval.run();

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

				log("result: supply %g %g  damage %g %g\n", eval.teams[0].end_supply, eval.teams[1].end_supply, eval.teams[0].damage_dealt, eval.teams[1].damage_dealt);
			}

			//bool fight = eval.teams[0].damage_dealt > eval.teams[1].damage_dealt*0.75;
			double fact = 1.0;
			if (current_frame - cu->last_fight <= 40) fact = 0.5;
			//bool fight = eval.teams[0].end_supply > eval.teams[1].end_supply * fact;
			double my_killed = eval.teams[1].start_supply - eval.teams[1].end_supply;
			double op_killed = eval.teams[0].start_supply - eval.teams[0].end_supply;
			bool fight = my_killed > op_killed*fact;
			fight |= eval.teams[0].end_supply > eval.teams[1].end_supply * fact;
			if (fight) log("fight!\n");
			else log("run!\n");


			bool ignore = false;
			if (eval.teams[1].damage_dealt < eval.teams[0].damage_dealt / 10) {
				if (diag_distance(cu->u->pos - cu->goal_pos) >= 32 * 15) {
					if (eval.total_frames > 15 * 30) {
						//ignore = true;
					}
				}
			}

			if (!ignore) {
				for (auto*a : nearby_combat_units) {
					a->last_fight = current_frame;
					if (fight) {
						do_attack(a, nearby_enemies);
					} else {
						do_run(a, nearby_enemies);
					}
					multitasking::yield_point();
				}
			} else {
				for (auto*a : nearby_combat_units) {
					a->last_fight = current_frame;
				}
			}

		} else {
			for (auto*a : nearby_combat_units) {
				visited.insert(a);
			}
// 			cu->action = combat_unit::action_idle;

// 			cu->subaction = combat_unit::subaction_move;
// 			cu->target_pos = cu->u->pos;
		}
	}

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

		multitasking::sleep(20);

		update_groups();

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

}

void init() {

	my_base.resize(grid::build_grid_width*grid::build_grid_height);
	op_base.resize(grid::build_grid_width*grid::build_grid_height);

	multitasking::spawn(generate_defence_spots_task, "generate defence spots");
	multitasking::spawn(combat_task, "combat");
	multitasking::spawn(update_combat_groups_task, "update combat groups");

	render::add(render);

}

}
