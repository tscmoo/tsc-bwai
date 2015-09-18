

struct strat_z_fast_mass_expand : strat_z_base {


	virtual void init() override {

		sleep_time = 15;

		scouting::scout_supply = 60;

		min_bases = 10;

		combat::no_aggressive_groups = false;

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

		return current_frame >= 15 * 60 * 20;
		//return current_used_total_supply >= 190;
	}
	virtual bool build(buildpred::state&st) override {
		using namespace buildpred;
		if (opening_state != -1) return false;

		auto army = this->army;

		st.auto_build_hatcheries = true;
		st.dont_build_refineries = drone_count < 50;

		if (drone_count < 100) {
			army = [army](state&st) {
				return nodelay(st, unit_types::drone, army);
			};
		}

		if (hatch_count < 8) {
			army = [army](state&st) {
				return nodelay(st, unit_types::hatchery, army);
			};
		}

		if (hatch_count >= 5) {
			if (count_units_plus_production(st, unit_types::greater_spire) == 0) {
				army = [army](state&st) {
					return nodelay(st, unit_types::greater_spire, army);
				};
			}
			if (count_units_plus_production(st, unit_types::hive) == 0) {
				army = [army](state&st) {
					return nodelay(st, unit_types::hive, army);
				};
			}
			if (count_units_plus_production(st, unit_types::extractor) == 0) {
				army = [army](state&st) {
					return nodelay(st, unit_types::extractor, army);
				};
			}
		}

		if (drone_count < 12) {
			army = [army](state&st) {
				return nodelay(st, unit_types::drone, army);
			};
		}

		if (is_defending && zergling_count < 18) {
			army = [army](state&st) {
				return nodelay(st, unit_types::zergling, army);
			};
		}
		if ((!is_defending || enemy_attacking_army_supply < 6.0) && enemy_army_supply > 2.0 || enemy_gateway_count >= 2 || enemy_barracks_count >= 2) {
			if (sunken_count == 0) {
				army = [army](state&st) {
					return nodelay(st, unit_types::sunken_colony, army);
				};
			}
		}

		if (count_units_plus_production(st, unit_types::greater_spire)) {
			if (drone_count >= 66) {
				army = [army](state&st) {
					return nodelay(st, unit_types::mutalisk, army);
				};
				army = [army](state&st) {
					return nodelay(st, unit_types::guardian, army);
				};
			}
// 			if (devourer_count < enemy_air_army_supply) {
// 				army = [army](state&st) {
// 					return nodelay(st, unit_types::devourer, army);
// 				};
// 			}
			if (scourge_count < enemy_air_army_supply * 2) {
				army = [army](state&st) {
					return nodelay(st, unit_types::scourge, army);
				};
			}
		}

		return army(st);
	}

};

