
struct strat_tvz_opening {

	wall_in::wall_builder wall;

	void run() {

		combat::no_aggressive_groups = true;
		combat::aggressive_wraiths = true;

		using namespace buildpred;

		resource_gathering::max_gas = 100;
		int wall_calculated = 0;
		bool has_wall = false;
		bool has_zergling_tight_wall = false;
		bool being_zergling_rushed = false;
		bool has_built_bunker = false;
		while (true) {

			int my_marine_count = my_units_of_type[unit_types::marine].size();
			int enemy_zergling_count = 0;
			for (unit*e : enemy_units) {
				if (e->type == unit_types::zergling) ++enemy_zergling_count;
			}

			auto build = [&](state&st) {
				if (my_completed_units_of_type[unit_types::factory].empty()) {
					if (enemy_zergling_count > my_marine_count && !being_zergling_rushed) {
						log("waa being zergling rushed!\n");
						being_zergling_rushed = true;
					}
					bool initial_marine = !st.units[unit_types::barracks].empty() && count_units_plus_production(st, unit_types::marine) == 0;
					if ((being_zergling_rushed && my_marine_count < 5) || initial_marine) {
						return nodelay(st, unit_types::marine, [&](state&st) {
							return nodelay(st, unit_types::scv, [&](state&st) {
								return depbuild(st, state(st), unit_types::vulture);
							});
						});
					}
				} else {
					if (count_units_plus_production(st, unit_types::starport) == 0) {
						return depbuild(st, state(st), unit_types::starport);
					}
					if (count_units_plus_production(st, unit_types::vulture) < 2) return depbuild(st, state(st), unit_types::vulture);
				}
				int scv_count = count_units_plus_production(st, unit_types::scv);
				if (scv_count >= 11 && count_units_plus_production(st, unit_types::barracks) == 0) return depbuild(st, state(st), unit_types::barracks);
				if (scv_count >= 12 && count_units_plus_production(st, unit_types::refinery) == 0) return depbuild(st, state(st), unit_types::refinery);
				if (scv_count >= 16 && count_units_plus_production(st, unit_types::factory) == 0) return depbuild(st, state(st), unit_types::factory);
				return nodelay(st, unit_types::scv, [&](state&st) {
					st.dont_build_refineries = true;
					auto backbone = [&](state&st) {
						return maxprod1(st, unit_types::vulture);
					};
					int vulture_count = count_units_plus_production(st, unit_types::vulture);
					int siege_tank_count = count_units_plus_production(st, unit_types::siege_tank_tank_mode) + count_units_plus_production(st, unit_types::siege_tank_siege_mode);
					int marine_count = count_units_plus_production(st, unit_types::marine);
					if (vulture_count < 2) {
						return nodelay(st, unit_types::vulture, [&](state&st) {
							return depbuild(st, state(st), unit_types::marine);
						});
					}
					if (count_units_plus_production(st, unit_types::factory) && marine_count < 3) {
						return nodelay(st, unit_types::marine, backbone);
					}
					if (count_units_plus_production(st, unit_types::starport)) {
						if (count_units_plus_production(st, unit_types::wraith) == 0) {
							return nodelay(st, unit_types::wraith, backbone);
						}
						if (count_units_plus_production(st, unit_types::armory) == 0) {
							return nodelay(st, unit_types::armory, backbone);
						}
					}
					return backbone(st);
				});
			};

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
			int siege_tank_count = my_completed_units_of_type[unit_types::siege_tank_tank_mode].size() + my_completed_units_of_type[unit_types::siege_tank_siege_mode].size();
			int marine_count = my_completed_units_of_type[unit_types::marine].size();
			int wraith_count = my_completed_units_of_type[unit_types::wraith].size();
			auto my_st = get_my_current_state();
			bool has_bunker = !my_units_of_type[unit_types::bunker].empty();
			if (my_st.bases.size() > 1 && marine_count >= 1) combat::build_bunker_count = 1;
			if (!has_zergling_tight_wall && !my_units_of_type[unit_types::barracks].empty()) {
				combat::build_bunker_count = 1;
			}
			if (!my_units_of_type[unit_types::bunker].empty()) has_built_bunker = true;
			if (has_built_bunker) combat::build_bunker_count = 0;
			if (!my_completed_units_of_type[unit_types::wraith].empty()) break;
			execute_build(false, build);

			if (combat::defence_choke.center != xy()) {
				if (wall_calculated < 2) {
					++wall_calculated;

					wall = wall_in::wall_builder();

					wall.spot = wall_in::find_wall_spot_from_to(unit_types::zealot, combat::my_closest_base, combat::defence_choke.outside, false);
					wall.spot.outside = combat::defence_choke.outside;

					wall.against(wall_calculated == 1 ? unit_types::zergling : unit_types::zealot);
					wall.add_building(unit_types::supply_depot);
					wall.add_building(unit_types::barracks);
					has_wall = true;
					if (wall_calculated == 1) has_zergling_tight_wall = true;
					if (!wall.find()) {
						wall.add_building(unit_types::supply_depot);
						if (!wall.find()) {
							log("failed to find wall in :(\n");
							has_wall = false;
							if (wall_calculated == 1) has_zergling_tight_wall = false;
						}
					}
					if (has_wall) wall_calculated = 2;
				}
			}
			if (has_wall) wall.build();

			multitasking::sleep(15 * 5);
		}
		combat::build_bunker_count = 0;
		resource_gathering::max_gas = 0.0;

		//combat::no_aggressive_groups = false;

	}

	void render() {
		wall.render();
	}

};

