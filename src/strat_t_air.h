

struct strat_t_air : strat_t_base {

	virtual void init() override {

		combat::aggressive_vultures = true;
		combat::aggressive_wraiths = false;

	}
	int max_workers = 0;
	virtual bool tick() override {

		if (combat::no_aggressive_groups) {
			combat::no_aggressive_groups = current_used_total_supply < 120;
			combat::aggressive_groups_done_only = !combat::no_aggressive_groups;
			if (enemy_army_supply < 16 && current_used_total_supply - my_workers.size() >= 40) {
				combat::no_aggressive_groups = false;
				combat::aggressive_groups_done_only = false;
			}
		}

		if (!my_units_of_type[unit_types::battlecruiser].empty()) {
			get_upgrades::set_upgrade_value(upgrade_types::yamato_gun, -1.0);
		}

		if (current_used_total_supply >= 120 && get_upgrades::no_auto_upgrades) {
			get_upgrades::set_no_auto_upgrades(false);
		}

		max_workers = get_max_mineral_workers();
		
		return false;
	}
	virtual bool build(buildpred::state&st) override {
		using namespace buildpred;

		auto army = this->army;

		army = [army](state&st) {
			return nodelay(st, unit_types::marine, army);
		};

		army = [army](state&st) {
			return nodelay(st, unit_types::vulture, army);
		};
		army = [army](state&st) {
			return maxprod(st, unit_types::battlecruiser, army);
		};

		if (tank_count < army_supply / 18.0) {
			army = [army](state&st) {
				return maxprod(st, unit_types::siege_tank_tank_mode, army);
			};
		}

		if (science_vessel_count < (army_supply - 40) / 24.0) {
			army = [army](state&st) {
				return nodelay(st, unit_types::science_vessel, army);
			};
		}

		if (enemy_air_army_supply > wraith_count * 2) {
			army = [army](state&st) {
				return maxprod(st, unit_types::wraith, army);
			};
		}

		if (scv_count < max_workers || battlecruiser_count >= 3 || current_used_total_supply >= 80) {
			if (scv_count < 60) {
				army = [army](state&st) {
					return nodelay(st, unit_types::scv, army);
				};
			}
		}

		if (force_expand && count_production(st, unit_types::cc) == 0) {
			army = [army](state&st) {
				return nodelay(st, unit_types::cc, army);
			};
		}

		return army(st);
	}

};

