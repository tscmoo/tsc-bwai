

struct strat_z_1hatch_lurker : strat_z_base {


	virtual void init() override {

		sleep_time = 15;

		scouting::scout_supply = 20;

		default_upgrades = false;

		get_upgrades::set_upgrade_value(upgrade_types::lurker_aspect, -1.0);

	}
	virtual bool tick() override {
		if (opening_state == 0) {
			if (my_resource_depots.size() != 1 || my_workers.size() != 4) opening_state = -1;
			else {
				build::add_build_task(0.0, unit_types::drone);
				build::add_build_task(0.0, unit_types::drone);
				build::add_build_task(0.0, unit_types::drone);
				build::add_build_task(0.0, unit_types::drone);
				build::add_build_task(0.0, unit_types::drone);
				build::add_build_task(1.0, unit_types::spawning_pool);
				build::add_build_task(2.0, unit_types::drone);
				build::add_build_task(3.0, unit_types::extractor);
				build::add_build_task(4.0, unit_types::overlord);
				build::add_build_task(3.0, unit_types::drone);
				build::add_build_task(5.0, unit_types::zergling);
				build::add_build_task(5.0, unit_types::zergling);
				build::add_build_task(5.0, unit_types::zergling);
				++opening_state;
			}
		} else if (opening_state != -1) {
			if (bo_all_done()) {
				opening_state = -1;
			}
		}

		combat::no_aggressive_groups = my_units_of_type[unit_types::lurker].empty();
		combat::aggressive_groups_done_only = false;

		resource_gathering::max_gas = 100.0;
		if (!my_units_of_type[unit_types::lair].empty()) {
			resource_gathering::max_gas = 500.0;
		}

		return current_used_total_supply >= 40;
	}
	virtual bool build(buildpred::state&st) override {
		using namespace buildpred;
		if (opening_state != -1) return false;

		auto army = this->army;

		army = [army](state&st) {
			return nodelay(st, unit_types::hatchery, army);
		};

		if (count_units_plus_production(st, unit_types::lair) == 0) {
			army = [army](state&st) {
				return nodelay(st, unit_types::lair, army);
			};
		} else {
			if (drone_count < 11) {
				army = [army](state&st) {
					return nodelay(st, unit_types::drone, army);
				};
			} else {
				if (count_units_plus_production(st, unit_types::hydralisk_den) == 0) {
					army = [army](state&st) {
						return nodelay(st, unit_types::hydralisk_den, army);
					};
				}
			}
		}

		if (is_defending) {
			army = [army](state&st) {
				return nodelay(st, unit_types::zergling, army);
			};
		}
		
		if (!st.units[unit_types::hydralisk_den].empty()) {
			if (lurker_count == 0 && hydralisk_count < 4) {
				army = [army](state&st) {
					return nodelay(st, unit_types::hydralisk, army);
				};
			} else {
				army = [army](state&st) {
					return nodelay(st, unit_types::lurker, army);
				};
			}
		}
		if (lurker_count >= 4 && drone_count < army_supply * 2) {
			army = [army](state&st) {
				return nodelay(st, unit_types::drone, army);
			};
		}

		return army(st);
	}

};

