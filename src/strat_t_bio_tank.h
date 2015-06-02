

struct strat_t_bio_tank : strat_t_base {

	virtual void init() override {

		scouting::comsat_supply = 30.0;

		get_upgrades::set_upgrade_value(upgrade_types::u_238_shells, -1.0);
		get_upgrades::set_upgrade_value(upgrade_types::stim_packs, -1.0);
		get_upgrades::set_upgrade_order(upgrade_types::u_238_shells, -10.0);
		get_upgrades::set_upgrade_order(upgrade_types::stim_packs, -9.0);

		get_upgrades::set_upgrade_value(upgrade_types::terran_infantry_weapons_1, -1.0);
		get_upgrades::set_upgrade_value(upgrade_types::terran_infantry_armor_1, -1.0);
		get_upgrades::set_upgrade_order(upgrade_types::terran_infantry_weapons_1, -3.0);
		get_upgrades::set_upgrade_order(upgrade_types::terran_infantry_armor_1, -2.0);
		get_upgrades::set_upgrade_value(upgrade_types::terran_infantry_weapons_2, -1.0);
		get_upgrades::set_upgrade_value(upgrade_types::terran_infantry_armor_2, -1.0);
		get_upgrades::set_upgrade_value(upgrade_types::terran_infantry_weapons_3, -1.0);
		get_upgrades::set_upgrade_value(upgrade_types::terran_infantry_armor_3, -1.0);

	}
	virtual bool tick() override {

		if (combat::no_aggressive_groups) {
			combat::no_aggressive_groups = current_used_total_supply < 120;
			combat::aggressive_groups_done_only = !combat::no_aggressive_groups;
			if (enemy_army_supply < 16 && current_used_total_supply - my_workers.size() >= 40) {
				combat::no_aggressive_groups = false;
				combat::aggressive_groups_done_only = false;
			}
		}

		if (my_units_of_type[unit_types::marine].size() >= 6) {
			combat::build_bunker_count = 2;
		}

		if (current_used_total_supply >= 120 && get_upgrades::no_auto_upgrades) {
			get_upgrades::set_no_auto_upgrades(false);
		}
		
		return false;
	}
	virtual bool build(buildpred::state&st) override {
		using namespace buildpred;

		auto army = this->army;

		army = [army](state&st) {
			return maxprod(st, unit_types::marine, army);
		};
		if (firebat_count < enemy_ground_small_army_supply && firebat_count < 6 && marine_count > firebat_count) {
			army = [army](state&st) {
				return maxprod(st, unit_types::firebat, army);
			};
		}

		if (army_supply >= 74.0 || ghost_count) {
			if (ghost_count < army_supply / 15.0) {
				army = [army](state&st) {
					return nodelay(st, unit_types::ghost, army);
				};
			}
		}

		if (medic_count < (marine_count + firebat_count + ghost_count) / 4) {
			army = [army](state&st) {
				return nodelay(st, unit_types::medic, army);
			};
		}

		if (st.used_supply[race_terran] >= 70) {
			if (count_units_plus_production(st, unit_types::science_vessel) < army_supply / 24.0) {
				army = [army](state&st) {
					return nodelay(st, unit_types::science_vessel, army);
				};
			}
		}
		
		if (tank_count < marine_count / 5) {
			army = [army](state&st) {
				return nodelay(st, unit_types::siege_tank_tank_mode, army);
			};
		}
		if (army_supply >= 60.0 && tank_count < enemy_ground_army_supply / 4.0 && marine_count > tank_count * 4) {
			army = [army](state&st) {
				return nodelay(st, unit_types::siege_tank_tank_mode, army);
			};
		}

		if (army_supply >= 8 && army_supply >= enemy_army_supply) {
			if (count_units_plus_production(st, unit_types::engineering_bay) < (army_supply >= 40.0 ? 2 : 1)) {
				army = [army](state&st) {
					return nodelay(st, unit_types::engineering_bay, army);
				};
			}
			if (count_units_plus_production(st, unit_types::science_vessel) == 0) {
				if (st.used_supply[race_terran] >= 70) {
					army = [army](state&st) {
						return nodelay(st, unit_types::science_vessel, army);
					};
				}
				if (count_units_plus_production(st, unit_types::missile_turret) == 0) {
					army = [army](state&st) {
						return nodelay(st, unit_types::missile_turret, army);
					};
				}
			}
		}
		if (count_units_plus_production(st, unit_types::academy) == 0) {
			army = [army](state&st) {
				return nodelay(st, unit_types::academy, army);
			};
		}

		if (current_used_total_supply >= 150.0 && army_supply >= enemy_army_supply * 4) {
			army = [army](state&st) {
				return nodelay(st, unit_types::wraith, army);
			};
		}

		if (tank_count == 0) {
			army = [army](state&st) {
				return nodelay(st, unit_types::siege_tank_tank_mode, army);
			};
		}

		if (marine_count < 4) {
			army = [army](state&st) {
				return nodelay(st, unit_types::marine, army);
			};
		}

		if (scv_count < 60) {
			army = [army](state&st) {
				return nodelay(st, unit_types::scv, army);
			};
		}

		if (force_expand && count_production(st, unit_types::cc) == 0) {
			army = [army](state&st) {
				return nodelay(st, unit_types::cc, army);
			};
		}

		return army(st);
	}

};

