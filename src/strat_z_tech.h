

struct strat_z_tech : strat_z_base {

	virtual void init() override {

		scouting::scout_supply = 20.0;

		combat::no_scout_around = false;
		combat::aggressive_zerglings = true;

		default_upgrades = true;

	}

	bool fight_ok = true;
	bool defence_fight_ok = true;
	int damaged_overlord_count = 0;
	virtual bool tick() override {
// 		if (opening_state == 0) {
// 			if (my_resource_depots.size() != 1 || my_workers.size() != 4) opening_state = -1;
// 			else {
// 				build::add_build_task(0.0, unit_types::drone);
// 				build::add_build_task(0.0, unit_types::drone);
// 				build::add_build_task(0.0, unit_types::drone);
// 				build::add_build_task(0.0, unit_types::drone);
// 				build::add_build_task(0.0, unit_types::drone);
// 				++opening_state;
// 			}
// 		} else if (opening_state == 1) {
// 			bo_gas_trick();
// 		} else if (opening_state == 2) {
// 			build::add_build_task(0.0, unit_types::hatchery);
// 			++opening_state;
// 		} else if (opening_state == 3) {
// 			bo_gas_trick();
// 		} else if (opening_state == 4) {
// 			build::add_build_task(0.0, unit_types::spawning_pool);
// 			build::add_build_task(1.0, unit_types::hatchery);
// 			build::add_build_task(2.0, unit_types::overlord);
// 			build::add_build_task(3.0, unit_types::zergling);
// 			build::add_build_task(3.0, unit_types::zergling);
// 			build::add_build_task(3.0, unit_types::zergling);
// 			build::add_build_task(3.0, unit_types::zergling);
// 			build::add_build_task(3.0, unit_types::zergling);
// 			build::add_build_task(3.0, unit_types::zergling);
// 			++opening_state;
// 		} else if (opening_state != -1) {
// 			if (bo_all_done()) {
// 				opening_state = -1;
// 			}
// 		}

		if (opening_state == 0) {
			if (my_resource_depots.size() != 1 || my_workers.size() != 4) opening_state = -1;
			else {
				build::add_build_task(0.0, unit_types::drone);
				build::add_build_task(0.0, unit_types::drone);
				build::add_build_task(0.0, unit_types::drone);
				build::add_build_task(0.0, unit_types::drone);
				build::add_build_task(0.0, unit_types::drone);
				build::add_build_task(1.0, unit_types::overlord);
				build::add_build_task(2.0, unit_types::drone);
				build::add_build_task(2.0, unit_types::drone);
				build::add_build_task(2.0, unit_types::drone);
				build::add_build_task(3.0, unit_types::hatchery);
				build::add_build_task(4.0, unit_types::spawning_pool);
				build::add_build_task(5.0, unit_types::zergling);
				build::add_build_task(5.0, unit_types::zergling);
				build::add_build_task(5.0, unit_types::zergling);
				build::add_build_task(5.0, unit_types::zergling);
				++opening_state;
			}
		} else if (opening_state != -1) {
			if (bo_all_done()) {
				opening_state = -1;
			}
		}

		fight_ok = eval_combat(false, 0);
		defence_fight_ok = fight_ok;

		army_supply = current_used_total_supply - my_workers.size();

		min_bases = 3;

		if (is_defending) {
			combat::no_scout_around = true;
			combat::aggressive_zerglings = false;
		} else {
			combat::no_scout_around = army_supply <= enemy_army_supply || enemy_static_defence_count >= 2;
			if (enemy_attacking_army_supply) combat::no_scout_around = true;
			combat::aggressive_zerglings = true;
		}

// 		if (enemy_attacking_army_supply >= 12.0) was_rushed = true;
// 		if (players::opponent_player->minerals_lost >= 600.0) was_rushed = true;
// 		log(log_level_info, "was_rushed is %d\n", was_rushed);

		if (being_rushed) rm_all_scouts();

		if (my_workers.size() < 12 && (enemy_gateway_count >= 2 || enemy_army_supply)) rm_all_scouts();
		if (my_workers.size() < 12 && enemy_static_defence_count) rm_all_scouts();

		if (defence_fight_ok || army_supply > enemy_attacking_army_supply) {
			if (!my_completed_units_of_type[unit_types::hydralisk_den].empty()) {
				get_upgrades::set_upgrade_value(upgrade_types::muscular_augments, -1.0);
				get_upgrades::set_upgrade_order(upgrade_types::muscular_augments, -10.0);
				if (players::my_player->has_upgrade(upgrade_types::muscular_augments)) {
					get_upgrades::set_upgrade_value(upgrade_types::grooved_spines, -1.0);
				}

				get_upgrades::set_upgrade_value(upgrade_types::pneumatized_carapace, -1.0);

				if (!my_completed_units_of_type[unit_types::lair].empty()) {
					get_upgrades::set_upgrade_value(upgrade_types::lurker_aspect, -1.0);
					get_upgrades::set_upgrade_order(upgrade_types::lurker_aspect, -11.0);
				}
			}
			if (!my_completed_units_of_type[unit_types::evolution_chamber].empty()) {
				get_upgrades::set_upgrade_value(upgrade_types::zerg_missile_attacks_1, -1.0);
				get_upgrades::set_upgrade_value(upgrade_types::zerg_carapace_1, -1.0);
				get_upgrades::set_upgrade_value(upgrade_types::zerg_missile_attacks_2, -1.0);
				get_upgrades::set_upgrade_value(upgrade_types::zerg_carapace_2, -1.0);
			}
		}

		//combat::aggressive_zerglings = !being_rushed || army_supply >= enemy_army_supply * 1.5;
		combat::aggressive_zerglings = !being_rushed || army_supply >= enemy_army_supply;

		//if (opponent_has_expanded || enemy_cannon_count + enemy_bunker_count) min_bases = 3;

		if (enemy_attacking_army_supply >= 8.0) {
			//combat::no_aggressive_groups = !is_defending;
			combat::no_aggressive_groups = true;
			combat::aggressive_groups_done_only = false;
			combat::aggressive_zerglings = false;
		}

		if (army_supply > enemy_army_supply + 8.0) {
			combat::no_aggressive_groups = false;
			combat::aggressive_groups_done_only = false;
			combat::no_scout_around = true;
		}

		damaged_overlord_count = 0;
		for (unit*u : my_completed_units_of_type[unit_types::overlord]) {
			if (u->hp < u->stats->hp) ++damaged_overlord_count;
		}

		resource_gathering::max_gas = 100.0;
		if (my_workers.size() >= 14) resource_gathering::max_gas = 250.0;
		if (my_workers.size() < 16 && !defence_fight_ok) resource_gathering::max_gas = 1.0;
		
		return current_used_total_supply >= 160;
	}
	virtual bool build(buildpred::state&st) override {
		using namespace buildpred;
		if (opening_state != -1) return false;

		auto army = this->army;

		st.auto_build_hatcheries = true;
		st.dont_build_refineries = drone_count < 20;

		bool maybe_being_rushed = being_rushed || (!opponent_has_expanded && enemy_cannon_count + enemy_bunker_count == 0);

		army = [army](state&st) {
			return nodelay(st, unit_types::drone, army);
		};

		if (drone_count >= 11 && hatch_count < 3) {
			army = [army](state&st) {
				return nodelay(st, unit_types::hatchery, army);
			};
		}

		if (drone_count >= 13) {
			if (count_units_plus_production(st, unit_types::extractor) == 0) {
				army = [army](state&st) {
					return nodelay(st, unit_types::extractor, army);
				};
			}
		}

		double desired_army_supply = drone_count*drone_count * 0.015 + drone_count * 0.8 - 16;

		if (enemy_attacking_army_supply >= 8.0 && drone_count >= 18) {
			if (sunken_count < 2) {
				army = [army](state&st) {
					return nodelay(st, unit_types::sunken_colony, army);
				};
			}
		}

		if ((!defence_fight_ok && is_defending) || army_supply < desired_army_supply || being_rushed && army_supply < 6.0) {
			if (count_units_plus_production(st, unit_types::hydralisk_den) == 0 && zergling_count < 14) {
				army = [army](state&st) {
					return nodelay(st, unit_types::zergling, army);
				};
			} else {
				army = [army](state&st) {
					return nodelay(st, unit_types::zergling, army);
				};
				army = [army](state&st) {
					return nodelay(st, unit_types::hydralisk, army);
				};
				if (players::my_player->has_upgrade(upgrade_types::lurker_aspect) && hydralisk_count > enemy_air_army_supply) {
					army = [army](state&st) {
						return nodelay(st, unit_types::lurker, army);
					};
				}
			}
		}
		if (army_supply > enemy_attacking_army_supply || army_supply >= 16.0) {
// 			if (drone_count < 20 && zergling_count < 8) {
// 				if (maybe_being_rushed && (enemy_gateway_count >= 2 || enemy_barracks_count >= 2 || enemy_zerg_unit_count)) {
// 					if (zergling_count < drone_count) {
// 						army = [army](state&st) {
// 							return nodelay(st, unit_types::zergling, army);
// 						};
// 					}
// 				}
// 			}
			if (count_units_plus_production(st, unit_types::lair) == 0) {
				if (st.gas >= 100) {
					army = [army](state&st) {
						return nodelay(st, unit_types::lair, army);
					};
				}
			}
			if (!my_completed_units_of_type[unit_types::lair].empty()) {
				if (count_units_plus_production(st, unit_types::hive) == 0) {
					army = [army](state&st) {
						return nodelay(st, unit_types::hive, army);
					};
				}
			}
			if (!my_completed_units_of_type[unit_types::hive].empty()) {
				if (army_supply > enemy_army_supply) {
					if (count_units_plus_production(st, unit_types::spire) == 0) {
						army = [army](state&st) {
							return nodelay(st, unit_types::spire, army);
						};
					}
				}
				if (count_units_plus_production(st, unit_types::defiler_mound) == 0) {
					army = [army](state&st) {
						return nodelay(st, unit_types::defiler_mound, army);
					};
				}
			}
		}
		if (army_supply >= 28.0 && !st.units[unit_types::defiler_mound].empty()) {
			if (defiler_count < 4) {
				army = [army](state&st) {
					return nodelay(st, unit_types::defiler, army);
				};
			}
		}

		if (damaged_overlord_count && st.used_supply[race_zerg] >= st.max_supply[race_zerg] + count_production(st, unit_types::overlord) * 8 - damaged_overlord_count * 8 - 4) {
			army = [army](state&st) {
				return nodelay(st, unit_types::overlord, army);
			};
		}

		double anti_air_supply = enemy_air_army_supply;
		if (defence_fight_ok && drone_count >= 26) anti_air_supply += 2;
		if (st.units[unit_types::spire].empty()) {
			if (hydralisk_count < anti_air_supply) {
				army = [army](state&st) {
					return nodelay(st, unit_types::hydralisk, army);
				};
			}
		} else {
			if (scourge_count < anti_air_supply) {
				army = [army](state&st) {
					return nodelay(st, unit_types::scourge, army);
				};
			}
		}

		if (force_expand && count_production(st, unit_types::hatchery) == 0) {
			army = [army](state&st) {
				return nodelay(st, unit_types::hatchery, army);
			};
		}

		return army(st);
	}

};

