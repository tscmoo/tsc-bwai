

struct strat_z_2hatch_muta : strat_z_base {

	virtual void init() override {

		scouting::scout_supply = 12.0;

		//pull_workers_for_ling_zealot_defence = true;

		sleep_time = 15;
	}

	bool fight_ok = true;
	bool defence_fight_ok = true;
	bool defence_fight_x1_sunken_ok = true;
	int damaged_overlord_count = 0;
	double spire_progress = 0.0;
	virtual bool tick() override {

		if (opening_state == 0) {
			if (my_resource_depots.size() != 1 || my_workers.size() > 9) opening_state = -1;
			else {
				if (players::opponent_player->race == race_zerg || players::opponent_player->random) {
					build::add_build_total(0.0, unit_types::drone, 9);
					build::add_build_total(1.0, unit_types::overlord, 2);
					build::add_build_task(2.0, unit_types::spawning_pool);
					build::add_build_task(3.0, unit_types::extractor);
					build::add_build_task(4.0, unit_types::drone);
					build::add_build_task(4.0, unit_types::drone);
					build::add_build_task(4.0, unit_types::drone);
					build::add_build_task(5.0, unit_types::zergling);
					build::add_build_task(5.0, unit_types::zergling);
					build::add_build_task(5.0, unit_types::zergling);
 					build::add_build_task(5.0, unit_types::zergling);
					build::add_build_task(6.0, unit_types::hatchery);
				} else {
					build::add_build_total(0.0, unit_types::drone, 9);
					build::add_build_total(1.0, unit_types::overlord, 2);
					build::add_build_task(2.0, unit_types::drone);
					build::add_build_task(2.0, unit_types::drone);
					build::add_build_task(2.0, unit_types::drone);
					build::add_build_task(3.0, unit_types::hatchery);
					build::add_build_task(4.0, unit_types::spawning_pool);
					build::add_build_task(4.25, unit_types::drone);
					build::add_build_task(4.5, unit_types::extractor);
				}
				++opening_state;
			}
		} else if (opening_state != -1) {
			if (bo_all_done()) {
				opening_state = -1;
			}
		}

		fight_ok = eval_combat(false, 0);
		defence_fight_ok = eval_combat(true, 0);
		defence_fight_x1_sunken_ok = eval_combat(true, 1);

		army_supply = current_used_total_supply - my_workers.size();

		min_bases = 5;
		if (my_hatch_count < 3) max_bases = 2;
		else max_bases = 0;

		default_upgrades = drone_count >= 24;

		if (is_defending || ((!fight_ok || enemy_zergling_count >= zergling_count - 6) && my_units_of_type[unit_types::mutalisk].empty())) {
			combat::no_scout_around = true;
			combat::aggressive_zerglings = players::my_player->has_upgrade(upgrade_types::metabolic_boost) && players::opponent_player->race == race_zerg && fight_ok && zergling_count > enemy_zergling_count + 8;
			combat::aggressive_mutalisks = false;
		} else {
			combat::no_scout_around = army_supply <= enemy_army_supply || enemy_static_defence_count >= 2;
			if (enemy_attacking_army_supply || enemy_zergling_count >= 4 + zergling_count / 2) combat::no_scout_around = true;
			combat::aggressive_zerglings = fight_ok;
			combat::aggressive_mutalisks = true;
		}

		if (enemy_attacking_army_supply >= 2.0) rm_all_scouts();

		damaged_overlord_count = 0;
		for (unit*u : my_completed_units_of_type[unit_types::overlord]) {
			if (u->hp < u->stats->hp) ++damaged_overlord_count;
		}

		spire_progress = 0.0;
		for (unit*u : my_units_of_type[unit_types::spire]) {
			double p = 1.0 - (double)u->remaining_build_time / u->type->build_time;
			if (p > spire_progress) spire_progress = p;
		}

		resource_gathering::max_gas = 1;
		if (my_units_of_type[unit_types::lair].empty()) resource_gathering::max_gas = 100;
		else {
			for (unit*u : my_units_of_type[unit_types::lair]) {
				if (u->remaining_build_time <= 15 * 60) resource_gathering::max_gas = 150;
			}
		}
		if (spire_progress) resource_gathering::max_gas = 900;
		
		return current_used_total_supply >= 120;
	}
	virtual bool build(buildpred::state&st) override {
		using namespace buildpred;
		if (opening_state != -1) return false;

		auto army = this->army;

		st.auto_build_hatcheries = current_minerals >= 400 || !my_units_of_type[unit_types::spire].empty();

		if (spire_progress && count_units_plus_production(st, unit_types::extractor) < 2 && st.bases.size() >= 2) {
			army = [army](state&st) {
				return nodelay(st, unit_types::extractor, army);
			};
		}
		if (count_units_plus_production(st, unit_types::extractor) == 0) {
			army = [army](state&st) {
				return nodelay(st, unit_types::extractor, army);
			};
		}

		bool maybe_being_rushed = being_rushed || (!opponent_has_expanded && enemy_cannon_count + enemy_bunker_count == 0);

		if ((!maybe_being_rushed || !my_completed_units_of_type[unit_types::spawning_pool].empty()) && (drone_count >= 9 || zergling_count)) {
			if (!maybe_being_rushed || zergling_count >= 8) {
				army = [army](state&st) {
					return nodelay(st, unit_types::drone, army);
				};
			}
		}

		if (current_gas >= 100.0 && drone_count >= 12) {
			if (zergling_count > enemy_zergling_count || sunken_count) {
				if (count_units_plus_production(st, unit_types::lair) == 0) {
					if (my_units_of_type[unit_types::hatchery].size() < 2) {
						army = [army](state&st) {
							return nodelay(st, unit_types::hatchery, army);
						};
					} else {
						army = [army](state&st) {
							return nodelay(st, unit_types::lair, army);
						};
					}
				}
			}
		}
		if (current_gas >= 150.0 && !my_completed_units_of_type[unit_types::lair].empty()) {
			if (count_units_plus_production(st, unit_types::spire) == 0) {
				army = [army](state&st) {
					return nodelay(st, unit_types::spire, army);
				};
			}
		}

		if (my_completed_units_of_type[unit_types::hatchery].size() + my_units_of_type[unit_types::lair].size() >= 2 && drone_count >= 11) {
			if (spire_progress < 1.0) {
				if (maybe_being_rushed && drone_count >= 20 && zergling_count < 12) {
					army = [army](state&st) {
						return nodelay(st, unit_types::zergling, army);
					};
				}
				if (maybe_being_rushed && drone_count >= 14 && sunken_count < (drone_count >= 18 ? 4 : 3)) {
					army = [army](state&st) {
						return nodelay(st, unit_types::sunken_colony, army);
					};
				}
				if ((is_defending || sunken_count >= 2) && !defence_fight_ok && zergling_count < 16) {
					army = [army](state&st) {
						return nodelay(st, unit_types::zergling, army);
					};
				}
			}
		} else {
			if (!defence_fight_ok || (players::opponent_player->race == race_zerg && army_supply < drone_count)) {
				if (zergling_count < 12 || (drone_count > enemy_worker_count && zergling_count < enemy_zergling_count + 4)) {
					army = [army](state&st) {
						return nodelay(st, unit_types::zergling, army);
					};
				}
				if (enemy_zealot_count + enemy_marine_count && sunken_count < 2) {
					army = [army](state&st) {
						return nodelay(st, unit_types::sunken_colony, army);
					};
				}
			}
		}

		if (static_defence_pos_is_valid || my_completed_units_of_type[unit_types::hatchery].size() + my_units_of_type[unit_types::lair].size() >= 2) {
			if (!defence_fight_ok || players::opponent_player->minerals_lost < 200.0) {
				if (maybe_being_rushed && sunken_count < (enemy_attacking_army_supply >= 5.0 ? 3 : 2) && drone_count >= 11) {
					army = [army](state&st) {
						return nodelay(st, unit_types::sunken_colony, army);
					};
				}
				if (sunken_count && is_defending && (!defence_fight_x1_sunken_ok || (drone_count >= 12 && zergling_count < 8))) {
					army = [army](state&st) {
						return nodelay(st, unit_types::zergling, army);
					};
				}
			}
		}

		if (maybe_being_rushed && zergling_count < 8) {
			army = [army](state&st) {
				return nodelay(st, unit_types::zergling, army);
			};
		}


		if (players::opponent_player->race == race_zerg && hatch_count >= 2) {
			if (drone_count > enemy_worker_count + 2 && zergling_count < enemy_zergling_count * 2) {
				army = [army](state&st) {
					return nodelay(st, unit_types::zergling, army);
				};
			}
			if (zergling_count < enemy_zergling_count || !defence_fight_ok) {
				army = [army](state&st) {
					return nodelay(st, unit_types::zergling, army);
				};
			}
		}

		double desired_army_supply = drone_count*drone_count * 0.015 + drone_count * 0.8 - 16;

		if (spire_progress >= 1.0 && army_supply < desired_army_supply) {
			army = [army](state&st) {
				return nodelay(st, unit_types::mutalisk, army);
			};
			if (enemy_corsair_count >= 6) {
				army = [army](state&st) {
					return nodelay(st, unit_types::hydralisk, army);
				};
			}
		}
		if (spire_progress >= 0.75) {
			if ((enemy_zergling_count || enemy_spawning_pool_count) && mutalisk_count < drone_count && drone_count > enemy_worker_count) {
				army = [army](state&st) {
					return nodelay(st, unit_types::mutalisk, army);
				};
			}
		}

		if (scourge_count < enemy_air_army_supply) {
			army = [army](state&st) {
				return nodelay(st, unit_types::scourge, army);
			};
		}

		if (force_expand && count_production(st, unit_types::hatchery) == 0) {
			army = [army](state&st) {
				return nodelay(st, unit_types::hatchery, army);
			};
		}

		if (damaged_overlord_count && st.used_supply[race_zerg] >= st.max_supply[race_zerg] - 8 && st.used_supply[race_zerg] >= st.max_supply[race_zerg] - damaged_overlord_count * 8 - 12) {
			army = [army](state&st) {
				return nodelay(st, unit_types::overlord, army);
			};
		} else {
			if (spire_progress >= 1.0 && players::opponent_player->race == race_protoss && defence_fight_ok && mutalisk_count == 0) {
				if (scourge_count < 2) {
					army = [army](state&st) {
						return nodelay(st, unit_types::scourge, army);
					};
				}
			}
		}

		return army(st);
	}

};

