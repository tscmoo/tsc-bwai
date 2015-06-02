
struct strat_zvt_opening {

	void run() {

		combat::no_aggressive_groups = false;

		scouting::scout_supply = 9;

		using namespace buildpred;

		get_upgrades::set_no_auto_upgrades(true);
		resource_gathering::max_gas = 150.0;

		auto place_static_defence = [&]() {
			for (auto&b : build::build_tasks) {
				if (b.built_unit || b.dead) continue;
				if (b.type->unit == unit_types::creep_colony) {
					if (b.build_near != combat::defence_choke.center) {
						b.build_near = combat::defence_choke.center;
						b.require_existing_creep = true;
						build::unset_build_pos(&b);
					}
				}
			}
		};

		while (true) {

			int my_zergling_count = my_units_of_type[unit_types::zergling].size();

			int proxy_barracks_count = 0;
			int proxy_scv_count = 0;
			int proxy_marine_count = 0;
			int enemy_barracks_count = 0;
			int enemy_vulture_count = 0;
			int enemy_tank_count = 0;
			int enemy_marine_count = 0;
			double enemy_air_supply = 0.0;
			update_possible_start_locations();
			for (unit*e : enemy_units) {
				if (e->type == unit_types::barracks || e->type == unit_types::scv || e->type == unit_types::marine) {
					double e_d = get_best_score_value(possible_start_locations, [&](xy pos) {
						return unit_pathing_distance(unit_types::scv, e->pos, pos);
					});
					if (unit_pathing_distance(unit_types::scv, e->pos, combat::my_closest_base)*0.5 < e_d) {
						if (e->type == unit_types::barracks) ++proxy_barracks_count;
						if (e->type == unit_types::scv) ++proxy_scv_count;
						if (e->type == unit_types::marine) ++proxy_marine_count;
					}
				}
				if (e->type == unit_types::barracks) ++enemy_barracks_count;
				if (e->type == unit_types::vulture) ++enemy_vulture_count;
				if (e->type == unit_types::siege_tank_tank_mode || e->type == unit_types::siege_tank_siege_mode) ++enemy_tank_count;
				if (e->type == unit_types::marine) ++enemy_marine_count;
				if (e->is_flying) enemy_air_supply += e->type->required_supply;
			}

			if (my_units_of_type[unit_types::lair].empty()) resource_gathering::max_gas = 100.0;
			else if (my_units_of_type[unit_types::spire].empty()) resource_gathering::max_gas = 150.0;
			else {
				resource_gathering::max_gas = 300.0;
				get_upgrades::set_upgrade_value(upgrade_types::metabolic_boost, -1.0);
				get_upgrades::set_upgrade_value(upgrade_types::zerg_carapace_1, -1.0);
			}

			auto build = [&](state&st) {

				int drone_count = count_units_plus_production(st, unit_types::drone);
				int zergling_count = count_units_plus_production(st, unit_types::zergling);
				int hatch_count = count_units_plus_production(st, unit_types::hatchery) + count_units_plus_production(st, unit_types::lair) + count_units_plus_production(st, unit_types::hive);

				st.dont_build_refineries = drone_count < 20;

				std::function<bool(state&)> army = [&](state&st) {
					return depbuild(st, state(st), unit_types::drone);
				};

				if (drone_count >= 22) {
					if (count_units_plus_production(st, unit_types::hydralisk_den) == 0) {
						army = [army](state&st) {
							return nodelay(st, unit_types::hydralisk_den, army);
						};
					}
// 					army = [army](state&st) {
// 						return nodelay(st, unit_types::mutalisk, army);
// 					};
					if (count_units_plus_production(st, unit_types::lair) == 0) {
						army = [army](state&st) {
							return nodelay(st, unit_types::lair, army);
						};
					} else {
						if (count_units_plus_production(st, unit_types::spire) == 0) {
							army = [army](state&st) {
								return nodelay(st, unit_types::spire, army);
							};
						}
					}
				}
				if (drone_count >= 16) {
					if (hatch_count < 3) {
						army = [army](state&st) {
							return nodelay(st, unit_types::hatchery, army);
						};
					}

					if (zergling_count < 3) {
						army = [army](state&st) {
							return nodelay(st, unit_types::zergling, army);
						};
					}
				}

				if (drone_count >= 13) {
					if (count_units_plus_production(st, unit_types::extractor) == 0) {
						army = [army](state&st) {
							return nodelay(st, unit_types::extractor, army);
						};
					}
					if (enemy_vulture_count + proxy_marine_count || count_units_plus_production(st, unit_types::spire)) {
						if (my_completed_units_of_type[unit_types::hatchery].size() >= 2 && count_units_plus_production(st, unit_types::sunken_colony) < 2 + enemy_marine_count / 4) {
							army = [army](state&st) {
								return depbuild(st, state(st), unit_types::sunken_colony);
							};
						}
					}
				}

				if (drone_count >= 12 && count_units_plus_production(st, unit_types::spawning_pool) == 0) {
					army = [army](state&st) {
						return nodelay(st, unit_types::spawning_pool, army);
					};
				}

				if (enemy_air_supply || drone_count >= 20) {
					if (count_units_plus_production(st, unit_types::hydralisk) < (int)enemy_air_supply / 2 + 2) {
						army = [army](state&st) {
							return nodelay(st, unit_types::hydralisk, army);
						};
					}
				}

				return army(st);
			};
			if (!my_units_of_type[unit_types::spire].empty() && my_units_of_type[unit_types::spire].front()->remaining_build_time <= 15 * 30) break;
			//if (!my_completed_units_of_type[unit_types::spire].empty()) break;
			if (my_completed_units_of_type[unit_types::mutalisk].size() >= 2) break;
			//if (current_used_total_supply >= 20) break;

			bool expand = false;
			if (current_used_total_supply >= 13 && get_my_current_state().bases.size() < 2) expand = true;

			execute_build(expand, build);

			place_static_defence();

			multitasking::sleep(15 * 5);
		}


		resource_gathering::max_gas = 0.0;

	}

	void render() {

	}

};

