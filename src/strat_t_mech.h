

struct strat_t_mech : strat_t_base {

	virtual void init() override {

		default_upgrades = false;

		combat::aggressive_vultures = true;
		combat::defensive_spider_mine_count = 30;

		combat::build_bunker_count = 0;

		get_upgrades::set_upgrade_value(upgrade_types::siege_mode, -1.0);
		get_upgrades::set_upgrade_order(upgrade_types::siege_mode, -10.0);

		get_upgrades::set_upgrade_value(upgrade_types::spider_mines, -1.0);
		get_upgrades::set_upgrade_value(upgrade_types::ion_thrusters, -1.0);
		get_upgrades::set_upgrade_order(upgrade_types::spider_mines, -2.0);
		get_upgrades::set_upgrade_order(upgrade_types::ion_thrusters, -1.0);

	}
	virtual bool tick() override {

// 		if (combat::no_aggressive_groups) {
// 			combat::no_aggressive_groups = current_used_total_supply < 120;
// 			combat::aggressive_groups_done_only = !combat::no_aggressive_groups;
// 			if (enemy_army_supply < 16 && current_used_total_supply - my_workers.size() >= 40) {
// 				combat::no_aggressive_groups = false;
// 				combat::aggressive_groups_done_only = false;
// 			}
// 		}
		combat::no_aggressive_groups = current_used_total_supply < 120 || current_used_total_supply - my_workers.size() < enemy_army_supply + 8.0;
		combat::aggressive_groups_done_only = combat::no_aggressive_groups;
		if (enemy_army_supply < 16 && current_used_total_supply - my_workers.size() >= 40) {
			combat::no_aggressive_groups = false;
			combat::aggressive_groups_done_only = false;
		}

// 		if ((players::opponent_player->race != race_terran && current_used_total_supply >= 32) || enemy_dt_count + enemy_lurker_count) {
// 			if (current_used_total_supply - my_workers.size() >= enemy_army_supply) {
// 				combat::build_missile_turret_count = 2;
// 			}
// 		}

		if (current_used_total_supply >= 120 && get_upgrades::no_auto_upgrades) {
			get_upgrades::set_no_auto_upgrades(false);
		}
		
		return false;
	}
	virtual bool build(buildpred::state&st) override {
		using namespace buildpred;

		auto army = this->army;

		army = [army](state&st) {
			return maxprod(st, unit_types::vulture, army);
		};
		if (vulture_count >= tank_count * 2) {
			army = [army](state&st) {
				return maxprod(st, unit_types::siege_tank_tank_mode, army);
			};
		}

		if (enemy_air_army_supply > goliath_count * 2) {
			army = [army](state&st) {
				return maxprod(st, unit_types::goliath, army);
			};
		}
		if (army_supply >= 34.0) {
			int max_vessels = enemy_dt_count / 4 + enemy_lurker_count / 4 + enemy_arbiter_count;
			if (army_supply >= 60.0 && max_vessels < 2) max_vessels = 2;
			if (science_vessel_count < max_vessels) {
				army = [army](state&st) {
					return nodelay(st, unit_types::science_vessel, army);
				};
			}
		}
		if ((army_supply >= 24.0 && players::opponent_player->race != race_terran) || enemy_dt_count + enemy_lurker_count) {
		//if (enemy_dt_count + enemy_lurker_count) {
			if (science_vessel_count == 0) {
				army = [army](state&st) {
					return nodelay(st, unit_types::science_vessel, army);
				};
			}
		}

		if (army_supply >= 20.0 && (players::opponent_player->race == race_terran || enemy_tank_count)) {
			if (wraith_count < 4) {
				army = [army](state&st) {
					return nodelay(st, unit_types::wraith, army);
				};
			}
		}
// 		if (army_supply >= 20.0) {
// 			if (players::my_player->has_upgrade(upgrade_types::cloaking_field) || army_supply <= 80.0) {
// 				if (wraith_count < 4) {
// 					army = [army](state&st) {
// 						return nodelay(st, unit_types::wraith, army);
// 					};
// 				}
// 			}
// 		}

		if (st.minerals > 200 && st.gas < 50) {
			army = [army](state&st) {
				return maxprod(st, unit_types::factory, army);
			};
			army = [army](state&st) {
				return maxprod(st, unit_types::vulture, army);
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

