
struct strat_tvp_opening {

	wall_in::wall_builder wall;

	void run() {

		combat::no_aggressive_groups = true;

		using namespace buildpred;

		get_upgrades::set_upgrade_value(upgrade_types::terran_infantry_weapons_1, 2000.0);
		get_upgrades::set_upgrade_value(upgrade_types::terran_infantry_armor_1, 2000.0);

		get_upgrades::set_upgrade_value(upgrade_types::ion_thrusters, 1000.0);

		get_upgrades::set_upgrade_value(upgrade_types::u_238_shells, -1.0);
		get_upgrades::set_upgrade_value(upgrade_types::stim_packs, 200.0);

		get_upgrades::set_no_auto_upgrades(true);

		resource_gathering::max_gas = 150;
		bool wall_calculated = false;
		bool has_wall = false;
		bool is_scared = false;
		int scared_until = 0;
		while (true) {

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
			update_possible_start_locations();
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
			
			if (enemy_dt_count) combat::aggressive_vultures = false;

			int my_siege_tank_count = my_units_of_type[unit_types::siege_tank_tank_mode].size() + my_units_of_type[unit_types::siege_tank_siege_mode].size();
			bool need_missile_turret = enemy_zealot_count + enemy_dragoon_count < 8 && my_siege_tank_count >= 1 && my_units_of_type[unit_types::missile_turret].empty();
			if (enemy_forge_count && my_siege_tank_count<2) need_missile_turret = false;
			if (enemy_cannon_count > 1 && my_siege_tank_count < 5) need_missile_turret = false;
			if (attacking_zealot_count + attacking_dragoon_count && enemy_dt_count == 0) need_missile_turret = false;

			if (attacking_zealot_count + attacking_dragoon_count >= 4 && my_siege_tank_count == 0) {
				if (!scouting::all_scouts.empty()) scouting::rm_scout(scouting::all_scouts.front().scout_unit);
			}

			if (attacking_zealot_count >= 5 || (enemy_zealot_count >= 3 || attacking_zealot_count >= 2 && my_completed_units_of_type[unit_types::bunker].size() < 2)) {
				if (my_completed_units_of_type[unit_types::vulture].size() < 2 && my_completed_units_of_type[unit_types::siege_tank_tank_mode].empty()) {
					if (enemy_zealot_count > (int)my_completed_units_of_type[unit_types::marine].size() || my_completed_units_of_type[unit_types::bunker].size() <= 2) {
						if (my_completed_units_of_type[unit_types::marine].size() < 10) {
							scared_until = current_frame + 15 * 30;
							is_scared = true;
							combat::force_defence_is_scared_until = current_frame + 15 * 10;
							get_upgrades::set_upgrade_value(upgrade_types::u_238_shells, 2000.0);
						}
					}
				}
			}
			if (current_frame >= scared_until) {
				is_scared = false;
				combat::force_defence_is_scared_until = 0;
				get_upgrades::set_upgrade_value(upgrade_types::u_238_shells, -1.0);
			}

			if (!my_completed_units_of_type[unit_types::factory].empty()) {
				resource_gathering::max_gas = 300;
			}

			if (is_scared) {
				for (auto&b : build::build_tasks) {
					if (b.type->unit == unit_types::academy) {
						build::cancel_build_task(&b);
						break;
					}
				}
				for (unit*u : my_units_of_type[unit_types::academy]) {
					if (u->game_unit->getUpgrade() == BWAPI::UpgradeTypes::U_238_Shells) {
						u->game_unit->cancelUpgrade();
						break;
					}
				}
				if (my_units_of_type[unit_types::cc].size() == 2) {
					unit*cc = my_units_of_type[unit_types::cc][1];
					if (!cc->addon || enemy_dt_count == 0) {
						cc->controller->action = unit_controller::action_building_movement;
						cc->controller->lift = true;
						cc->controller->target_pos = cc->pos;
						cc->controller->timeout = current_frame + 15 * 30;
						if (cc->remaining_train_time) cc->game_unit->cancelTrain();
					}
				}
			}

			auto build = [&](state&st) {
				st.dont_build_refineries = count_units_plus_production(st, unit_types::marine) < 5;

				if (is_scared) {
					return nodelay(st, unit_types::marine, [&](state&st) {
						return nodelay(st, unit_types::scv, [&](state&st) {
							return maxprod1(st, unit_types::vulture);
						});
					});
				}

				if (my_completed_units_of_type[unit_types::factory].empty()) {
					int scv_count = count_units_plus_production(st, unit_types::scv);
					if (count_units_plus_production(st, unit_types::cc) < 2) {
						if (scv_count >= 14 && count_units_plus_production(st, unit_types::barracks) == 0) return depbuild(st, state(st), unit_types::barracks);
						return depbuild(st, state(st), unit_types::scv);
					}

					if (scv_count >= 16) {
						if (count_units_plus_production(st, unit_types::refinery) == 0) return depbuild(st, state(st), unit_types::refinery);
						if (count_units_plus_production(st, unit_types::academy) == 0) return depbuild(st, state(st), unit_types::academy);
						int marine_count = count_units_plus_production(st, unit_types::marine);
						if (marine_count >= 8 && count_units_plus_production(st, unit_types::barracks) == 1) return depbuild(st, state(st), unit_types::barracks);
						if (attacking_zealot_count + attacking_dragoon_count < 3 || marine_count >= 4 || enemy_citadel_of_adun_count + enemy_templar_archives_count) {
							bool is_researching_range = false;
							for (unit*u : my_units_of_type[unit_types::academy]) {
								if (u->game_unit->getUpgrade() == BWAPI::UpgradeTypes::U_238_Shells) {
									is_researching_range = true;
									break;
								}
							}
							if (is_researching_range || players::my_player->has_upgrade(upgrade_types::u_238_shells)) {
								int comsats = 0;
								for (auto&v : st.units[unit_types::cc]) {
									if (v.has_addon) ++comsats;
								}
								if (comsats < (enemy_dt_count ? 2 : 1)) return depbuild(st, state(st), unit_types::comsat_station);
							}
						}
					}
					if (my_completed_units_of_type[unit_types::bunker].empty() && count_units_plus_production(st, unit_types::marine) < 6) {
						return nodelay(st, unit_types::marine, [&](state&st) {
							return depbuild(st, state(st), unit_types::scv);
						});
					}
					if (scv_count >= 20 || (scv_count >= 17 && my_units_of_type[unit_types::academy].empty())) {
						int min_marines = 8;
						if (attacking_zealot_count + attacking_dragoon_count) min_marines = 12;
						if (count_units_plus_production(st, unit_types::marine) < min_marines) {
							return nodelay(st, unit_types::marine, [&](state&st) {
								return nodelay(st, unit_types::scv, [&](state&st) {
									if (attacking_zealot_count > attacking_dragoon_count) return depbuild(st, state(st), unit_types::vulture);
									return depbuild(st, state(st), unit_types::siege_tank_tank_mode);
								});
							});
						}
					}
				}
				return nodelay(st, unit_types::scv, [&](state&st) {
					if (count_units_plus_production(st, unit_types::marine) < 4) {
						return depbuild(st, state(st), unit_types::marine);
					}
					auto backbone = [&](state&st) {
						return maxprod(st, unit_types::siege_tank_tank_mode, [&](state&st) {
							return nodelay(st, unit_types::firebat, [&](state&st) {
								return depbuild(st, state(st), unit_types::marine);
							});
						});
					};
					int siege_tank_count = count_units_plus_production(st, unit_types::siege_tank_tank_mode) + count_units_plus_production(st, unit_types::siege_tank_siege_mode);
					int marine_count = count_units_plus_production(st, unit_types::marine);
					int vulture_count = count_units_plus_production(st, unit_types::vulture);

					if (marine_count > 8 && !st.units[unit_types::academy].empty() && count_units_plus_production(st, unit_types::medic) < (int)st.units[unit_types::barracks].size()) {
						return depbuild(st, state(st), unit_types::medic);
					}

					if ((siege_tank_count == 2 || current_minerals >= 300) && count_units_plus_production(st, unit_types::factory) == 1) {
						return depbuild(st, state(st), unit_types::factory);
					}
					if (siege_tank_count < 2) {
						return nodelay(st, unit_types::siege_tank_tank_mode, [&](state&st) {
							return depbuild(st, state(st), unit_types::marine);
						});
					}
					if (siege_tank_count) {
						if (vulture_count < enemy_dt_count / 2 || enemy_zealot_count > vulture_count || (siege_tank_count >= enemy_dragoon_count / 2 && vulture_count < 6)) {
							return depbuild(st, state(st), unit_types::vulture);
						}
					}
					if (!my_units_of_type[unit_types::factory].empty() && marine_count < 3) {
						return nodelay(st, unit_types::marine, backbone);
					}
					return backbone(st);
				});
			};

			if (!my_units_of_type[unit_types::siege_tank_tank_mode].empty()) {
				get_upgrades::set_upgrade_value(upgrade_types::siege_mode, -1.0);
				if (players::my_player->has_upgrade(upgrade_types::siege_mode)) get_upgrades::set_upgrade_value(upgrade_types::spider_mines, -1.0);
			}
			bool expand = false;
			int siege_tank_count = my_completed_units_of_type[unit_types::siege_tank_tank_mode].size() + my_completed_units_of_type[unit_types::siege_tank_siege_mode].size();
			int marine_count = my_completed_units_of_type[unit_types::marine].size();
			int vulture_count = my_completed_units_of_type[unit_types::vulture].size();
			auto my_st = get_my_current_state();
			bool has_bunker = !my_units_of_type[unit_types::bunker].empty();
			if (my_st.bases.size() > 1 && siege_tank_count >= 2 && (has_bunker || siege_tank_count >= 3) && players::my_player->has_upgrade(upgrade_types::siege_mode)) break;
			if (my_st.bases.size() > 1 && marine_count >= 1) combat::build_bunker_count = 2;
			if (my_st.bases.size() == 1 && current_used_total_supply >= 14) {
				if (siege_tank_count * 3 + vulture_count >= 9) {
					expand = true;
				}
				for (unit*u : enemy_units) {
					if (!u->type->is_resource_depot) continue;
					bool is_expo = true;
					for (xy p : start_locations) {
						if (u->building->build_pos == p) {
							is_expo = false;
							break;
						}
					}
					if (is_expo) expand = true;
				}
				if (need_missile_turret) expand = false;
				if (!players::my_player->has_upgrade(upgrade_types::siege_mode)) expand = false;
				expand = true;
				if (is_scared) expand = false;
				if (my_units_of_type[unit_types::cc].size() >= 2) expand = false;
			}
			if (!has_wall && !my_units_of_type[unit_types::factory].empty() || is_scared) {
				combat::build_bunker_count = 2;
				if (get_op_current_state().bases.size() <= 1 || enemy_zealot_count + enemy_dragoon_count >= 10) combat::build_bunker_count = 4;
				if (siege_tank_count >= 1 && enemy_cannon_count < 5) combat::build_bunker_count = 4;
				if (marine_count >= 5) combat::build_bunker_count = 4;
				if (is_scared && marine_count >= 2) combat::build_bunker_count = 4;
				if (is_scared && marine_count >= 6) combat::build_bunker_count = 6;
			}
			if (!is_scared) {
				bool is_researching_range = false;
				for (unit*u : my_units_of_type[unit_types::academy]) {
					if (u->game_unit->getUpgrade() == BWAPI::UpgradeTypes::U_238_Shells) {
						is_researching_range = true;
						break;
					}
				}
				if (!is_researching_range && !players::my_player->has_upgrade(upgrade_types::u_238_shells)) {
					combat::build_bunker_count = 0;
					if (!my_units_of_type[unit_types::academy].empty()) combat::build_bunker_count = 1;
				}
			}

			execute_build(expand, build);

// 			if (my_st.bases.size() == 2 && combat::defence_choke.center != xy() && !wall_calculated) {
// 				wall_calculated = true;
// 
// 				wall.spot = wall_in::find_wall_spot_from_to(unit_types::zealot, combat::my_closest_base, combat::defence_choke.outside, false);
// 				wall.spot.outside = combat::defence_choke.outside;
// 
// 				wall.against(unit_types::zealot);
// 				wall.add_building(unit_types::supply_depot);
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

