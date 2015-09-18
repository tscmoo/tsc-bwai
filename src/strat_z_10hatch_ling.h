

struct strat_z_10hatch_ling : strat_z_base {


	virtual void init() override {

		sleep_time = 15;

		scouting::scout_supply = 10;

		default_worker_defence = false;

	}
	bool defence_fight_ok = true;
	virtual bool tick() override {
		if (opening_state == 0) {
			if (my_resource_depots.size() != 1 || my_workers.size() != 4) opening_state = -1;
			else {
				build::add_build_task(0.0, unit_types::drone);
				build::add_build_task(0.0, unit_types::drone);
				build::add_build_task(0.0, unit_types::drone);
				build::add_build_task(0.0, unit_types::drone);
				build::add_build_task(0.0, unit_types::drone);
				++opening_state;
			}
		} else if (opening_state == 1) {
			bo_gas_trick();
		} else if (opening_state == 2) {
			build::add_build_task(0.0, unit_types::hatchery);
			build::add_build_task(1.0, unit_types::spawning_pool);
			build::add_build_task(2.0, unit_types::drone);
			build::add_build_task(3.0, unit_types::overlord);
			build::add_build_task(4.0, unit_types::zergling);
			build::add_build_task(4.0, unit_types::zergling);
			build::add_build_task(4.0, unit_types::zergling);
			build::add_build_task(4.0, unit_types::zergling);
			++opening_state;
		} else if (opening_state != -1) {
			if (bo_all_done()) {
				opening_state = -1;
			}
		}

		defence_fight_ok = eval_combat(true, 1);

		bool anything_to_attack = false;
		for (unit*e : enemy_units) {
			if (e->gone) continue;
			if (e->is_flying) continue;
			if (e->type->is_worker) continue;
			anything_to_attack = true;
			break;
		}
		if (anything_to_attack) rm_all_scouts();
		else {
			if (!my_units_of_type[unit_types::zergling].empty()) {
				for (unit*u : my_units_of_type[unit_types::zergling]) {
					if (!scouting::is_scout(u)) scouting::add_scout(u);
				}
			} else {
				if (scouting::all_scouts.empty() && !my_units_of_type[unit_types::spawning_pool].empty()) {
					unit*u = get_best_score(my_workers, [&](unit*u) {
						if (u->controller->action != unit_controller::action_gather) return std::numeric_limits<double>::infinity();
						return 0.0;
					}, std::numeric_limits<double>::infinity());
					if (u) scouting::add_scout(u);
				}
			}
		}

		if (enemy_proxy_building_count && army_supply == 0.0) max_bases = 1;

		resource_gathering::max_gas = 150.0;
		combat::no_aggressive_groups = false;
		if (enemy_zergling_count || enemy_zealot_count) {
			combat::no_aggressive_groups = true;
			if ((int)my_units_of_type[unit_types::zergling].size() > enemy_army_supply * 2 + 4) combat::no_aggressive_groups = false;
		}

		return current_used_total_supply >= 40;
	}
	virtual bool build(buildpred::state&st) override {
		using namespace buildpred;
		if (opening_state != -1) return false;

		auto army = this->army;

		st.auto_build_hatcheries = st.minerals >= 300;

		army = [army](state&st) {
			return nodelay(st, unit_types::zergling, army);
		};

		//if (army_supply < enemy_army_supply || enemy_static_defence_count >= army_supply / 8.0) {
		if (enemy_static_defence_count >= army_supply / 8.0 && drone_count < 9 + enemy_static_defence_count * 6) {
			if (army_supply > enemy_attacking_army_supply + 8.0) {
				army = [army](state&st) {
					return nodelay(st, unit_types::drone, army);
				};
			}
		}
		if (enemy_zealot_count >= 8 && zergling_count >= 24) {
			army = [army](state&st) {
				return nodelay(st, unit_types::drone, army);
			};
			if (drone_count >= 18) {
				if (!defence_fight_ok) {
					if (sunken_count < 5) {
						army = [army](state&st) {
							return nodelay(st, unit_types::sunken_colony, army);
						};
					}
					if (is_defending) {
						army = [army](state&st) {
							return nodelay(st, unit_types::zergling, army);
						};
					}
				}
			}
		}
		if (drone_count < 9 + army_supply / 5.0 || (army_supply >= 12.0 && count_production(st, unit_types::drone) == 0) && drone_count < 14) {
//		if (drone_count < 9 + army_supply / 5.0) {
			army = [army](state&st) {
				return nodelay(st, unit_types::drone, army);
			};
		}
		if (drone_count >= 13) {
			if (count_units_plus_production(st, unit_types::extractor) == 0) {
				army = [army](state&st) {
					return nodelay(st, unit_types::extractor, army);
				};
			}
		}
		if (hydralisk_count > enemy_air_army_supply) {
			army = [army](state&st) {
				return nodelay(st, unit_types::hydralisk, army);
			};
		}

		return army(st);
	}

};

