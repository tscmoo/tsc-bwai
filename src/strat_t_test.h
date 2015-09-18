

struct strat_t_test : strat_t_base {


	virtual void init() override {

		sleep_time = 15;

		combat::no_aggressive_groups = true;

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
				build::add_build_task(4.0, unit_types::scv);
				build::add_build_task(5.0, unit_types::barracks);
				++opening_state;
			}
		} else if (opening_state == 1) {
			if (bo_all_done()) {
				opening_state = -1;
			}
		}
		
		if (being_rushed) rm_all_scouts();

		if (current_used_total_supply >= 35.0) {
			combat::no_aggressive_groups = false;
			combat::aggressive_groups_done_only = false;
		}

// 		if (current_used_total_supply >= 48) return true;
// 		if (my_units_of_type[unit_types::cc].size() >= 2) return true;
		return false;
	}
	virtual bool build(buildpred::state&st) override {
		using namespace buildpred;
		if (opening_state != -1) return false;

		st.dont_build_refineries = count_units_plus_production(st, unit_types::cc) < 2;

		army = [army = army](state&st) {
			return maxprod(st, unit_types::marine, army);
		};

		if (marine_count >= 6 && medic_count < marine_count / 4) {
			army = [army = army](state&st) {
				return maxprod(st, unit_types::medic, army);
			};
		}

		if (scv_count < 60) {
			army = [army = army](state&st) {
				return nodelay(st, unit_types::scv, army);
			};
		}

		return army(st);
	}

};

