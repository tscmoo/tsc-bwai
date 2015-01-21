
struct strat_tvz {


	void run() {

		combat::no_aggressive_groups = true;
		combat::defensive_spider_mine_count = 20;

		get_upgrades::set_upgrade_value(upgrade_types::terran_vehicle_weapons_1, 2000.0);
		get_upgrades::set_upgrade_value(upgrade_types::terran_vehicle_plating_1, 500.0);
		//get_upgrades::set_upgrade_value(upgrade_types::terran_infantry_armor_1, 100.0);
		get_upgrades::set_upgrade_value(upgrade_types::terran_infantry_armor_1, 2000.0);
		get_upgrades::set_upgrade_value(upgrade_types::terran_infantry_weapons_1, 2000.0);
		get_upgrades::set_upgrade_value(upgrade_types::ion_thrusters, 300.0);
		get_upgrades::set_upgrade_value(upgrade_types::spider_mines, 200.0);
		get_upgrades::set_upgrade_value(upgrade_types::siege_mode, 1.0);
		get_upgrades::no_auto_upgrades = true;

		bool maxed_out_aggression = false;
		bool has_built_control_tower = false;
		bool has_built_bunker = false;
		while (true) {

			using namespace buildpred;

			int my_vulture_count = my_units_of_type[unit_types::vulture].size();
			int my_tank_count = my_units_of_type[unit_types::siege_tank_tank_mode].size() + my_units_of_type[unit_types::siege_tank_siege_mode].size();
			int my_goliath_count = my_units_of_type[unit_types::goliath].size();
			int my_marine_count = my_units_of_type[unit_types::marine].size();
			int my_firebat_count = my_units_of_type[unit_types::firebat].size();
			int my_science_vessel_count = my_units_of_type[unit_types::science_vessel].size();
			int my_wraith_count = my_units_of_type[unit_types::wraith].size();
			int my_valkyrie_count = my_units_of_type[unit_types::valkyrie].size();

			int enemy_zergling_count = 0;
			int enemy_mutalisk_count = 0;
			int enemy_guardian_count = 0;
			int enemy_lurker_count = 0;
			int enemy_hydralisk_count = 0;
			int enemy_hydralisk_den_count = 0;
			int enemy_lair_count = 0;
			int enemy_spire_count = 0;
			int enemy_spore_count = 0;
			int enemy_hatch_count = 0;
			for (unit*e : enemy_units) {
				if (e->type == unit_types::zergling) ++enemy_zergling_count;
				if (e->type == unit_types::mutalisk) ++enemy_mutalisk_count;
				if (e->type == unit_types::guardian || e->type == unit_types::cocoon) ++enemy_guardian_count;
				if (e->type == unit_types::lurker || e->type == unit_types::lurker_egg) ++enemy_lurker_count;
				if (e->type == unit_types::hydralisk) ++enemy_hydralisk_count;
				if (e->type == unit_types::hydralisk_den) ++enemy_hydralisk_den_count;
				if (e->type == unit_types::lair) ++enemy_lair_count;
				if (e->type == unit_types::spire || e->type==unit_types::greater_spire) ++enemy_spire_count;
				if (e->type == unit_types::spore_colony) ++enemy_spore_count;
				if (e->type == unit_types::hatchery || e->type == unit_types::lair || e->type == unit_types::hive) ++enemy_hatch_count;
			}
			if (enemy_hydralisk_count && !enemy_hydralisk_den_count) ++enemy_hydralisk_den_count;
			if (enemy_mutalisk_count && !enemy_spire_count) ++enemy_spire_count;

			if (my_tank_count + my_goliath_count / 2 + my_marine_count / 3 >= 18) combat::no_aggressive_groups = false;
			if (my_tank_count + my_goliath_count / 2 + my_marine_count / 3 < 6) combat::no_aggressive_groups = true;
			if (enemy_lurker_count >= my_tank_count && my_science_vessel_count == 0) combat::no_aggressive_groups = true;
			if (enemy_lurker_count >= 4 && my_tank_count < 4) combat::no_aggressive_groups = true;

			if (current_used_total_supply >= 190.0) maxed_out_aggression = true;
			if (maxed_out_aggression) {
				combat::no_aggressive_groups = false;
				if (current_used_total_supply < 100.0) maxed_out_aggression = false;
			}

			bool aggressive_vultures = true;
			if (enemy_hydralisk_den_count && !players::my_player->has_upgrade(upgrade_types::spider_mines)) aggressive_vultures = false;
			if (current_used_total_supply >= 100) aggressive_vultures = true;
			if (aggressive_vultures != combat::aggressive_vultures) combat::aggressive_vultures = aggressive_vultures;

			if (!has_built_control_tower && !my_units_of_type[unit_types::control_tower].empty()) {
				has_built_control_tower = true;
			}

			if (enemy_spire_count + enemy_mutalisk_count) {
				combat::build_missile_turret_count = 2;
			}

			//if (my_tank_count < 2 && enemy_mutalisk_count == 0 && enemy_spire_count == 0) {
			if (enemy_mutalisk_count == 0 && enemy_hydralisk_count == 0 && enemy_spore_count == 0) {
				//if ((enemy_hydralisk_den_count != 0) != (enemy_lair_count != 0)) {
				//if (enemy_hydralisk_den_count == 0 || enemy_lair_count == 0) {
				if (true) {
					// TODO: instead of doing this, the wraith should be added as a scout with a mission to scout buildings,
					//       or something like that
					unit*scout_building = get_best_score(enemy_buildings, [&](unit*u) {
						int age = current_frame - u->last_seen;
						if (age < 15 * 60) return std::numeric_limits<double>::infinity();
						return (double)age;
					}, std::numeric_limits<double>::infinity());
					xy scout_pos;
					if (scout_building) scout_pos = scout_building->pos;
					else {
						update_possible_start_locations();
						for (xy pos : possible_start_locations) {
							int age = current_frame - grid::get_build_square(pos).last_seen;
							if (age < 15 * 60) continue;
							scout_pos = pos;
							break;
						}
					}
					int time = 15 * 8;
// 					if (scout_pos == xy()) {
// 						unit*wraith = nullptr;
// 						for (auto*c : combat::live_combat_units) {
// 							if (c->u->type == unit_types::wraith) {
// 								wraith = c->u;
// 								break;
// 							}
// 						}
// 						if (wraith) {
// 							unit*overlord = get_best_score(enemy_units, [&](unit*e) {
// 								if (e->gone) return std::numeric_limits<double>::infinity();
// 								if (e->type != unit_types::overlord) return std::numeric_limits<double>::infinity();
// 								return diag_distance(e->pos - wraith->pos);
// 							}, std::numeric_limits<double>::infinity());
// 							if (overlord) {
// 								if (units_distance(wraith, overlord) > 32 * 8) {
// 									scout_pos = overlord->pos;
// 									time = 15 * 2;
// 								}
// 							}
// 						}
// 					}
					if (scout_pos != xy()) {
						for (auto*c : combat::live_combat_units) {
							if (c->u->type == unit_types::wraith) {
								c->strategy_busy_until = current_frame + time;
								c->action = combat::combat_unit::action_offence;
								c->subaction = combat::combat_unit::subaction_move;
								c->target_pos = scout_pos;
								break;
							}
						}
					}
				}
			}

			bool no_auto_upgrades = get_upgrades::no_auto_upgrades;

			if (enemy_hydralisk_den_count || (current_used_total_supply >= 60 && enemy_spire_count == 0)) get_upgrades::set_upgrade_value(upgrade_types::spider_mines, -1.0);
			else get_upgrades::set_upgrade_value(upgrade_types::spider_mines, 200.0);

			bool lurkers_are_coming = my_tank_count <= 4 && enemy_mutalisk_count == 0 && (enemy_lurker_count || (enemy_hydralisk_den_count && enemy_lair_count));
			if (lurkers_are_coming) {
				log("lurkers are coming!\n");
				scouting::comsat_supply = 70.0;
				if (!get_upgrades::no_auto_upgrades) no_auto_upgrades = true;
// 				get_upgrades::set_upgrade_value(upgrade_types::siege_mode, -1.0);
// 				get_upgrades::set_upgrade_value(upgrade_types::spider_mines, 200.0);
				get_upgrades::set_upgrade_value(upgrade_types::siege_mode, 1.0);
				get_upgrades::set_upgrade_value(upgrade_types::spider_mines, -1.0);
				get_upgrades::set_upgrade_value(upgrade_types::ion_thrusters, 300.0);
				if (players::my_player->has_upgrade(upgrade_types::spider_mines)) get_upgrades::set_upgrade_value(upgrade_types::siege_mode, -1);
			} else {
				scouting::comsat_supply = 60.0;
				if (my_tank_count >= 2 && get_upgrades::no_auto_upgrades) no_auto_upgrades = false;
				if (my_tank_count >= 1) get_upgrades::set_upgrade_value(upgrade_types::siege_mode, -1);
			}

			if (!has_built_bunker && !my_units_of_type[unit_types::bunker].empty()) {
				has_built_bunker = true;
			}

			//if ((my_tank_count < 2 && enemy_hydralisk_den_count) || (my_goliath_count < 4 && enemy_spire_count)) {
			//if (my_tank_count < 2 && enemy_hydralisk_den_count) {
			if (my_tank_count < 2 && !has_built_bunker) {
				no_auto_upgrades = true;
				combat::build_bunker_count = 1;
			} else combat::build_bunker_count = 0;
			if (current_used_total_supply >= 120) no_auto_upgrades = false;

			if (no_auto_upgrades != get_upgrades::no_auto_upgrades) get_upgrades::set_no_auto_upgrades(no_auto_upgrades);

			if (enemy_spire_count || enemy_mutalisk_count) scouting::comsat_supply = 80.0;

			int desired_science_vessel_count = (enemy_lurker_count + 5) / 6;
			if (desired_science_vessel_count > (my_tank_count + my_goliath_count) / 4) desired_science_vessel_count = (my_tank_count + my_goliath_count) / 4;
			int desired_goliath_count = 2 + enemy_mutalisk_count + enemy_guardian_count * 2;
			if (enemy_spire_count) desired_goliath_count += 2;
			if (enemy_spire_count + enemy_mutalisk_count) desired_goliath_count += 2;
			if (my_tank_count < 2 && enemy_mutalisk_count == 0 && enemy_spire_count == 0) desired_goliath_count = 0;
			int desired_wraith_count = 1 + (my_tank_count + my_goliath_count) / 8;
			if (my_tank_count >= 4 && enemy_mutalisk_count + enemy_hydralisk_count < 6) desired_wraith_count += 2;
			int desired_valkyrie_count = std::min(enemy_mutalisk_count / 4, 12);
			if ((enemy_spire_count || enemy_mutalisk_count) && my_goliath_count + my_wraith_count >= 3) desired_valkyrie_count += 2;
			//if (my_goliath_count < 4) desired_valkyrie_count = 0;
			if (my_tank_count >= 12) desired_valkyrie_count += 3;

			if (my_goliath_count >= 4 && desired_valkyrie_count) {
				get_upgrades::set_upgrade_value(upgrade_types::terran_vehicle_plating_1, -1);
				if (my_goliath_count >= 6) {
					get_upgrades::set_upgrade_value(upgrade_types::terran_vehicle_weapons_1, -1);
					get_upgrades::set_upgrade_value(upgrade_types::terran_ship_plating_1, -1);
					get_upgrades::set_upgrade_value(upgrade_types::terran_ship_weapons_1, -1);
				}
			}

			combat::aggressive_wraiths = enemy_mutalisk_count <= my_wraith_count;
			combat::aggressive_valkyries = enemy_mutalisk_count <= my_valkyrie_count * 4 && my_valkyrie_count >= 4;

			if (lurkers_are_coming) {
				desired_goliath_count = 0;
				desired_wraith_count = 0;
				desired_valkyrie_count = 0;
			}

			if (desired_goliath_count - my_goliath_count >= 4 || (enemy_spire_count && my_goliath_count < 4)) desired_wraith_count = 0;

			if (desired_goliath_count > 4) get_upgrades::set_upgrade_value(upgrade_types::charon_boosters, -1);
			if (desired_wraith_count > 2) get_upgrades::set_upgrade_value(upgrade_types::cloaking_field, -1.0);

			log("desired_goliath_count is %d, desired_wraith_count is %d, desired_valkyrie_count is %d, desired_science_vessel_count is %d\n", desired_goliath_count, desired_wraith_count, desired_valkyrie_count, desired_science_vessel_count);
			
			auto build = [&](state&st) {
				return nodelay(st, unit_types::scv, [&](state&st) {
					std::function<bool(state&)> army = [&](state&st) {
						if (my_tank_count >= 3 && st.gas >= 200) {
							// This is temporary until I fix addon production
							int machine_shops = count_production(st, unit_types::machine_shop);
							for (auto&v : st.units[unit_types::factory]) {
								if (v.has_addon) ++machine_shops;
							}
							if (machine_shops < 2) return depbuild(st, state(st), unit_types::machine_shop);
						}
						return maxprod(st, unit_types::siege_tank_tank_mode, [&](state&st) {
							return maxprod1(st, unit_types::vulture);
						});
					};

					int marine_count = count_units_plus_production(st, unit_types::marine);
					int vulture_count = count_units_plus_production(st, unit_types::vulture);
					int goliath_count = count_units_plus_production(st, unit_types::goliath);
					int tank_count = count_units_plus_production(st, unit_types::siege_tank_tank_mode) + count_units_plus_production(st, unit_types::siege_tank_siege_mode);
					if (enemy_mutalisk_count >= 6) {
						if (goliath_count < vulture_count * 2) {
							army = [army](state&st) {
								return nodelay(st, unit_types::goliath, army);
							};
						}
					}
					if (vulture_count < tank_count * 2) {
						army = [army](state&st) {
							return nodelay(st, unit_types::vulture, army);
						};
					}

					int wraith_count = count_units_plus_production(st, unit_types::wraith);
					if (wraith_count < desired_wraith_count) {
						army = [army](state&st) {
							return nodelay(st, unit_types::wraith, army);
						};
					}
					if (goliath_count < desired_goliath_count) {
						army = [army](state&st) {
							return maxprod(st, unit_types::goliath, army);
						};
					}
					if (goliath_count >= 4) {
						int valkyrie_count = count_units_plus_production(st, unit_types::valkyrie);
						if (valkyrie_count < desired_valkyrie_count) {
							if (desired_valkyrie_count - valkyrie_count >= 3 && (my_goliath_count >= 7 || my_valkyrie_count >= 3)) {
								int control_towers = count_production(st, unit_types::control_tower);
								for (auto&v : st.units[unit_types::starport]) {
									if (v.has_addon) ++control_towers;
								}
								if (control_towers < 2) {
									army = [army](state&st) {
										return depbuild(st, state(st), unit_types::control_tower);
									};
								}
							}
							army = [army](state&st) {
								return nodelay(st, unit_types::valkyrie, army);
							};
						}
					}
					if (lurkers_are_coming) {
						if (tank_count < 2) {
							if (vulture_count < 4) {
								army = [army](state&st) {
									return nodelay(st, unit_types::vulture, army);
								};
							}
							army = [army](state&st) {
								return nodelay(st, unit_types::siege_tank_tank_mode, army);
							};
						}
					}
					int science_vessel_count = count_units_plus_production(st, unit_types::science_vessel);
					if (science_vessel_count < desired_science_vessel_count) {
						army = [army](state&st) {
							return nodelay(st, unit_types::science_vessel, army);
						};
					}

// 					if (!has_built_control_tower && !st.units[unit_types::starport].empty()) {
// 						int control_towers = count_production(st, unit_types::control_tower);
// 						for (auto&v : st.units[unit_types::starport]) {
// 							if (v.has_addon) ++control_towers;
// 						}
// 						if (control_towers == 0) {
// 							army = [army](state&st) {
// 								return depbuild(st, state(st), unit_types::control_tower);
// 							};
// 						}
// 					}

					if (tank_count < goliath_count / 8 + vulture_count / 12) {
						army = [army](state&st) {
							return nodelay(st, unit_types::siege_tank_tank_mode, army);
						};
					}

					if (goliath_count >= 5) {
						if (count_units_plus_production(st, unit_types::armory) < 2) {
							army = [army](state&st) {
								return nodelay(st, unit_types::armory, army);
							};
						}
					}

					if (lurkers_are_coming) {
						if (count_units_plus_production(st, unit_types::missile_turret) < 2) {
							army = [army](state&st) {
								return nodelay(st, unit_types::missile_turret, army);
							};
						}
					}

					int machine_shops = count_production(st, unit_types::machine_shop);
					for (auto&v : st.units[unit_types::factory]) {
						if (v.has_addon) ++machine_shops;
					}
					if (machine_shops == 0) {
						army = [army](state&st) {
							return nodelay(st, unit_types::machine_shop, army);
						};
					}

					if (vulture_count <= enemy_zergling_count / 5) {
						army = [army](state&st) {
							return nodelay(st, unit_types::vulture, army);
						};
					}

					if (enemy_zergling_count < 8 && enemy_hydralisk_den_count == 0) {
						int goliath_count = count_units_plus_production(st, unit_types::goliath);
						if (goliath_count < 2) {
							army = [army](state&st) {
								return nodelay(st, unit_types::goliath, army);
							};
						}
					}

					return army(st);
				});
			};

			auto long_distance_miners = [&]() {
				int count = 0;
				for (auto&g : resource_gathering::live_gatherers) {
					if (!g.resource) continue;
					unit*ru = g.resource->u;
					resource_spots::spot*rs = nullptr;
					for (auto&s : resource_spots::spots) {
						if (grid::get_build_square(s.cc_build_pos).building) continue;
						for (auto&r : s.resources) {
							if (r.u == ru) {
								rs = &s;
								break;
							}
						}
						if (rs) break;
					}
					if (rs) ++count;
				}
				return count;
			};
			auto can_expand = [&]() {
				if (buildpred::get_my_current_state().bases.size() == 1) return true;
				if (lurkers_are_coming) return false;
				//if (buildpred::get_my_current_state().bases.size() == 1 && my_tank_count >= 5) return true;
				if (long_distance_miners() >= 4) return true;
				if (buildpred::get_my_current_state().bases.size() == 2 && combat::no_aggressive_groups) return false;
				return long_distance_miners() >= 8;
			};
			log("there are %d long distance miners\n", long_distance_miners());

			execute_build(can_expand(), build);

			multitasking::sleep(15 * 5);
		}

	}

};

