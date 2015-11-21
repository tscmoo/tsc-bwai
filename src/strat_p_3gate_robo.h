

struct strat_p_3gate_robo: strat_p_base {


	virtual void init() override {

		sleep_time = 15;

	}

	bool defence_fight_ok = true;
	virtual bool tick() override {
		if (opening_state == 0) {
			if (my_resource_depots.size() != 1 || my_workers.size() != 4) opening_state = -1;
			else {
				build::add_build_task(0.0, unit_types::probe);
				build::add_build_task(0.0, unit_types::probe);
				build::add_build_task(0.0, unit_types::probe);
				build::add_build_task(0.0, unit_types::probe);
				build::add_build_task(1.0, unit_types::pylon);
				build::add_build_task(2.0, unit_types::probe);
				build::add_build_task(2.0, unit_types::probe);
				build::add_build_task(3.0, unit_types::gateway);
				build::add_build_task(4.0, unit_types::probe);
				build::add_build_task(4.0, unit_types::probe);
//				build::add_build_task(5.0, unit_types::assimilator);

// 				build::add_build_task(6.0, unit_types::probe);
// 				build::add_build_task(6.0, unit_types::probe);
// 				build::add_build_task(7.0, unit_types::cybernetics_core);
// 				build::add_build_task(8.0, unit_types::zealot);
// 				build::add_build_task(9.0, unit_types::pylon);
// 				build::add_build_task(10.0, unit_types::probe);
// 				build::add_build_task(10.0, unit_types::probe);
// 				build::add_build_task(11.0, unit_types::dragoon);
				++opening_state;
			}
		} else if (opening_state != -1) {
			if (bo_all_done()) {
				opening_state = -1;
			}
		}

		attack_interval = 15 * 60 * 2;

		defence_fight_ok = eval_combat(true, 0);

		if (!defence_fight_ok) attack_interval = 0;

		return !my_completed_units_of_type[unit_types::observer].empty() || current_used_total_supply >= 60;
	}
	virtual bool build(buildpred::state&st) override {
		using namespace buildpred;

		auto army = this->army;

		st.dont_build_refineries = enemy_gas_count == 0 && count_units_plus_production(st, unit_types::gateway) < 2;

		army = [army](state&st) {
			return maxprod(st, unit_types::zealot, army);
		};
		if (count_units_plus_production(st, unit_types::assimilator) || army_supply >= 10.0) {
			army = [army](state&st) {
				return maxprod(st, unit_types::dragoon, army);
			};
		}
		if (count_units_plus_production(st, unit_types::gateway) < 3) {
			army = [army](state&st) {
				return nodelay(st, unit_types::gateway, army);
			};
		}

		if (enemy_gateway_count < 2 || count_units_plus_production(st, unit_types::gateway) >= 3) {
			if (observer_count == 0) {
				army = [army](state&st) {
					return nodelay(st, unit_types::observer, army);
				};
			}
		}

// 		if (!defence_fight_ok) {
// 			if (count_units_plus_production(st, unit_types::shield_battery) < count_units_plus_production(st, unit_types::gateway)) {
// 				army = [army](state&st) {
// 					return nodelay(st, unit_types::shield_battery, army);
// 				};
// 			}
// 		}

		if (((enemy_army_supply >= 4.0 || !defence_fight_ok) && army_supply < 4.0) || st.units[unit_types::assimilator].empty()) {
			army = [army](state&st) {
				return nodelay(st, unit_types::zealot, army);
			};
		} else {
			army = [army](state&st) {
				return nodelay(st, unit_types::dragoon, army);
			};
		}

		if (count_units_plus_production(st, unit_types::gateway) >= 3 && defence_fight_ok) {
			if (observer_count == 0) {
				army = [army](state&st) {
					return nodelay(st, unit_types::observer, army);
				};
			}
		}

		if (defence_fight_ok && enemy_cannon_count >= 3) {
			if (count_units_plus_production(st, unit_types::nexus) < 1 + enemy_cannon_count / 2) {
				army = [army](state&st) {
					return nodelay(st, unit_types::nexus, army);
				};
			}
		}

		if (probe_count < 18 || count_units_plus_production(st, unit_types::gateway) >= 3) {
			if (!is_defending || defence_fight_ok) {
				army = [army](state&st) {
					return nodelay(st, unit_types::probe, army);
				};
			}
		}

		return army(st);
	}

};

