

struct strat_t_8rax_fe : strat_t_base {


	virtual void init() override {

		sleep_time = 15;

	}
	bool go_attack = false;
	virtual bool tick() override {
		if (opening_state == 0) {
			if (my_resource_depots.size() != 1 || my_workers.size() != 4) opening_state = -1;
			else {
				build::add_build_task(0.0, unit_types::scv);
				build::add_build_task(0.0, unit_types::scv);
				build::add_build_task(0.0, unit_types::scv);
				build::add_build_task(0.0, unit_types::scv);
				build::add_build_task(1.0, unit_types::barracks);
				build::add_build_task(2.0, unit_types::scv);
				build::add_build_task(3.0, unit_types::supply_depot);
				++opening_state;
			}
		} else if (opening_state == 1) {
			if (bo_all_done()) {
				opening_state = -1;
			}
		}
		
		if (being_rushed) rm_all_scouts();

		if (my_completed_units_of_type[unit_types::marine].size() >= 2) go_attack = true;

		combat::no_aggressive_groups = !go_attack;
		combat::aggressive_groups_done_only = false;

		combat::force_defence_is_scared_until = current_frame + 15 * 10;

		//if (current_used_total_supply >= 20) return true;
		if (my_units_of_type[unit_types::cc].size() >= 2) return true;
		return false;
	}
	virtual bool build(buildpred::state&st) override {
		using namespace buildpred;
		if (opening_state != -1) return false;

		st.dont_build_refineries = count_units_plus_production(st, unit_types::cc) < 2;

		

		if (scv_count < 60) {
			army = [army = army](state&st) {
				return nodelay(st, unit_types::scv, army);
			};
		}

		if (my_units_of_type[unit_types::marine].size() >= 4 && st.used_supply[race_terran] >= 15 && !being_rushed) {
			if (count_units_plus_production(st, unit_types::cc) < 2) {
				army = [army = army](state&st) {
					return nodelay(st, unit_types::cc, army);
				};
			}
		}

		army = [army = army](state&st) {
			return nodelay(st, unit_types::marine, army);
		};

		if (being_rushed || (enemy_army_supply >= 8.0 && enemy_army_supply + 4.0 > army_supply)) {
			army = [army = army](state&st) {
				return maxprod(st, unit_types::marine, army);
			};
		}

		if (marine_count >= 8 && army_supply > enemy_army_supply) {
			if (count_units_plus_production(st, unit_types::cc) < 2) {
				army = [army = army](state&st) {
					return nodelay(st, unit_types::cc, army);
				};
			}
		}

		return army(st);
	}

};

