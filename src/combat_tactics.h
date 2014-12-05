namespace combat_tactics {
;

struct tactic {
	a_vector<unit*> enemies;
	a_vector<combat_unit*> allies;
};

struct tactic_siege: tactic {
	a_vector<unit*> targets;
	combat_unit*spotter = nullptr;
};

a_vector<tactic*> all_tactics;
a_list<tactic_siege> siege_tactics;
a_unordered_map<unit*, tactic_siege*> siege_target_map;

auto spotter_score = [&](combat_unit*c) {
	return -c->u->stats->sight_range - (c->u->is_flying ? 32 * 4 : 0);
};

void run_siege_tactic(tactic_siege&t) {

	size_t siege_tank_count = 0;
	for (auto*c : t.allies) {
		if (c->u->type == unit_types::siege_tank_tank_mode || c->u->type == unit_types::siege_tank_siege_mode) ++siege_tank_count;
	}

	for (auto i = t.allies.begin(); i != t.allies.end();) {
		if ((*i)->u->dead || siege_tank_count == 0) {
			(*i)->action = combat_unit::action_idle;
			if (*i == t.spotter) t.spotter = nullptr;
			i = t.allies.erase(i);
		} else ++i;
	}
	for (auto i = t.targets.begin(); i != t.targets.end();) {
		unit*e = *i;
		if (e->dead || e->gone) {
			siege_target_map[e] = nullptr;
			i = t.targets.erase(i);
		} else ++i;
	}
	if (t.allies.empty() || t.targets.empty()) return;

	a_vector<unit*> unit_allies;
	for (auto*c : t.allies) {
		unit_allies.push_back(c->u);
	}

	tsc::dynamic_bitset spot_taken(grid::build_grid_width*grid::build_grid_height);

	for (auto*c : t.allies) {
		if (c->u->type == unit_types::siege_tank_siege_mode) {
			auto set = [&](xy pos) {
				if ((size_t)pos.x >= (size_t)grid::map_width || (size_t)pos.y >= (size_t)grid::map_height) return;
				spot_taken.set(grid::build_square_index(pos));
			};
			set(c->u->pos);
			set(c->u->pos + xy(-16, -16));
			set(c->u->pos + xy(+16, -16));
			set(c->u->pos + xy(+16, +16));
			set(c->u->pos + xy(-16, +16));
		}
	}

	t.spotter = get_best_score(t.allies, spotter_score);

	auto get_danger = [&](combat_unit*cu) {
		unit*net = get_best_score(t.enemies, [&](unit*e) {
			double wr;
			if (e->type == unit_types::bunker) wr = 32 * 5;
			else {
				weapon_stats*ew = cu->u->is_flying ? e->stats->air_weapon : e->stats->ground_weapon;
				if (!ew) return std::numeric_limits<double>::infinity();
				wr = ew->max_range;
			}
			return units_distance(e, cu->u) - wr;
		}, std::numeric_limits<double>::infinity());
		if (net) {
			double wr;
			if (net->type == unit_types::bunker) wr = 32 * 5;
			else wr = (cu->u->is_flying ? net->stats->air_weapon : net->stats->ground_weapon)->max_range;
			return std::make_tuple(net->pos, wr);
		} else return std::make_tuple(xy(), 0.0);
	};

	a_unordered_map<unit*, int> target_count;
	for (auto*c : t.allies) {
		c->action = combat_unit::action_tactic;
		c->subaction = combat_unit::subaction_move;
		c->target_pos = t.targets.front()->pos;

		if (c == t.spotter) continue;
		unit*ne = get_best_score(t.enemies, [&](unit*e) {
			weapon_stats*w = e->is_flying ? c->u->stats->air_weapon : c->u->stats->ground_weapon;
			if (!w) return std::numeric_limits<double>::infinity();
			return units_distance(e, c->u);
		}, std::numeric_limits<double>::infinity());
		if (ne) {
			++target_count[ne];
			weapon_stats*w = ne->is_flying ? c->u->stats->air_weapon : c->u->stats->ground_weapon;
			double d = units_distance(ne, c->u);
			double margin = 4;
			double wr = w->max_range;
			if (c->u->type == unit_types::siege_tank_tank_mode) wr = 32 * 12;
			xy danger_pos;
			double danger_range;
			std::tie(danger_pos, danger_range) = get_danger(c);
			double close_range = std::max(wr * 2, danger_range * 2);
			if (d < close_range) {
				a_deque<xy> path = find_path(c->u->type, c->u->pos, [&](xy pos, xy npos) {
					if (diag_distance(ne->pos - npos) >= wr * 3) return false;
					if (c->u->is_flying) return true;
					return !spot_taken.test(grid::build_square_index(npos));
				}, [&](xy pos, xy npos) {
					return 0.0;
				}, [&](xy pos) {
					return units_distance(pos, c->u->type, ne->pos, ne->type) <= wr;
				});
				xy goal_pos = ne->pos;
				if (!path.empty()) goal_pos = path.back();
				if (!c->u->is_flying) spot_taken.set(grid::build_square_index(goal_pos));
				c->target_pos = goal_pos;
				if (d <= wr - margin) {
					if (c->u->type == unit_types::siege_tank_tank_mode) {
						if (current_frame >= c->u->controller->noorder_until) {
							if (c->u->game_unit->siege()) {
								c->u->controller->noorder_until = current_frame + 30;
							}
						}
					}
					do_attack(c, unit_allies, t.enemies);
				}
				if (d > wr) {
					if (c->u->type == unit_types::siege_tank_siege_mode) {
						if (current_frame >= c->u->controller->noorder_until) {
							if (c->u->game_unit->unsiege()) {
								c->u->controller->noorder_until = current_frame + 30;
							}
						}
					}
				}
			}
		}
	}
	unit*spot_target = nullptr;
	int highest_count = 0;
	for (auto&v : target_count) {
		if (!spot_target || v.second > highest_count) {
			spot_target = v.first;
			highest_count = v.second;
		}
	}
	if (spot_target) {
		if (current_frame - spot_target->last_seen >= 15) spot_target->scan_me_until = current_frame + 90;
	}
	if (t.spotter) {
		t.spotter->target_pos = t.allies.front()->u->pos;
		combat_unit*cu = t.spotter;
		xy danger_pos;
		double danger_range;
		std::tie(danger_pos, danger_range) = get_danger(cu);
		danger_range += 32 * 4;
		xy closest_pos = t.allies.front()->u->pos;
		double closest_dist = std::numeric_limits<double>::infinity();
		bool is_in_danger = diag_distance(danger_pos - cu->u->pos) <= danger_range;
		if (!spot_target || spot_target->visible) do_run(cu, t.enemies);
		else if (spot_target) {
			a_deque<xy> path = find_path(cu->u->type, cu->u->pos, [&](xy pos, xy npos) {
				double travel_d = diag_distance(npos - cu->u->pos);
				if (travel_d >= 32 * 5) return false;
				if (diag_distance(danger_pos - npos) > danger_range) {
					double spot_d = is_in_danger ? travel_d : diag_distance(spot_target->pos - npos);
					if (spot_d < closest_dist) {
						closest_pos = npos;
						closest_dist = spot_d;
					}
				}
				return true;
			}, [&](xy pos, xy npos) {
				double d = diag_distance(danger_pos - npos);
				if (d <= danger_range) return danger_range - d;
				return 0.0;
			}, [&](xy pos) {
				//if (diag_distance(pos - cu->u->pos) <= 32 * 2) return false;
				return diag_distance(spot_target->pos - pos) <= cu->u->stats->sight_range;
			});
			//if (!path.empty()) closest_pos = path.back();
			cu->subaction = combat_unit::subaction_move;
			cu->target_pos = closest_pos;
		}
	}

}

void on_groups_updated() {

	if (players::my_player->has_upgrade(upgrade_types::siege_mode)) {
		for (auto&g : groups) {
			a_vector<combat_unit*> tanks;
			for (auto*c : g.allies) {
				if (c->u->type == unit_types::siege_tank_tank_mode || c->u->type == unit_types::siege_tank_siege_mode) tanks.push_back(c);
			}
			if (tanks.empty()) continue;
			bool siege_this = false;
			int siege_tank_count = 0;
			a_vector<unit*> siege_targets;
			for (unit*e : g.enemies) {
				if (e->is_flying) continue;
				bool add = e->type == unit_types::bunker;
				add |= e->type == unit_types::missile_turret;
				add |= e->type->is_building;
				if (add) {
					siege_targets.push_back(e);
					siege_this = true;
				}
				if (e->type == unit_types::siege_tank_siege_mode) {
					siege_targets.push_back(e);
					++siege_tank_count;
				}
				if (siege_tank_count >= 4) siege_this = true;
			}
			if (siege_this) {
				tactic_siege*t = nullptr;
				for (unit*e : siege_targets) {
					t = siege_target_map[e];
					if (t) break;
				}
				if (!t) {
					siege_tactics.emplace_back();
					all_tactics.push_back(&siege_tactics.back());
					t = &siege_tactics.back();
				}
				for (auto*c : tanks) t->allies.push_back(c);
				for (unit*e : siege_targets) {
					auto*&ptr = siege_target_map[e];
					if (!ptr) {
						ptr = t;
						t->targets.push_back(e);
					}
				}
			}
		}
		for (auto&g : groups) {
			auto*best_spotter = get_best_score(g.allies, spotter_score);
			if (best_spotter) {
				for (unit*e : g.enemies) {
					auto*st = siege_target_map[e];
					if (st) {
						if (st->spotter) {
							double myscore = spotter_score(best_spotter);
							double nscore = spotter_score(st->spotter);
							if (myscore < nscore) {
								st->allies.push_back(best_spotter);
								st->spotter = best_spotter;
								break;
							}
						}
					}
				}
			}
		}
	}

}


void run() {

	for (auto i = siege_tactics.begin(); i != siege_tactics.end();) {
		if (i->allies.empty() || i->targets.empty()) {
			find_and_erase(all_tactics, &*i);
			for (auto*c : i->allies) c->action = combat_unit::action_idle;
			for (unit*e : i->targets) siege_target_map[e] = nullptr;
			i = siege_tactics.erase(i);
		} else ++i;
	}

	for (auto*t : all_tactics) {
		t->enemies.clear();
		for (auto*e : enemy_units) {
			if (e->type->is_non_usable) continue;
			if (e->gone) continue;
			double d = get_best_score_value(t->allies, [&](combat_unit*c) {
				return diag_distance(c->u->pos - e->pos);
			});
			if (d <= 32 * 15) t->enemies.push_back(e);
		}
	}

	for (auto&t : siege_tactics) {
		run_siege_tactic(t);
	}

}


void render() {

	for (auto&t : siege_tactics) {
		if (t.allies.empty()) continue;
		BWAPI::Color c(0, 128, 32);
		xy pos = t.allies.front()->u->pos;
		for (auto*a : t.allies) {
			game->drawLineMap(a->u->pos.x, a->u->pos.y, pos.x, pos.y, c);
		}
		for (unit*e : t.targets) {
			game->drawLineMap(e->pos.x, e->pos.y, pos.x, pos.y, c);
		}
	}

}

void init() {

	render::add(render);

}

}
