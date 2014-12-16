
struct strat_tvz_opening {

	void run() {

		combat::no_aggressive_groups = true;

		using namespace buildpred;

		auto build = [&](state&st) {
			if (count_units_plus_production(st, unit_types::vulture) + count_units_plus_production(st, unit_types::vulture) == 0) {
				if (!my_completed_units_of_type[unit_types::machine_shop].empty()) {
					if (count_units_plus_production(st, unit_types::starport) == 0) {
						return depbuild(st, state(st), unit_types::starport);
					}
					return depbuild(st, state(st), unit_types::vulture);
				}
			}
			return nodelay(st, unit_types::scv, [&](state&st) {
				st.dont_build_refineries = true;
				if (count_units_plus_production(st, unit_types::refinery) == 0) {
					return depbuild(st, state(st), unit_types::refinery);
				}
				auto backbone = [&](state&st) {
					return maxprod(st, unit_types::goliath, [&](state&st) {
						return maxprod1(st, unit_types::vulture);
					});
				};
				int vulture_count = count_units_plus_production(st, unit_types::vulture);
				int goliath_count = count_units_plus_production(st, unit_types::goliath);
				int siege_tank_count = count_units_plus_production(st, unit_types::siege_tank_tank_mode) + count_units_plus_production(st, unit_types::siege_tank_siege_mode);
				int marine_count = count_units_plus_production(st, unit_types::marine);
				if (vulture_count < 2) {
					return nodelay(st, unit_types::vulture, [&](state&st) {
						return depbuild(st, state(st), unit_types::marine);
					});
				}
				if (!my_units_of_type[unit_types::factory].empty() && marine_count < 3) {
					return nodelay(st, unit_types::marine, backbone);
				}
				if (!my_units_of_type[unit_types::starport].empty() && count_units_plus_production(st, unit_types::wraith) == 0) {
					return nodelay(st, unit_types::wraith, backbone);
				}
				if (goliath_count >= 4 && siege_tank_count < 2) {
					return nodelay(st, unit_types::siege_tank_tank_mode, backbone);
				}
				return backbone(st);
			});
		};

		resource_gathering::max_gas = 100;
		while (true) {
			if (!my_units_of_type[unit_types::factory].empty()) {
				resource_gathering::max_gas = 250;
			}
			if (!my_units_of_type[unit_types::vulture].empty()) {
				resource_gathering::max_gas = 0.0;
				get_upgrades::set_upgrade_value(upgrade_types::spider_mines, -1.0);
				if (players::my_player->has_upgrade(upgrade_types::spider_mines)) {
					get_upgrades::set_upgrade_value(upgrade_types::siege_mode, -1.0);
					if (players::my_player->has_upgrade(upgrade_types::siege_mode)) {
						get_upgrades::set_upgrade_value(upgrade_types::ion_thrusters, -1.0);
					}
				}
			}
			bool expand = false;
			int vulture_count = my_completed_units_of_type[unit_types::vulture].size();
			int goliath_count = my_completed_units_of_type[unit_types::goliath].size();
			int siege_tank_count = my_completed_units_of_type[unit_types::siege_tank_tank_mode].size() + my_completed_units_of_type[unit_types::siege_tank_siege_mode].size();
			int marine_count = my_completed_units_of_type[unit_types::marine].size();
			auto my_st = get_my_current_state();
			bool has_bunker = !my_units_of_type[unit_types::bunker].empty();
			if (my_st.bases.size() > 1 && (siege_tank_count >= 2 && (has_bunker || siege_tank_count >= 3)) || goliath_count + siege_tank_count * 2 >= 6) break;
			if (my_st.bases.size() > 1 && marine_count >= 1) combat::build_bunker_count = 1;
			if (my_st.bases.size() == 1 && current_used_total_supply >= 18 && vulture_count >= 2) {
				if (siege_tank_count >= 2) {
					expand = true;
				}
				for (unit*u : enemy_units) {
					if (!u->type->is_resource_depot) continue;
					bool is_expo = true;
					for (xy p : start_locations) {
						if (u->building->build_pos == p) {
							is_expo = false;
							break;
						}
					}
					if (is_expo) expand = true;
				}
			}
			execute_build(expand, build);

			multitasking::sleep(15 * 5);
		}
		combat::build_bunker_count = 0;
		resource_gathering::max_gas = 0.0;

		//combat::no_aggressive_groups = false;

	}

	void render() {

	}

};

