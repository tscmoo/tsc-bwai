

struct strat_z_hydra_into_muta : strat_z_base {

	virtual void init() override {

		scouting::scout_supply = 9.0;

		combat::no_scout_around = true;
		combat::aggressive_zerglings = false;

		default_upgrades = false;

	}

	bool fight_ok = true;
	bool defence_fight_ok = true;
	int max_workers = 0;
	bool go_mutas = false;
	int last_not_fight_ok = 0;
	virtual bool tick() override {

		fight_ok = eval_combat(false, 0);
		defence_fight_ok = eval_combat(true, 2);

		min_bases = 2;

		if (being_rushed) rm_all_scouts();

		max_workers = get_max_mineral_workers() + 6;

		if (defence_fight_ok) {
			if (!my_completed_units_of_type[unit_types::hydralisk_den].empty()) {
				get_upgrades::set_upgrade_value(upgrade_types::muscular_augments, -1.0);
				get_upgrades::set_upgrade_order(upgrade_types::muscular_augments, -10.0);
				if (players::my_player->has_upgrade(upgrade_types::muscular_augments)) {
					get_upgrades::set_upgrade_value(upgrade_types::grooved_spines, -1.0);
				}

				get_upgrades::set_upgrade_value(upgrade_types::pneumatized_carapace, -1.0);
			}
			if (!my_completed_units_of_type[unit_types::evolution_chamber].empty()) {
				get_upgrades::set_upgrade_value(upgrade_types::zerg_missile_attacks_1, -1.0);
			}
		}

		if (my_completed_units_of_type[unit_types::hydralisk].size() >= 40 || current_used_total_supply >= 100 || my_workers.size() >= 40) {
			go_mutas = true;
		}

		if (!fight_ok) last_not_fight_ok = current_frame;
		if (current_frame - last_not_fight_ok >= 15 * 60) attack_interval = 15 * 60;
		else attack_interval = 0;
		
		return current_used_total_supply >= 110;
	}
	virtual bool build(buildpred::state&st) override {
		using namespace buildpred;

		auto army = this->army;

		st.auto_build_hatcheries = true;

		if (drone_count < max_workers) {
			army = [army](state&st) {
				return nodelay(st, unit_types::drone, army);
			};
		} else {
			army = [army](state&st) {
				return nodelay(st, unit_types::zergling, army);
			};

			if (defence_fight_ok && count_production(st, unit_types::drone) < 2) {
				army = [army](state&st) {
					return nodelay(st, unit_types::drone, army);
				};
			}
		}

		bool maybe_being_rushed = being_rushed || (!opponent_has_expanded && enemy_cannon_count + enemy_bunker_count == 0);

		if (!maybe_being_rushed) {
			if (count_units_plus_production(st, unit_types::lair) == 0) {
				army = [army](state&st) {
					return nodelay(st, unit_types::lair, army);
				};
			}
			if (!st.units[unit_types::lair].empty() && count_units_plus_production(st, unit_types::spire) == 0) {
				army = [army](state&st) {
					return nodelay(st, unit_types::spire, army);
				};
			}
		}
		if (drone_count >= std::min(max_workers, 24)) {
			if (!st.units[unit_types::lair].empty() && count_units_plus_production(st, unit_types::spire) == 0) {
				army = [army](state&st) {
					return nodelay(st, unit_types::spire, army);
				};
			}
			if (maybe_being_rushed && count_units_plus_production(st, unit_types::hydralisk_den) == 0) {
				army = [army](state&st) {
					return nodelay(st, unit_types::hydralisk_den, army);
				};
			}
			if (count_units_plus_production(st, unit_types::extractor) < 2) {
				army = [army](state&st) {
					return nodelay(st, unit_types::extractor, army);
				};
			}
		}
		if (drone_count >= std::min(max_workers, 32) && army_supply < drone_count) {
			if (go_mutas) {
				army = [army](state&st) {
					return nodelay(st, unit_types::mutalisk, army);
				};
			} else {
				army = [army](state&st) {
					return nodelay(st, unit_types::hydralisk, army);
				};
				if (defence_fight_ok && count_units_plus_production(st, unit_types::evolution_chamber) == 0) {
					army = [army](state&st) {
						return nodelay(st, unit_types::evolution_chamber, army);
					};
				}
			}
		}

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

