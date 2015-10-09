

struct strat_z_vr_scout : strat_z_base {


	virtual void init() override {

		sleep_time = 8;

		scouting::scout_supply = 20;

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
				build::add_build_task(0.0, unit_types::overlord);
				++opening_state;
			}
		} else if (opening_state != -1) {
			if (bo_all_done()) {
				opening_state = -1;
			}
		}

		int scouts = 0;
		if (start_locations.size() <= 2) {
			if (current_used_total_supply >= 7) scouts = 1;
		} else {
			if (current_used_total_supply >= 6) scouts = 1;
			if (current_used_total_supply >= 8) scouts = 2;
		}

		if ((int)scouting::all_scouts.size() < scouts) {
			unit*scout_unit = get_best_score(my_workers, [&](unit*u) {
				if (u->controller->action != unit_controller::action_gather) return std::numeric_limits<double>::infinity();
				return 0.0;
			}, std::numeric_limits<double>::infinity());
			if (scout_unit) scouting::add_scout(scout_unit);
		}

		if (!players::opponent_player->random) {
			rm_all_scouts();
		}

		return !players::opponent_player->random;
	}
	virtual bool build(buildpred::state&st) override {
		return false;
	}

};

