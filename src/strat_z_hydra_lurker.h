

struct strat_z_hydra_lurker : strat_z_base {

	virtual void init() override {

		scouting::scout_supply = 14.0;

		combat::no_scout_around = true;

		default_upgrades = false;

		get_upgrades::set_upgrade_value(upgrade_types::lurker_aspect, -1.0);
		get_upgrades::set_upgrade_order(upgrade_types::lurker_aspect, -10.0);
		get_upgrades::set_upgrade_order(upgrade_types::muscular_augments, -9.0);
		get_upgrades::set_upgrade_order(upgrade_types::grooved_spines, -8.0);

	}

	bool fight_ok = true;
	bool defence_fight_ok = true;
	int max_workers = 0;
	virtual bool tick() override {

		fight_ok = eval_combat(false, 0);
		defence_fight_ok = eval_combat(true, 1);

		min_bases = 2;

		if (being_rushed) rm_all_scouts();

		max_workers = get_max_mineral_workers() + 6;

		if (!my_completed_units_of_type[unit_types::evolution_chamber].empty()) {
			get_upgrades::set_upgrade_value(upgrade_types::zerg_carapace_1, -1.0);
			get_upgrades::set_upgrade_value(upgrade_types::zerg_missile_attacks_1, -1.0);
			get_upgrades::set_upgrade_order(upgrade_types::zerg_carapace_1, -10.0);
		}
		if (defence_fight_ok) {
			if (players::my_player->has_upgrade(upgrade_types::zerg_carapace_1)) {
				get_upgrades::set_upgrade_value(upgrade_types::zerg_carapace_2, -1.0);
			}
			if (players::my_player->has_upgrade(upgrade_types::zerg_missile_attacks_1)) {
				get_upgrades::set_upgrade_value(upgrade_types::zerg_missile_attacks_2, -1.0);
			}
		}

		if (!my_completed_units_of_type[unit_types::lair].empty()) {
			get_upgrades::set_upgrade_value(upgrade_types::muscular_augments, -1.0);
			get_upgrades::set_upgrade_value(upgrade_types::grooved_spines, -1.0);
		}

		if (!my_units_of_type[unit_types::lair].empty()) {
			get_upgrades::set_upgrade_value(upgrade_types::metabolic_boost, -1.0);
		}

		if (!my_units_of_type[unit_types::lurker].empty() && fight_ok) {
			attack_interval = 15 * 30;
			min_bases = 3;
		} else attack_interval = 0;
		
		return current_used_total_supply >= 110;
	}
	virtual bool build(buildpred::state&st) override {
		using namespace buildpred;

		auto army = this->army;

		//st.auto_build_hatcheries = players::my_player->has_upgrade(upgrade_types::lurker_aspect);
		st.auto_build_hatcheries = true;

		st.dont_build_refineries = st.units[unit_types::lair].empty();

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

		if (drone_count >= 13) {
			if (hatch_count < 3) {
				army = [army](state&st) {
					return nodelay(st, unit_types::hatchery, army);
				};
			}
		}

		if (st.used_supply[race_zerg] >= 16) {
			if (count_units_plus_production(st, unit_types::lair) == 0) {
				army = [army](state&st) {
					return nodelay(st, unit_types::lair, army);
				};
			} else {
				if (count_units_plus_production(st, unit_types::hydralisk_den) == 0) {
					army = [army](state&st) {
						return nodelay(st, unit_types::hydralisk_den, army);
					};
				} else {
					army = [army](state&st) {
						return nodelay(st, unit_types::hydralisk, army);
					};
					if (lurker_count * 2 < std::max(7.0, enemy_ground_small_army_supply)) {
						army = [army](state&st) {
							return nodelay(st, unit_types::lurker, army);
						};
					}
					if (hydralisk_count < enemy_ground_large_army_supply) {
						army = [army](state&st) {
							return nodelay(st, unit_types::hydralisk, army);
						};
					}
				}
			}
			if (count_units_plus_production(st, unit_types::extractor) < 1) {
				army = [army](state&st) {
					return nodelay(st, unit_types::extractor, army);
				};
			}
		}

		if (players::my_player->has_upgrade(upgrade_types::lurker_aspect)) {
			if (hatch_count < 5) {
				army = [army](state&st) {
					return nodelay(st, unit_types::hatchery, army);
				};
			}
			if (defence_fight_ok && completed_hatch_count >= 5) {
				if (count_units_plus_production(st, unit_types::evolution_chamber) < 2) {
					army = [army](state&st) {
						return nodelay(st, unit_types::evolution_chamber, army);
					};
				}
			}
		}

		if (army_supply >= drone_count && defence_fight_ok) {
			if (count_production(st, unit_types::drone) < 2) {
				army = [army](state&st) {
					return nodelay(st, unit_types::drone, army);
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
			if (maybe_being_rushed && sunken_count < 1) {
				army = [army](state&st) {
					return nodelay(st, unit_types::sunken_colony, army);
				};
			}
		}
		
		bool build_army = is_defending && army_supply < enemy_army_supply;
// 		if (drone_count >= 28 && army_supply < 8.0) build_army = true;
// 		if (maybe_being_rushed && drone_count >= 24 && army_supply < 12.0) build_army = true;
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

