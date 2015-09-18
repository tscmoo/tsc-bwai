

struct strat_t_1rax_fe : strat_t_base {


	virtual void init() override {

		sleep_time = 15;

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
				++opening_state;
			}
		} else if (opening_state == 1) {
			if (bo_all_done()) {
				opening_state = -1;
			}
		}
		
		if (being_rushed) rm_all_scouts();
		
		if (my_units_of_type[unit_types::marine].size() >= 1) {
			if (natural_pos == xy()) {
				natural_pos = get_next_base()->pos;
			}
			if (players::opponent_player->race != race_zerg) {
				combat::build_bunker_count = 1;
			} else {
				if (being_rushed) combat::force_defence_is_scared_until = current_frame + 15 * 10;
			}
		}
		if (natural_pos != xy() && !being_rushed) {
			combat::my_closest_base_override = natural_pos;
			combat::my_closest_base_override_until = current_frame + 15 * 10;
		}

		combat::no_aggressive_groups = true;
		combat::aggressive_groups_done_only = true;

		if (current_used_total_supply >= 48) return true;
		if (my_units_of_type[unit_types::cc].size() >= 2) return true;
		return false;
	}
	virtual bool build(buildpred::state&st) override {
		using namespace buildpred;
		if (opening_state != -1) return false;

		st.dont_build_refineries = count_units_plus_production(st, unit_types::cc) < 2;

		army = [army = army](state&st) {
			return nodelay(st, unit_types::marine, army);
		};
		if (st.minerals >= 150 && being_rushed) {
			army = [army = army](state&st) {
				return maxprod(st, unit_types::marine, army);
			};
		}

		if (scv_count < 60) {
			army = [army = army](state&st) {
				return nodelay(st, unit_types::scv, army);
			};
		}

		if (my_units_of_type[unit_types::marine].size() >= 2 && st.used_supply[race_terran] >= 15 && !being_rushed) {
			if (count_units_plus_production(st, unit_types::cc) < 2) {
				army = [army = army](state&st) {
					return nodelay(st, unit_types::cc, army);
				};
			}
		}
		if (marine_count < 2) {
			army = [army = army](state&st) {
				return nodelay(st, unit_types::marine, army);
			};
		}

		return army(st);
	}

};

