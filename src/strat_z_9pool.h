

struct strat_z_9pool: strat_z_base {


	virtual void init() override {

		sleep_time = 15;

		scouting::scout_supply = 10;

	}
	virtual bool tick() override {
		if (opening_state == 0) {
			if (my_resource_depots.size() != 1 || my_workers.size() > 9) opening_state = -1;
			else {
				build::add_build_total(0.0, unit_types::drone, 9);
				build::add_build_task(1.0, unit_types::spawning_pool);
				build::add_build_task(2.0, unit_types::drone);
				++opening_state;
			}
		} else if (opening_state == 1) {
			bo_gas_trick();
		} else if (opening_state == 2) {
			build::add_build_total(0.0, unit_types::overlord, 2);
			build::add_build_task(1.0, unit_types::zergling);
			build::add_build_task(1.0, unit_types::zergling);
			build::add_build_task(1.0, unit_types::zergling);
			if (players::opponent_player->race != race_zerg && !players::opponent_player->random) build::add_build_task(2.0, unit_types::hatchery);
			++opening_state;
		} else if (opening_state != -1) {
			if (enemy_attacking_army_supply || enemy_proxy_building_count) bo_cancel_all();
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

