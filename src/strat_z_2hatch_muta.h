

struct strat_z_2hatch_muta : strat_z_base {

	virtual void init() override {

		scouting::scout_supply = 12.0;

		default_upgrades = true;

	}

	bool fight_ok = true;
	bool defence_fight_ok = true;
	int damaged_overlord_count = 0;
	double spire_progress = 0.0;
	virtual bool tick() override {

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
				build::add_build_task(4.25, unit_types::drone);
				build::add_build_task(4.5, unit_types::extractor);
				build::add_build_task(5.0, unit_types::zergling);
				if (players::opponent_player->race != race_terran) {
					build::add_build_task(5.0, unit_types::zergling);
					build::add_build_task(5.0, unit_types::zergling);
					build::add_build_task(5.0, unit_types::zergling);
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

		army_supply = current_used_total_supply - my_workers.size();

		min_bases = 5;
		if (my_hatch_count < 3) max_bases = 2;
		else max_bases = 0;

		if (is_defending || ((!fight_ok || enemy_zergling_count >= zergling_count - 6) && my_units_of_type[unit_types::mutalisk].empty())) {
			combat::no_scout_around = true;
			combat::aggressive_zerglings = false;
			combat::aggressive_mutalisks = false;
		} else {
			combat::no_scout_around = army_supply <= enemy_army_supply || enemy_static_defence_count >= 2;
			if (enemy_attacking_army_supply || enemy_zergling_count >= 4 + zergling_count / 2) combat::no_scout_around = true;
			combat::aggressive_zerglings = true;
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

		st.auto_build_hatcheries = true;
		//st.dont_build_refineries = drone_count < 20;

		if (spire_progress >= 0.5 && (spire_progress < 1.0 || my_units_of_type[unit_types::mutalisk].empty())) {
			if (drone_count < 20) {
				army = [army](state&st) {
					return nodelay(st, unit_types::drone, army);
				};
			}
			army = [army](state&st) {
				return nodelay(st, unit_types::mutalisk, army);
			};
			return army(st);
		}

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

		army = [army](state&st) {
			return nodelay(st, unit_types::drone, army);
		};

		if (drone_count >= 10 && hatch_count < 2) {
			army = [army](state&st) {
				return nodelay(st, unit_types::hatchery, army);
			};
		}

		if (current_gas >= 100.0) {
			if (count_units_plus_production(st, unit_types::lair) == 0) {
				army = [army](state&st) {
					return nodelay(st, unit_types::lair, army);
				};
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
				if (enemy_attacking_army_supply && !defence_fight_ok && sunken_count < 3) {
					army = [army](state&st) {
						return nodelay(st, unit_types::sunken_colony, army);
					};
				}
				if ((is_defending || sunken_count >= 2) && !defence_fight_ok) {
					army = [army](state&st) {
						return nodelay(st, unit_types::zergling, army);
					};
				}
			}
			if (maybe_being_rushed && drone_count >= 14) {
				if (sunken_count < (spire_progress ? 4 : drone_count >= 18 ? 2 : 1)) {
					army = [army](state&st) {
						return nodelay(st, unit_types::sunken_colony, army);
					};
				}
			}
		} else {
			if (!defence_fight_ok || (players::opponent_player->race == race_zerg && army_supply < drone_count)) {
				army = [army](state&st) {
					return nodelay(st, unit_types::zergling, army);
				};
			}
		}
		if (enemy_zergling_count >= 12 && spire_progress) {
			if (!defence_fight_ok || (enemy_zergling_count && drone_count >= 14 && drone_count > enemy_worker_count && sunken_count < 4 && enemy_spire_count == 0)) {
				army = [army](state&st) {
					return nodelay(st, unit_types::sunken_colony, army);
				};
			}
			if (sunken_count >= 4 || is_defending || (enemy_zergling_count && drone_count > enemy_worker_count + 4)) {
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
		}

		if (damaged_overlord_count && st.used_supply[race_zerg] >= st.max_supply[race_zerg] + count_production(st, unit_types::overlord) * 8 - damaged_overlord_count * 8 - 4) {
			army = [army](state&st) {
				return nodelay(st, unit_types::overlord, army);
			};
		}

		if (st.units[unit_types::spire].empty()) {
			if (scourge_count < enemy_air_army_supply) {
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

