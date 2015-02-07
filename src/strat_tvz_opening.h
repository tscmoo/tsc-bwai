
struct strat_tvz_opening {

	wall_in::wall_builder wall;

	void run() {

		combat::no_aggressive_groups = true;
		combat::aggressive_wraiths = false;
		get_upgrades::no_auto_upgrades = true;
		
		get_upgrades::set_upgrade_value(upgrade_types::stim_packs, -1.0);

		get_upgrades::set_upgrade_value(upgrade_types::emp_shockwave, 10000.0);
		get_upgrades::set_upgrade_value(upgrade_types::lockdown, 10000.0);
		get_upgrades::set_upgrade_value(upgrade_types::restoration, 10000.0);
		get_upgrades::set_upgrade_value(upgrade_types::optical_flare, 10000.0);
		get_upgrades::set_upgrade_value(upgrade_types::caduceus_reactor, 10000.0);

		scouting::scout_supply = 11;

		using namespace buildpred;

		resource_gathering::max_gas = 100;
		int wall_calculated = 0;
		bool has_wall = false;
		bool has_zergling_tight_wall = false;
		bool being_zergling_rushed = false;
		bool initial_push = false;
		bool initial_push_done = false;
		double initial_push_my_lost;
		double initial_push_op_lost;
		bool has_built_bunkers = false;
		while (true) {

			int my_marine_count = my_units_of_type[unit_types::marine].size();
			int my_vulture_count = my_units_of_type[unit_types::vulture].size();
			int my_wraith_count = my_units_of_type[unit_types::wraith].size();
			int my_tank_count = my_units_of_type[unit_types::siege_tank_tank_mode].size() + my_units_of_type[unit_types::siege_tank_siege_mode].size();
			int enemy_zergling_count = 0;
			int enemy_spire_count = 0;
			int enemy_mutalisk_count = 0;
			int enemy_hydralisk_count = 0;
			int enemy_spore_count = 0;
			int enemy_lurker_count = 0;
			for (unit*e : enemy_units) {
				if (e->type == unit_types::mutalisk) ++enemy_mutalisk_count;
				if (e->type == unit_types::spire || e->type == unit_types::greater_spire) ++enemy_spire_count;
				if (e->type == unit_types::zergling) ++enemy_zergling_count;
				if (e->type == unit_types::hydralisk) ++enemy_hydralisk_count;
				if (e->type == unit_types::spore_colony) ++enemy_spore_count;
				if (e->type == unit_types::lurker) ++enemy_lurker_count;
			}
			if (enemy_spire_count == 0 && enemy_mutalisk_count) enemy_spire_count = 1;

			if (players::my_player->has_upgrade(upgrade_types::stim_packs)) get_upgrades::set_upgrade_value(upgrade_types::u_238_shells, -1.0);
			if (my_tank_count) get_upgrades::set_upgrade_value(upgrade_types::siege_mode, -1.0);

			if (being_zergling_rushed) {
				get_upgrades::set_upgrade_value(upgrade_types::terran_infantry_weapons_1, 0.0);
				get_upgrades::set_upgrade_value(upgrade_types::terran_infantry_armor_1, 0.0);
			} else {
				get_upgrades::set_upgrade_value(upgrade_types::terran_infantry_weapons_1, -1.0);
				get_upgrades::set_upgrade_value(upgrade_types::terran_infantry_armor_1, -1.0);
				get_upgrades::set_upgrade_value(upgrade_types::terran_infantry_weapons_2, -1.0);
				get_upgrades::set_upgrade_value(upgrade_types::terran_infantry_armor_2, -1.0);
				get_upgrades::set_upgrade_value(upgrade_types::terran_infantry_weapons_3, -1.0);
				get_upgrades::set_upgrade_value(upgrade_types::terran_infantry_armor_3, -1.0);
			}

			if (my_units_of_type[unit_types::engineering_bay].size() >= 2 && (enemy_spire_count == 0 || !my_units_of_type[unit_types::bunker].empty())) {

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

				combat::my_closest_base_override = natural_cc_pos;
				combat::my_closest_base_override_until = current_frame + 15 * 10;

				combat::build_bunker_count = 2;
			}
			if (my_units_of_type[unit_types::bunker].size() >= 2 && !has_built_bunkers) has_built_bunkers = true;
			if (has_built_bunkers) combat::build_bunker_count = 0;

			double op_lost = players::opponent_player->minerals_lost + players::opponent_player->gas_lost;
			double my_lost = players::my_player->minerals_lost + players::my_player->gas_lost;

			if (my_completed_units_of_type[unit_types::cc].size() >= 2 && !initial_push && !initial_push_done) {
				log("initial push begin\n");
				initial_push = true;
				initial_push_my_lost = my_lost;
				initial_push_op_lost = op_lost;
			}
			if (initial_push) {
				double bad = my_lost - initial_push_my_lost;
				double good = op_lost - initial_push_op_lost;

				combat::no_aggressive_groups = false;

				if (bad > 800 && bad - good > 200) {
					log("initial push end (bad %g good %g)\n", bad, good);
					combat::no_aggressive_groups = true;
					initial_push = false;
					initial_push_done = true;
				} else {
					int lose_count = 0;
					int total_count = 0;
					for (auto*a : combat::live_combat_units) {
						if (a->u->type->is_worker) continue;
						++total_count;
						if (current_frame - a->last_fight <= 15 * 10) {
							if (a->last_win_ratio < 1.0) ++lose_count;
						}
					}
					if (lose_count >= total_count / 2) {
						log("initial push end (lose %d total %d)\n", lose_count, total_count);
						combat::no_aggressive_groups = true;
						initial_push = false;
						initial_push_done = true;
					}
				}

				if (enemy_spire_count) combat::build_missile_turret_count = 2;
			}
			if (initial_push_done || current_used_total_supply >= 110) break;

			if (current_used_total_supply >= 100) get_upgrades::no_auto_upgrades = false;

			if (my_marine_count + my_vulture_count * 3 >= 4 || current_used_total_supply >= 45) being_zergling_rushed = false;
			else if (!being_zergling_rushed && enemy_zergling_count > my_marine_count + my_vulture_count * 3) {
				log("waa being zergling rushed!\n");
				being_zergling_rushed = true;
			}

			auto build = [&](state&st) {
				//st.dont_build_refineries = true;
				int marine_count = count_units_plus_production(st, unit_types::marine);
				int vulture_count = count_units_plus_production(st, unit_types::vulture);
				int firebat_count = count_units_plus_production(st, unit_types::firebat);
				bool initial_marine = !st.units[unit_types::barracks].empty() && count_units_plus_production(st, unit_types::marine) == 0;
				if (being_zergling_rushed || initial_marine) {
					if (being_zergling_rushed) {
						if (!scouting::all_scouts.empty()) scouting::rm_scout(scouting::all_scouts.front().scout_unit);
					}
					return nodelay(st, unit_types::marine, [&](state&st) {
						return nodelay(st, unit_types::scv, [&](state&st) {
							return depbuild(st, state(st), unit_types::vulture);
						});
					});
				}
				int scv_count = count_units_plus_production(st, unit_types::scv);
				if (scv_count >= 11 && count_units_plus_production(st, unit_types::barracks) == 0) return depbuild(st, state(st), unit_types::barracks);
				if (scv_count >= 12 && count_units_plus_production(st, unit_types::refinery) == 0) return depbuild(st, state(st), unit_types::refinery);
				if (scv_count >= 16 && count_units_plus_production(st, unit_types::factory) == 0) return depbuild(st, state(st), unit_types::factory);
				if (count_units_plus_production(st, unit_types::factory) == 0) return depbuild(st, state(st), unit_types::scv);
				return nodelay(st, unit_types::scv, [&](state&st) {

					std::function<bool(state&)> army = [&](state&st) {
						return depbuild(st, state(st), unit_types::marine);
					};

					int marine_count = count_units_plus_production(st, unit_types::marine);
					int medic_count = count_units_plus_production(st, unit_types::medic);
					int firebat_count = count_units_plus_production(st, unit_types::firebat);
					int ghost_count = count_units_plus_production(st, unit_types::ghost);
					int wraith_count = count_units_plus_production(st, unit_types::wraith);
					int valkyrie_count = count_units_plus_production(st, unit_types::valkyrie);
					int science_vessel_count = count_units_plus_production(st, unit_types::science_vessel);
					int tank_count = count_units_plus_production(st, unit_types::siege_tank_tank_mode) + count_units_plus_production(st, unit_types::siege_tank_siege_mode);

					if (ghost_count >= 3) {
						army = [army](state&st) {
							return nodelay(st, unit_types::vulture, army);
						};
					}

					army = [army](state&st) {
						return maxprod(st, unit_types::ghost, army);
					};

					if (st.minerals >= 500) {
						army = [army](state&st) {
							return maxprod(st, unit_types::vulture, army);
						};
					}

					if (marine_count + ghost_count >= 8) {
						if (medic_count < 2) {
							army = [army](state&st) {
								return maxprod(st, unit_types::medic, army);
							};
						}
					}
					if (marine_count < 8) {
						army = [army](state&st) {
							return nodelay(st, unit_types::marine, army);
						};
					}

					if (enemy_spire_count || !my_units_of_type[unit_types::science_vessel].empty()) {
						if (count_units_plus_production(st, unit_types::engineering_bay) < 2) {
							army = [army](state&st) {
								return maxprod(st, unit_types::engineering_bay, army);
							};
						}
					} else {
						army = [army](state&st) {
							return maxprod(st, unit_types::vulture, army);
						};
					}

					if (science_vessel_count == 0 && enemy_spire_count == 0) {
						army = [army](state&st) {
							return nodelay(st, unit_types::science_vessel, army);
						};
					}
					if (st.used_supply[race_terran] >= 70 && enemy_lurker_count >= 4 && science_vessel_count < 2) {
						army = [army](state&st) {
							return nodelay(st, unit_types::science_vessel, army);
						};
					}
					if (count_units_plus_production(st, unit_types::academy) == 0) {
						army = [army](state&st) {
							return maxprod(st, unit_types::academy, army);
						};
					}

					if (tank_count < (enemy_hydralisk_count + enemy_lurker_count) / 4) {
						army = [army](state&st) {
							return nodelay(st, unit_types::siege_tank_tank_mode, army);
						};
					}

					return army(st);
				});
			};

			if (enemy_spire_count) {
				for (auto&b : build::build_tasks) {
					if (b.type->unit == unit_types::science_vessel) b.dead = true;
				}
				for (unit*u : my_completed_units_of_type[unit_types::starport]) {
					if (!u->train_queue.empty() && u->train_queue.front() == unit_types::science_vessel) {
						u->game_unit->cancelTrain();
					}
				}
			}

			if (!my_units_of_type[unit_types::factory].empty()) {
				resource_gathering::max_gas = 400.0;
			}
			if (!my_units_of_type[unit_types::vulture].empty()) {
				resource_gathering::max_gas = 0.0;
			}
			int vulture_count = my_completed_units_of_type[unit_types::vulture].size();
			int siege_tank_count = my_completed_units_of_type[unit_types::siege_tank_tank_mode].size() + my_completed_units_of_type[unit_types::siege_tank_siege_mode].size();
			int marine_count = my_completed_units_of_type[unit_types::marine].size();
			int wraith_count = my_completed_units_of_type[unit_types::wraith].size();
			int valkyrie_count = my_completed_units_of_type[unit_types::valkyrie].size();
			auto my_st = get_my_current_state();
			bool expand = false;

			if (my_st.bases.size() == 1 && (current_used_total_supply >= 60 || players::my_player->has_upgrade(upgrade_types::terran_ship_weapons_1))) expand = true;
			if (my_st.bases.size() == 2 && initial_push) expand = true;
			
			execute_build(expand, build);

			if (combat::defence_choke.center != xy() && !my_units_of_type[unit_types::barracks].empty()) {
				if (wall_calculated < 2) {
					++wall_calculated;

					wall = wall_in::wall_builder();

					wall.spot = wall_in::find_wall_spot_from_to(unit_types::zealot, combat::my_closest_base, combat::defence_choke.outside, false);
					wall.spot.outside = combat::defence_choke.outside;

					wall.against(wall_calculated == 1 ? unit_types::zergling : unit_types::zealot);
					wall.add_building(unit_types::factory);
					wall.add_building(unit_types::academy);
					has_wall = true;
					if (wall_calculated == 1) has_zergling_tight_wall = true;
					if (!wall.find()) {
						wall.add_building(unit_types::supply_depot);
						if (!wall.find()) {
							log("failed to find wall in :(\n");
							has_wall = false;
							if (wall_calculated == 1) has_zergling_tight_wall = false;
						}
					}
					if (has_wall) wall_calculated = 2;
				}
			}
			if (has_wall) {
				if (!being_zergling_rushed) wall.build();
				for (auto&b : build::build_tasks) {
					if (b.type->unit == unit_types::machine_shop) {
						if (b.built_unit || b.build_frame - current_frame <= 15 * 5) {
							wall = wall_in::wall_builder();
							has_wall = false;
							break;
						}
					}
				}
				if (current_used_total_supply >= 45) {
					wall = wall_in::wall_builder();
					has_wall = false;
				}
			}

			multitasking::sleep(15 * 5);
		}
		combat::build_bunker_count = 0;
		resource_gathering::max_gas = 0.0;

		//combat::no_aggressive_groups = false;

	}

	void render() {
		wall.render();
	}

};

