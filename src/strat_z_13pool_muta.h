

struct strat_z_13pool_muta : strat_z_base {


	virtual void init() override {

		sleep_time = 15;

		scouting::scout_supply = 20;

		default_upgrades = false;

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
				build::add_build_task(1.0, unit_types::overlord);
				build::add_build_task(2.0, unit_types::drone);
				build::add_build_task(2.0, unit_types::drone);
				build::add_build_task(2.0, unit_types::drone);
				build::add_build_task(2.0, unit_types::drone);
				build::add_build_task(3.0, unit_types::spawning_pool);
				build::add_build_task(4.0, unit_types::extractor);
				build::add_build_task(5.0, unit_types::drone);
				build::add_build_task(5.0, unit_types::drone);
				build::add_build_task(6.0, unit_types::lair);
				build::add_build_task(7.0, unit_types::hatchery);
				++opening_state;
			}
		} else if (opening_state != -1) {
			if (bo_all_done()) {
				opening_state = -1;
			}
		}

		combat::no_aggressive_groups = my_units_of_type[unit_types::mutalisk].empty();
		combat::aggressive_groups_done_only = false;

		resource_gathering::max_gas = 100.0;
		if (!my_units_of_type[unit_types::lair].empty()) {
			resource_gathering::max_gas = 600.0;
		}

		return current_used_total_supply >= 60;
	}
	virtual bool build(buildpred::state&st) override {
		using namespace buildpred;
		if (opening_state != -1) return false;

		st.dont_build_refineries = true;

		auto army = this->army;

		if (mutalisk_count >= 6) {
			army = [army](state&st) {
				return nodelay(st, unit_types::hatchery, army);
			};

			army = [army](state&st) {
				return nodelay(st, unit_types::drone, army);
			};
		}

		if (drone_count < 18) {
			army = [army](state&st) {
				return nodelay(st, unit_types::drone, army);
			};
		}

		if (count_units_plus_production(st, unit_types::lair) == 0) {
			army = [army](state&st) {
				return nodelay(st, unit_types::lair, army);
			};
		} else {
			if (drone_count < 13) {
				army = [army](state&st) {
					return nodelay(st, unit_types::drone, army);
				};
			} else {
				if (count_units_plus_production(st, unit_types::spire) == 0) {
					army = [army](state&st) {
						return nodelay(st, unit_types::spire, army);
					};
				}
			}
		}

		if (zergling_count < 4) {
			army = [army](state&st) {
				return nodelay(st, unit_types::zergling, army);
			};
		}

		if (is_defending) {
			army = [army](state&st) {
				return nodelay(st, unit_types::zergling, army);
			};
		}

		if (drone_count >= 18) {
			army = [army](state&st) {
				return nodelay(st, unit_types::mutalisk, army);
			};
		}

		return army(st);
	}

};

