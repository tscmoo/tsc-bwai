

struct strat_t_2fact_vulture : strat_t_base {

	virtual void init() override {

		combat::aggressive_vultures = false;

		get_upgrades::set_upgrade_value(upgrade_types::ion_thrusters, -1.0);
		get_upgrades::set_upgrade_value(upgrade_types::spider_mines, -1.0);
		get_upgrades::set_upgrade_order(upgrade_types::ion_thrusters, -2.0);
		get_upgrades::set_upgrade_order(upgrade_types::spider_mines, -1.0);

		default_upgrades = false;

	}
	bool is_attacking = false;
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

		if (my_units_of_type[unit_types::vulture].size() >= 6) {
			is_attacking = true;
		}

		combat::no_aggressive_groups = !is_attacking;
		combat::aggressive_groups_done_only = false;
		
		if (enemy_air_army_supply) return true;
		if (current_used_total_supply >= 60) return true;
		return false;
	}
	virtual bool build(buildpred::state&st) override {
		using namespace buildpred;
		if (opening_state != -1) return false;

		army = [army = army](state&st) {
			return nodelay(st, unit_types::vulture, army);
		};

		if (marine_count < 1) {
			army = [army = army](state&st) {
				return nodelay(st, unit_types::marine, army);
			};
		}

		if (count_units_plus_production(st, unit_types::factory) < 2) {
			army = [army = army](state&st) {
				return nodelay(st, unit_types::factory, army);
			};
		}

		int machine_shops = count_production(st, unit_types::machine_shop);
		for (auto&v : st.units[unit_types::factory]) {
			if (v.has_addon) ++machine_shops;
		}
		if (machine_shops < 2) {
			army = [army = army](state&st) {
				return nodelay(st, unit_types::machine_shop, army);
			};
		}

		if (scv_count < 60) {
			army = [army = army](state&st) {
				return nodelay(st, unit_types::scv, army);
			};
		}

		if (st.minerals >= 400) {
			if (count_units_plus_production(st, unit_types::cc) < 3) {
				army = [army = army](state&st) {
					return nodelay(st, unit_types::cc, army);
				};
			} else {
				army = [army = army](state&st) {
					return nodelay(st, unit_types::factory, army);
				};
			}
		}

		return army(st);
	}

};

