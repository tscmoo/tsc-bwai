

struct strat_z_5pool: strat_z_base {


	virtual void init() override {

		sleep_time = 15;

		scouting::scout_supply = 30;
		scouting::no_proxy_scout = true;

		default_worker_defence = false;

	}
	virtual bool tick() override {
		if (opening_state == 0) {
			if (my_resource_depots.size() != 1 || my_workers.size() > 9) opening_state = -1;
			else {
				build::add_build_total(0.0, unit_types::drone, 5);
				build::add_build_task(1.0, unit_types::spawning_pool);
				build::add_build_total(2.0, unit_types::drone, 6);
				++opening_state;
			}
		} else if (opening_state == 1) {
			if (my_units_of_type[unit_types::larva].size() >= 3 && current_minerals >= 150) {
				build::add_build_task(3.0, unit_types::zergling);
				build::add_build_task(3.0, unit_types::zergling);
				build::add_build_task(3.0, unit_types::zergling);
				++opening_state;
			}
		} else if (opening_state == 2) {
			bo_gas_trick(unit_types::zergling);
		} else if (opening_state == 3) {
			build::add_build_total(0.0, unit_types::overlord, 2);
			build::add_build_task(1.0, unit_types::zergling);
			build::add_build_task(1.0, unit_types::zergling);
			++opening_state;
		} else if (opening_state != -1) {
			if (bo_all_done()) {
				opening_state = -1;
			}
		}

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
// 					update_possible_start_locations();
// 					if (possible_start_locations.size() > 1) {
					if (true) {
						unit*u = get_best_score(my_workers, [&](unit*u) {
							if (u->controller->action != unit_controller::action_gather) return std::numeric_limits<double>::infinity();
							return 0.0;
						}, std::numeric_limits<double>::infinity());
						if (u) scouting::add_scout(u);
					}
				}
			}
		}

		combat::combat_mult_override = 1.0;
		combat::combat_mult_override_until = current_frame + 15 * 10;

		combat::no_aggressive_groups = false;
		combat::aggressive_groups_done_only = false;
		if ((being_rushed || enemy_attacking_army_supply) && current_used_total_supply < 12.0) {
			combat::no_aggressive_groups = true;
		}
		
		return my_workers.size() >= 12 || current_used_total_supply >= 24;
	}
	virtual bool build(buildpred::state&st) override {
		using namespace buildpred;
		if (opening_state != -1) return false;

		auto army = this->army;

// 		army = [army](state&st) {
// 			return nodelay(st, unit_types::hatchery, army);
// 		};
		st.auto_build_hatcheries = true;

		if (drone_count < 40) {
			army = [army](state&st) {
				return nodelay(st, unit_types::drone, army);
			};
		}

		if ((players::opponent_player->minerals_lost < 250 && players::my_player->minerals_lost < 250) || is_defending || (being_rushed && count_production(st, unit_types::drone))) {
			if (enemy_static_defence_count == 0 || army_supply < enemy_army_supply + 4) {
				army = [army](state&st) {
					return nodelay(st, unit_types::zergling, army);
				};
			}
		}
		if (players::opponent_player->race == race_zerg && drone_count >= 9 && army_supply < drone_count * 2) {
			army = [army](state&st) {
				return nodelay(st, unit_types::zergling, army);
			};
		}
		if (army_supply < enemy_army_supply + 2.0) {
			army = [army](state&st) {
				return nodelay(st, unit_types::zergling, army);
			};
		}

		return army(st);
	}

};

