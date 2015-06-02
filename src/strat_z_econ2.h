

struct strat_z_econ2 : strat_z_base {

	virtual void init() override {

		scouting::scout_supply = 9.0;

		combat::no_scout_around = true;
		combat::aggressive_zerglings = false;

		default_upgrades = false;

		attack_interval = 15 * 30;

	}

	bool fight_ok = true;
	bool defence_fight_ok = true;
	int max_workers = 0;
	virtual bool tick() override {

		fight_ok = eval_combat(false, 0);
		defence_fight_ok = eval_combat(true, 2);

		min_bases = 2;

		if (being_rushed) rm_all_scouts();

		max_workers = get_max_mineral_workers() + 6;

		if (!my_completed_units_of_type[unit_types::evolution_chamber].empty()) {
			get_upgrades::set_upgrade_value(upgrade_types::zerg_carapace_1, -1.0);
			get_upgrades::set_upgrade_value(upgrade_types::zerg_melee_attacks_1, -1.0);
			get_upgrades::set_upgrade_order(upgrade_types::zerg_carapace_1, -10.0);
		}
		
		return current_used_total_supply >= 60;
	}
	virtual bool build(buildpred::state&st) override {
		using namespace buildpred;

		auto army = this->army;

// 		if (sunken_count < 6) {
// 			army = [army](state&st) {
// 				return nodelay(st, unit_types::sunken_colony, army);
// 			};
// 		}

		st.auto_build_hatcheries = true;

		if (drone_count < max_workers) {
			army = [army](state&st) {
				return nodelay(st, unit_types::drone, army);
			};
// 			if (drone_count >= 11 && hatch_count < 3) {
// 				army = [army](state&st) {
// 					return nodelay(st, unit_types::hatchery, army);
// 				};
// 			}
		} else {
// 			st.auto_build_hatcheries = true;
// 
			army = [army](state&st) {
				return nodelay(st, unit_types::zergling, army);
			};

			if (defence_fight_ok && count_production(st, unit_types::drone) < 2) {
				army = [army](state&st) {
					return nodelay(st, unit_types::drone, army);
				};
			}
		}

		if (drone_count >= std::min(max_workers, 34)) {
			if (count_units_plus_production(st, unit_types::hydralisk_den) == 0) {
				army = [army](state&st) {
					return nodelay(st, unit_types::hydralisk_den, army);
				};
			}
			if (count_units_plus_production(st, unit_types::evolution_chamber) < 2) {
				army = [army](state&st) {
					return nodelay(st, unit_types::evolution_chamber, army);
				};
			}
			if (count_units_plus_production(st, unit_types::extractor) < 2) {
				army = [army](state&st) {
					return nodelay(st, unit_types::extractor, army);
				};
			}
		}

		bool maybe_being_rushed = being_rushed || (!opponent_has_expanded && enemy_cannon_count + enemy_bunker_count == 0);

		if (my_completed_units_of_type[unit_types::hatchery].size() >= 2 && drone_count >= 13) {
			if (!defence_fight_ok) {
				army = [army](state&st) {
					return nodelay(st, unit_types::sunken_colony, army);
				};
			}
			if (maybe_being_rushed && sunken_count < 2) {
				army = [army](state&st) {
					return nodelay(st, unit_types::sunken_colony, army);
				};
			}
		}
		
		bool build_army = is_defending && army_supply < enemy_army_supply;
		if (drone_count >= 28 && army_supply < 8.0) build_army = true;
		if (maybe_being_rushed && drone_count >= 24 && army_supply < 12.0) build_army = true;
		if (sunken_count == 0 && !defence_fight_ok) {
			if (is_defending || count_production(st, unit_types::drone)) build_army = true;
		}

		if (build_army) {
			army = [army](state&st) {
				return nodelay(st, unit_types::zergling, army);
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

		return army(st);
	}

};

