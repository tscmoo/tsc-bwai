

struct strat_t_14cc : strat_t_base {


	virtual void init() override {

	}
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
				build::add_build_task(2.0, unit_types::scv);
				build::add_build_task(2.0, unit_types::scv);
				build::add_build_task(2.0, unit_types::scv);
				build::add_build_task(3.0, unit_types::cc);
				build::add_build_task(4.0, unit_types::scv);
				build::add_build_task(5.0, unit_types::barracks);
				build::add_build_task(6.0, unit_types::scv);
				build::add_build_task(7.0, unit_types::refinery);
				++opening_state;
			}
		} else if (opening_state == 1) {
			if (bo_all_done()) {
				opening_state = -1;
			}
		}

		if (my_units_of_type[unit_types::marine].size() >= 2) {
			combat::build_bunker_count = 1;
		}

		return !my_completed_units_of_type[unit_types::factory].empty();
	}
	virtual bool build(buildpred::state&st) override {
		using namespace buildpred;
		if (opening_state != -1) return false;

		if (marine_count < 3) {
			army = [army = army](state&st) {
				return nodelay(st, unit_types::marine, army);
			};
		}

		if (scv_count < 40) {
			army = [army = army](state&st) {
				return nodelay(st, unit_types::scv, army);
			};
		}

		if (count_units_plus_production(st, unit_types::factory) == 0) {
			army = [army = army](state&st) {
				return nodelay(st, unit_types::factory, army);
			};
		}

		return army(st);
	}

};

