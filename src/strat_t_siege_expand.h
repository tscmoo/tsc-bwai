

struct strat_t_siege_expand : strat_t_base {

	virtual void init() override {

		combat::defensive_spider_mine_count = 30;
		combat::aggressive_vultures = true;

		default_upgrades = false;

	}
	xy natural_pos;
	virtual bool tick() override {
		if (opening_state == 0) {
			if (my_resource_depots.size() != 1 || my_workers.size() != 4) opening_state = -1;
			else {
				build::add_build_task(0.0, unit_types::scv);
				build::add_build_task(0.0, unit_types::scv);
				build::add_build_task(0.0, unit_types::scv);
				build::add_build_task(0.0, unit_types::scv);
				build::add_build_task(0.0, unit_types::scv);
				build::add_build_task(1.0, unit_types::supply_depot);
				build::add_build_task(2.0, unit_types::scv);
				build::add_build_task(2.0, unit_types::scv);
				build::add_build_task(3.0, unit_types::barracks);
				build::add_build_task(4.0, unit_types::refinery);
				++opening_state;
			}
		} else if (opening_state == 1) {
			if (bo_all_done()) {
				opening_state = -1;
			}
		}

		if (my_units_of_type[unit_types::marine].size() >= 1) {
			if (natural_pos == xy()) {
				natural_pos = get_next_base()->pos;
			}
			combat::build_bunker_count = 1;
		}
		if (natural_pos != xy()) {
			combat::my_closest_base_override = natural_pos;
			combat::my_closest_base_override_until = current_frame + 15 * 10;
		}

		if (!my_units_of_type[unit_types::siege_tank_tank_mode].empty()) {
			get_upgrades::set_upgrade_value(upgrade_types::siege_mode, -1.0);

			combat::build_bunker_count = 1;
		}
		if (!my_units_of_type[unit_types::vulture].empty()) {
			get_upgrades::set_upgrade_value(upgrade_types::spider_mines, -1.0);
		}
// 		if (!my_completed_units_of_type[unit_types::engineering_bay].empty()) {
// 			combat::build_missile_turret_count = 1;
// 		}

		resource_gathering::max_gas = 150.0;

		combat::no_aggressive_groups = true;
		combat::aggressive_groups_done_only = true;
		
		if (enemy_air_army_supply) return true;
		if (current_used_total_supply >= 48) return true;
		return false;
	}
	virtual bool build(buildpred::state&st) override {
		using namespace buildpred;
		if (opening_state != -1) return false;

		if (count_units_plus_production(st, unit_types::cc) >= 2) {
			army = [army = army](state&st) {
				return nodelay(st, unit_types::factory, army);
			};
		}

		army = [army = army](state&st) {
			return nodelay(st, unit_types::marine, army);
		};

		army = [army = army](state&st) {
			return nodelay(st, unit_types::vulture, army);
		};

		if (tank_count && count_units_plus_production(st, unit_types::missile_turret) == 0) {
			army = [army = army](state&st) {
				return nodelay(st, unit_types::missile_turret, army);
			};
		}

		if (scv_count < 60) {
			army = [army = army](state&st) {
				return nodelay(st, unit_types::scv, army);
			};
		}
		
		if (tank_count < 2 + vulture_count / 2) {
			army = [army = army](state&st) {
				return nodelay(st, unit_types::siege_tank_tank_mode, army);
			};
		}

// 		if (count_units_plus_production(st, unit_types::factory) < 1) {
// 			army = [army = army](state&st) {
// 				return nodelay(st, unit_types::factory, army);
// 			};
// 		}

// 		int machine_shops = count_production(st, unit_types::machine_shop);
// 		for (auto&v : st.units[unit_types::factory]) {
// 			if (v.has_addon) ++machine_shops;
// 		}
// 		if (machine_shops < 1) {
// 			army = [army = army](state&st) {
// 				return nodelay(st, unit_types::machine_shop, army);
// 			};
// 		}

		if (st.minerals >= 400 || tank_count + vulture_count >= 3) {
			if (count_units_plus_production(st, unit_types::cc) < 2) {
				army = [army = army](state&st) {
					return nodelay(st, unit_types::cc, army);
				};
			}
		}

		return army(st);
	}

};

