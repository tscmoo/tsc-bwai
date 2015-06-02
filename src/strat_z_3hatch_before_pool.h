

struct strat_z_3hatch_before_pool : strat_z_base {


	virtual void init() override {

		sleep_time = 15;

		scouting::scout_supply = 10;

		min_bases = 3;

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
				++opening_state;
			}
		} else if (opening_state == 1) {
			if (bo_all_done()) {
				build::add_build_task(0.0, unit_types::hatchery);
				build::add_build_task(1.0, unit_types::drone);
				build::add_build_task(1.0, unit_types::drone);
				build::add_build_task(2.0, unit_types::hatchery);
				build::add_build_task(3.0, unit_types::spawning_pool);
				++opening_state;
			}
		} else if (opening_state == 3) {
			if (being_rushed) bo_cancel_all();
			if (bo_all_done()) {
				++opening_state;
			}
		} else if (opening_state != -1) {
			if (bo_all_done()) {
				opening_state = -1;
			}
		}

		return opening_state == -1;
	}
	virtual bool build(buildpred::state&st) override {
		return false;
	}

};

