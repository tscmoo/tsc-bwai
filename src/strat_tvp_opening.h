
struct strat_tvp_opening {

	wall_in::wall_builder wall;

	void run() {

		combat::no_aggressive_groups = true;
		combat::defensive_spider_mine_count = 10;

		using namespace buildpred;

		get_upgrades::set_upgrade_value(upgrade_types::siege_mode, -1);

		get_upgrades::set_no_auto_upgrades(true);

		resource_gathering::max_gas = 100;
		bool wall_calculated = false;
		bool has_wall = false;
		unit*scout_unit = nullptr;
		size_t scout_index = 0;
		bool opponent_has_fast_expanded = false;
		size_t previous_base_count = 1;
		while (true) {

			update_possible_start_locations();
			if (possible_start_locations.size() == 1 && current_used_total_supply < 20 && !opponent_has_fast_expanded) {
				xy op_start_loc = possible_start_locations.front();
				a_vector<std::tuple<double, xy>> possible_expos;
				for (auto&s : resource_spots::spots) {
					if (s.cc_build_pos == op_start_loc) continue;
					auto&bs = grid::get_build_square(s.cc_build_pos);
					if (bs.building) continue;
					possible_expos.emplace_back(unit_pathing_distance(unit_types::probe, op_start_loc, s.cc_build_pos), s.cc_build_pos);
				}
				log("possible_expos.size() is %d\n", possible_expos.size());
				std::sort(possible_expos.begin(), possible_expos.end());
				if (possible_expos.size() > 2) possible_expos.resize(2);
				if (!possible_expos.empty()) {
					if (scout_unit == nullptr) {
						auto*s = get_best_score_p(scouting::all_scouts, [&](scouting::scout*s) {
							if (!s->scout_unit) return std::numeric_limits<double>::infinity();
							return diag_distance(std::get<1>(possible_expos.front()) - s->scout_unit->pos);
						});
						if (s && s->scout_unit) {
							scout_unit = s->scout_unit;
							scouting::rm_scout(scout_unit);
							log("taking over scout unit %p\n", scout_unit);
						}
					}
					if (scout_unit) {
						for (auto&s : scouting::all_scouts) {
							if (s.type == scouting::scout::type_default || opponent_has_fast_expanded) scouting::rm_scout(scouting::all_scouts.front().scout_unit);
						}
						combat::combat_unit*cu = nullptr;
						for (auto*a : combat::live_combat_units) {
							if (a->u == scout_unit) {
								cu = a;
								break;
							}
						}
						log("cu is %p\n", cu);
						if (cu) {
							if (scout_index >= possible_expos.size()) scout_index = 0;
							cu->strategy_busy_until = current_frame + 15 * 10;
							cu->action = combat::combat_unit::action_offence;
							cu->subaction = combat::combat_unit::subaction_move;
							cu->target_pos = std::get<1>(possible_expos[scout_index]) + xy(4 * 16, 3 * 16);
							if (grid::get_build_square(cu->target_pos).visible) ++scout_index;
						}
					}
				}
			}

			if (current_used_total_supply < 20 && !opponent_has_fast_expanded) {
				bool has_expanded = false;
				for (auto&s : resource_spots::spots) {
					bool is_start_location = false;
					for (xy pos : start_locations) {
						if (diag_distance(pos - s.cc_build_pos) <= 32 * 10) {
							is_start_location = true;
							break;
						}
					}
					if (is_start_location) continue;
					auto&bs = grid::get_build_square(s.cc_build_pos);
					if (bs.building && bs.building->owner == players::opponent_player) has_expanded = true;
				}
				log("has_expanded is %d\n", has_expanded);
				if (has_expanded) {
					opponent_has_fast_expanded = true;
				}
			}

			int my_vulture_count = my_units_of_type[unit_types::vulture].size();
			int enemy_zealot_count = 0;
			int enemy_dragoon_count = 0;
			int enemy_dt_count = 0;
			int enemy_forge_count = 0;
			int enemy_cannon_count = 0;
			int enemy_citadel_of_adun_count = 0;
			int enemy_templar_archives_count = 0;
			for (unit*e : enemy_units) {
				if (e->type == unit_types::zealot) ++enemy_zealot_count;
				if (e->type == unit_types::dragoon) ++enemy_dragoon_count;
				if (e->type == unit_types::dark_templar) ++enemy_dt_count;
				if (e->type == unit_types::forge) ++enemy_forge_count;
				if (e->type == unit_types::photon_cannon) ++enemy_cannon_count;
				if (e->type == unit_types::citadel_of_adun) ++enemy_citadel_of_adun_count;
				if (e->type == unit_types::templar_archives) ++enemy_templar_archives_count;
			}

			int attacking_zealot_count = 0;
			int attacking_dragoon_count = 0;
			for (unit*e : enemy_units) {
				if (!e->visible) continue;
				if (e->type == unit_types::zealot || e->type == unit_types::dragoon) {
					double e_d = get_best_score_value(possible_start_locations, [&](xy pos) {
						return unit_pathing_distance(unit_types::scv, e->pos, pos);
					});
					if (unit_pathing_distance(unit_types::scv, e->pos, combat::my_closest_base)*0.33 < e_d) {
						if (e->type == unit_types::zealot) ++attacking_zealot_count;
						if (e->type == unit_types::dragoon) ++attacking_dragoon_count;
					}
				}
			}
			log("attacking_zealot_count is %d, attacking_dragoon_count is %d\n", attacking_zealot_count, attacking_dragoon_count);

			if (current_used_total_supply >= 20 && current_used_total_supply < 30 && !my_units_of_type[unit_types::bunker].empty()) {
				unit*bunker = my_units_of_type[unit_types::bunker].front();

				if (scout_unit) {
					if (scout_unit->dead) scout_unit = nullptr;
				}

				if (attacking_zealot_count + attacking_dragoon_count == 0) {
					if (scouting::all_scouts.empty()) {
						unit*u = get_best_score(my_workers, [&](unit*u) {
							if (u->controller->action != unit_controller::action_gather) return std::numeric_limits<double>::infinity();
							return 0.0;
						}, std::numeric_limits<double>::infinity());
						scouting::add_scout(u);
					}
				} else {
					if (!scouting::all_scouts.empty()) scouting::rm_scout(scouting::all_scouts.front().scout_unit);
					int n = (attacking_zealot_count + attacking_dragoon_count + 1) / 2;
					if (current_used_total_supply < 25) n = (enemy_zealot_count + enemy_dragoon_count + 1) / 2;
					for (unit*u : my_workers) {
						if (u->controller->action == unit_controller::action_repair) --n;
					}
					log("preemptively sending %d scvs to repair bunker\n", n);
					for (auto*a : combat::live_combat_units) {
						if (n <= 0) break;
						if (!a->u->type->is_worker) continue;
						if (a->u->controller->action == unit_controller::action_scout) continue;
						if (a->u->controller->action == unit_controller::action_build) continue;
						a->strategy_busy_until = current_frame + 15 * 30;
						a->action = combat::combat_unit::action_offence;
						a->subaction = combat::combat_unit::subaction_repair;
						a->target = bunker;
						--n;
					}
				}
			}

			
			if (enemy_dt_count) combat::aggressive_vultures = false;

			int my_siege_tank_count = my_units_of_type[unit_types::siege_tank_tank_mode].size() + my_units_of_type[unit_types::siege_tank_siege_mode].size();
			bool need_missile_turret = enemy_zealot_count + enemy_dragoon_count < 8 && !my_units_of_type[unit_types::machine_shop].empty() && my_units_of_type[unit_types::missile_turret].empty() && !opponent_has_fast_expanded;
			if (enemy_forge_count && my_siege_tank_count < 2) need_missile_turret = false;
			if (enemy_cannon_count > 1 && my_siege_tank_count < 5) need_missile_turret = false;
			if (attacking_zealot_count + attacking_dragoon_count > 2 && enemy_dt_count == 0) need_missile_turret = false;
			if (enemy_dt_count || my_siege_tank_count >= 2) need_missile_turret = true;

			double my_lost = players::my_player->minerals_lost + players::my_player->gas_lost;
			double op_lost = players::opponent_player->minerals_lost + players::opponent_player->gas_lost;

			if (attacking_zealot_count + attacking_dragoon_count >= 4 && my_siege_tank_count == 0) {
				if (!scouting::all_scouts.empty()) scouting::rm_scout(scouting::all_scouts.front().scout_unit);
			}

			if (!my_units_of_type[unit_types::factory].empty() || current_gas >= 100) {
				resource_gathering::max_gas = 0;
				if (!my_units_of_type[unit_types::factory].empty() && my_units_of_type[unit_types::factory].front()->remaining_build_time <= 15 * 20) resource_gathering::max_gas = 50;
			}
			if (!my_completed_units_of_type[unit_types::factory].empty()) {
				resource_gathering::max_gas = 300;
			}

			//if (opponent_has_fast_expanded || my_units_of_type[unit_types::siege_tank_tank_mode].size() >= 2) {
			if (players::my_player->has_upgrade(upgrade_types::siege_mode)) {
				get_upgrades::set_upgrade_value(upgrade_types::spider_mines, -1.0);
			}

			if (get_my_current_state().bases.size() > previous_base_count) {
				previous_base_count = get_my_current_state().bases.size();
				combat::defensive_spider_mine_count += 12;
			}

			auto build = [&](state&st) {
				st.dont_build_refineries = true;

				int scv_count = count_units_plus_production(st, unit_types::scv);
				if (scv_count < 12) return depbuild(st, state(st), unit_types::scv);
				if (count_units_plus_production(st, unit_types::barracks) == 0) return depbuild(st, state(st), unit_types::barracks);
				if (count_units_plus_production(st, unit_types::refinery) == 0) return depbuild(st, state(st), unit_types::refinery);
				if (scv_count < 15) return depbuild(st, state(st), unit_types::scv);
				if (count_units_plus_production(st, unit_types::factory) == 0) return depbuild(st, state(st), unit_types::factory);

				int tank_count = count_units_plus_production(st, unit_types::siege_tank_tank_mode) + count_units_plus_production(st, unit_types::siege_tank_siege_mode);
				int vulture_count = count_units_plus_production(st, unit_types::vulture);
				
				int machine_shops = count_production(st, unit_types::machine_shop);
				for (auto&v : st.units[unit_types::factory]) {
					if (v.has_addon) ++machine_shops;
				}
// 				if (st.used_supply[race_terran] >= 28 && count_units_plus_production(st, unit_types::cc) == 1 && machine_shops == 1) return depbuild(st, state(st), unit_types::cc);
// 				if (st.used_supply[race_terran] >= 30 && opponent_has_fast_expanded && count_units_plus_production(st, unit_types::cc) == 2) {
// 					//return depbuild(st, state(st), unit_types::cc);
// 				}

				std::function<bool(state&)> army = [&](state&st) {
					return nodelay(st, unit_types::siege_tank_tank_mode, [&](state&st) {
						if (count_units_plus_production(st, unit_types::marine) < 4) return depbuild(st, state(st), unit_types::marine);
						if (tank_count && count_units_plus_production(st, unit_types::factory) < 2) return depbuild(st, state(st), unit_types::factory);
						return depbuild(st, state(st), unit_types::vulture);
					});
				};

				if ((attacking_zealot_count > 1 && attacking_zealot_count > attacking_dragoon_count) || (opponent_has_fast_expanded && tank_count >= 2)) {
					army = [army](state&st) {
						return nodelay(st, unit_types::vulture, army);
					};
				}
				//if (tank_count >= 2 && vulture_count < tank_count * 2) {
				if (tank_count >= 1 && vulture_count < 8) {
					army = [army](state&st) {
						return nodelay(st, unit_types::vulture, army);
					};
				}

				if (has_wall && count_units_plus_production(st, unit_types::bunker) == 0) {
					army = [army](state&st) {
						return nodelay(st, unit_types::bunker, army);
					};
				}

				if (!opponent_has_fast_expanded && vulture_count < 4 && tank_count >= 1) {
					army = [army](state&st) {
						return nodelay(st, unit_types::scv, army);
					};
					army = [army](state&st) {
						return nodelay(st, unit_types::vulture, army);
					};
				}
				if (tank_count) {
					if (need_missile_turret && count_units_plus_production(st, unit_types::missile_turret) == 0) {
						army = [army](state&st) {
							return nodelay(st, unit_types::missile_turret, army);
						};
					}
				}

				return nodelay(st, unit_types::scv, [&](state&st) {
					return army(st);
				});

			};

			xy natural_cc_pos = my_start_location;
			if (true) {
				auto*s = get_best_score_p(resource_spots::spots, [&](resource_spots::spot*s) {
					if (diag_distance(s->cc_build_pos - my_start_location) <= 32 * 10) return std::numeric_limits<double>::infinity();
					double score = unit_pathing_distance(unit_types::scv, s->cc_build_pos, my_start_location);
					double res = 0;
					bool has_gas = false;
					for (auto&r : s->resources) {
						res += r.u->resources;
						if (r.u->type->is_gas) has_gas = true;
					}
					score -= (has_gas ? res : res / 4) / 10;
					return score;
				}, std::numeric_limits<double>::infinity());
				if (s) natural_cc_pos = s->cc_build_pos;
			}

			unit*free_cc = nullptr;
			for (unit*u : my_completed_units_of_type[unit_types::cc]) {
				bool is_free = true;
				for (auto&s : resource_spots::spots) {
					if (u->building->build_pos == s.cc_build_pos) {
						is_free = false;
					}
				}
				if (!is_free) continue;
				free_cc = u;
				break;
			}
			int my_completed_tank_count = my_completed_units_of_type[unit_types::siege_tank_tank_mode].size() + my_completed_units_of_type[unit_types::siege_tank_siege_mode].size();
			int my_completed_vulture_count = my_completed_units_of_type[unit_types::vulture].size();
			if (free_cc) {
				bool expand = false;
				if (my_completed_tank_count >= 4 || my_completed_vulture_count >= 6) {
					expand = true;
				}
				if (expand) {
					combat::build_bunker_count = 1;

					xy land_pos = natural_cc_pos;
					if (attacking_zealot_count + attacking_dragoon_count > 3) {
						land_pos = my_start_location;
						if (!my_completed_units_of_type[unit_types::siege_tank_tank_mode].empty()) land_pos = my_completed_units_of_type[unit_types::siege_tank_tank_mode].front()->pos;
						if (!my_completed_units_of_type[unit_types::siege_tank_siege_mode].empty()) land_pos = my_completed_units_of_type[unit_types::siege_tank_siege_mode].front()->pos;
					}
					log("land_pos is %d %d\n", land_pos.x, land_pos.y);
					if (land_pos != xy()) {

						free_cc->controller->action = unit_controller::action_building_movement;
						if (free_cc->building->is_lifted) free_cc->controller->lift = false;
						else if (free_cc->building->build_pos != land_pos) free_cc->controller->lift = true;
						free_cc->controller->target_pos = land_pos;
						free_cc->controller->timeout = current_frame + 15 * 30;

						combat::my_closest_base_override = natural_cc_pos;
						combat::my_closest_base_override_until = current_frame + 15 * 20;
					} else break;
				}
			}

			if (get_my_current_state().bases.size() >= 2) {
				combat::build_bunker_count = 1;
				if (current_used_total_supply >= 60) break;
				if (!my_units_of_type[unit_types::bunker].empty() && my_completed_tank_count >= 2) {
					break;
				}
			}

			//if (wall_calculated && !has_wall && !my_units_of_type[unit_types::marine].empty()) {
			if (!my_units_of_type[unit_types::marine].empty()) {
// 				combat::my_closest_base_override = natural_cc_pos;
// 				combat::my_closest_base_override_until = current_frame + 15 * 20;
				combat::build_bunker_count = 1;
			}

			combat::my_closest_base_override = natural_cc_pos;
			combat::my_closest_base_override_until = current_frame + 15 * 20;

			bool expand = false;
			if (my_units_of_type[unit_types::cc].size() == 1) {
				if (opponent_has_fast_expanded) {
					if (current_used_total_supply >= 18) expand = true;
				}
				if (attacking_zealot_count < my_vulture_count + my_siege_tank_count) {
					if (!need_missile_turret || !my_units_of_type[unit_types::engineering_bay].empty()) {
						if (current_used_total_supply >= 24) expand = true;
					}
				}
			}

			execute_build(expand, build);

// 			if (combat::defence_choke.center != xy() && !wall_calculated && combat::my_closest_base == natural_cc_pos) {
// 				wall_calculated = true;
// 
// 				wall.spot = wall_in::find_wall_spot_from_to(unit_types::zealot, combat::my_closest_base, combat::defence_choke.outside, false);
// 				wall.spot.outside = combat::defence_choke.outside;
// 
// 				//wall.against(unit_types::zealot);
// 				//wall.add_building(unit_types::supply_depot);
// 				wall.against(unit_types::dragoon);
// 				wall.add_building(unit_types::bunker);
// 				wall.add_building(unit_types::barracks);
// 				has_wall = true;
// 				if (!wall.find()) {
// 					wall.add_building(unit_types::supply_depot);
// 					if (!wall.find()) {
// 						log("failed to find wall in :(\n");
// 						has_wall = false;
// 					}
// 				}
// 			}
// 			if (has_wall) wall.build();

			multitasking::sleep(15 * 5);
		}
		combat::build_bunker_count = 0;
		resource_gathering::max_gas = 0.0;

		get_upgrades::set_upgrade_value(upgrade_types::ion_thrusters, 0.0);
		get_upgrades::set_no_auto_upgrades(false);

		combat::aggressive_vultures = true;

	}

	void render() {
		wall.render();
	}

};

