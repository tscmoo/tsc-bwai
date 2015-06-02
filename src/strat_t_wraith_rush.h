

struct strat_t_wraith_rush : strat_t_base {

	virtual void init() override {

		default_upgrades = false;

		get_upgrades::set_upgrade_value(upgrade_types::cloaking_field, -1.0);

		combat::hide_wraiths = true;

		sleep_time = 15;

	}
	int max_workers = 0;
	bool has_found_enemy_workers = false;
	bool expand = false;
	xy natural_pos;
	virtual bool tick() override {

		if (opening_state == 0) {
			if (my_resource_depots.size() != 1 || my_workers.size() != 4) opening_state = -1;
			else {
				build::add_build_task(0.0, unit_types::scv);
				build::add_build_task(0.0, unit_types::scv);
				build::add_build_task(0.0, unit_types::scv);
				build::add_build_task(0.0, unit_types::scv);
				build::add_build_task(0.0, unit_types::scv);
				build::add_build_task(1.0, unit_types::supply_depot);
				build::add_build_task(2.0, unit_types::scv);
				build::add_build_task(2.0, unit_types::scv);
				build::add_build_task(3.0, unit_types::barracks);
				build::add_build_task(4.0, unit_types::refinery);
				build::add_build_task(4.5, unit_types::scv);
				build::add_build_task(4.5, unit_types::scv);
				build::add_build_task(4.5, unit_types::scv);
				build::add_build_task(4.5, unit_types::marine);
				build::add_build_task(4.5, unit_types::marine);
				build::add_build_task(5.0, unit_types::factory);
				if (players::opponent_player->race == race_protoss) build::add_build_task(6.0, unit_types::academy);
				++opening_state;
			}
		} else if (opening_state == 1) {
			if (bo_all_done()) {
				opening_state = -1;
			}
		}

		combat::no_aggressive_groups = current_used_total_supply < 120;
		combat::aggressive_groups_done_only = !combat::no_aggressive_groups;

		if (my_completed_units_of_type[unit_types::wraith].size() >= 3) {
			combat::aggressive_wraiths = true;
			combat::hide_wraiths = false;
		}

		if (players::opponent_player->race == race_protoss) {
			get_upgrades::set_upgrade_value(upgrade_types::u_238_shells, -1.0);
		}

		if (combat::aggressive_wraiths && my_units_of_type[unit_types::wraith].size() < 6) {
			xy target;
			for (auto&g : combat::groups) {
				int workers = 0;
				for (auto*e : g.enemies) {
					if (e->type->is_worker) ++workers;
				}
				if (workers >= 5) {
					target = g.enemies.front()->pos;
					if (!has_found_enemy_workers) has_found_enemy_workers = true;
				}
			}
			if (!has_found_enemy_workers) {
				update_possible_start_locations();
				if (!possible_start_locations.empty()) target = possible_start_locations.front();
			}
			for (auto*a : combat::live_combat_units) {
				if (a->u->type == unit_types::wraith) {
					bool in_danger = false;
					for (auto&g : combat::groups) {
						if (g.air_dpf >= 1.0 && g.threat_area.test(grid::build_square_index(a->u->pos))) {
							in_danger = true;
							break;
						}
					}
					if (target != xy() && a->u->hp >= a->u->stats->hp*0.9 && a->u->energy >= 50) {
						if (diag_distance(a->u->pos - target) > 32 * 15) {
							a->strategy_busy_until = current_frame + 15 * 2;
							a->action = combat::combat_unit::action_offence;
							a->subaction = combat::combat_unit::subaction_move;
							a->target_pos = target;
						}
					} else {
						if (a->u->cloaked && a->u->energy <= 5) {
							a->strategy_busy_until = current_frame + 15 * 2;
							a->action = combat::combat_unit::action_offence;
							a->subaction = combat::combat_unit::subaction_move;
							a->target_pos = my_start_location;
						}
					}
					if (players::my_player->has_upgrade(upgrade_types::cloaking_field) && !a->u->cloaked) {
						if (current_frame < a->strategy_busy_until) {
							if (in_danger) {
								if (current_frame - a->last_used_special >= 15) {
									a->u->game_unit->cloak();
									a->last_used_special = current_frame;
								}
								break;
							}
						}
					}
				}
			}
		}

		if (current_used_total_supply >= 120 && get_upgrades::no_auto_upgrades) {
			get_upgrades::set_no_auto_upgrades(false);
		}

		combat::build_bunker_count = 0;
		if (my_units_of_type[unit_types::marine].size() >= 1) {
			if (natural_pos == xy()) {
				natural_pos = get_next_base()->pos;
			}
			combat::build_bunker_count = 1;
			if (combat::aggressive_wraiths) combat::build_bunker_count = 2;
		}
		if (natural_pos != xy()) {
			combat::my_closest_base_override = natural_pos;
			combat::my_closest_base_override_until = current_frame + 15 * 10;
		}

		if (combat::aggressive_wraiths) {
			expand = true;
		}

		max_workers = get_max_mineral_workers();
		
		//bool end = my_completed_units_of_type[unit_types::cc].size() >= 2 || current_used_total_supply >= 50 || current_minerals >= 600;
		bool end = current_used_total_supply >= 70 || current_minerals >= 600;
		if (combat::aggressive_wraiths) {
			for (unit*e : visible_enemy_units) {
				if (!e->is_completed) continue;
				if (e->type->is_detector && e->type != unit_types::overlord) {
					end = true;
					break;
				}
			}
		}
		if (end) {
			combat::hide_wraiths = false;
		}
		return end;
	}
	virtual bool build(buildpred::state&st) override {
		using namespace buildpred;

		auto army = this->army;

		if (players::opponent_player->race != race_terran && wraith_count >= 2) {
			if (count_units_plus_production(st, unit_types::science_facility) == 0) {
				army = [army](state&st) {
					return nodelay(st, unit_types::science_facility, army);
				};
			}
		}

		army = [army](state&st) {
			return nodelay(st, unit_types::marine, army);
		};

		army = [army](state&st) {
			return nodelay(st, unit_types::wraith, army);
		};

		if (count_units_plus_production(st, unit_types::starport) < 2) {
			army = [army](state&st) {
				return nodelay(st, unit_types::starport, army);
			};
		}

		if (players::opponent_player->race == race_protoss) {
			if (count_units_plus_production(st, unit_types::academy) == 0) {
				army = [army](state&st) {
					return nodelay(st, unit_types::academy, army);
				};
			}
		}

		if (control_tower_count < 1) {
			army = [army = army](state&st) {
				return nodelay(st, unit_types::control_tower, army);
			};
		}

		if (players::opponent_player->race != race_terran) {
			if (wraith_count >= 6) {
				if (science_vessel_count == 0) {
					army = [army = army](state&st) {
						return nodelay(st, unit_types::science_vessel, army);
					};
				}
			}
			if (enemy_dt_count + enemy_lurker_count) {
				if (science_vessel_count < wraith_count / 4) {
					army = [army = army](state&st) {
						return nodelay(st, unit_types::science_vessel, army);
					};
				}
			}
		}

		if (marine_count < 4) {
			army = [army](state&st) {
				return nodelay(st, unit_types::marine, army);
			};
		}

		//if (scv_count < max_workers || count_production(st, unit_types::cc)) {
		if (scv_count < 60) {
			army = [army](state&st) {
				return nodelay(st, unit_types::scv, army);
			};
		}

		if (count_units_plus_production(st, unit_types::science_facility)) {
			if (expand && count_production(st, unit_types::cc) == 0 && count_units_plus_production(st, unit_types::cc) < 2) {
				army = [army](state&st) {
					return nodelay(st, unit_types::cc, army);
				};
			}
			if (force_expand && count_production(st, unit_types::cc) == 0) {
				army = [army](state&st) {
					return nodelay(st, unit_types::cc, army);
				};
			}
		}

		return army(st);
	}

};

