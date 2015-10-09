

struct strat_z_12hatch : strat_z_base {


	virtual void init() override {

		sleep_time = 15;

		scouting::scout_supply = 10;

	}
	virtual bool tick() override {
		if (opening_state == 0) {
			if (my_resource_depots.size() != 1 || my_workers.size() > 9) opening_state = -1;
			else {
				build::add_build_total(0.0, unit_types::drone, 9);
				build::add_build_total(1.0, unit_types::overlord, 2);
				build::add_build_task(2.0, unit_types::drone);
				build::add_build_task(2.0, unit_types::drone);
				build::add_build_task(2.0, unit_types::drone);
				build::add_build_task(3.0, unit_types::hatchery);
				build::add_build_task(4.0, unit_types::spawning_pool);
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

